#pragma once

#include <string>
#include "parser/ParserTypes.h"

namespace VoiceJournal::Parser {

class JournalParser {
public:
    JournalParser() = default;

    /**
     * @brief Analyse une transcription vocale avec le pipeline contextuel v0.2.0.1.
     * @param rawText Texte brut UTF-8 issu de la transcription STT.
     * @param confirmationPending Indique si une confirmation d'effacement est en attente.
     * @return Structure ParseResult enrichie, explicable et déterministe.
     */
    ParseResult parse(const std::string& rawText, bool confirmationPending = false);

    /**
     * @brief Retourne une chaîne de diagnostic structurée prête pour la journalisation série.
     * @param result Résultat du parsing.
     * @param maskPrivatePayload Si vrai, masque le contenu brut des observations.
     */
    static std::string formatDiagnostics(const ParseResult& result, bool maskPrivatePayload = false);

    /**
     * @brief Valide la conformité UTF-8 stricte de l'entrée.
     */
    static bool isValidUtf8(const std::string& str);
};

} // namespace VoiceJournal::Parser
