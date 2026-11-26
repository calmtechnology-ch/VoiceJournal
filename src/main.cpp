/**
 * @file main.cpp
 * @brief VoiceJournal v0.3.0 — Structured Memory pour M5Stack CoreS3.
 *
 * Utilise la façade publique <VoiceKit.h>, LittleFS, LVGL 9 et M5Unified.
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <VoiceKit.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "AppConfig.h"
#include "CommandRouter.h"
#include "JournalStorage.h"
#include "JournalUI.h"
#include "SerialParserProtocol.h"
#include "VoiceController.h"

#include "AnalysisService.h"
#include "JournalRepository.h"
#include "LittleFSFileSystem.h"
#include "ProjectService.h"
#include "SearchService.h"
#include "StorageMigration.h"
#include "TaskService.h"
#include "TimeService.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning                                                                       \
    "include/secrets.h introuvable : utilisation des valeurs modeles de secrets.example.h."
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

String sanitizeApiKey(const char *raw) {
  if (!raw)
    return "";
  String k(raw);
  k.trim();
  if (k.startsWith("VOTRE_CLE_API_") || k == "MA_CLE_OPENAI" ||
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
static VoiceJournal::AnalysisService
    s_analysisService(s_repo, s_projectService, s_searchService, s_timeService);

static uint32_t s_lastMemDiagMs = 0;
static bool s_loggedStep12Tls = false;
static bool s_loggedStep13Ws = false;
static bool s_loggedStep14Ready = false;
static uint32_t s_bootStartMs = 0;
static volatile uint8_t s_lastWifiDisconnectReason = 0;
static volatile bool s_wifiDisconnectPending = false;

namespace {

void logBootMemory(uint8_t stepNum, const char *stepName, uint32_t stepDurMs) {
  const size_t intFree =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t intMax =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t intMin =
      heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t dmaFree = heap_caps_get_free_size(MALLOC_CAP_DMA);
  const size_t psFree =
      heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t psMax =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  Serial.printf("[VJ][BOOT][%02u] %s (dt=%u ms, t=%u ms)\n",
                static_cast<unsigned>(stepNum), stepName,
                static_cast<unsigned>(stepDurMs),
                static_cast<unsigned>(millis() - s_bootStartMs));
  Serial.printf("RAM_INT_FREE : %u B\n", static_cast<unsigned>(intFree));
  Serial.printf("RAM_INT_MAX  : %u B\n", static_cast<unsigned>(intMax));
  Serial.printf("RAM_INT_MIN  : %u B\n", static_cast<unsigned>(intMin));
  Serial.printf("DMA_FREE     : %u B\n", static_cast<unsigned>(dmaFree));
  Serial.printf("PSRAM_FREE   : %u B\n", static_cast<unsigned>(psFree));
  Serial.printf("PSRAM_MAX    : %u B\n", static_cast<unsigned>(psMax));
}

const char *wifiReasonToString(uint8_t reason) {
  switch (reason) {
  case 2:
    return "AUTH_EXPIRE";
  case 4:
    return "ASSOC_EXPIRE";
  case 6:
    return "NOT_AUTHED";
  case 7:
    return "NOT_ASSOCED";
  case 8:
    return "ASSOC_LEAVE";
  case 15:
    return "4WAY_HANDSHAKE_TIMEOUT";
  case 200:
    return "BEACON_TIMEOUT";
  case 201:
    return "NO_AP_FOUND";
  case 202:
    return "AUTH_FAIL";
  case 203:
    return "ASSOC_FAIL";
  case 204:
    return "HANDSHAKE_TIMEOUT";
  case 205:
    return "CONNECTION_FAIL";
  default:
    return "OTHER";
  }
}

} // namespace

namespace {

void processSerialCommand(const char *line, CommandRouter &router) {
  if (!line || *line == '\0')
    return;

  String cmdStr(line);
  cmdStr.trim();
  if (cmdStr.length() == 0)
    return;

  if (cmdStr.startsWith("PARSE ")) {
    String input = cmdStr.substring(6);
    input.trim();
    const std::string textStd(input.c_str());
    auto pr = router.getParser().parse(textStd, false);
    Serial.println(VoiceJournal::Protocol::formatParseJson(pr).c_str());
  } else if (cmdStr.startsWith("PARSE_SHADOW ")) {
    String input = cmdStr.substring(13);
    input.trim();
    CommandRouter tempRouter(AppConfig::CLEAR_CONFIRM_TIMEOUT_MS,
                             ParserMode::SHADOW);
    CommandResult res = tempRouter.routeShadow(input, millis());
    Serial.println(
        VoiceJournal::Protocol::formatParseShadowJson(res, input.c_str())
            .c_str());
  } else if (cmdStr == "PARSER_STATUS") {
    Serial.println(VoiceJournal::Protocol::formatParserStatusJson(
                       AppConfig::APP_VERSION,
                       parserModeToLabel(router.getParserMode()),
                       static_cast<uint32_t>(ESP.getFreeHeap()),
                       static_cast<uint32_t>(ESP.getMaxAllocHeap()),
                       static_cast<uint32_t>(ESP.getFreePsram()),
                       VoiceJournal::Parser::GRAMMAR_RULE_COUNT,
                       VoiceJournal::Parser::PHRASE_COUNT)
                       .c_str());
  } else if (cmdStr == "JOURNAL_STATUS") {
    const bool fsMounted = s_fs.isMounted();
    const bool repoReady = s_repo.isInitialized();
    const std::string lastErr =
        !fsMounted ? s_fs.getLastError() : s_repo.getLastError();
    Serial.println(VoiceJournal::Protocol::formatJournalStatusJson(
                       AppConfig::APP_VERSION,
                       repoReady ? s_repo.countEntries() : 0,
                       s_repo.capacity(),
                       repoReady ? s_projectService.listProjects().size() : 0,
                       repoReady ? s_taskService.countOpenTasks() : 0,
                       fsMounted ? s_repo.getUsedBytes() : 0,
                       fsMounted ? s_repo.getTotalBytes() : 0,
                       s_timeService.isTimeReliable(),
                       repoReady ? s_projectService.getCurrentProjectName() : "",
                       s_controller.isStructuredMemoryMode() ? "LITTLEFS"
                                                             : "LEGACY_NVS",
                       s_storage.isInitialized(), s_storage.count(), fsMounted,
                       repoReady, s_repo.getReadErrorCount(),
                       s_repo.getWriteErrorCount(), lastErr)
                       .c_str());
  } else if (cmdStr == "FORMAT_LITTLEFS_CONFIRM") {
    // COR-ST05 : Procédure explicite et sécurisée de première initialisation (jamais automatique)
    if (s_fs.isMounted() && s_fs.exists(VoiceJournal::JournalRepository::LOG_PATH)) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         "FORMAT_REFUSED",
                         "Partition LittleFS deja initialisee : donnees preservees")
                         .c_str());
    } else if (!s_fs.formatExplicitly()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         "FORMAT_FAILED", s_fs.getLastError())
                         .c_str());
    } else if (!s_repo.begin()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         "REPO_INIT_FAILED", s_repo.getLastError())
                         .c_str());
    } else {
      Serial.println("[VJ][STORAGE] Formatage explicite LittleFS OK");
#if VJ_DIAG_ENABLE_MIGRATION
      if (s_storage.getStore()) {
        auto mig = VoiceJournal::StorageMigration::migrate(s_storage.getStore(),
                                                           s_repo);
        Serial.printf("[VJ][MIG] %s (notes migrees=%u)\n",
                      mig.diagnostic.c_str(),
                      static_cast<unsigned>(mig.notesMigrated));
      }
#endif
      s_router.setStructuredMemoryEnabled(true);
      s_controller.setStructuredMemoryMode(true);
      s_ui.refreshJournal();
      Serial.println("{\"command\":\"FORMAT_LITTLEFS_CONFIRM\",\"status\":\"OK\"}");
    }
  } else if (cmdStr.startsWith("LIST_ENTRIES")) {
    if (!s_repo.isInitialized()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         s_fs.isMounted() ? "REPO_NOT_INITIALIZED"
                                          : "LITTLEFS_NOT_MOUNTED",
                         s_fs.isMounted() ? s_repo.getLastError()
                                          : "LittleFS non monte")
                         .c_str());
      return;
    }
    VoiceJournal::EntryFilter ef;
    auto ids = s_repo.queryEntryIds(ef, 20);
    Serial.println("[");
    bool first = true;
    for (size_t i = 0; i < ids.size(); i++) {
      VoiceJournal::JournalEntry e;
      if (s_repo.getEntry(ids[i], e)) {
        if (!first)
          Serial.println(",");
        first = false;
        Serial.print(VoiceJournal::Protocol::formatEntryJson(e).c_str());
      } else {
        Serial.printf("[VJ][STORAGE] LIST_ENTRIES ReadFail ID=%u Reason=%s\n",
                      static_cast<unsigned>(ids[i]),
                      s_repo.getLastError().c_str());
      }
    }
    Serial.println("\n]");
  } else if (cmdStr.startsWith("GET_ENTRY ")) {
    if (!s_repo.isInitialized()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         s_fs.isMounted() ? "REPO_NOT_INITIALIZED"
                                          : "LITTLEFS_NOT_MOUNTED",
                         s_fs.isMounted() ? s_repo.getLastError()
                                          : "LittleFS non monte")
                         .c_str());
      return;
    }
    uint32_t id = static_cast<uint32_t>(cmdStr.substring(10).toInt());
    VoiceJournal::JournalEntry e;
    if (s_repo.getEntry(id, e)) {
      Serial.println(VoiceJournal::Protocol::formatEntryJson(e).c_str());
    } else {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         s_repo.getLastError().empty() ? "NOT_FOUND"
                                                       : s_repo.getLastError(),
                         "Entree introuvable ou illisible")
                         .c_str());
    }
  } else if (cmdStr == "LIST_PROJECTS") {
    if (!s_repo.isInitialized()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         "REPO_NOT_INITIALIZED", "Repository non initialise")
                         .c_str());
      return;
    }
    auto projs = s_projectService.listProjects(false);
    Serial.println("[");
    for (size_t i = 0; i < projs.size(); i++) {
      if (i > 0)
        Serial.println(",");
      Serial.print(VoiceJournal::Protocol::formatProjectJson(projs[i]).c_str());
    }
    Serial.println("\n]");
  } else if (cmdStr == "LIST_TASKS") {
    if (!s_repo.isInitialized()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         "REPO_NOT_INITIALIZED", "Repository non initialise")
                         .c_str());
      return;
    }
    auto tasks = s_taskService.listTasks(false, -1, 50);
    Serial.println("[");
    for (size_t i = 0; i < tasks.size(); i++) {
      if (i > 0)
        Serial.println(",");
      Serial.print(VoiceJournal::Protocol::formatEntryJson(tasks[i]).c_str());
    }
    Serial.println("\n]");
  } else if (cmdStr == "EXPORT_JOURNAL") {
    if (!s_repo.isInitialized()) {
      Serial.println(VoiceJournal::Protocol::formatErrorJson(
                         s_fs.isMounted() ? "REPO_NOT_INITIALIZED"
                                          : "LITTLEFS_NOT_MOUNTED",
                         s_fs.isMounted() ? s_repo.getLastError()
                                          : "LittleFS non monte")
                         .c_str());
      return;
    }
    Serial.println("{\"command\":\"EXPORT_START\",\"version\":\"0.3.0\"}");
    VoiceJournal::EntryFilter ef;
    auto ids = s_repo.queryEntryIds(ef, 1000);
    size_t exported = 0;
    for (uint32_t id : ids) {
      VoiceJournal::JournalEntry e;
      if (s_repo.getEntry(id, e)) {
        Serial.println(VoiceJournal::Protocol::formatEntryJson(e).c_str());
        ++exported;
      }
    }
    Serial.println("{\"command\":\"EXPORT_END\",\"count\":" +
                   String(static_cast<unsigned>(exported)) + "}");
  } else if (cmdStr == "PARSER_HELP") {
    Serial.println(VoiceJournal::Protocol::formatParserHelpJson().c_str());
  } else {
    Serial.println(
        VoiceJournal::Protocol::formatErrorJson(
            "UNKNOWN_COMMAND", String("Commande inconnue : " + cmdStr).c_str())
            .c_str());
  }
}

void pollSerialCommands(CommandRouter &router) {
  size_t charsProcessed = 0;
  while (Serial.available() > 0 && charsProcessed < 64) {
    int c = Serial.read();
    ++charsProcessed;
    if (c < 0)
      break;
    if (c == '\n' || c == '\r') {
      if (s_serialOverflow) {
        s_serialOverflow = false;
        s_serialCmdLen = 0;
        Serial.println(VoiceJournal::Protocol::formatErrorJson(
                           "INPUT_TOO_LONG",
                           "Ligne serie depassant la capacite maximale (1152)")
                           .c_str());
      } else if (s_serialCmdLen > 0) {
        s_serialCmdBuf[s_serialCmdLen] = '\0';
        processSerialCommand(s_serialCmdBuf, router);
        s_serialCmdLen = 0;
      }
    } else {
      if (s_serialOverflow)
        continue;
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
  s_bootStartMs = millis();

  Serial.println();
  Serial.printf("[VJ][BOOT] %s (LVGL=%d LFS=%d REPO=%d MIG=%d TIME=%d NTP=%d "
                "SVC=%d PSRAM_LV=%d)\n",
                AppConfig::APP_TITLE, VJ_DIAG_ENABLE_LVGL,
                VJ_DIAG_ENABLE_LITTLEFS, VJ_DIAG_ENABLE_REPO,
                VJ_DIAG_ENABLE_MIGRATION, VJ_DIAG_ENABLE_TIMESERVICE,
                VJ_DIAG_ENABLE_TIMESERVICE_NTP,
                VJ_DIAG_ENABLE_STRUCTURED_SERVICES, VJ_DIAG_LVGL_PSRAM_POOL);

  // [01] Avant M5.begin()
  logBootMemory(1, "BEFORE_M5", 0);

  // 1. Initialisation matérielle M5Stack CoreS3
  uint32_t t0 = millis();
  auto m5cfg = M5.config();
  m5cfg.internal_spk = true;
  m5cfg.internal_mic = true;
  M5.begin(m5cfg);
  logBootMemory(2, "AFTER_M5", millis() - t0);

  // 2. Initialisation stockage NVS
  t0 = millis();
  if (s_storage.begin()) {
    Serial.println("[VJ][STORAGE] NVS initialisee");
  } else {
    Serial.printf("[VJ][ERROR] Echec NVS: %s\n",
                  s_storage.getLastError().c_str());
  }
  logBootMemory(3, "AFTER_NVS", millis() - t0);

  // 3. Montage LittleFS et initialisation JournalRepository (COR-ST05)
  t0 = millis();
#if VJ_DIAG_ENABLE_LITTLEFS
  bool fsOk = s_fs.begin();
  if (fsOk) {
    Serial.println("[VJ][STORAGE] Backend=LITTLEFS");
    Serial.println("[VJ][STORAGE] Mounted=YES");
  } else {
    Serial.println("[VJ][STORAGE] Backend=LITTLEFS");
    Serial.println("[VJ][STORAGE] Mounted=NO");
    Serial.println("[VJ][WARN] LittleFS indisponible (non formate automatiquement). "
                   "Commande serie explicite si partition vierge : FORMAT_LITTLEFS_CONFIRM");
  }
#else
  bool fsOk = false;
  Serial.println("[VJ][DIAG] LittleFS desactive");
#endif
  logBootMemory(4, "AFTER_LITTLEFS", millis() - t0);

  t0 = millis();
  bool repoOk = false;
#if VJ_DIAG_ENABLE_LITTLEFS && VJ_DIAG_ENABLE_REPO
  if (fsOk) {
    repoOk = s_repo.begin();
    if (repoOk) {
      Serial.println("[VJ][STORAGE] RepositoryReady=YES");
      const size_t entryCount = s_repo.countEntries();
      Serial.printf("[VJ][STORAGE] Count=%u%s\n",
                    static_cast<unsigned>(entryCount),
                    (entryCount == 0) ? " (Journal vide)" : "");
    } else {
      Serial.println("[VJ][STORAGE] RepositoryReady=NO");
      Serial.printf("[VJ][ERROR] Journal endommage / echec init repository : %s\n",
                    s_repo.getLastError().c_str());
    }
  } else {
    Serial.println("[VJ][STORAGE] RepositoryReady=NO");
  }
#else
  Serial.println("[VJ][DIAG] JournalRepository desactive");
#endif
  logBootMemory(5, "AFTER_REPO", millis() - t0);

  // 4. Initialisation du service horaire (UTC + Europe/Zurich)
  t0 = millis();
#if VJ_DIAG_ENABLE_TIMESERVICE
  s_timeService.begin(VJ_DIAG_ENABLE_TIMESERVICE_NTP != 0);
#else
  Serial.println("[VJ][DIAG] TimeService desactive");
#endif
  logBootMemory(6, "AFTER_TIME", millis() - t0);

  // 5. Migration automatique et idempotente NVS -> LittleFS
  t0 = millis();
#if VJ_DIAG_ENABLE_MIGRATION && VJ_DIAG_ENABLE_LITTLEFS && VJ_DIAG_ENABLE_REPO
  if (s_storage.getStore() && s_repo.isInitialized()) {
    auto mig =
        VoiceJournal::StorageMigration::migrate(s_storage.getStore(), s_repo);
    Serial.printf("[VJ][MIG] %s (notes migrees=%u)\n", mig.diagnostic.c_str(),
                  static_cast<unsigned>(mig.notesMigrated));
  }
#else
  Serial.println("[VJ][DIAG] Migration desactivee");
#endif
  logBootMemory(7, "AFTER_MIGRATION", millis() - t0);

  // 6. Configuration du routeur et liaison des services (COR-ST01 / COR-ST05)
#if VJ_DIAG_ENABLE_STRUCTURED_SERVICES
  // N'activer les commandes mémoire structurée dans le routeur que si le repository est prêt
  s_router.setStructuredMemoryEnabled(repoOk);
  s_ui.attachServices(
#if VJ_DIAG_ENABLE_REPO
      &s_repo,
#else
      nullptr,
#endif
      &s_projectService, &s_taskService,
#if VJ_DIAG_ENABLE_TIMESERVICE
      &s_timeService
#else
      nullptr
#endif
  );
  // VoiceController reste en mode structuré (source unique LittleFS, jamais de bascule silencieuse NVS)
  s_controller.attachStructuredMemory(
#if VJ_DIAG_ENABLE_REPO
      &s_repo,
#else
      nullptr,
#endif
      &s_projectService, &s_taskService, &s_searchService, &s_analysisService,
#if VJ_DIAG_ENABLE_TIMESERVICE
      &s_timeService,
#else
      nullptr,
#endif
      &s_ui);
#else
  s_router.setStructuredMemoryEnabled(false);
  s_controller.setStructuredMemoryMode(false);
  Serial.println("[VJ][DIAG] Mode legacy NVS explicitement active");
#endif

  s_controller.onViewUpdated([](const JournalViewModel &vm) {
    s_ui.updateView(vm);
    if (!s_loggedStep12Tls && vm.state == JournalState::CONNECTING &&
        WiFi.status() == WL_CONNECTED) {
      s_loggedStep12Tls = true;
      logBootMemory(12, "BEFORE_TLS", 0);
    }
    if (!s_loggedStep14Ready && vm.state == JournalState::READY) {
      s_loggedStep14Ready = true;
      logBootMemory(14, "AFTER_READY", 0);
    }
  });

  // 7. Initialisation LVGL / IHM
  logBootMemory(8, "BEFORE_LVGL", 0);
  t0 = millis();
  s_ui.begin(s_controller.getViewModel());
  logBootMemory(9, "AFTER_LVGL", millis() - t0);

  // 8. Configuration de VoiceKit
  const AiProvider initialProvider = AppConfig::DEFAULT_AI_PROVIDER;
  const String openAiKey = sanitizeApiKey(OPENAI_KEY);
  const String geminiKey = sanitizeApiKey(GEMINI_KEY);

  Serial.printf("[VJ][VOICE] Initialisation VoiceKit (%s)\n",
                aiProviderToLogString(initialProvider));
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
      Serial.printf("[VJ][WIFI] STA_CONNECTED ch=%u\n",
                    static_cast<unsigned>(info.wifi_sta_connected.channel));
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      const uint8_t r = info.wifi_sta_disconnected.reason;
      s_lastWifiDisconnectReason = r;
      s_wifiDisconnectPending = true;
      Serial.printf("[VJ][WIFI] STA_DISCONNECTED reason=%u (%s)\n",
                    static_cast<unsigned>(r), wifiReasonToString(r));
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      s_wifiDisconnectPending = false;
      Serial.printf("[VJ][WIFI] GOT_IP %s (RSSI=%d ch=%u)\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                    static_cast<unsigned>(WiFi.channel()));
    }
  });
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

  logBootMemory(10, "BEFORE_VOICEKIT", 0);
  t0 = millis();
  if (!s_voiceAdapter.begin(cfg)) {
    String err = s_voiceAdapter.getLastError();
    if (err.length() == 0)
      err = "Echec initialisation VoiceKit";
    s_controller.notifyInitError(err);
  } else {
    s_controller.syncProviderFromEngine();
    if (!s_voiceAdapter.hasApiKeyForProvider(initialProvider)) {
      String err = String("Cle API ") +
                   aiProviderToDisplayName(initialProvider) +
                   " manquante (secrets.h)";
      s_controller.notifyInitError(err);
    }
  }
  logBootMemory(11, "AFTER_VOICEKIT", millis() - t0);

  s_ui.renderIfNeeded();
}

void loop() {
  const uint32_t nowMs = millis();

  // 1. Mise à jour matérielle M5Unified et horloge
  M5.update();
#if VJ_DIAG_ENABLE_TIMESERVICE
  s_timeService.update();
#endif

  // 2. Mise à jour VoiceKit + machine à états applicative
  s_controller.update(nowMs);

  // Remonter l'erreur Wi-Fi réelle à l'écran (évite "Connexion en cours" muet)
  if (s_wifiDisconnectPending) {
    s_wifiDisconnectPending = false;
    const uint8_t r = s_lastWifiDisconnectReason;
    if (s_controller.getState() == JournalState::CONNECTING ||
        s_controller.getState() == JournalState::DISCONNECTED) {
      JournalViewModel vm = s_controller.getViewModel();
      vm.statusMessage = String("Wi-Fi echec: ") + wifiReasonToString(r) +
                         " (" + String(static_cast<unsigned>(r)) + ")";
      s_ui.updateView(vm);
    }
  }

  // Suivi mémoire étapes [12] BEFORE_TLS et [13] AFTER_WS
  if (!s_loggedStep12Tls &&
      s_voiceAdapter.getState() == VoiceEngineState::CONNECTING_SERVICE) {
    s_loggedStep12Tls = true;
    logBootMemory(12, "BEFORE_TLS", 0);
  }
  if (!s_loggedStep13Ws && s_voiceAdapter.isReady()) {
    s_loggedStep13Ws = true;
    logBootMemory(13, "AFTER_WS", 0);
  }

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
    if (!s_repo.isInitialized()) break;
    uint32_t eid = s_ui.getSelectedDetailEntryId();
    VoiceJournal::JournalEntry entry;
    if (s_repo.getEntry(eid, entry)) {
      if (entry.type == VoiceJournal::EntryType::PROBLEM) {
        entry.status = (entry.status == VoiceJournal::EntryStatus::RESOLVED)
                           ? VoiceJournal::EntryStatus::OPEN
                           : VoiceJournal::EntryStatus::RESOLVED;
      } else if (entry.type == VoiceJournal::EntryType::TASK) {
        entry.status = (entry.status == VoiceJournal::EntryStatus::COMPLETED)
                           ? VoiceJournal::EntryStatus::OPEN
                           : VoiceJournal::EntryStatus::COMPLETED;
      }
      entry.updatedAt = s_timeService.getCurrentUtc();
      s_repo.updateEntry(entry);
      s_ui.showEntryDetail(eid);
    }
    break;
  }
  case TouchAction::READ_ENTRY: {
    if (!s_repo.isInitialized()) break;
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
    Serial.printf(
        "[VJ][DIAG] intFree=%u B | intMax=%u B | dma=%u B | psram=%u B | "
        "state=%s | wifi=%d\n",
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
        journalStateToLogString(s_controller.getState()),
        static_cast<int>(WiFi.status()));
  }

  delay(5);
}
