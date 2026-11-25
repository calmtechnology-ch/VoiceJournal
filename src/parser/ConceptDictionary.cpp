#include "parser/ConceptDictionary.h"
#include "parser/FuzzyMatcher.h"
#include <algorithm>
#include <cstring>

namespace VoiceJournal::Parser {

namespace {

const LexiconEntry* findExact(const std::string& word) {
    size_t left = 0;
    size_t right = LEXICON_COUNT;

    while (left < right) {
        const size_t mid = left + (right - left) / 2;
        const int cmp = std::strcmp(word.c_str(), LEXICON_TABLE[mid].word);
        if (cmp == 0) {
            return &LEXICON_TABLE[mid];
        } else if (cmp < 0) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    return nullptr;
}

} // namespace

bool ConceptDictionary::lookupWord(const std::string& word, uint32_t& outMask, bool& outIsFuzzy, std::string& outMatched) {
    outMask = 0;
    outIsFuzzy = false;
    outMatched.clear();

    if (word.empty()) {
        return false;
    }

    // 1. Recherche exacte prioritaire
    const LexiconEntry* exact = findExact(word);
    if (exact != nullptr) {
        outMask = exact->conceptMask;
        outMatched = exact->word;
        return true;
    }

    // 2. Recherche approximative bornée
    FuzzyMatchResult fuzzy = FuzzyMatcher::findBestLexiconMatch(word);
    if (fuzzy.matched && fuzzy.matchedWord != nullptr) {
        outMask = fuzzy.conceptMask;
        outIsFuzzy = true;
        outMatched = fuzzy.matchedWord;
        return true;
    }

    return false;
}

void ConceptDictionary::annotateTokens(std::vector<Token>& tokens, bool& anyFuzzyUsed) {
    anyFuzzyUsed = false;
    const size_t n = tokens.size();
    if (n == 0) return;

    // 1. Annotation mot par mot
    for (size_t i = 0; i < n; ++i) {
        bool isFuzzy = false;
        std::string matched;
        uint32_t mask = 0;
        if (lookupWord(tokens[i].text, mask, isFuzzy, matched)) {
            tokens[i].conceptMask |= mask;
            tokens[i].isFuzzy = isFuzzy;
            tokens[i].matchedLexicon = matched;
            if (isFuzzy) {
                anyFuzzyUsed = true;
            }
        }
    }

    // 2. Annotation des expressions composées (2 mots)
    for (size_t i = 0; i + 1 < n; ++i) {
        const std::string bigram = tokens[i].text + " " + tokens[i + 1].text;
        const LexiconEntry* entry = findExact(bigram);
        if (entry != nullptr) {
            tokens[i].conceptMask |= entry->conceptMask;
            tokens[i + 1].conceptMask |= entry->conceptMask;
        }
    }
}

std::vector<ConceptId> ConceptDictionary::extractPrimaryConcepts(const std::vector<Token>& tokens) {
    uint32_t combinedMask = 0;
    for (const auto& tok : tokens) {
        combinedMask |= tok.conceptMask;
    }

    std::vector<ConceptId> concepts;
    for (uint8_t c = 1; c <= 31; ++c) {
        const uint32_t bit = (1u << (c - 1));
        if ((combinedMask & bit) != 0) {
            concepts.push_back(static_cast<ConceptId>(c));
        }
    }
    return concepts;
}

} // namespace VoiceJournal::Parser
