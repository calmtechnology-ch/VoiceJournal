#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"
#include "parser/NegationDetector.h"

namespace VoiceJournal::Parser {

class IntentMatcher {
public:
    /**
     * @brief Vérifie si la chaîne correspond exactement à une phrase de confirmation autorisée.
     */
    static bool isExactConfirmationPhrase(const std::string& normalizedText);

    /**
     * @brief Analyse contextuelle et correspondance des intentions (COR-01 à COR-07).
     */
    static ParseResult match(
        const std::vector<Token>& tokens,
        const std::vector<RecognizedPhrase>& phrases,
        const std::vector<EntityMatch>& entities,
        const SemanticFrame& frame,
        const NegationAnalysis& negation,
        bool confirmationPending,
        bool anyFuzzyUsed,
        const std::string& originalText,
        const std::string& normalizedText
    );
};

} // namespace VoiceJournal::Parser
