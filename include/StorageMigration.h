/**
 * @file StorageMigration.h
 * @brief Migration non destructive et idempotente NVS v0.2.1 vers LittleFS v0.3.0
 */

#pragma once

#include <string>
#include "JournalStorage.h"
#include "JournalRepository.h"

namespace VoiceJournal {

struct MigrationResult {
    bool executed = false;       ///< true si la migration a été effectuée, false si déjà faite ou vide
    size_t notesMigrated = 0;    ///< Nombre de notes transférées
    bool success = true;         ///< Succès global
    std::string diagnostic;      ///< Message de diagnostic
};

class StorageMigration {
public:
    /**
     * @brief Exécute la migration depuis NVS vers JournalRepository.
     * @param nvsStore Magasin clé-valeur NVS ("vjournal")
     * @param repo Dépôt cible v0.3.0
     * @return Résultat détaillé de la migration
     */
    static MigrationResult migrate(IKeyValueStore* nvsStore, JournalRepository& repo);
};

} // namespace VoiceJournal
