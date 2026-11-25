/**
 * @file main.cpp
 * @brief VoiceJournal v0.3.0 — Structured Memory pour M5Stack CoreS3.
 *
 * Utilise la façade publique <VoiceKit.h>, LittleFS, LVGL 9 et M5Unified.
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <VoiceKit.h>

#include "AppConfig.h"
#include "CommandRouter.h"
#include "JournalStorage.h"
#include "JournalUI.h"
#include "SerialParserProtocol.h"
#include "VoiceController.h"

#include "LittleFSFileSystem.h"
#include "JournalRepository.h"
#include "ProjectService.h"
#include "TaskService.h"
#include "SearchService.h"
#include "AnalysisService.h"
#include "TimeService.h"
#include "StorageMigration.h"

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

// Tampon série aligné sur la capacité maximale (1152 octets)
static char s_serialCmdBuf[1152];
static size_t s_serialCmdLen = 0;
static bool s_serialOverflow = false;

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

// Services structurés v0.3.0
static VoiceJournal::LittleFSFileSystem s_fs;
static VoiceJournal::JournalRepository s_repo(&s_fs);
static VoiceJournal::TimeService s_timeService;
static VoiceJournal::ProjectService s_projectService(s_repo, s_timeService);
static VoiceJournal::TaskService s_taskService(s_repo, s_timeService);
static VoiceJournal::SearchService s_searchService(s_repo, s_timeService);
static VoiceJournal::AnalysisService s_analysisService(s_repo, s_projectService, s_searchService, s_timeService);

static uint32_t s_lastMemDiagMs = 0;

namespace {

void processSerialCommand(const char* line, CommandRouter& router) {
    if (!line || *line == '\0') return;

    String cmdStr(line);
    cmdStr.trim();
    if (cmdStr.length() == 0) return;

    if (cmdStr.startsWith("PARSE ")) {
        String input = cmdStr.substring(6);
        input.trim();
        const std::string textStd(input.c_str());
        auto pr = router.getParser().parse(textStd, false);
        Serial.println(VoiceJournal::Protocol::formatParseJson(pr).c_str());
    } else if (cmdStr.startsWith("PARSE_SHADOW ")) {
        String input = cmdStr.substring(13);
        input.trim();
        CommandRouter tempRouter(AppConfig::CLEAR_CONFIRM_TIMEOUT_MS, ParserMode::SHADOW);
        CommandResult res = tempRouter.routeShadow(input, millis());
        Serial.println(VoiceJournal::Protocol::formatParseShadowJson(res, input.c_str()).c_str());
    } else if (cmdStr == "PARSER_STATUS") {
        Serial.println(VoiceJournal::Protocol::formatParserStatusJson(
            AppConfig::APP_VERSION,
            parserModeToLabel(router.getParserMode()),
            static_cast<uint32_t>(ESP.getFreeHeap()),
            static_cast<uint32_t>(ESP.getMaxAllocHeap()),
            static_cast<uint32_t>(ESP.getFreePsram()),
            VoiceJournal::Parser::GRAMMAR_RULE_COUNT,
            VoiceJournal::Parser::PHRASE_COUNT
        ).c_str());
    } else if (cmdStr == "JOURNAL_STATUS") {
        Serial.println(VoiceJournal::Protocol::formatJournalStatusJson(
            AppConfig::APP_VERSION,
            s_repo.countEntries(),
            s_repo.capacity(),
            s_projectService.listProjects().size(),
            s_taskService.countOpenTasks(),
            s_repo.getUsedBytes(),
            s_repo.getTotalBytes(),
            s_timeService.isTimeReliable(),
            s_projectService.getCurrentProjectName()
        ).c_str());
    } else if (cmdStr.startsWith("LIST_ENTRIES")) {
        VoiceJournal::EntryFilter ef;
        auto ids = s_repo.queryEntryIds(ef, 20);
        Serial.println("[");
        for (size_t i = 0; i < ids.size(); i++) {
            VoiceJournal::JournalEntry e;
            if (s_repo.getEntry(ids[i], e)) {
                if (i > 0) Serial.println(",");
                Serial.print(VoiceJournal::Protocol::formatEntryJson(e).c_str());
            }
        }
        Serial.println("\n]");
    } else if (cmdStr.startsWith("GET_ENTRY ")) {
        uint32_t id = static_cast<uint32_t>(cmdStr.substring(10).toInt());
        VoiceJournal::JournalEntry e;
        if (s_repo.getEntry(id, e)) {
            Serial.println(VoiceJournal::Protocol::formatEntryJson(e).c_str());
        } else {
            Serial.println(VoiceJournal::Protocol::formatErrorJson("NOT_FOUND", "Entree introuvable").c_str());
        }
    } else if (cmdStr == "LIST_PROJECTS") {
        auto projs = s_projectService.listProjects(false);
        Serial.println("[");
        for (size_t i = 0; i < projs.size(); i++) {
            if (i > 0) Serial.println(",");
            Serial.print(VoiceJournal::Protocol::formatProjectJson(projs[i]).c_str());
        }
        Serial.println("\n]");
    } else if (cmdStr == "LIST_TASKS") {
        auto tasks = s_taskService.listTasks(false, -1, 50);
        Serial.println("[");
        for (size_t i = 0; i < tasks.size(); i++) {
            if (i > 0) Serial.println(",");
            Serial.print(VoiceJournal::Protocol::formatEntryJson(tasks[i]).c_str());
        }
        Serial.println("\n]");
    } else if (cmdStr == "EXPORT_JOURNAL") {
        Serial.println("{\"command\":\"EXPORT_START\",\"version\":\"0.3.0\"}");
        VoiceJournal::EntryFilter ef;
        auto ids = s_repo.queryEntryIds(ef, 1000);
        for (uint32_t id : ids) {
            VoiceJournal::JournalEntry e;
            if (s_repo.getEntry(id, e)) {
                Serial.println(VoiceJournal::Protocol::formatEntryJson(e).c_str());
            }
        }
        Serial.println("{\"command\":\"EXPORT_END\",\"count\":" + String(static_cast<unsigned>(ids.size())) + "}");
    } else if (cmdStr == "PARSER_HELP") {
        Serial.println(VoiceJournal::Protocol::formatParserHelpJson().c_str());
    } else {
        Serial.println(VoiceJournal::Protocol::formatErrorJson("UNKNOWN_COMMAND", String("Commande inconnue : " + cmdStr).c_str()).c_str());
    }
}

void pollSerialCommands(CommandRouter& router) {
    size_t charsProcessed = 0;
    while (Serial.available() > 0 && charsProcessed < 64) {
        int c = Serial.read();
        ++charsProcessed;
        if (c < 0) break;
        if (c == '\n' || c == '\r') {
            if (s_serialOverflow) {
                s_serialOverflow = false;
                s_serialCmdLen = 0;
                Serial.println(VoiceJournal::Protocol::formatErrorJson("INPUT_TOO_LONG", "Ligne serie depassant la capacite maximale (1152)").c_str());
            } else if (s_serialCmdLen > 0) {
                s_serialCmdBuf[s_serialCmdLen] = '\0';
                processSerialCommand(s_serialCmdBuf, router);
                s_serialCmdLen = 0;
            }
        } else {
            if (s_serialOverflow) continue;
            if (s_serialCmdLen + 1 < sizeof(s_serialCmdBuf)) {
                s_serialCmdBuf[s_serialCmdLen++] = static_cast<char>(c);
            } else {
                s_serialOverflow = true;
                s_serialCmdLen = 0;
            }
        }
    }
}

} // namespace

void setup() {
    Serial.begin(AppConfig::SERIAL_BAUD_RATE);
    delay(200);

    Serial.println();
    Serial.printf("[VJ][BOOT] %s\n", AppConfig::APP_TITLE);

    // 1. Initialisation matérielle M5Stack CoreS3
    auto m5cfg = M5.config();
    m5cfg.internal_spk = true;
    m5cfg.internal_mic = true;
    M5.begin(m5cfg);

    // 2. Initialisation stockage NVS
    if (s_storage.begin()) {
        Serial.println("[VJ][STORAGE] NVS initialisee");
    } else {
        Serial.printf("[VJ][ERROR] Echec NVS: %s\n", s_storage.getLastError().c_str());
    }

    // 3. Montage LittleFS et initialisation JournalRepository
    if (s_fs.begin()) {
        Serial.println("[VJ][FS] LittleFS monte avec succes");
        if (s_repo.begin()) {
            Serial.printf("[VJ][REPO] Repository initialise (%u entrees en memoire)\n",
                          static_cast<unsigned>(s_repo.countEntries()));
        } else {
            Serial.printf("[VJ][ERROR] Echec init repository : %s\n", s_repo.getLastError().c_str());
        }
    } else {
        Serial.println("[VJ][WARN] Echec montage LittleFS (non formate automatiquement)");
    }

    // 4. Initialisation du service horaire (UTC + Europe/Zurich)
    s_timeService.begin(true);

    // 5. Migration automatique et idempotente NVS -> LittleFS
    if (s_storage.getStore() && s_repo.isInitialized()) {
        auto mig = VoiceJournal::StorageMigration::migrate(s_storage.getStore(), s_repo);
        Serial.printf("[VJ][MIG] %s (notes migrees=%u)\n", mig.diagnostic.c_str(), static_cast<unsigned>(mig.notesMigrated));
    }

    // 6. Configuration du routeur en mode structuré v0.3.0
    s_router.setStructuredMemoryEnabled(true);

    // 7. Liaison des services au contrôleur et à l'IHM LVGL 9
    s_ui.attachServices(&s_repo, &s_projectService, &s_taskService, &s_timeService);
    s_controller.attachStructuredMemory(&s_repo, &s_projectService, &s_taskService, &s_searchService, &s_analysisService, &s_timeService, &s_ui);

    s_controller.onViewUpdated([](const JournalViewModel& vm) {
        s_ui.updateView(vm);
    });

    s_ui.begin(s_controller.getViewModel());

    // 8. Configuration de VoiceKit
    const AiProvider initialProvider = AppConfig::DEFAULT_AI_PROVIDER;
    const String openAiKey = sanitizeApiKey(OPENAI_KEY);
    const String geminiKey = sanitizeApiKey(GEMINI_KEY);

    Serial.printf("[VJ][VOICE] Initialisation VoiceKit (%s)\n", aiProviderToLogString(initialProvider));
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
        if (err.length() == 0) err = "Echec initialisation VoiceKit";
        s_controller.notifyInitError(err);
    } else {
        s_controller.syncProviderFromEngine();
        if (!s_voiceAdapter.hasApiKeyForProvider(initialProvider)) {
            String err = String("Cle API ") + aiProviderToDisplayName(initialProvider) + " manquante (secrets.h)";
            s_controller.notifyInitError(err);
        }
    }

    s_ui.renderIfNeeded();
}

void loop() {
    const uint32_t nowMs = millis();

    // 1. Mise à jour matérielle M5Unified et horloge
    M5.update();
    s_timeService.update();

    // 2. Mise à jour VoiceKit + machine à états applicative
    s_controller.update(nowMs);

    // 3. Traitement des événements tactiles LVGL / direct
    const TouchAction action = s_ui.pollTouch(nowMs);
    switch (action) {
        case TouchAction::STOP:
            s_controller.onStopButtonPressed(nowMs);
            break;
        case TouchAction::TALK_TOGGLE:
            s_controller.onTalkButtonPressed(nowMs);
            break;
        case TouchAction::SWITCH_PROVIDER:
            s_controller.onSwitchProviderRequested(nowMs);
            break;
        case TouchAction::NAV_HOME:
            s_ui.setPage(UiPage::HOME);
            break;
        case TouchAction::NAV_JOURNAL:
            s_ui.setPage(UiPage::JOURNAL);
            break;
        case TouchAction::NAV_PROJECTS:
            s_ui.setPage(UiPage::PROJECTS);
            break;
        case TouchAction::NAV_TASKS:
            s_ui.setPage(UiPage::TASKS);
            break;
        case TouchAction::RESOLVE_ENTRY: {
            uint32_t eid = s_ui.getSelectedDetailEntryId();
            VoiceJournal::JournalEntry entry;
            if (s_repo.getEntry(eid, entry)) {
                if (entry.type == VoiceJournal::EntryType::PROBLEM) {
                    entry.status = (entry.status == VoiceJournal::EntryStatus::RESOLVED) ?
                                   VoiceJournal::EntryStatus::OPEN : VoiceJournal::EntryStatus::RESOLVED;
                } else if (entry.type == VoiceJournal::EntryType::TASK) {
                    entry.status = (entry.status == VoiceJournal::EntryStatus::COMPLETED) ?
                                   VoiceJournal::EntryStatus::OPEN : VoiceJournal::EntryStatus::COMPLETED;
                }
                entry.updatedAt = s_timeService.getCurrentUtc();
                s_repo.updateEntry(entry);
                s_ui.showEntryDetail(eid);
            }
            break;
        }
        case TouchAction::READ_ENTRY: {
            uint32_t eid = s_ui.getSelectedDetailEntryId();
            VoiceJournal::JournalEntry entry;
            if (s_repo.getEntry(eid, entry)) {
                s_controller.enqueueSpeech(entry.text.c_str());
            }
            break;
        }
        default:
            break;
    }

    // 4. Protocole série USB
    pollSerialCommands(s_router);

    // 5. Rendu graphique LVGL 9
    s_ui.renderIfNeeded();

    // 6. Diagnostic mémoire périodique
    if (AppConfig::MEMORY_DIAG_ENABLED &&
        (nowMs - s_lastMemDiagMs) >= AppConfig::MEMORY_DIAG_INTERVAL_MS) {
        s_lastMemDiagMs = nowMs;
        Serial.printf("[VJ][DIAG] heap=%u B | psram=%u B | repo=%u | state=%s\n",
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()),
                      static_cast<unsigned>(s_repo.countEntries()),
                      journalStateToLogString(s_controller.getState()));
    }

    delay(5);
}
