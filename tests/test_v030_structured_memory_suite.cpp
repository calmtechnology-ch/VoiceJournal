/**
 * @file test_v030_structured_memory_suite.cpp
 * @brief Implémentation des tests unitaires v0.3.0 Structured Memory.
 */

#include "test_v030_structured_memory_suite.h"
#include "NativeFileSystem.h"
#include "JournalRepository.h"
#include "ProjectService.h"
#include "TaskService.h"
#include "SearchService.h"
#include "AnalysisService.h"
#include "TimeService.h"
#include "StorageMigration.h"
#include "MockKeyValueStore.h"
#include "MockVoiceEngine.h"
#include "CommandRouter.h"
#include "VoiceController.h"
#include "JournalUI.h"
#include "SerialParserProtocol.h"
#include <cstdio>
#include <cassert>
#include <cstring>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace VoiceJournal::Tests {

namespace {

static int s_v030Passed = 0;
static int s_v030Failed = 0;

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        std::printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++s_v030Failed; \
        return; \
    } \
} while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))
#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))

void runSubTest(const char* name, void (*fn)()) {
    const int before = s_v030Failed;
    fn();
    if (s_v030Failed == before) {
        ++s_v030Passed;
        std::printf("[PASS] v0.3.0: %s\n", name);
    } else {
        std::printf("[FAIL] v0.3.0: %s\n", name);
    }
}

// Nettoyage préalable d'un dossier de test
void cleanupTestDir(NativeFileSystem& fs, bool removeDir = false) {
    fs.remove(JournalRepository::LOG_PATH);
    fs.remove(JournalRepository::TMP_PATH);
    if (removeDir) {
        const std::string& dir = fs.getBaseDir();
        if (!dir.empty() && dir != ".") {
#if defined(_WIN32)
            _rmdir(dir.c_str());
#else
            rmdir(dir.c_str());
#endif
        }
    }
}

// 1. Tests JournalRepository
void test_repo_basic_crud_and_reboot() {
    NativeFileSystem fs("tests_data_repo");
    cleanupTestDir(fs);

    JournalRepository repo(&fs);
    EXPECT_TRUE(repo.begin());
    EXPECT_EQ(repo.countEntries(), 0);

    JournalEntry e1;
    e1.type = EntryType::NOTE;
    e1.text = "Première observation de test";
    e1.createdAt = 1710000000;
    EXPECT_TRUE(repo.addEntry(e1));
    EXPECT_EQ(e1.id, 1);
    EXPECT_EQ(repo.countEntries(), 1);

    JournalEntry readBack;
    EXPECT_TRUE(repo.getEntry(1, readBack));
    EXPECT_EQ(readBack.id, 1);
    EXPECT_EQ(readBack.text, "Première observation de test");
    EXPECT_EQ(readBack.type, EntryType::NOTE);

    // Mise à jour
    readBack.text = "Observation modifiée";
    readBack.status = EntryStatus::IN_PROGRESS;
    EXPECT_TRUE(repo.updateEntry(readBack));

    JournalEntry readUpdated;
    EXPECT_TRUE(repo.getEntry(1, readUpdated));
    EXPECT_EQ(readUpdated.text, "Observation modifiée");
    EXPECT_EQ(readUpdated.status, EntryStatus::IN_PROGRESS);

    // Redémarrage simulé : fermeture et réouverture
    JournalRepository repoReboot(&fs);
    EXPECT_TRUE(repoReboot.begin());
    EXPECT_EQ(repoReboot.countEntries(), 1);

    JournalEntry readReboot;
    EXPECT_TRUE(repoReboot.getEntry(1, readReboot));
    EXPECT_EQ(readReboot.text, "Observation modifiée");
    EXPECT_EQ(readReboot.status, EntryStatus::IN_PROGRESS);

    cleanupTestDir(fs);
}

void test_repo_power_cut_recovery() {
    NativeFileSystem fs("tests_data_powercut");
    cleanupTestDir(fs);

    JournalRepository repo(&fs);
    EXPECT_TRUE(repo.begin());

    for (int i = 0; i < 5; i++) {
        JournalEntry e;
        e.text = "Note numéro " + std::to_string(i + 1);
        EXPECT_TRUE(repo.addEntry(e));
    }
    EXPECT_EQ(repo.countEntries(), 5);

    // Simulation d'une coupure de courant pendant l'écriture d'un 6ème enregistrement
    // Ajout d'octets tronqués à la fin du fichier
    IFileHandle* file = fs.open(JournalRepository::LOG_PATH, "a");
    EXPECT_TRUE(file != nullptr);
    uint8_t garbage[7] = {0xAA, 0x55, 1, 50, 0, 0, 0}; // En-tête tronqué sans payload complet
    file->write(garbage, sizeof(garbage));
    file->flush();
    file->close();
    delete file;

    // Réouverture après crash : le scanner doit réparer le fichier et conserver les 5 notes intactes
    JournalRepository repoRecovered(&fs);
    EXPECT_TRUE(repoRecovered.begin());
    EXPECT_EQ(repoRecovered.countEntries(), 5);

    JournalEntry e5;
    EXPECT_TRUE(repoRecovered.getEntry(5, e5));
    EXPECT_EQ(e5.text, "Note numéro 5");

    cleanupTestDir(fs);
}

void test_repo_compaction() {
    NativeFileSystem fs("tests_data_compact");
    cleanupTestDir(fs);

    JournalRepository repo(&fs);
    EXPECT_TRUE(repo.begin());

    JournalEntry e1; e1.text = "Note 1"; repo.addEntry(e1);
    JournalEntry e2; e2.text = "Note 2"; repo.addEntry(e2);
    JournalEntry e3; e3.text = "Note 3"; repo.addEntry(e3);

    // Suppression de la note 2
    EXPECT_TRUE(repo.deleteEntry(2));
    EXPECT_EQ(repo.countEntries(), 2);

    // Compactage
    EXPECT_TRUE(repo.compact());
    EXPECT_EQ(repo.countEntries(), 2);

    JournalEntry check1, check3;
    EXPECT_TRUE(repo.getEntry(1, check1));
    EXPECT_FALSE(repo.getEntry(2, check1)); // Supprimée
    EXPECT_TRUE(repo.getEntry(3, check3));
    EXPECT_EQ(check3.text, "Note 3");

    cleanupTestDir(fs);
}

// 2. Tests StorageMigration
void test_nvs_migration_idempotence_and_preservation() {
    NativeFileSystem fs("tests_data_migration");
    cleanupTestDir(fs);

    JournalRepository repo(&fs);
    EXPECT_TRUE(repo.begin());

    MockKeyValueStore mockNvs;
    mockNvs.open(AppConfig::NVS_NAMESPACE, false);

    // Écrire 3 notes historiques dans la NVS (format v0.1.0.1 banque A)
    JournalStorage oldStorage(&mockNvs);
    EXPECT_TRUE(oldStorage.begin());
    EXPECT_TRUE(oldStorage.addNote("Ancienne note 1"));
    EXPECT_TRUE(oldStorage.addNote("Ancienne note 2"));
    EXPECT_TRUE(oldStorage.addNote("Ancienne note 3"));
    EXPECT_EQ(oldStorage.count(), 3);

    // 1ère migration
    MigrationResult res1 = StorageMigration::migrate(&mockNvs, repo);
    EXPECT_TRUE(res1.success);
    EXPECT_TRUE(res1.executed);
    EXPECT_EQ(res1.notesMigrated, 3);
    EXPECT_EQ(repo.countEntries(), 3);

    // Vérifier les propriétés des notes migrées (Section 8)
    JournalEntry m1;
    EXPECT_TRUE(repo.getEntry(1, m1));
    EXPECT_EQ(m1.text, "Ancienne note 1");
    EXPECT_EQ(m1.type, EntryType::NOTE);
    EXPECT_EQ(m1.projectId, 0); // Aucun projet inventé
    EXPECT_EQ(m1.createdAt, 0); // Date inconnue préservée

    // 2ème migration (reboot simulé) : doit être idempotente (ALREADY_MIGRATED, 0 ajout)
    MigrationResult res2 = StorageMigration::migrate(&mockNvs, repo);
    EXPECT_TRUE(res2.success);
    EXPECT_FALSE(res2.executed);
    EXPECT_EQ(res2.notesMigrated, 0);
    EXPECT_EQ(repo.countEntries(), 3);

    // Vérifier que la NVS historique est toujours intacte (non destructive)
    JournalStorage verifyNvs(&mockNvs);
    EXPECT_TRUE(verifyNvs.begin());
    EXPECT_EQ(verifyNvs.count(), 3);
    EXPECT_EQ(verifyNvs.getNote(0), "Ancienne note 1");

    cleanupTestDir(fs);
}

// 3. Tests ProjectService
void test_project_service() {
    NativeFileSystem fs("tests_data_proj");
    cleanupTestDir(fs);
    JournalRepository repo(&fs);
    repo.begin();
    TimeService ts;

    ProjectService ps(repo, ts);
    uint32_t id1 = 0, id2 = 0;
    EXPECT_TRUE(ps.createProject("VoiceKit", {"vk", "voice-kit"}, id1));
    EXPECT_EQ(id1, 1);
    EXPECT_TRUE(ps.createProject("RuleBot", {"rb"}, id2));
    EXPECT_EQ(id2, 2);

    // Refus de doublon
    uint32_t dupId = 0;
    EXPECT_FALSE(ps.createProject("voicekit", {}, dupId));
    EXPECT_EQ(dupId, 1);

    // Sélection de projet courant
    EXPECT_EQ(ps.getCurrentProjectId(), 0);
    uint32_t opened = 0;
    EXPECT_TRUE(ps.openProject("voice-kit", opened));
    EXPECT_EQ(opened, 1);
    EXPECT_EQ(ps.getCurrentProjectId(), 1);
    EXPECT_EQ(ps.getCurrentProjectName(), "VoiceKit");

    ps.closeCurrentProject();
    EXPECT_EQ(ps.getCurrentProjectId(), 0);

    cleanupTestDir(fs);
}

// 4. Tests TaskService
void test_task_service() {
    NativeFileSystem fs("tests_data_tasks");
    cleanupTestDir(fs);
    JournalRepository repo(&fs);
    repo.begin();
    TimeService ts;

    TaskService tsrv(repo, ts);
    uint32_t t1 = 0, t2 = 0;
    EXPECT_TRUE(tsrv.addTask("Tester Gemini demain", 1, 0, t1));
    EXPECT_TRUE(tsrv.addTask("Mesurer la mémoire", 0, 0, t2));
    EXPECT_EQ(tsrv.countOpenTasks(), 2);

    // Marquer terminée
    EXPECT_TRUE(tsrv.completeTask(t1));
    EXPECT_EQ(tsrv.countOpenTasks(), 1);

    auto openTasks = tsrv.listTasks(true);
    EXPECT_EQ(openTasks.size(), 1);
    EXPECT_EQ(openTasks[0].id, t2);

    // Rouvrir
    EXPECT_TRUE(tsrv.reopenTask(t1));
    EXPECT_EQ(tsrv.countOpenTasks(), 2);

    cleanupTestDir(fs);
}

// 5. Tests SearchService
void test_search_service() {
    NativeFileSystem fs("tests_data_search");
    cleanupTestDir(fs);
    JournalRepository repo(&fs);
    repo.begin();
    TimeService ts;

    JournalEntry e1; e1.text = "Gemini se déconnecte après quelques secondes."; e1.type = EntryType::PROBLEM; repo.addEntry(e1);
    JournalEntry e2; e2.text = "Consommation mesurée à 120 mA avec OpenAI."; e2.type = EntryType::RESULT; repo.addEntry(e2);
    JournalEntry e3; e3.text = "Tester la connexion WebSocket."; e3.type = EntryType::TASK; repo.addEntry(e3);

    SearchService ss(repo, ts);

    // Recherche par mot-clé accentué / normalisé ("deconnecte" vs "déconnecte")
    SearchCriteria sc1;
    sc1.textQuery = "deconnecte";
    auto r1 = ss.search(sc1);
    EXPECT_EQ(r1.size(), 1);
    EXPECT_EQ(r1[0].id, 1);

    // Recherche problèmes
    auto rProb = ss.searchProblems();
    EXPECT_EQ(rProb.size(), 1);
    EXPECT_EQ(rProb[0].id, 1);

    cleanupTestDir(fs);
}

// 6. Tests TimeService (Zurich, DST, dates relatives)
void test_time_service_relative_dates() {
    TimeService ts;
    ts.begin(false);
    // 25 novembre 2026 14:30:00 UTC = 1795617000 -> 15:30 heure suisse (CET, UTC+1)
    ts.setMockTime(1795617000);
    EXPECT_TRUE(ts.isTimeReliable());
    EXPECT_EQ(ts.formatLocalTime(), "15:30");

    // Simuler une réinitialisation de TZ en UTC (comme VoiceKit::WiFiManager::configTime(0, 0, ...))
#if defined(_WIN32)
    _putenv("TZ=UTC0");
    _tzset();
#else
    setenv("TZ", "UTC0", 1);
    tzset();
#endif
    EXPECT_EQ(ts.formatLocalTime(), "15:30");

    DateRange today = ts.parseRelativeDateFrench("aujourd'hui");
    EXPECT_TRUE(today.isValid);
    EXPECT_TRUE(today.startUtc <= 1795617000);
    EXPECT_TRUE(today.endUtc >= 1795617000);

    DateRange yesterday = ts.parseRelativeDateFrench("hier");
    EXPECT_TRUE(yesterday.isValid);
    EXPECT_TRUE(yesterday.endUtc < today.startUtc);

    DateRange tomorrow = ts.parseRelativeDateFrench("demain");
    EXPECT_TRUE(tomorrow.isValid);
    EXPECT_TRUE(tomorrow.startUtc > today.endUtc);

    DateRange thisWeek = ts.parseRelativeDateFrench("cette semaine");
    EXPECT_TRUE(thisWeek.isValid);

    ts.clearMockTime();
}

// 7. Tests AnalysisService
void test_analysis_service_bounded_context() {
    NativeFileSystem fs("tests_data_analysis");
    cleanupTestDir(fs);
    JournalRepository repo(&fs);
    repo.begin();
    TimeService ts;
    ProjectService ps(repo, ts);
    SearchService ss(repo, ts);

    uint32_t pid = 0;
    ps.createProject("VoiceKit", {}, pid);

    for (int i = 0; i < 15; i++) {
        JournalEntry e;
        e.projectId = pid;
        e.text = "Observation de test " + std::to_string(i);
        repo.addEntry(e);
    }

    AnalysisService as(repo, ps, ss, ts);
    std::string prompt = as.buildProjectAnalysisPrompt(pid);

    EXPECT_TRUE(prompt.find("<OBSERVATIONS_UTILISATEUR>") != std::string::npos);
    EXPECT_TRUE(prompt.find("</OBSERVATIONS_UTILISATEUR>") != std::string::npos);
    EXPECT_TRUE(prompt.size() <= 2000); // Contexte borné

    cleanupTestDir(fs);
}

// 8. Tests CommandRouter v0.3.0
void test_command_router_v030() {
    CommandRouter router(30000, ParserMode::INTELLIGENT);
    router.setStructuredMemoryEnabled(true);

    // 1. Ajout de tâche
    CommandResult r1 = router.route("Je dois tester Gemini demain", 1000);
    EXPECT_EQ(r1.type, CommandType::ADD_TASK);
    EXPECT_TRUE(r1.isExecutable);

    // 2. Classification automatique d'un problème
    CommandResult r2 = router.route("Enregistre un problème : Gemini se déconnecte", 2000);
    EXPECT_EQ(r2.type, CommandType::ADD_NOTE);
    EXPECT_EQ(r2.entryType, EntryType::PROBLEM);
    EXPECT_TRUE(r2.isExecutable);

    // 3. Projets
    CommandResult r3 = router.route("Crée le projet VoiceKit", 3000);
    EXPECT_EQ(r3.type, CommandType::CREATE_PROJECT);
    EXPECT_TRUE(r3.isExecutable);

    CommandResult r4 = router.route("Ouvre le projet VoiceKit", 4000);
    EXPECT_EQ(r4.type, CommandType::OPEN_PROJECT);
    EXPECT_TRUE(r4.isExecutable);

    // 4. Tâches
    CommandResult r5 = router.route("Quelles sont mes tâches ?", 5000);
    EXPECT_EQ(r5.type, CommandType::LIST_TASKS);
    EXPECT_TRUE(r5.isExecutable);

    CommandResult r6 = router.route("La tâche tester Gemini est terminée", 6000);
    EXPECT_EQ(r6.type, CommandType::COMPLETE_TASK);
    EXPECT_TRUE(r6.isExecutable);
}

// =============================================================================
// 9. Tests Storage & Display Recovery (COR-ST01 .. COR-ST07, Section 10)
// =============================================================================

class FaultFileHandle final : public IFileHandle {
public:
    FaultFileHandle(IFileHandle* inner, bool failWrite, bool failRead, bool failTrunc)
        : inner_(inner), failWrite_(failWrite), failRead_(failRead), failTrunc_(failTrunc) {}
    ~FaultFileHandle() override { delete inner_; }

    size_t read(uint8_t* buf, size_t size) override {
        if (failRead_) return 0;
        return inner_->read(buf, size);
    }
    size_t write(const uint8_t* buf, size_t size) override {
        if (failWrite_) return 0;
        return inner_->write(buf, size);
    }
    bool seek(size_t pos) override { return inner_->seek(pos); }
    size_t position() override { return inner_->position(); }
    size_t size() override { return inner_->size(); }
    void flush() override { inner_->flush(); }
    void close() override { inner_->close(); }
    bool truncate(size_t newSize) override {
        if (failTrunc_) return false;
        return inner_->truncate(newSize);
    }

private:
    IFileHandle* inner_;
    bool failWrite_;
    bool failRead_;
    bool failTrunc_;
};

class FaultInjectingFileSystem final : public IFileSystem {
public:
    explicit FaultInjectingFileSystem(IFileSystem* inner) : inner_(inner) {}

    bool mounted = true;
    bool failAllWrites = false;
    bool failTmpWrites = false;
    bool failReads = false;
    bool failTruncate = false;

    bool isMounted() const override { return mounted; }
    bool exists(const char* path) override { return mounted && inner_->exists(path); }
    bool remove(const char* path) override { return mounted && inner_->remove(path); }
    bool rename(const char* oldPath, const char* newPath) override {
        return mounted && inner_->rename(oldPath, newPath);
    }
    size_t fileSize(const char* path) override { return mounted ? inner_->fileSize(path) : 0; }
    size_t totalBytes() override { return mounted ? inner_->totalBytes() : 0; }
    size_t usedBytes() override { return mounted ? inner_->usedBytes() : 0; }

    IFileHandle* open(const char* path, const char* mode) override {
        if (!mounted) return nullptr;
        IFileHandle* h = inner_->open(path, mode);
        if (!h) return nullptr;
        const bool isTmp = (path && std::strstr(path, ".tmp") != nullptr);
        const bool fw = failAllWrites || (failTmpWrites && isTmp);
        return new FaultFileHandle(h, fw, failReads, failTruncate);
    }

private:
    IFileSystem* inner_;
};

void simulateVoiceCommand(MockVoiceEngine& engine, VoiceController& ctrl, const char* text, uint32_t tMs) {
    setMockMillis(tMs);
    EXPECT_TRUE(ctrl.onTalkButtonPressed(tMs));
    const uint32_t turn = engine.activeTurnId;
    EXPECT_TRUE(ctrl.onTalkButtonPressed(tMs + 200));
    engine.simulateTranscription(text, true, turn);
    ctrl.update(tMs + 300);
    // Consommer l'annonce TTS éventuelle et revenir à READY
    if (ctrl.getState() == JournalState::SPEAKING) {
        engine.simulateSpeechFinished();
        ctrl.update(tMs + 400);
    }
}

void test_recovery_add_1_2_3_read_state_nav_reboot_and_list() {
    NativeFileSystem baseFs("tests_data_recovery_flow");
    cleanupTestDir(baseFs);

    MockKeyValueStore nvsStore;
    JournalStorage legacyStorage(&nvsStore);
    EXPECT_TRUE(legacyStorage.begin());
    EXPECT_EQ(legacyStorage.count(), static_cast<size_t>(0));

    {
        JournalRepository repo(&baseFs);
        EXPECT_TRUE(repo.begin());
        TimeService ts;
        ProjectService ps(repo, ts);
        TaskService taskSvc(repo, ts);
        SearchService searchSvc(repo, ts);
        AnalysisService analysisSvc(repo, ps, searchSvc, ts);

        CommandRouter router(30000, ParserMode::INTELLIGENT);
        router.setStructuredMemoryEnabled(true);
        MockVoiceEngine engine;
        JournalUI ui;
        ui.attachServices(&repo, &ps, &taskSvc, &ts);

        VoiceController ctrl(engine, legacyStorage, router);
        ctrl.attachStructuredMemory(&repo, &ps, &taskSvc, &searchSvc, &analysisSvc, &ts, &ui);
        ctrl.onViewUpdated([&ui](const JournalViewModel& vm) { ui.updateView(vm); });
        ui.begin(ctrl.getViewModel());
        ctrl.attachCallbacks();
        engine.simulateReady();

        // Journal initialement vide
        auto snap0 = ui.buildJournalSnapshot();
        EXPECT_EQ(snap0.totalEntries, static_cast<size_t>(0));
        EXPECT_EQ(snap0.visibleCards, static_cast<size_t>(0));
        EXPECT_EQ(snap0.statusBanner, "Journal vide");
        EXPECT_FALSE(snap0.statusIsError);

        // 1. Ajouter une première note -> Compteur 1, carte visible, NVS intacte (0)
        simulateVoiceCommand(engine, ctrl, "Note, première mesure 24 degrés", 1000);
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(1));
        EXPECT_EQ(repo.countEntries(), static_cast<size_t>(1));
        EXPECT_EQ(legacyStorage.count(), static_cast<size_t>(0)); // COR-ST01 : aucune double écriture NVS
        EXPECT_EQ(ui.getCurrentPage(), UiPage::JOURNAL);
        auto snap1 = ui.buildJournalSnapshot();
        EXPECT_EQ(snap1.queryResultCount, static_cast<size_t>(1));
        EXPECT_EQ(snap1.visibleCards, static_cast<size_t>(1));
        EXPECT_EQ(snap1.cards[0].entryId, static_cast<uint32_t>(1));

        // 2. Ajouter une deuxième note -> Compteur 2 et deux cartes visibles
        simulateVoiceCommand(engine, ctrl, "Note, deuxième mesure tension stable", 2000);
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(2));
        auto snap2 = ui.buildJournalSnapshot();
        EXPECT_EQ(snap2.queryResultCount, static_cast<size_t>(2));
        EXPECT_EQ(snap2.visibleCards, static_cast<size_t>(2));

        // 3. Ajouter une troisième note -> Compteur 3 et trois cartes visibles
        simulateVoiceCommand(engine, ctrl, "Note, troisième mesure ventilateur OK", 3000);
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(3));
        auto snap3 = ui.buildJournalSnapshot();
        EXPECT_EQ(snap3.queryResultCount, static_cast<size_t>(3));
        EXPECT_EQ(snap3.visibleCards, static_cast<size_t>(3));
        EXPECT_EQ(snap3.cards[0].entryId, static_cast<uint32_t>(3));
        EXPECT_EQ(snap3.cards[1].entryId, static_cast<uint32_t>(2));
        EXPECT_EQ(snap3.cards[2].entryId, static_cast<uint32_t>(1));

        // 4. Lire la dernière note -> Texte exact de la dernière entrée
        simulateVoiceCommand(engine, ctrl, "Lis la dernière note", 4000);
        EXPECT_FALSE(engine.spokenTexts.empty());
        EXPECT_TRUE(std::string(engine.spokenTexts.back().c_str()).find("troisième mesure ventilateur OK") != std::string::npos);

        // 5. Changer d'état vocal (READY -> LISTENING -> READY) -> Compteur inchangé
        EXPECT_TRUE(ctrl.onTalkButtonPressed(5000));
        EXPECT_EQ(ctrl.getState(), JournalState::LISTENING);
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(3));
        ctrl.onStopButtonPressed(5100);
        EXPECT_EQ(ctrl.getState(), JournalState::READY);
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(3));

        // 6. Naviguer puis revenir au Journal -> Notes toujours visibles
        ui.setPage(UiPage::HOME);
        ui.setPage(UiPage::PROJECTS);
        ui.setPage(UiPage::JOURNAL);
        auto snapNav = ui.buildJournalSnapshot();
        EXPECT_EQ(snapNav.visibleCards, static_cast<size_t>(3));

        // 8. LIST_ENTRIES (queryEntryIds + getEntry) identique au Journal
        auto ids = repo.queryEntryIds(EntryFilter{}, 20, 0);
        EXPECT_EQ(ids.size(), static_cast<size_t>(3));
        EXPECT_EQ(ids[0], snapNav.cards[0].entryId);
        EXPECT_EQ(ids[1], snapNav.cards[1].entryId);
        EXPECT_EQ(ids[2], snapNav.cards[2].entryId);
    }

    // 7. Redémarrer -> Notes conservées
    {
        JournalRepository repoReboot(&baseFs);
        EXPECT_TRUE(repoReboot.begin());
        EXPECT_EQ(repoReboot.countEntries(), static_cast<size_t>(3));
        JournalEntry last;
        EXPECT_TRUE(repoReboot.getEntry(3, last));
        EXPECT_EQ(last.text, "troisième mesure ventilateur OK");
    }

    cleanupTestDir(baseFs, true);
}

void test_recovery_more_than_five_notes_and_over_180_bytes() {
    NativeFileSystem baseFs("tests_data_recovery_cap");
    cleanupTestDir(baseFs);

    MockKeyValueStore nvsStore;
    JournalStorage legacyStorage(&nvsStore);
    EXPECT_TRUE(legacyStorage.begin());

    JournalRepository repo(&baseFs);
    EXPECT_TRUE(repo.begin());
    TimeService ts;
    ProjectService ps(repo, ts);
    TaskService taskSvc(repo, ts);
    SearchService searchSvc(repo, ts);
    AnalysisService analysisSvc(repo, ps, searchSvc, ts);
    CommandRouter router(30000, ParserMode::INTELLIGENT);
    router.setStructuredMemoryEnabled(true);
    MockVoiceEngine engine;
    JournalUI ui;
    ui.attachServices(&repo, &ps, &taskSvc, &ts);
    VoiceController ctrl(engine, legacyStorage, router);
    ctrl.attachStructuredMemory(&repo, &ps, &taskSvc, &searchSvc, &analysisSvc, &ts, &ui);
    ctrl.attachCallbacks();
    engine.simulateReady();

    // Ajouter 7 notes (> limite historique NVS de 5) dont une de 260 octets (> 180 octets NVS)
    for (int i = 1; i <= 6; ++i) {
        std::string cmd = "Note, observation numero " + std::to_string(i);
        simulateVoiceCommand(engine, ctrl, cmd.c_str(), static_cast<uint32_t>(1000 * i));
    }
    std::string longBody(260, 'x');
    std::string longCmd = "Note, " + longBody;
    simulateVoiceCommand(engine, ctrl, longCmd.c_str(), 7000);

    EXPECT_EQ(repo.countEntries(), static_cast<size_t>(7));
    EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(7));
    EXPECT_EQ(legacyStorage.count(), static_cast<size_t>(0)); // Aucune dépendance NVS

    JournalEntry e7;
    EXPECT_TRUE(repo.getEntry(7, e7));
    EXPECT_EQ(e7.text, longBody);

    // Le Journal affiche les 3 dernières entrées (7, 6, 5)
    auto snap = ui.buildJournalSnapshot();
    EXPECT_EQ(snap.totalEntries, static_cast<size_t>(7));
    EXPECT_EQ(snap.visibleCards, static_cast<size_t>(3));
    EXPECT_EQ(snap.cards[0].entryId, static_cast<uint32_t>(7));
    EXPECT_EQ(snap.cards[1].entryId, static_cast<uint32_t>(6));
    EXPECT_EQ(snap.cards[2].entryId, static_cast<uint32_t>(5));

    cleanupTestDir(baseFs, true);
}

void test_recovery_simulated_write_and_mount_failures() {
    NativeFileSystem baseFs("tests_data_recovery_faults");
    cleanupTestDir(baseFs);
    FaultInjectingFileSystem faultFs(&baseFs);

    MockKeyValueStore nvsStore;
    JournalStorage legacyStorage(&nvsStore);
    EXPECT_TRUE(legacyStorage.begin());

    // 1. Échec de montage LittleFS -> erreur explicite, aucune bascule sur NVS
    {
        faultFs.mounted = false;
        JournalRepository repo(&faultFs);
        EXPECT_FALSE(repo.begin());
        EXPECT_FALSE(repo.isInitialized());
        EXPECT_EQ(repo.getLastError(), "FS_NOT_MOUNTED");

        CommandRouter router(30000, ParserMode::INTELLIGENT);
        MockVoiceEngine engine;
        JournalUI ui;
        ui.attachServices(&repo, nullptr, nullptr, nullptr);
        VoiceController ctrl(engine, legacyStorage, router);
        ctrl.attachStructuredMemory(&repo, nullptr, nullptr, nullptr, nullptr, nullptr, &ui);
        ctrl.attachCallbacks();
        engine.simulateReady();

        simulateVoiceCommand(engine, ctrl, "Note, ne doit pas basculer en NVS", 1000);
        EXPECT_EQ(legacyStorage.count(), static_cast<size_t>(0));
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(0));
        EXPECT_TRUE(engine.spokenTexts.empty() ||
                    std::string(engine.spokenTexts.back().c_str()).find("Note enregistree") == std::string::npos);

        auto snap = ui.buildJournalSnapshot();
        EXPECT_FALSE(snap.littleFsMounted);
        EXPECT_TRUE(snap.statusIsError);
        EXPECT_TRUE(snap.statusBanner.find("LittleFS") != std::string::npos);
    }

    // 2. Échec d'écriture simulé -> aucune confirmation de réussite, compteur inchangé
    {
        faultFs.mounted = true;
        JournalRepository repo(&faultFs);
        EXPECT_TRUE(repo.begin());

        CommandRouter router(30000, ParserMode::INTELLIGENT);
        MockVoiceEngine engine;
        JournalUI ui;
        ui.attachServices(&repo, nullptr, nullptr, nullptr);
        VoiceController ctrl(engine, legacyStorage, router);
        ctrl.attachStructuredMemory(&repo, nullptr, nullptr, nullptr, nullptr, nullptr, &ui);
        ctrl.attachCallbacks();
        engine.simulateReady();

        faultFs.failAllWrites = true;
        simulateVoiceCommand(engine, ctrl, "Note, ecriture en echec", 2000);
        faultFs.failAllWrites = false;

        EXPECT_EQ(repo.countEntries(), static_cast<size_t>(0));
        EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(0));
        EXPECT_EQ(legacyStorage.count(), static_cast<size_t>(0));
        EXPECT_TRUE(engine.spokenTexts.empty() ||
                    std::string(engine.spokenTexts.back().c_str()).find("Note enregistree") == std::string::npos);
        EXPECT_TRUE(repo.getWriteErrorCount() > 0);
    }

    // 3. Échec getEntry() simulé dans JournalUI -> erreur explicite, jamais carte vide/ancienne
    {
        faultFs.mounted = true;
        faultFs.failAllWrites = false;
        faultFs.failReads = false;
        JournalRepository repo(&faultFs);
        EXPECT_TRUE(repo.begin());
        JournalEntry e;
        e.text = "Observation valide";
        EXPECT_TRUE(repo.addEntry(e));

        JournalUI ui;
        ui.attachServices(&repo, nullptr, nullptr, nullptr);

        faultFs.failReads = true;
        auto snapErr = ui.buildJournalSnapshot();
        faultFs.failReads = false;

        EXPECT_EQ(snapErr.readErrors, static_cast<size_t>(1));
        EXPECT_TRUE(snapErr.cards[0].visible);
        EXPECT_TRUE(snapErr.cards[0].readError);
        EXPECT_TRUE(snapErr.cards[0].text.find("Erreur lecture") != std::string::npos);
        EXPECT_EQ(snapErr.cards[0].errorReason, "GET_ENTRY_SHORT_READ");
    }

    cleanupTestDir(baseFs, true);
}

void test_recovery_clear_preserves_tasks_and_projects_and_filter_reset() {
    NativeFileSystem baseFs("tests_data_recovery_clear");
    cleanupTestDir(baseFs);

    MockKeyValueStore nvsStore;
    JournalStorage legacyStorage(&nvsStore);
    EXPECT_TRUE(legacyStorage.begin());

    JournalRepository repo(&baseFs);
    EXPECT_TRUE(repo.begin());
    TimeService ts;
    ProjectService ps(repo, ts);
    TaskService taskSvc(repo, ts);
    SearchService searchSvc(repo, ts);
    AnalysisService analysisSvc(repo, ps, searchSvc, ts);
    CommandRouter router(30000, ParserMode::INTELLIGENT);
    router.setStructuredMemoryEnabled(true);
    MockVoiceEngine engine;
    JournalUI ui;
    ui.attachServices(&repo, &ps, &taskSvc, &ts);
    VoiceController ctrl(engine, legacyStorage, router);
    ctrl.attachStructuredMemory(&repo, &ps, &taskSvc, &searchSvc, &analysisSvc, &ts, &ui);
    ctrl.attachCallbacks();
    engine.simulateReady();

    // Créer 1 projet, 1 tâche et 2 observations
    uint32_t pid = 0;
    EXPECT_TRUE(ps.createProject("Labo", {}, pid));
    uint32_t tid = 0;
    EXPECT_TRUE(taskSvc.addTask("Verifier alimentation", pid, 0, tid));
    simulateVoiceCommand(engine, ctrl, "Note, observation alpha", 1000);
    simulateVoiceCommand(engine, ctrl, "Note, observation beta", 2000);
    EXPECT_EQ(repo.countEntries(), static_cast<size_t>(3)); // 1 tâche + 2 observations

    // Tester filtre vide vs journal vide + reset filtre (COR-ST06)
    ui.setJournalFilter(999, -1, "ProjetVide");
    auto snapFilteredEmpty = ui.buildJournalSnapshot();
    EXPECT_TRUE(snapFilteredEmpty.filterActive);
    EXPECT_EQ(snapFilteredEmpty.totalEntries, static_cast<size_t>(3));
    EXPECT_EQ(snapFilteredEmpty.queryResultCount, static_cast<size_t>(0));
    EXPECT_EQ(snapFilteredEmpty.statusBanner, "Aucun résultat pour ce filtre");

    ui.clearJournalFilter();
    auto snapReset = ui.buildJournalSnapshot();
    EXPECT_FALSE(snapReset.filterActive);
    EXPECT_EQ(snapReset.visibleCards, static_cast<size_t>(3));

    // Suppression confirmée des observations : ne doit supprimer NI le projet NI la tâche
    simulateVoiceCommand(engine, ctrl, "Efface toutes les notes", 3000);
    EXPECT_TRUE(ctrl.getViewModel().clearConfirmPending);
    simulateVoiceCommand(engine, ctrl, "Confirmer la suppression", 4000);

    EXPECT_EQ(ps.listProjects().size(), static_cast<size_t>(1));
    EXPECT_EQ(taskSvc.countOpenTasks(), static_cast<size_t>(1));
    EXPECT_EQ(repo.countEntries(), static_cast<size_t>(1)); // Seule la tâche subsiste
    EXPECT_EQ(ctrl.getViewModel().noteCount, static_cast<size_t>(1));

    cleanupTestDir(baseFs, true);
}

void test_recovery_truncate_and_compact_failure_preserves_original() {
    NativeFileSystem baseFs("tests_data_recovery_integrity");
    cleanupTestDir(baseFs);
    FaultInjectingFileSystem faultFs(&baseFs);

    // 1. Préparer un journal valide avec 2 notes
    {
        JournalRepository repo(&faultFs);
        EXPECT_TRUE(repo.begin());
        JournalEntry e1; e1.text = "Note intacte 1"; EXPECT_TRUE(repo.addEntry(e1));
        JournalEntry e2; e2.text = "Note intacte 2"; EXPECT_TRUE(repo.addEntry(e2));
    }

    // 2. Échec d'écriture pendant compact() -> retourne false et préserve l'original intact
    {
        JournalRepository repo(&faultFs);
        EXPECT_TRUE(repo.begin());
        faultFs.failTmpWrites = true;
        EXPECT_FALSE(repo.compact());
        faultFs.failTmpWrites = false;
        EXPECT_EQ(repo.countEntries(), static_cast<size_t>(2));
        JournalEntry check;
        EXPECT_TRUE(repo.getEntry(2, check));
        EXPECT_EQ(check.text, "Note intacte 2");
    }

    // 3. Injecter des octets corrompus en fin de fichier et simuler un échec truncate() -> begin() échoue proprement
    {
        IFileHandle* f = baseFs.open(JournalRepository::LOG_PATH, "a");
        EXPECT_TRUE(f != nullptr);
        uint8_t garbage[7] = {0xAA, 0x55, 1, 99, 0, 0, 0};
        f->write(garbage, sizeof(garbage));
        f->flush();
        f->close();
        delete f;

        faultFs.failTruncate = true;
        JournalRepository repoFail(&faultFs);
        EXPECT_FALSE(repoFail.begin());
        EXPECT_FALSE(repoFail.isInitialized());
        EXPECT_EQ(repoFail.getLastError(), "TRUNCATE_REPAIR_FAILED");
        faultFs.failTruncate = false;

        // Ensuite, avec truncate fonctionnel, les 2 notes originales sont toujours intactes et réparables !
        JournalRepository repoOk(&faultFs);
        EXPECT_TRUE(repoOk.begin());
        EXPECT_EQ(repoOk.countEntries(), static_cast<size_t>(2));
        JournalEntry check;
        EXPECT_TRUE(repoOk.getEntry(2, check));
        EXPECT_EQ(check.text, "Note intacte 2");
    }

    cleanupTestDir(baseFs, true);
}

} // namespace

V030Report runV030StructuredMemorySuite() {
    std::printf("\n=== VoiceJournal v0.3.0 — Suite Structured Memory ===\n");
    s_v030Passed = 0;
    s_v030Failed = 0;

    runSubTest("JournalRepository: CRUD, persistance et reprise apres redemarrage", test_repo_basic_crud_and_reboot);
    runSubTest("JournalRepository: recuperation et reparation apres coupure d'alimentation", test_repo_power_cut_recovery);
    runSubTest("JournalRepository: compactage controlle et preservation", test_repo_compaction);
    runSubTest("StorageMigration: migration NVS idempotente et non destructive", test_nvs_migration_idempotence_and_preservation);
    runSubTest("ProjectService: creation, contexte courant et recherche", test_project_service);
    runSubTest("TaskService: ajout, terminaison, reouverture et liste", test_task_service);
    runSubTest("SearchService: recherche multicritere et normalisation francaise", test_search_service);
    runSubTest("TimeService: fuseau Zurich, RTC et dates relatives", test_time_service_relative_dates);
    runSubTest("AnalysisService: generation de prompt IA cible et borne", test_analysis_service_bounded_context);
    runSubTest("CommandRouter: intentions structurees v0.3.0 actives", test_command_router_v030);
    runSubTest("Recovery: ajout 1-2-3, lecture, etats, navigation, reboot, LIST_ENTRIES", test_recovery_add_1_2_3_read_state_nav_reboot_and_list);
    runSubTest("Recovery: >5 observations et >180 octets independants de NVS", test_recovery_more_than_five_notes_and_over_180_bytes);
    runSubTest("Recovery: echec montage LittleFS, echec ecriture et echec getEntry", test_recovery_simulated_write_and_mount_failures);
    runSubTest("Recovery: suppression confirmee preserve taches/projets et reset filtre", test_recovery_clear_preserves_tasks_and_projects_and_filter_reset);
    runSubTest("Recovery: echec truncate et compactage preserve le journal original", test_recovery_truncate_and_compact_failure_preserves_original);

    std::printf("\nTotal v0.3.0 Tests : %d PASS, %d FAIL\n", s_v030Passed, s_v030Failed);

    static const char* const s_dirs[] = {
        "tests_data_repo", "tests_data_powercut", "tests_data_compact", "tests_data_migration",
        "tests_data_proj", "tests_data_tasks", "tests_data_search", "tests_data_analysis"
    };
    for (const char* d : s_dirs) {
#if defined(_WIN32)
        _rmdir(d);
#else
        rmdir(d);
#endif
    }

    V030Report report;
    report.passed = static_cast<size_t>(s_v030Passed);
    report.failed = static_cast<size_t>(s_v030Failed);
    return report;
}

} // namespace VoiceJournal::Tests
