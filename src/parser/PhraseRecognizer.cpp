#include "parser/PhraseRecognizer.h"
#include <algorithm>

namespace VoiceJournal::Parser {

std::vector<RecognizedPhrase> PhraseRecognizer::recognize(
    std::vector<Token>& tokens,
    const std::string& originalText
) {
    (void)originalText;
    std::vector<RecognizedPhrase> recognized;
    const size_t n = tokens.size();
    if (n == 0) return recognized;

    for (size_t i = 0; i < n; ++i) {
        if (recognized.size() >= MAX_PHRASES) break;

        // Longest-match : tester les phrases de PHRASE_TABLE
        // Comme PHRASE_TABLE est trié par tokenCount décroissant puis longueur décroissante,
        // la première correspondance rencontrée est garantie d'être la plus longue.
        for (size_t p = 0; p < PHRASE_COUNT; ++p) {
            const auto& pe = PHRASE_TABLE[p];
            const size_t k = pe.tokenCount;
            if (i + k > n) continue;

            // Construire la séquence de k tokens
            std::string seq;
            for (size_t j = 0; j < k; ++j) {
                if (j > 0) seq += " ";
                seq += tokens[i + j].text;
            }

            if (seq == pe.phrase) {
                RecognizedPhrase rp;
                rp.phrase = pe.phrase;
                rp.startToken = i;
                rp.tokenCount = k;
                rp.origStart = tokens[i].origStart;
                rp.origEnd = tokens[i + k - 1].origEnd;
                rp.role = pe.role;
                rp.conceptId = pe.conceptId;
                rp.targetCategory = pe.targetCategory;
                rp.isPositiveObligation = pe.isPositiveObligation;
                rp.isGlobalScope = pe.isGlobalScope;

                recognized.push_back(rp);

                // Annoter les tokens couverts (COR-01, COR-02)
                const uint32_t phraseMask = (pe.conceptId != ConceptId::NONE)
                    ? (1u << (static_cast<uint8_t>(pe.conceptId) - 1))
                    : 0;

                for (size_t j = 0; j < k; ++j) {
                    if (pe.role != SemanticRole::UNKNOWN) {
                        if (j == 0 && (tokens[i].text == "je" || tokens[i].text == "tu" ||
                                       tokens[i].text == "il" || tokens[i].text == "elle" ||
                                       tokens[i].text == "on" || tokens[i].text == "nous" ||
                                       tokens[i].text == "vous" || tokens[i].text == "ils")) {
                            tokens[i].semanticRole = SemanticRole::SUBJECT;
                        } else {
                            tokens[i + j].semanticRole = pe.role;
                        }
                    }
                    if (pe.conceptId != ConceptId::NONE) {
                        tokens[i + j].conceptMask = phraseMask;
                    }
                }

                // Avancer le curseur pour consommer la locution
                i += (k - 1);
                break;
            }
        }
    }

    return recognized;
}

} // namespace VoiceJournal::Parser
