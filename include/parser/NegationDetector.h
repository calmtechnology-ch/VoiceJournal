#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"

namespace VoiceJournal::Parser {

struct NegationAnalysis {
    bool hasAnyNegation = false;        // Présence d'au moins un marqueur négatif
    bool commandNegated = false;        // La négation porte sur la commande elle-même
    bool confirmationNegated = false;   // Négation explicite d'une confirmation
    bool deletionNegated = false;       // Négation explicite d'une suppression
    bool obligationNegated = false;     // Négation d'une obligation ("je ne dois pas")
    bool positiveObligation = false;    // Négation idiomatique positive ("ne pas oublier de")
    size_t firstNegationIndex = 0;
};

class NegationDetector {
public:
    /**
     * @brief Analyse la présence et la portée des négations dans la séquence de tokens (COR-06).
     * @param tokens Liste des tokens annotés.
     * @param startsWithAdd True si le premier token est un mot-clé d'ajout (NOTE/ADD).
     * @param phrases Liste des locutions reconnues pour détecter les locutions idiomatiques.
     */
    static NegationAnalysis analyze(
        const std::vector<Token>& tokens,
        bool startsWithAdd,
        const std::vector<RecognizedPhrase>& phrases = {}
    );

    /**
     * @brief Vérifie spécifiquement si une phrase contient une négation empêchant la confirmation.
     */
    static bool isConfirmationNegated(const std::string& normalizedText);
};

} // namespace VoiceJournal::Parser
