#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"
#include "parser/NegationDetector.h"

namespace VoiceJournal::Parser {

class SemanticRoleAnalyzer {
public:
    /**
     * @brief Assigne les rôles sémantiques (COR-01) à chaque token.
     */
    static void annotateRoles(
        std::vector<Token>& tokens,
        const std::vector<RecognizedPhrase>& phrases,
        const std::vector<EntityMatch>& entities
    );

    /**
     * @brief Construit la représentation sémantique structurée (SemanticFrame).
     */
    static SemanticFrame buildFrame(
        const std::vector<Token>& tokens,
        const std::vector<RecognizedPhrase>& phrases,
        const std::vector<EntityMatch>& entities,
        const NegationAnalysis& negation
    );
};

} // namespace VoiceJournal::Parser
