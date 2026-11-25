/**
 * @file MemoryModel.h
 * @brief Modèle de données pour la mémoire structurée — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace VoiceJournal {

/**
 * @enum EntryType
 * @brief Types d'entrées supportés dans la mémoire structurée.
 */
enum class EntryType : uint8_t {
    NOTE = 0,      ///< Observation libre ou générale
    PROBLEM = 1,   ///< Problème technique ou anomalie
    TASK = 2,      ///< Tâche à réaliser
    IDEA = 3,      ///< Idée ou suggestion
    DECISION = 4,  ///< Décision technique ou projet
    RESULT = 5     ///< Résultat de mesure ou de test
};

inline const char* entryTypeToString(EntryType type) {
    switch (type) {
        case EntryType::NOTE:     return "NOTE";
        case EntryType::PROBLEM:  return "PROBLEM";
        case EntryType::TASK:     return "TASK";
        case EntryType::IDEA:     return "IDEA";
        case EntryType::DECISION: return "DECISION";
        case EntryType::RESULT:   return "RESULT";
        default:                  return "NOTE";
    }
}

inline const char* entryTypeToFrench(EntryType type) {
    switch (type) {
        case EntryType::NOTE:     return "Note";
        case EntryType::PROBLEM:  return "Problème";
        case EntryType::TASK:     return "Tâche";
        case EntryType::IDEA:     return "Idée";
        case EntryType::DECISION: return "Décision";
        case EntryType::RESULT:   return "Résultat";
        default:                  return "Note";
    }
}

inline EntryType stringToEntryType(const std::string& str) {
    if (str == "PROBLEM" || str == "PROBLEME") return EntryType::PROBLEM;
    if (str == "TASK" || str == "TACHE")       return EntryType::TASK;
    if (str == "IDEA" || str == "IDEE")        return EntryType::IDEA;
    if (str == "DECISION")                     return EntryType::DECISION;
    if (str == "RESULT" || str == "RESULTAT")  return EntryType::RESULT;
    return EntryType::NOTE;
}

/**
 * @enum EntryStatus
 * @brief Statuts possibles d'une entrée.
 */
enum class EntryStatus : uint8_t {
    NONE = 0,        ///< Aucun statut particulier (défaut pour NOTE, IDEA, DECISION, RESULT)
    OPEN = 1,        ///< Ouvert / À faire (défaut pour TASK et PROBLEM)
    IN_PROGRESS = 2, ///< En cours
    COMPLETED = 3,   ///< Terminée (pour TASK)
    RESOLVED = 4,    ///< Résolu (pour PROBLEM)
    CANCELLED = 5    ///< Annulée (pour TASK)
};

inline const char* entryStatusToString(EntryStatus status) {
    switch (status) {
        case EntryStatus::NONE:        return "NONE";
        case EntryStatus::OPEN:        return "OPEN";
        case EntryStatus::IN_PROGRESS: return "IN_PROGRESS";
        case EntryStatus::COMPLETED:   return "COMPLETED";
        case EntryStatus::RESOLVED:    return "RESOLVED";
        case EntryStatus::CANCELLED:   return "CANCELLED";
        default:                       return "NONE";
    }
}

inline const char* entryStatusToFrench(EntryStatus status) {
    switch (status) {
        case EntryStatus::NONE:        return "Normal";
        case EntryStatus::OPEN:        return "Ouvert";
        case EntryStatus::IN_PROGRESS: return "En cours";
        case EntryStatus::COMPLETED:   return "Terminée";
        case EntryStatus::RESOLVED:    return "Résolu";
        case EntryStatus::CANCELLED:   return "Annulée";
        default:                       return "";
    }
}

inline EntryStatus stringToEntryStatus(const std::string& str) {
    if (str == "OPEN")        return EntryStatus::OPEN;
    if (str == "IN_PROGRESS") return EntryStatus::IN_PROGRESS;
    if (str == "COMPLETED")   return EntryStatus::COMPLETED;
    if (str == "RESOLVED")    return EntryStatus::RESOLVED;
    if (str == "CANCELLED")   return EntryStatus::CANCELLED;
    return EntryStatus::NONE;
}

inline bool isStatusValidForType(EntryType type, EntryStatus status) {
    switch (type) {
        case EntryType::TASK:
            return (status == EntryStatus::OPEN ||
                    status == EntryStatus::IN_PROGRESS ||
                    status == EntryStatus::COMPLETED ||
                    status == EntryStatus::CANCELLED);
        case EntryType::PROBLEM:
            return (status == EntryStatus::OPEN ||
                    status == EntryStatus::RESOLVED);
        case EntryType::NOTE:
        case EntryType::IDEA:
        case EntryType::DECISION:
        case EntryType::RESULT:
        default:
            return (status == EntryStatus::NONE);
    }
}

inline EntryStatus defaultStatusForType(EntryType type) {
    switch (type) {
        case EntryType::TASK:    return EntryStatus::OPEN;
        case EntryType::PROBLEM: return EntryStatus::OPEN;
        default:                 return EntryStatus::NONE;
    }
}

/**
 * @struct JournalEntry
 * @brief Entrée individuelle dans la mémoire structurée.
 */
struct JournalEntry {
    uint32_t id = 0;                     ///< Identifiant unique persistant (1..)
    EntryType type = EntryType::NOTE;    ///< Type d'entrée
    uint32_t projectId = 0;              ///< ID du projet associé (0 si aucun)
    int64_t createdAt = 0;               ///< Horodatage création UTC (secondes epoch, 0 si inconnu)
    int64_t updatedAt = 0;               ///< Horodatage modification UTC (secondes epoch, 0 si aucune)
    EntryStatus status = EntryStatus::NONE; ///< Statut courant
    int64_t dueAt = 0;                   ///< Échéance UTC (secondes epoch, 0 si aucune)
    std::string text;                    ///< Texte original de la transcription (max 1024 octets)
    std::vector<std::string> entities;   ///< Entités détectées (borné à 4)
    std::vector<std::string> tags;       ///< Mots-clés / étiquettes (borné à 4)
};

/**
 * @enum ProjectStatus
 * @brief Statut d'un projet.
 */
enum class ProjectStatus : uint8_t {
    ACTIVE = 0,
    ARCHIVED = 1
};

inline const char* projectStatusToString(ProjectStatus status) {
    return (status == ProjectStatus::ARCHIVED) ? "ARCHIVED" : "ACTIVE";
}

/**
 * @struct Project
 * @brief Définition d'un projet structuré.
 */
struct Project {
    uint32_t id = 0;                     ///< ID unique de projet (1..)
    std::string name;                    ///< Nom du projet (max 48 octets)
    std::vector<std::string> aliases;    ///< Alias de recherche/reconnaissance
    int64_t createdAt = 0;               ///< Date de création UTC (0 si inconnue)
    ProjectStatus status = ProjectStatus::ACTIVE; ///< Statut du projet
};

} // namespace VoiceJournal
