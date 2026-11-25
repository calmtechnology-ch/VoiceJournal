#pragma once

#include <string>
#include "parser/ParserTypes.h"

namespace VoiceJournal::Parser {

class JournalParser {
public:
    JournalParser() = default;

    /**
     * @brief Analyse une transcription vocale en langage naturel.
     * @param rawText Texte brut UTF-8 issu de la transcription STT.
     * @param confirmationPending Indique si une confirmation d'effacement est en attente.
     * @return Structure ParseResult détaillée et explicable.
     */
    ParseResult parse(const std::string& rawText, bool confirmationPending = false);

    /**
     * @brief Retourne une chaîne de diagnostic structurée prête pour la journalisation série.
     * @param result Résultat du parsing.
     * @param maskPrivatePayload Si vrai, masque le contenu brut des observations.
     */
    static std::string formatDiagnostics(const ParseResult& result, bool maskPrivatePayload = false);
};

} // namespace VoiceJournal::Parser
