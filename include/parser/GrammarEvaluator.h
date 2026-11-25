#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"
#include "parser/NegationDetector.h"

namespace VoiceJournal::Parser {

struct IntentCandidate {
    IntentId intent = IntentId::UNKNOWN;
    uint8_t score = 0;
    std::string ruleId;
    bool executable = false;
    uint8_t priority = 50;
};

class GrammarEvaluator {
public:
    /**
     * @brief Évalue l'ensemble des règles déclaratives (COR-03, COR-04) et produit les candidats.
     */
    static std::vector<IntentCandidate> evaluate(
        const std::vector<Token>& tokens,
        uint32_t combinedConceptMask,
        const std::vector<RecognizedPhrase>& phrases,
        const std::vector<EntityMatch>& entities,
        const SemanticFrame& frame,
        const NegationAnalysis& negation,
        const std::string& normalizedText,
        bool confirmationPending
    );
};

} // namespace VoiceJournal::Parser
