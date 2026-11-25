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
    }

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
