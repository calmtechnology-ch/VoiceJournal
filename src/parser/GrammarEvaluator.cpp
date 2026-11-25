#include "parser/GrammarEvaluator.h"
#include "parser/ConceptDictionary.h"
#include <algorithm>

namespace VoiceJournal::Parser {

namespace {

bool isExactConfirmText(const std::string& normalizedText) {
    for (size_t i = 0; i < CONFIRM_PHRASE_COUNT; ++i) {
        if (normalizedText == CONFIRM_PHRASES[i]) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<IntentCandidate> GrammarEvaluator::evaluate(
    const std::vector<Token>& tokens,
    uint32_t combinedConceptMask,
    const std::vector<RecognizedPhrase>& phrases,
    const std::vector<EntityMatch>& entities,
    const SemanticFrame& frame,
    const NegationAnalysis& negation,
    const std::string& normalizedText,
    bool confirmationPending
) {
    (void)phrases;
    (void)confirmationPending;
    std::vector<IntentCandidate> candidates;
    if (tokens.empty()) return candidates;

    const bool exactConfirm = isExactConfirmText(normalizedText);

    for (size_t r = 0; r < GRAMMAR_RULE_COUNT; ++r) {
        if (candidates.size() >= MAX_CANDIDATE_INTENTS) break;
        const auto& rule = GRAMMAR_RULES[r];

        // 1. Détection de confirmation exacte
        if (rule.exactMatchRequired) {
            if (!exactConfirm) continue;
            if (rule.rejectNegation && (negation.confirmationNegated || negation.hasAnyNegation)) {
                continue;
            }
        }

        // 2. Si le mot initial est exigé (ex: préfixe ADD)
        if (rule.startsWithConcept != ConceptId::NONE) {
            const uint32_t mask = (1u << (static_cast<uint8_t>(rule.startsWithConcept) - 1));
            if ((tokens[0].conceptMask & mask) == 0) {
                continue;
            }
        }

        // 3. Concepts obligatoires
        if (rule.requiredConcepts != 0) {
            if ((combinedConceptMask & rule.requiredConcepts) != rule.requiredConcepts) {
                continue;
            }
        }

        // 4. Concepts interdits
        if (rule.forbiddenConcepts != 0) {
            if ((combinedConceptMask & rule.forbiddenConcepts) != 0) {
                continue;
            }
        }

        // 5. Cibles autorisées (allowedTargetsMask)
        if (rule.allowedTargetsMask != 0) {
            const uint16_t targetBit = static_cast<uint16_t>(1u << static_cast<uint8_t>(frame.targetCategory));
            // Si la catégorie cible n'est ni explicite ni dans les cibles autorisées, rejet
            if ((rule.allowedTargetsMask & targetBit) == 0) {
                continue;
            }
        }

        // 6. Cibles interdites (forbiddenTargetsMask)
        if (rule.forbiddenTargetsMask != 0) {
            const uint16_t targetBit = static_cast<uint16_t>(1u << static_cast<uint8_t>(frame.targetCategory));
            if ((rule.forbiddenTargetsMask & targetBit) != 0) {
                continue;
            }
        }

        // 7. Portée globale exigée (requireGlobalScope)
        if (rule.requireGlobalScope && !frame.hasGlobalScope) {
            continue;
        }

        // 8. Rejet si commande niée
        if (rule.rejectNegatedCommand && negation.commandNegated) {
            continue;
        }

        // 9. Rejet si obligation niée
        if (rule.rejectNegatedObligation) {
            if (negation.obligationNegated || frame.negationScope == "obligation") {
                continue;
            }
        }

        // 10. Exigence ou rejet d'entités
        if (rule.requiresEntity && entities.empty()) {
            continue;
        }
        if (rule.forbiddenEntities && !entities.empty()) {
            continue;
        }

        // 11. Exigence ou rejet de négation
        if (rule.requiresNegation && !negation.hasAnyNegation) {
            continue;
        }
        if (rule.rejectNegation && negation.hasAnyNegation) {
            continue;
        }

        // Si la règle a passé tous les filtres, créer le candidat
        IntentCandidate cand;
        cand.intent = rule.intent;
        cand.ruleId = rule.id;
        cand.executable = rule.executable;
        cand.priority = rule.priority;
        cand.score = rule.baseScore;

        // Bonus pour concepts facultatifs correspondants (+2 pts par concept facultatif)
        if (rule.optionalConcepts != 0) {
            uint32_t matchedOpt = (combinedConceptMask & rule.optionalConcepts);
            while (matchedOpt > 0) {
                if (matchedOpt & 1u) {
                    if (cand.score < 98) cand.score += 2;
                }
                matchedOpt >>= 1u;
            }
        }

        candidates.push_back(cand);
    }

    return candidates;
}

} // namespace VoiceJournal::Parser
