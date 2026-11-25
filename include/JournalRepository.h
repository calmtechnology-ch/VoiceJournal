/**
 * @file JournalRepository.h
 * @brief Dépôt LittleFS / natif append-only avec index reconstructible en RAM — VoiceJournal v0.3.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "IFileSystem.h"
#include "MemoryModel.h"

namespace VoiceJournal {

/**
 * @struct EntryFilter
 * @brief Critères de filtrage applicables directement sur l'index en RAM.
 */
struct EntryFilter {
    int32_t projectId = -1;       ///< -1 = tous les projets, 0 = aucun projet, >0 = projet spécifique
    int16_t type = -1;            ///< -1 = tous types, >=0 correspond à EntryType
    int16_t status = -1;          ///< -1 = tous statuts, >=0 correspond à EntryStatus
    int64_t createdAfter = 0;     ///< 0 = sans borne inférieure
    int64_t createdBefore = 0;    ///< 0 = sans borne supérieure
    int64_t dueAfter = 0;
    int64_t dueBefore = 0;
};

/**
 * @struct IndexEntry
 * @brief Entrée d'index conservée en RAM (~32 octets par entrée, 32 Ko pour 1000 entrées).
 */
struct IndexEntry {
    uint32_t id = 0;
    uint32_t fileOffset = 0;     ///< Position du payload dans journal.log
    uint32_t payloadLength = 0;  ///< Longueur du payload
    EntryType type = EntryType::NOTE;
    EntryStatus status = EntryStatus::NONE;
    uint32_t projectId = 0;
    int64_t createdAt = 0;
    int64_t dueAt = 0;
    bool isDeleted = false;
};

/**
 * @struct IndexProject
 * @brief Index des projets en RAM.
 */
struct IndexProject {
    uint32_t id = 0;
    uint32_t fileOffset = 0;
    uint32_t payloadLength = 0;
    ProjectStatus status = ProjectStatus::ACTIVE;
    std::string name;
};

/**
 * @class JournalRepository
 * @brief Gestionnaire de persistance append-only pour les entrées et projets.
 */
class JournalRepository {
public:
    static constexpr size_t MAX_CAPACITY = 1000;
    static constexpr const char* LOG_PATH = "/journal.log";
    static constexpr const char* TMP_PATH = "/journal.tmp";

    explicit JournalRepository(IFileSystem* fs);
    ~JournalRepository();

    JournalRepository(const JournalRepository&) = delete;
    JournalRepository& operator=(const JournalRepository&) = delete;

    /**
     * @brief Initialise le fichier journal et reconstruit l'index en RAM.
     */
    bool begin();

    bool isInitialized() const { return initialized_; }
    const std::string& getLastError() const { return lastError_; }

    // --- Gestion des entrées ---
    bool addEntry(JournalEntry& entry);
    bool updateEntry(const JournalEntry& entry);
    bool deleteEntry(uint32_t id);
    bool getEntry(uint32_t id, JournalEntry& outEntry) const;

    size_t countEntries() const;
    size_t capacity() const { return MAX_CAPACITY; }
    bool isFull() const { return countEntries() >= MAX_CAPACITY; }
    bool isNearFull() const { return countEntries() >= (MAX_CAPACITY * 9 / 10); }

    std::vector<uint32_t> queryEntryIds(const EntryFilter& filter, size_t limit = 1000, size_t offset = 0) const;

    // --- Gestion des projets ---
    bool addProject(Project& project);
    bool updateProject(const Project& project);
    bool getProject(uint32_t id, Project& outProject) const;
    std::vector<Project> listProjects(bool activeOnly = false) const;
    uint32_t findProjectIdByName(const std::string& name) const;

    // --- Maintenance et intégrité ---
    bool compact();
    size_t getUsedBytes() const;
    size_t getTotalBytes() const;

    // Pour tests et diagnostics
    uint32_t getNextEntryId() const { return nextEntryId_; }
    uint32_t getNextProjectId() const { return nextProjectId_; }

private:
    IFileSystem* fs_ = nullptr;
    bool initialized_ = false;
    std::string lastError_;

    uint32_t nextEntryId_ = 1;
    uint32_t nextProjectId_ = 1;

    std::vector<IndexEntry> entriesIndex_;
    std::unordered_map<uint32_t, size_t> entryIdToIdx_;

    std::vector<IndexProject> projectsIndex_;
    std::unordered_map<uint32_t, size_t> projectIdToIdx_;

    static uint32_t computeCrc32(const uint8_t* data, size_t length);
    static std::string serializeEntry(const JournalEntry& entry);
    static bool deserializeEntry(const std::string& json, JournalEntry& entry);
    static std::string serializeProject(const Project& project);
    static bool deserializeProject(const std::string& json, Project& project);

    bool appendRecord(uint8_t recordType, const std::string& payload, uint32_t& outOffset, uint32_t& outLen);
};

} // namespace VoiceJournal
