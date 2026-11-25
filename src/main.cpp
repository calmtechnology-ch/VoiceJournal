/**
 * @file main.cpp
 * @brief VoiceJournal v0.1.1 — Carnet de laboratoire vocal intelligent pour M5Stack CoreS3.
 *
 * Utilise la facade publique <VoiceKit.h> (VoiceKit rev 16646c73acf01a8d57408391bc6de15a288b8910),
 * Preferences (NVS), et M5GFX (sans LVGL 9).
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <VoiceKit.h>

#include "AppConfig.h"
#include "CommandRouter.h"
#include "JournalStorage.h"
#include "JournalUI.h"
#include "VoiceController.h"

#if __has_include("secrets.h")
    #include "secrets.h"
#else
    #warning "include/secrets.h introuvable : utilisation des valeurs modeles de secrets.example.h."
    #include "secrets.example.h"
#endif

#ifndef WIFI_SSID
    #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD ""
#endif
#ifndef OPENAI_KEY
    #ifdef OPENAI_API_KEY
        #define OPENAI_KEY OPENAI_API_KEY
    #else
        #define OPENAI_KEY ""
    #endif
#endif
#ifndef GEMINI_KEY
    #ifdef GEMINI_API_KEY
        #define GEMINI_KEY GEMINI_API_KEY
    #else
        #define GEMINI_KEY ""
    #endif
#endif

namespace {

String sanitizeApiKey(const char* raw) {
    if (!raw) return "";
    String k(raw);
    k.trim();
    if (k.startsWith("VOTRE_CLE_API_") ||
        k == "MA_CLE_OPENAI" ||
        k == "MA_CLE_GEMINI") {
        return "";
    }
    return k;
}

static char s_serialCmdBuf[512];
static size_t s_serialCmdLen = 0;

void processSerialCommand(const char* line, CommandRouter& router) {
    if (!line || *line == '\0') return;

    String cmdStr(line);
    cmdStr.trim();
    if (cmdStr.length() == 0) return;

    if (cmdStr.startsWith("PARSE ")) {
        String input = cmdStr.substring(6);
        input.trim();
        const std::string textStd(input.c_str());
        // Analyse avec le parser dans un contexte totalement isole (zero action metier, pas d'armement suppression)
        auto pr = router.getParser().parse(textStd, false);

        Serial.println("[VJ][PARSE_TEST]");
        Serial.printf("Input: %s\n", pr.originalText.c_str());
        Serial.printf("Intent: %s\n", VoiceJournal::Parser::intentToString(pr.intent));
        Serial.printf("Action: %s\n", pr.semanticFrame.action.empty() ? "-" : pr.semanticFrame.action.c_str());
        Serial.printf("Object: %s\n", pr.semanticFrame.object.empty() ? "-" : pr.semanticFrame.object.c_str());
        Serial.printf("Entity: %s\n", pr.semanticFrame.entity.empty() ? "-" : pr.semanticFrame.entity.c_str());
        Serial.printf("Date: %s\n", pr.semanticFrame.relativeDate.empty() ? "-" : pr.semanticFrame.relativeDate.c_str());
        Serial.printf("Executable: %s\n", pr.executable ? "true" : "false");
        Serial.printf("Status: %s\n", VoiceJournal::Parser::parseStatusToString(pr.status));
        Serial.printf("Score: %u\n", static_cast<unsigned>(pr.score));
        Serial.printf("Duration: %u us\n", static_cast<unsigned>(pr.processingTimeUs));
    } else if (cmdStr.startsWith("PARSE_SHADOW ")) {
        String input = cmdStr.substring(13);
        input.trim();
        // Routeur temporaire isole pour ne jamais modifier l'etat reel
        CommandRouter tempRouter(AppConfig::CLEAR_CONFIRM_TIMEOUT_MS, ParserMode::SHADOW);
        CommandResult res = tempRouter.routeShadow(input, millis());

        Serial.println("[VJ][PARSE_SHADOW]");
        Serial.printf("Input: %s\n", input.c_str());
        Serial.printf("Legacy: %s\n", commandTypeToString(res.legacyType));
        Serial.printf("Intelligent: %s\n", VoiceJournal::Parser::intentToString(res.parseResult.intent));
        Serial.printf("Status: %s\n", res.comparisonStatus.c_str());
        Serial.printf("Blocked: %s\n", res.blockedByDivergence ? "true" : "false");
    } else if (cmdStr == "PARSER_STATUS") {
        Serial.println("[VJ][PARSER_STATUS]");
        Serial.println("Version: v0.2.0.1");
        Serial.printf("Mode: %s\n", parserModeToLabel(router.getParserMode()));
        Serial.printf("FreeRAM: %u B\n", static_cast<unsigned>(ESP.getFreeHeap()));
        Serial.printf("MaxAllocBlock: %u B\n", static_cast<unsigned>(ESP.getMaxAllocHeap()));
        Serial.printf("PSRAM: %u B\n", static_cast<unsigned>(ESP.getFreePsram()));
        Serial.printf("Rules: %u\n", static_cast<unsigned>(VoiceJournal::Parser::GRAMMAR_RULE_COUNT));
        Serial.printf("Phrases: %u\n", static_cast<unsigned>(VoiceJournal::Parser::PHRASE_COUNT));
    } else if (cmdStr == "PARSER_HELP") {
        Serial.println("[VJ][PARSER_HELP]");
        Serial.println("Commandes serie disponibles :");
        Serial.println("  PARSE <texte>        : Teste le parsing contextuel sans modifier les notes");
        Serial.println("  PARSE_SHADOW <texte> : Compare les moteurs Legacy et Intelligent (isole)");
        Serial.println("  PARSER_STATUS        : Affiche l'etat memoire et les regles actives");
        Serial.println("  PARSER_HELP          : Affiche cette aide");
    } else {
        Serial.printf("[VJ][ERROR] Commande serie inconnue : %s (Tapez PARSER_HELP)\n", cmdStr.c_str());
    }
}

void pollSerialCommands(CommandRouter& router) {
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (s_serialCmdLen > 0) {
                s_serialCmdBuf[s_serialCmdLen] = '\0';
                processSerialCommand(s_serialCmdBuf, router);
                s_serialCmdLen = 0;
            }
        } else {
            if (s_serialCmdLen + 1 < sizeof(s_serialCmdBuf)) {
                s_serialCmdBuf[s_serialCmdLen++] = static_cast<char>(c);
            } else {
                Serial.println("[VJ][ERROR] Serial input too long (max 512 bytes)");
                s_serialCmdLen = 0;
            }
        }
    }
}

} // namespace

// =============================================================================
// Composants applicatifs globaux
// =============================================================================
static VoiceKit s_voiceKit;
static VoiceKitEngineAdapter s_voiceAdapter(s_voiceKit);
static JournalStorage s_storage;
static CommandRouter s_router(AppConfig::CLEAR_CONFIRM_TIMEOUT_MS);
static VoiceController s_controller(s_voiceAdapter, s_storage, s_router);
static JournalUI s_ui;

static uint32_t s_lastMemDiagMs = 0;

void setup() {
    Serial.begin(AppConfig::SERIAL_BAUD_RATE);
    delay(200);

    Serial.println();
    Serial.printf("[VJ][BOOT] %s\n", AppConfig::APP_TITLE);

    // 1. Initialisation materielle M5Stack CoreS3 (ecran, tactile, micro, haut-parleur)
    auto m5cfg = M5.config();
    m5cfg.internal_spk = true;
    m5cfg.internal_mic = true;
    M5.begin(m5cfg);

    // 2. Initialisation du stockage local NVS avant toute operation reseau
    if (s_storage.begin()) {
        Serial.println("[VJ][STORAGE] NVS initialisee");
        Serial.printf("[VJ][STORAGE] Notes: %u\n", static_cast<unsigned>(s_storage.count()));
    } else {
        Serial.printf("[VJ][ERROR] Echec initialisation NVS: %s\n",
                      s_storage.getLastError().c_str());
    }

    // 3. Initialisation de l'interface tactile M5GFX (aucun ecran bloquant de connexion)
    s_controller.onViewUpdated([](const JournalViewModel& vm) {
        s_ui.updateView(vm);
    });
    s_ui.begin(s_controller.getViewModel());

    // 4. Configuration de VoiceKit (fournisseur initial centralise dans AppConfig.h)
    const AiProvider initialProvider = AppConfig::DEFAULT_AI_PROVIDER;
    const String openAiKey = sanitizeApiKey(OPENAI_KEY);
    const String geminiKey = sanitizeApiKey(GEMINI_KEY);

    Serial.printf("[VJ][VOICE] Initialisation VoiceKit (fournisseur par defaut: %s)\n",
                  aiProviderToLogString(initialProvider));
    s_controller.attachCallbacks();

    VoiceKitConfig cfg;
    cfg.provider = VoiceKitEngineAdapter::toVoiceKitProvider(initialProvider);
    cfg.openAiApiKey = openAiKey;
    cfg.geminiApiKey = geminiKey;
    cfg.apiKey = (initialProvider == AiProvider::OPENAI) ? openAiKey : geminiKey;
    cfg.model = AppConfig::GEMINI_MODEL_NAME;

    cfg.wifiMode = WiFiMode::MANAGED_WIFI;
    cfg.wifiSSID = WIFI_SSID;
    cfg.wifiPassword = WIFI_PASSWORD;

    cfg.voice = defaultVoiceForProvider(initialProvider);
    cfg.language = AppConfig::VOICE_LANGUAGE;
    cfg.volume = AppConfig::VOICE_VOLUME;

    cfg.enableConversation = false;
    cfg.systemInstruction = AppConfig::VOICE_SYSTEM_INSTRUCTION;
    cfg.aiPromptPrefix = "";
    cfg.muteSpeakerDuringSTT = true;
    cfg.sttFinalTimeoutMs = AppConfig::STT_FINAL_TIMEOUT_MS;
    cfg.queuePolicy = QueuePolicy::REJECT_IF_BUSY;

    if (!s_voiceAdapter.begin(cfg)) {
        String err = s_voiceAdapter.getLastError();
        if (err.length() == 0) {
            err = "Echec initialisation VoiceKit";
        }
        s_controller.notifyInitError(err);
    } else {
        s_controller.syncProviderFromEngine();
        if (!s_voiceAdapter.hasApiKeyForProvider(initialProvider)) {
            String err = String("Cle API ") + aiProviderToDisplayName(initialProvider) +
                         " manquante (secrets.h)";
            s_controller.notifyInitError(err);
        }
    }

    s_ui.renderIfNeeded();
}

void loop() {
    const uint32_t nowMs = millis();

    // 1. Mise a jour materielle et tactile M5Stack
    M5.update();

    // 2. Mise a jour VoiceKit + machine a etats applicative (non bloquant)
    s_controller.update(nowMs);

    // 3. Traitement des evenements tactiles (avec anti-rebond)
    const TouchAction action = s_ui.pollTouch(nowMs);
    if (action == TouchAction::STOP) {
        s_controller.onStopButtonPressed(nowMs);
    } else if (action == TouchAction::TALK_TOGGLE) {
        s_controller.onTalkButtonPressed(nowMs);
    } else if (action == TouchAction::SWITCH_PROVIDER) {
        s_controller.onSwitchProviderRequested(nowMs);
    } else if (action == TouchAction::TOGGLE_LAB) {
        s_controller.toggleLabView();
    } else if (action == TouchAction::CYCLE_LAB_SUBPAGE) {
        s_controller.cycleLabViewSubPage();
    }

    // 4. Traitement du protocole de diagnostic serie USB (Section 17)
    pollSerialCommands(s_router);

    // 4. Rafraichissement graphique conditionnel
    s_ui.renderIfNeeded();

    // 5. Diagnostic memoire periodique leger (configurable / desactivable)
    if (AppConfig::MEMORY_DIAG_ENABLED &&
        (nowMs - s_lastMemDiagMs) >= AppConfig::MEMORY_DIAG_INTERVAL_MS) {
        s_lastMemDiagMs = nowMs;
        Serial.printf("[VJ][BOOT] Mem heap=%u B | psram=%u B | notes=%u | state=%s\n",
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()),
                      static_cast<unsigned>(s_storage.count()),
                      journalStateToLogString(s_controller.getState()));
    }

    delay(5);
}
