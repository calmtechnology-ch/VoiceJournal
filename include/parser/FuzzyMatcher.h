#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include "parser/ParserTables.h"

namespace VoiceJournal::Parser {

struct FuzzyMatchResult {
    bool matched = false;
    const char* matchedWord = nullptr;
    uint32_t conceptMask = 0;
    uint8_t distance = 0;
};

class FuzzyMatcher {
public:
    static constexpr size_t MIN_WORD_LEN_FOR_FUZZY = 4;
    static constexpr size_t MAX_WORD_LEN_FOR_DIST1 = 7;

    /**
     * @brief Calcule la distance de Damerau-Levenshtein entre deux chaînes ASCII.
     */
    static uint8_t damerauLevenshteinDistance(const std::string& s1, const std::string& s2);

    /**
     * @brief Détermine si un mot est éligible à la recherche approximative.
     *        Refuse les nombres, versions logicielles et identifiants techniques.
     */
    static bool isEligibleForFuzzy(const std::string& word);

    /**
     * @brief Recherche le meilleur candidat dans le lexique autorisé.
     *        Refuse s'il y a égalité entre plusieurs candidats équivalents.
     */
    static FuzzyMatchResult findBestLexiconMatch(const std::string& word);
};

} // namespace VoiceJournal::Parser
