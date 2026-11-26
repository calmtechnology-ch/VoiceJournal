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
#include "CommandRouter.h"
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
    // 25 novembre 2026 14:30:00 UTC = 1795617000
    ts.setMockTime(1795617000);
    EXPECT_TRUE(ts.isTimeReliable());

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
