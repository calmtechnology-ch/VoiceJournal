#include "parser/SlotExtractor.h"
#include "parser/ConceptDictionary.h"
#include "parser/TextNormalizer.h"
#include <cstring>

namespace VoiceJournal::Parser {

std::vector<EntityMatch> SlotExtractor::extractEntities(const std::vector<Token>& tokens) {
    std::vector<EntityMatch> matches;
    const size_t n = tokens.size();
    if (n == 0) return matches;

    // 1. Recherche d'alias composés (2 ou 3 mots)
    for (size_t i = 0; i < n; ++i) {
        // Trigramme
        if (i + 2 < n) {
            const std::string tri = tokens[i].text + " " + tokens[i + 1].text + " " + tokens[i + 2].text;
            for (size_t a = 0; a < ENTITY_ALIAS_COUNT; ++a) {
                if (tri == ENTITY_ALIAS_TABLE[a].alias) {
                    EntityMatch em;
                    em.id = ENTITY_ALIAS_TABLE[a].entityId;
                    em.type = ENTITY_ALIAS_TABLE[a].type;
                    em.canonical = entityToString(em.id);
                    em.tokenIndex = i;
                    em.tokenCount = 3;
                    matches.push_back(em);
                    i += 2; // Sauter les tokens consommés
                    goto next_token;
                }
            }
        }

        // Bigramme
        if (i + 1 < n) {
            const std::string bi = tokens[i].text + " " + tokens[i + 1].text;
            for (size_t a = 0; a < ENTITY_ALIAS_COUNT; ++a) {
                if (bi == ENTITY_ALIAS_TABLE[a].alias) {
                    EntityMatch em;
                    em.id = ENTITY_ALIAS_TABLE[a].entityId;
                    em.type = ENTITY_ALIAS_TABLE[a].type;
                    em.canonical = entityToString(em.id);
                    em.tokenIndex = i;
                    em.tokenCount = 2;
                    matches.push_back(em);
                    i += 1;
                    goto next_token;
                }
            }
        }

        // Unigramme
        for (size_t a = 0; a < ENTITY_ALIAS_COUNT; ++a) {
            if (tokens[i].text == ENTITY_ALIAS_TABLE[a].alias) {
                EntityMatch em;
                em.id = ENTITY_ALIAS_TABLE[a].entityId;
                em.type = ENTITY_ALIAS_TABLE[a].type;
                em.canonical = entityToString(em.id);
                em.tokenIndex = i;
                em.tokenCount = 1;
                matches.push_back(em);
                break;
            }
        }

    next_token:;
    }

    return matches;
}

std::string SlotExtractor::extractObservationPayload(const std::vector<Token>& tokens, const std::string& originalText) {
    if (tokens.empty() || originalText.empty()) {
        return "";
    }

    // Le premier token (ex: "note", "memorise", "enregistre") déclenche l'ajout.
    // La charge utile commence à la fin de ce token dans le texte original.
    const size_t startOffset = tokens[0].origEnd;
    return TextNormalizer::trimOriginalPayload(originalText, startOffset);
}

std::vector<SlotValue> SlotExtractor::extractSlots(
    const std::vector<Token>& tokens,
    const std::vector<EntityMatch>& entities,
    const std::string& originalText,
    IntentId intent
) {
    std::vector<SlotValue> slots;

    // 1. Entités / Projets détectés
    for (const auto& ent : entities) {
        if (ent.type == EntityType::PROJECT || ent.type == EntityType::FRAMEWORK ||
            ent.type == EntityType::HARDWARE || ent.type == EntityType::AI_PROVIDER ||
            ent.type == EntityType::APPLICATION) {
            slots.push_back({"entity", ent.canonical});
            if (ent.type == EntityType::PROJECT || ent.type == EntityType::APPLICATION) {
                slots.push_back({"project", ent.canonical});
            }
        }
    }

    // 2. Types de problème / tâche / statut
    bool resolvedFound = false;
    bool problemFound = false;
    bool taskFound = false;
    for (const auto& tok : tokens) {
        if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::RESOLVED)) {
            resolvedFound = true;
        } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::PROBLEM)) {
            problemFound = true;
        } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::TASK)) {
            taskFound = true;
        }
    }
    if (resolvedFound) {
        slots.push_back({"type", "RESOLVED"});
    } else if (problemFound) {
        slots.push_back({"type", "PROBLEM"});
    } else if (taskFound) {
        slots.push_back({"type", "TASK"});
    }

    // 3. Dates relatives
    const size_t n = tokens.size();
    for (size_t i = 0; i < n; ++i) {
        // Bigrammes de date : "aujourd hui", "ce matin", "ce soir", "apres demain", "avant hier"
        if (i + 1 < n) {
            const std::string bi = tokens[i].text + " " + tokens[i + 1].text;
            if (bi == "aujourd hui" || bi == "ce matin" || bi == "ce soir" ||
                bi == "apres demain" || bi == "avant hier") {
                slots.push_back({"date_relative", bi});
                i += 1;
                continue;
            }
        }

        if (tokens[i].text == "demain" || tokens[i].text == "hier" || tokens[i].text == "maintenant") {
            slots.push_back({"date_relative", tokens[i].text});
        }
    }

    // 4. Charge utile (pour ADD_NOTE)
    if (intent == IntentId::ADD_NOTE) {
        std::string payload = extractObservationPayload(tokens, originalText);
        if (!payload.empty()) {
            slots.push_back({"payload", payload});
        }
    }

    return slots;
}

} // namespace VoiceJournal::Parser
