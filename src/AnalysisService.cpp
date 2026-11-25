/**
 * @file AnalysisService.cpp
 * @brief Implémentation du service d'analyse ciblée pour l'IA — VoiceJournal v0.3.0
 */

#include "AnalysisService.h"
#include <sstream>

namespace VoiceJournal {

AnalysisService::AnalysisService(JournalRepository& repo,
                                 ProjectService& projectService,
                                 SearchService& searchService,
                                 TimeService& timeService)
    : repo_(repo),
      projectService_(projectService),
      searchService_(searchService),
      timeService_(timeService) {
}

std::string AnalysisService::formatEntriesContext(const std::vector<uint32_t>& entryIds) const {
    if (entryIds.empty()) {
        return "<OBSERVATIONS_UTILISATEUR>\n(Aucune observation enregistrée correspondant aux critères)\n</OBSERVATIONS_UTILISATEUR>\n";
    }

    std::ostringstream ss;
    ss << "<OBSERVATIONS_UTILISATEUR>\n";
    size_t currentBytes = 0;

    for (size_t i = 0; i < entryIds.size() && i < MAX_ANALYSIS_ENTRIES; i++) {
        JournalEntry entry;
        if (!repo_.getEntry(entryIds[i], entry)) continue;

        std::string line = "- [" + std::string(entryTypeToString(entry.type)) + " #" + std::to_string(entry.id) + "] ";
        if (entry.projectId > 0) {
            Project p;
            if (projectService_.getProject(entry.projectId, p)) {
                line += "(" + p.name + ") ";
            }
        }
        if (entry.createdAt > 0) {
            line += "[" + timeService_.formatDisplayDate(entry.createdAt) + "] ";
        }
        if (entry.status != EntryStatus::NONE) {
            line += "{" + std::string(entryStatusToString(entry.status)) + "} ";
        }
        line += entry.text + "\n";

        if (currentBytes + line.size() > MAX_PROMPT_PAYLOAD_BYTES) {
            ss << "... (observations supplémentaires tronquées pour respecter la limite mémoire)\n";
            break;
        }

        ss << line;
        currentBytes += line.size();
    }

    ss << "</OBSERVATIONS_UTILISATEUR>\n";
    return ss.str();
}

std::string AnalysisService::buildProjectAnalysisPrompt(uint32_t projectId, const std::string& userRequest) const {
    Project proj;
    std::string projName = "Inconnu";
    if (projectService_.getProject(projectId, proj)) {
        projName = proj.name;
    }

    EntryFilter filter;
    filter.projectId = static_cast<int32_t>(projectId);
    std::vector<uint32_t> entryIds = repo_.queryEntryIds(filter, MAX_ANALYSIS_ENTRIES);

    std::ostringstream ss;
    ss << "Fais une synthèse concise en deux à trois phrases pour le projet '" << projName << "'.\n";
    if (!userRequest.empty()) {
        ss << "Demande utilisateur : " << userRequest << "\n";
    }
    ss << formatEntriesContext(entryIds);
    return ss.str();
}

std::string AnalysisService::buildNotesAnalysisPrompt(const SearchCriteria& criteria, const std::string& userRequest) const {
    std::vector<SearchResult> results = searchService_.search(criteria);
    std::vector<uint32_t> entryIds;
    entryIds.reserve(results.size());
    for (const auto& r : results) {
        entryIds.push_back(r.id);
    }

    std::ostringstream ss;
    ss << "Fais un bilan factuel et concis en deux à trois phrases des observations sélectionnées.\n";
    if (!userRequest.empty()) {
        ss << "Demande utilisateur : " << userRequest << "\n";
    }
    ss << formatEntriesContext(entryIds);
    return ss.str();
}

} // namespace VoiceJournal
