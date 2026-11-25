#pragma once

#include <string>
#include <vector>
#include "parser/ParserTables.h"
#include "parser/ParserTypes.h"

namespace VoiceJournal::Parser {

class ConceptDictionary {
public:
    /**
     * @brief Recherche un mot unique dans le lexique (exact en priorité, sinon fuzzy).
     */
    static bool lookupWord(const std::string& word, uint32_t& outMask, bool& outIsFuzzy, std::string& outMatched);

    /**
     * @brief Enrichit la liste des tokens avec les masques de concepts (mots simples et composés).
     */
    static void annotateTokens(std::vector<Token>& tokens, bool& anyFuzzyUsed);

    /**
     * @brief Récupère la liste des concepts distincts présents dans les tokens.
     */
    static std::vector<ConceptId> extractPrimaryConcepts(const std::vector<Token>& tokens);

    static bool hasConcept(uint32_t mask, ConceptId targetConcept) {
        if (targetConcept == ConceptId::NONE) return false;
        const uint32_t cBit = (1u << (static_cast<uint8_t>(targetConcept) - 1));
        return (mask & cBit) != 0;
    }
};

} // namespace VoiceJournal::Parser
