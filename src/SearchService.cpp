/**
 * @file SearchService.cpp
 * @brief Implémentation du moteur de recherche multicritère local — VoiceJournal v0.3.0
 */

#include "SearchService.h"
#include "parser/TextNormalizer.h"
#include <sstream>
#include <algorithm>

namespace VoiceJournal {

SearchService::SearchService(JournalRepository& repo, TimeService& timeService)
    : repo_(repo), timeService_(timeService) {
}

std::vector<std::string> SearchService::tokenizeQuery(const std::string& query) {
    std::string norm = Parser::TextNormalizer::normalizeSimple(query);
    std::vector<std::string> tokens;
    std::istringstream iss(norm);
    std::string token;
    while (iss >> token) {
        if (token.size() >= 2) { // Ignorer les mots d'une seule lettre
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::vector<SearchResult> SearchService::search(const SearchCriteria& criteria) const {
    std::vector<SearchResult> results;

    // 1. Filtrage préalable haute performance via l'index en RAM
    EntryFilter filter;
    filter.projectId = criteria.projectId;
    filter.type = criteria.type;
    filter.status = criteria.status;
    filter.createdAfter = criteria.createdAfter;
    filter.createdBefore = criteria.createdBefore;
    filter.dueAfter = criteria.dueAfter;
    filter.dueBefore = criteria.dueBefore;

    std::vector<uint32_t> candidateIds = repo_.queryEntryIds(filter, 1000);
    if (candidateIds.empty()) {
        return results;
    }

    std::vector<std::string> queryTokens = tokenizeQuery(criteria.textQuery);
    std::string normEntity = Parser::TextNormalizer::normalizeSimple(criteria.entity);

    for (uint32_t id : candidateIds) {
        JournalEntry entry;
        if (!repo_.getEntry(id, entry)) continue;

        int score = 100;

        // Filtrage textuel si mots-clés présents
        if (!queryTokens.empty()) {
            std::string normText = Parser::TextNormalizer::normalizeSimple(entry.text);
            int matchedTokens = 0;
            for (const auto& qt : queryTokens) {
                if (normText.find(qt) != std::string::npos) {
                    matchedTokens++;
                }
            }
            if (matchedTokens == 0) continue; // Aucun mot-clé trouvé
            score = (matchedTokens * 100) / static_cast<int>(queryTokens.size());
        }

        // Filtrage entité si demandée
        if (!normEntity.empty()) {
            bool foundEntity = false;
            for (const auto& ent : entry.entities) {
                if (Parser::TextNormalizer::normalizeSimple(ent).find(normEntity) != std::string::npos) {
                    foundEntity = true;
                    break;
                }
            }
            if (!foundEntity) {
                // Vérifier si l'entité apparaît dans le corps du texte
                std::string normText = Parser::TextNormalizer::normalizeSimple(entry.text);
                if (normText.find(normEntity) != std::string::npos) {
                    foundEntity = true;
                }
            }
            if (!foundEntity) continue;
            score += 20;
        }

        SearchResult res;
        res.id = entry.id;
        res.type = entry.type;
        res.status = entry.status;
        res.projectId = entry.projectId;
        res.createdAt = entry.createdAt;
        res.matchScore = score;

        // Prévisualisation du texte tronquée proprement à 80 octets
        if (entry.text.size() <= 80) {
            res.previewText = entry.text;
        } else {
            size_t cut = 80;
            while (cut > 0 && ((uint8_t)entry.text[cut] & 0xC0) == 0x80) cut--;
            res.previewText = entry.text.substr(0, cut) + "...";
        }

        results.push_back(res);
        if (results.size() >= criteria.limit) break;
    }

    return results;
}

std::vector<SearchResult> SearchService::searchProblems(int32_t projectId, bool openOnly) const {
    SearchCriteria sc;
    sc.type = static_cast<int16_t>(EntryType::PROBLEM);
    sc.projectId = projectId;
    if (openOnly) {
        sc.status = static_cast<int16_t>(EntryStatus::OPEN);
    }
    return search(sc);
}

std::vector<SearchResult> SearchService::searchByProject(uint32_t projectId) const {
    SearchCriteria sc;
    sc.projectId = static_cast<int32_t>(projectId);
    return search(sc);
}

std::vector<SearchResult> SearchService::searchByRelativeDate(const std::string& frenchDateExpr) const {
    DateRange range = timeService_.parseRelativeDateFrench(frenchDateExpr);
    SearchCriteria sc;
    if (range.isValid) {
        sc.createdAfter = range.startUtc;
        sc.createdBefore = range.endUtc;
    }
    return search(sc);
}

} // namespace VoiceJournal
