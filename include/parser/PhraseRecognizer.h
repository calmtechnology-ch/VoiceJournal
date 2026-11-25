#pragma once

#include <vector>
#include <string>
#include "parser/ParserTypes.h"
#include "parser/ParserTables.h"

namespace VoiceJournal::Parser {

class PhraseRecognizer {
public:
    /**
     * @brief Identifie les locutions multi-mots selon l'algorithme longest-match déterministe.
     * @param tokens Liste des tokens normalisés (leurs rôles et masques peuvent être enrichis).
     * @param originalText Texte UTF-8 source pour calcul précis des coordonnées d'origine.
     * @return Liste ordonnée des locutions identifiées.
     */
    static std::vector<RecognizedPhrase> recognize(
        std::vector<Token>& tokens,
        const std::string& originalText
    );
};

} // namespace VoiceJournal::Parser
