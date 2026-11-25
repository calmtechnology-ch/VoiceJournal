/**
 * @file StorageMigration.cpp
 * @brief Implémentation de la migration non destructive NVS v0.2.1 -> LittleFS v0.3.0
 */

#include "StorageMigration.h"

namespace VoiceJournal {

MigrationResult StorageMigration::migrate(IKeyValueStore* nvsStore, JournalRepository& repo) {
    MigrationResult res;
    if (!nvsStore) {
        res.success = false;
        res.diagnostic = "NVS_STORE_NULL";
        return res;
    }

    if (!repo.isInitialized()) {
        res.success = false;
        res.diagnostic = "REPO_NOT_INITIALIZED";
        return res;
    }

    // Ouvrir le namespace NVS "vjournal" en lecture-écriture
    if (!nvsStore->open(AppConfig::NVS_NAMESPACE, false)) {
        res.success = false;
        res.diagnostic = "NVS_OPEN_FAILED";
        return res;
    }

    // Vérifier si la migration a déjà été effectuée (idempotence)
    uint32_t migratedFlag = nvsStore->getUInt("migrated_v030", 0);
    if (migratedFlag != 0) {
        nvsStore->close();
        res.executed = false;
        res.success = true;
        res.diagnostic = "ALREADY_MIGRATED";
        return res;
    }

    // Utiliser JournalStorage pour lire fidèlement les anciennes observations A/B ou legacy
    JournalStorage oldStorage(nvsStore);
    if (!oldStorage.begin()) {
        // Aucune ancienne observation valide trouvée : marquer comme migré pour éviter de re-scanner
        nvsStore->putUInt("migrated_v030", 1);
        nvsStore->close();
        res.executed = false;
        res.success = true;
        res.diagnostic = "NO_OLD_NOTES_FOUND";
        return res;
    }

    size_t count = oldStorage.count();
    if (count == 0) {
        nvsStore->putUInt("migrated_v030", 1);
        nvsStore->close();
        res.executed = false;
        res.success = true;
        res.diagnostic = "ZERO_NOTES_TO_MIGRATE";
        return res;
    }

    // Migrer chaque note dans l'ordre chronologique exact (0 = plus ancienne)
    for (size_t i = 0; i < count; i++) {
        String noteText = oldStorage.getNote(i);
        if (noteText.length() == 0) continue;

        JournalEntry entry;
        entry.type = EntryType::NOTE;
        entry.projectId = 0;          // Aucun projet inventé
        entry.createdAt = 0;          // Date inconnue préservée comme inconnue (0)
        entry.updatedAt = 0;
        entry.status = EntryStatus::NONE;
        entry.dueAt = 0;
        entry.text = std::string(noteText.c_str());

        if (repo.addEntry(entry)) {
            res.notesMigrated++;
        } else {
            res.success = false;
            res.diagnostic = "ADD_ENTRY_FAILED: " + repo.getLastError();
            nvsStore->close();
            return res;
        }
    }

    // Poser le marqueur de migration en NVS (les données NVS historiques restent intactes !)
    nvsStore->putUInt("migrated_v030", 1);
    nvsStore->close();

    res.executed = true;
    res.success = true;
    res.diagnostic = "MIGRATED_" + std::to_string(res.notesMigrated) + "_NOTES";
    return res;
}

} // namespace VoiceJournal
