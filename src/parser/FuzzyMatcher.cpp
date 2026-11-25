#include "parser/FuzzyMatcher.h"
#include <algorithm>
#include <cctype>

namespace VoiceJournal::Parser {

uint8_t FuzzyMatcher::damerauLevenshteinDistance(const std::string& s1, const std::string& s2) {
    const size_t len1 = s1.length();
    const size_t len2 = s2.length();

    if (len1 == 0) return static_cast<uint8_t>(len2);
    if (len2 == 0) return static_cast<uint8_t>(len1);
    if (len1 > 32 || len2 > 32) return 255; // Limite de sécurité

    // Matrice bornée sur pile (33 x 33)
    uint8_t d[33][33];

    for (size_t i = 0; i <= len1; ++i) d[i][0] = static_cast<uint8_t>(i);
    for (size_t j = 0; j <= len2; ++j) d[0][j] = static_cast<uint8_t>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            const uint8_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            uint8_t minVal = static_cast<uint8_t>(std::min({
                d[i - 1][j] + 1,       // suppression
                d[i][j - 1] + 1,       // insertion
                d[i - 1][j - 1] + cost // substitution
            }));

            // Transposition adjacente (Damerau)
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                minVal = std::min(minVal, static_cast<uint8_t>(d[i - 2][j - 2] + 1));
            }

            d[i][j] = minVal;
        }
    }

    return d[len1][len2];
}

bool FuzzyMatcher::isEligibleForFuzzy(const std::string& word) {
    const size_t len = word.length();
    if (len < MIN_WORD_LEN_FOR_FUZZY) {
        return false;
    }

    // Aucun rapprochement approximatif pour les nombres ou mots contenant des chiffres
    for (char c : word) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    // Refus des formats de version logicielle (ex: v0.1, v1)
    if (word[0] == 'v' && len > 1 && std::isdigit(static_cast<unsigned char>(word[1]))) {
        return false;
    }

    // Refus des identifiants techniques spécifiques
    if (word == "wifi" || word == "ble" || word == "nvs" || word == "i2c" || word == "spi" ||
        word == "mqtt" || word == "http" || word == "uart" || word == "gpio") {
        return false;
    }

    return true;
}

FuzzyMatchResult FuzzyMatcher::findBestLexiconMatch(const std::string& word) {
    FuzzyMatchResult res;
    if (!isEligibleForFuzzy(word)) {
        return res;
    }

    const size_t wordLen = word.length();
    const uint8_t maxAllowedDist = (wordLen <= MAX_WORD_LEN_FOR_DIST1) ? 1 : 2;

    uint8_t bestDistance = 255;
    const LexiconEntry* bestCandidate = nullptr;
    size_t candidatesAtBestDistance = 0;

    for (size_t i = 0; i < LEXICON_COUNT; ++i) {
        const LexiconEntry& entry = LEXICON_TABLE[i];
        const std::string target(entry.word);

        // Si la différence de taille excède la distance max autorisée, passer
        const size_t targetLen = target.length();
        if (targetLen < MIN_WORD_LEN_FOR_FUZZY) {
            continue; // Ne jamais faire de fuzzy vers un mot court (< 4 caractères)
        }
        const size_t lenDiff = (wordLen > targetLen) ? (wordLen - targetLen) : (targetLen - wordLen);
        if (lenDiff > maxAllowedDist) {
            continue;
        }

        // Ne jamais faire de rapprochement approximatif pour les confirmations
        if ((entry.conceptMask & ConceptMask::CONFIRM) != 0) {
            continue;
        }

        const uint8_t dist = damerauLevenshteinDistance(word, target);
        if (dist <= maxAllowedDist) {
            if (dist < bestDistance) {
                bestDistance = dist;
                bestCandidate = &entry;
                candidatesAtBestDistance = 1;
            } else if (dist == bestDistance) {
                // Plusieurs candidats à même distance minimale -> ambiguïté
                ++candidatesAtBestDistance;
            }
        }
    }

    // Règle stricte : refus en cas d'égalité entre plusieurs candidats
    if (candidatesAtBestDistance == 1 && bestCandidate != nullptr) {
        res.matched = true;
        res.matchedWord = bestCandidate->word;
        res.conceptMask = bestCandidate->conceptMask;
        res.distance = bestDistance;
    }

    return res;
}

} // namespace VoiceJournal::Parser
