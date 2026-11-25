/**
 * @file AnalysisService.h
 * @brief Construction de requêtes d'analyse IA ciblées et bornées — VoiceJournal v0.3.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "JournalRepository.h"
#include "ProjectService.h"
#include "SearchService.h"
#include "TimeService.h"

namespace VoiceJournal {

class AnalysisService {
public:
    static constexpr size_t MAX_ANALYSIS_ENTRIES = 10;
    static constexpr size_t MAX_PROMPT_PAYLOAD_BYTES = 1500;

    AnalysisService(JournalRepository& repo, ProjectService& projectService, SearchService& searchService, TimeService& timeService);

    /**
     * @brief Construit le prompt borné pour l'analyse d'un projet spécifique.
     */
    std::string buildProjectAnalysisPrompt(uint32_t projectId, const std::string& userRequest = "") const;

    /**
     * @brief Construit le prompt borné pour l'analyse globale ou par date/mots-clés.
     */
    std::string buildNotesAnalysisPrompt(const SearchCriteria& criteria, const std::string& userRequest = "") const;

private:
    JournalRepository& repo_;
    ProjectService& projectService_;
    SearchService& searchService_;
    TimeService& timeService_;

    std::string formatEntriesContext(const std::vector<uint32_t>& entryIds) const;
};

} // namespace VoiceJournal
