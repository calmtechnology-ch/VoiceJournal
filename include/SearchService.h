/**
 * @file SearchService.h
 * @brief Recherche multicritère locale et déterministe — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "JournalRepository.h"
#include "TimeService.h"

namespace VoiceJournal {

struct SearchCriteria {
    std::string textQuery;        ///< Mots-clés recherchés
    std::string entity;           ///< Entité nommée recherchée
    int32_t projectId = -1;       ///< ID de projet (-1 = tous)
    int16_t type = -1;            ///< Type d'entrée (-1 = tous)
    int16_t status = -1;          ///< Statut (-1 = tous)
    int64_t createdAfter = 0;     ///< Date minimale (UTC)
    int64_t createdBefore = 0;    ///< Date maximale (UTC)
    int64_t dueAfter = 0;
    int64_t dueBefore = 0;
    size_t limit = 50;            ///< Nombre maximal de résultats
};

struct SearchResult {
    uint32_t id = 0;
    EntryType type = EntryType::NOTE;
    EntryStatus status = EntryStatus::NONE;
    uint32_t projectId = 0;
    int64_t createdAt = 0;
    std::string previewText;
    int matchScore = 0;
};

class SearchService {
public:
    SearchService(JournalRepository& repo, TimeService& timeService);

    /**
     * @brief Exécute une recherche locale multicritère sur le carnet.
     */
    std::vector<SearchResult> search(const SearchCriteria& criteria) const;

    /**
     * @brief Recherche spécifique des problèmes (ouverts ou tous).
     */
    std::vector<SearchResult> searchProblems(int32_t projectId = -1, bool openOnly = true) const;

    /**
     * @brief Recherche des observations associées à un projet.
     */
    std::vector<SearchResult> searchByProject(uint32_t projectId) const;

    /**
     * @brief Recherche par expression de date relative en français ("hier", "aujourd'hui", etc.).
     */
    std::vector<SearchResult> searchByRelativeDate(const std::string& frenchDateExpr) const;

private:
    JournalRepository& repo_;
    TimeService& timeService_;

    static std::vector<std::string> tokenizeQuery(const std::string& query);
};

} // namespace VoiceJournal
