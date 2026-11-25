#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"
#include "parser/NegationDetector.h"

namespace VoiceJournal::Parser {

struct IntentCandidate {
    IntentId intent = IntentId::UNKNOWN;
    uint8_t score = 0;
    std::string ruleName;
    bool executable = false;
};

class IntentMatcher {
public:
    /**
     * @brief Évalue les règles d'intention sur les tokens et entités analysés.
     * @param tokens Tokens annotés.
     * @param entities Entités détectées.
     * @param negation Analyse de négation.
     * @param confirmationPending Indique si une confirmation de suppression est en attente.
     * @param anyFuzzyUsed Indique si une tolérance aux erreurs a été utilisée.
     * @param originalText Texte original UTF-8.
     */
    static ParseResult match(
        const std::vector<Token>& tokens,
        const std::vector<EntityMatch>& entities,
        const NegationAnalysis& negation,
        bool confirmationPending,
        bool anyFuzzyUsed,
        const std::string& originalText,
        const std::string& normalizedText
    );

private:
    static bool isExactConfirmationPhrase(const std::string& normalizedText);
};

} // namespace VoiceJournal::Parser
