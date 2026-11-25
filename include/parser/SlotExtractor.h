#pragma once

#include <string>
#include <vector>
#include "parser/ParserTypes.h"

namespace VoiceJournal::Parser {

class SlotExtractor {
public:
    /**
     * @brief Détecte et extrait les entités (projets, matériels, fournisseurs IA) dans les tokens.
     */
    static std::vector<EntityMatch> extractEntities(const std::vector<Token>& tokens);

    /**
     * @brief Extrait les slots sémantiques (type, projet/entité, date relative, payload).
     */
    static std::vector<SlotValue> extractSlots(
        const std::vector<Token>& tokens,
        const std::vector<EntityMatch>& entities,
        const std::string& originalText,
        IntentId intent
    );

    /**
     * @brief Extrait la charge utile textuelle brute d'une note en préservant la casse et les accents.
     */
    static std::string extractObservationPayload(const std::vector<Token>& tokens, const std::string& originalText);
};

} // namespace VoiceJournal::Parser
