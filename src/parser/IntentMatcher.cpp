#include "parser/IntentMatcher.h"
#include "parser/ConceptDictionary.h"
#include "parser/SlotExtractor.h"
#include "parser/GrammarEvaluator.h"
#include <algorithm>
#include <cstdlib>

namespace VoiceJournal::Parser {

bool IntentMatcher::isExactConfirmationPhrase(const std::string& normalizedText) {
    for (size_t i = 0; i < CONFIRM_PHRASE_COUNT; ++i) {
        if (normalizedText == CONFIRM_PHRASES[i]) {
            return true;
        }
    }
    return false;
}

ParseResult IntentMatcher::match(
    const std::vector<Token>& tokens,
    const std::vector<RecognizedPhrase>& phrases,
    const std::vector<EntityMatch>& entities,
    const SemanticFrame& frame,
    const NegationAnalysis& negation,
    bool confirmationPending,
    bool anyFuzzyUsed,
    const std::string& originalText,
    const std::string& normalizedText
) {
    ParseResult result;
    result.originalText = originalText;
    result.normalizedText = normalizedText;
    result.tokens = tokens;
    result.phrases = phrases;
    result.entities = entities;
    result.semanticFrame = frame;
    result.negationDetected = negation.hasAnyNegation;
    result.fuzzyMatchUsed = anyFuzzyUsed;
    result.acceptanceThreshold = DEFAULT_ACCEPTANCE_THRESHOLD;
    result.primaryConcepts = ConceptDictionary::extractPrimaryConcepts(tokens);

    if (tokens.empty()) {
        result.intent = IntentId::NO_ACTION;
        result.status = ParseStatus::EMPTY_INPUT;
        result.rawScore = 0;
        result.adjustedScore = 0;
        result.score = 0;
        result.executable = false;
        return result;
    }

    // 1. Détection de CONFIRM_CLEAR (Priorité absolue de sécurité COR-08)
    const bool isExactConfirm = isExactConfirmationPhrase(normalizedText);
    if (confirmationPending || isExactConfirm) {
        // Règle de sécurité non négociable : refus strict si négation
        if (negation.confirmationNegated || NegationDetector::isConfirmationNegated(normalizedText)) {
            result.intent = IntentId::CANCEL_CLEAR;
            result.status = ParseStatus::NEGATION_REJECTED;
            result.rawScore = 100;
            result.adjustedScore = 100;
            result.score = 100;
            result.executable = true;
            result.matchedRule = "security_reject_negated_confirmation";
            result.rejectionReason = "Négation détectée dans une formulation de confirmation";
            return result;
        }

        // Refus formel de tout rapprochement approximatif (fuzzy) pour une confirmation (COR-07)
        if (anyFuzzyUsed) {
            result.intent = IntentId::UNKNOWN;
            result.status = ParseStatus::CONFIRMATION_INVALID;
            result.rawScore = 0;
            result.adjustedScore = 0;
            result.score = 0;
            result.executable = false;
            result.rejectionReason = "Aucun rapprochement approximatif autorisé pour une confirmation";
            return result;
        }

        if (isExactConfirm) {
            result.intent = IntentId::CONFIRM_CLEAR;
            result.status = ParseStatus::OK;
            result.rawScore = 100;
            result.adjustedScore = 100;
            result.score = 100;
            result.executable = true;
            result.matchedRule = "confirm_clear_exact";
            return result;
        }
    }

    // 2. Détection de ADD_NOTE (Préfixe d'ajout : "note", "enregistre", "memorise", "retiens") (COR-06)
    // Règle fondamentale : le préfixe ADD isole le texte dicté avant d'évaluer les intentions concurrentes.
    // Les éventuels verbes ou concepts contenus dans le texte de l'observation
    // (ex: "Note, ne supprime pas les fichiers", "Mémorise que Gemini ne fonctionne pas")
    // font partie de la charge utile et NE DOIVENT PAS entrer en compétition !
    const bool startsWithAdd = ConceptDictionary::hasConcept(tokens[0].conceptMask, ConceptId::ADD);
    if (startsWithAdd) {
        std::string payload = SlotExtractor::extractObservationPayload(tokens, originalText);
        if (payload.empty() || tokens.size() < 2) {
            result.intent = IntentId::ADD_NOTE;
            result.status = ParseStatus::PAYLOAD_MISSING;
            result.rawScore = 50;
            result.adjustedScore = 50;
            result.score = 50;
            result.executable = false;
            result.rejectionReason = "Observation vide après la commande d'enregistrement";
            return result;
        }

        result.intent = IntentId::ADD_NOTE;
        result.status = ParseStatus::OK;
        result.rawScore = 95;
        result.fuzzyPenalty = anyFuzzyUsed ? 15 : 0;
        result.adjustedScore = result.rawScore - result.fuzzyPenalty;
        result.score = result.adjustedScore;
        result.matchedRule = "add_note_prefix";
        result.executable = true;
        result.observationPayload = payload;
        result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
        return result;
    }

    // 3. Détection directe de CANCEL_CLEAR
    const bool hasCancelConcept = result.hasConcept(ConceptId::CANCEL);
    const bool hasDeleteConcept = result.hasConcept(ConceptId::DELETE);

    if (hasCancelConcept || (hasDeleteConcept && negation.commandNegated)) {
        result.intent = IntentId::CANCEL_CLEAR;
        result.status = ParseStatus::OK;
        result.rawScore = 95;
        result.fuzzyPenalty = anyFuzzyUsed ? 15 : 0;
        result.adjustedScore = result.rawScore - result.fuzzyPenalty;
        result.score = result.adjustedScore;
        result.executable = true;
        result.matchedRule = (hasDeleteConcept && negation.commandNegated) ? "cancel_clear_negated_delete" : "cancel_clear_direct";
        result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
        return result;
    }

    // 3.5 Détection de verbes d'action en conflit direct sans structure (Catégorie 8, COR-07)
    const bool hasActRead = result.hasConcept(ConceptId::READ);
    const bool hasActDelete = result.hasConcept(ConceptId::DELETE);
    const bool hasActCount = result.hasConcept(ConceptId::COUNT);
    const bool hasActAnalyze = result.hasConcept(ConceptId::ANALYZE);
    const bool hasActHelp = result.hasConcept(ConceptId::HELP);
    const bool hasActTask = result.hasConcept(ConceptId::TASK);
    const bool hasActSearch = result.hasConcept(ConceptId::SEARCH);

    const int actionCount = (hasActRead ? 1 : 0) + (hasActDelete ? 1 : 0) + (hasActCount ? 1 : 0) +
                            (hasActAnalyze ? 1 : 0) + (hasActHelp ? 1 : 0) + (hasActTask ? 1 : 0) +
                            (hasActSearch ? 1 : 0);

    if (actionCount >= 2 && !startsWithAdd) {
        // Exception: LIST_TASKS = READ + TASK
        if (!(hasActRead && hasActTask && actionCount == 2)) {
            result.intent = IntentId::AMBIGUOUS;
            result.status = ParseStatus::AMBIGUOUS;
            result.ambiguityDetected = true;
            result.ambiguous = true;
            result.rawScore = 90;
            result.adjustedScore = 90;
            result.score = 90;
            result.executable = false;
            result.rejectionReason = "Conflit direct entre verbes d'action sans structure";
            result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
            return result;
        }
    }

    // 4. Évaluation générique des règles déclaratives (COR-03, COR-04)
    uint32_t combinedMask = 0;
    for (const auto& tok : tokens) {
        combinedMask |= tok.conceptMask;
    }

    std::vector<IntentCandidate> candidates = GrammarEvaluator::evaluate(
        tokens,
        combinedMask,
        phrases,
        entities,
        frame,
        negation,
        normalizedText,
        confirmationPending
    );

    // 5. Filtrage sémantique strict des objets et actions (COR-05, COR-13)
    // Sécurité destructive : REQUEST_CLEAR exige JOURNAL / JOURNAL_NOTE avec portée globale
    // et exclut formellement : rapprochement fuzzy, cibles concurrentes, fichiers, projets, périphériques, suppressions unitaires
    auto it = candidates.begin();
    while (it != candidates.end()) {
        if (it->intent == IntentId::REQUEST_CLEAR) {
            // Section 7.5 : Aucun rapprochement approximatif sur les éléments déterminants de la commande destructive
            if (anyFuzzyUsed) {
                it = candidates.erase(it);
                continue;
            }
            // Section 7 : Rejet si objets concurrents ou cibles non-carnet détectées
            if (result.hasConcept(ConceptId::FILE_TARGET) ||
                result.hasConcept(ConceptId::DEVICE_TARGET) ||
                result.hasConcept(ConceptId::PROJECT) ||
                result.hasConcept(ConceptId::TASK) ||
                result.hasConcept(ConceptId::PROBLEM)) {
                it = candidates.erase(it);
                continue;
            }
            if (frame.targetCategory == TargetCategory::FILE ||
                frame.targetCategory == TargetCategory::PROJECT ||
                frame.targetCategory == TargetCategory::DEVICE ||
                frame.targetCategory == TargetCategory::TASK ||
                frame.targetCategory == TargetCategory::PROBLEM) {
                // Ex: "Efface le fichier de configuration", "Supprime le projet VoiceKit"
                it = candidates.erase(it);
                continue;
            }
            if (frame.targetCategory == TargetCategory::JOURNAL_NOTE && !frame.hasGlobalScope) {
                // Ex: "Efface la dernière note" -> suppression unitaire non implémentée
                it = candidates.erase(it);
                continue;
            }
            if (result.hasConcept(ConceptId::QUESTION)) {
                // Ex: "Comment supprimer une note ?"
                it = candidates.erase(it);
                continue;
            }
        }

        // Obligation niée pour ADD_TASK (COR-06 : "Je ne dois pas tester Gemini")
        if (it->intent == IntentId::ADD_TASK) {
            if (negation.obligationNegated || frame.negationScope == "obligation") {
                it = candidates.erase(it);
                continue;
            }
        }

        ++it;
    }

    // 6. Aucun candidat retenu
    if (candidates.empty()) {
        result.intent = IntentId::UNKNOWN;
        result.status = ParseStatus::UNRECOGNIZED;
        result.rawScore = 0;
        result.adjustedScore = 0;
        result.score = 0;
        result.executable = false;
        result.rejectionReason = "Aucune règle d'intention correspondante";
        result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
        return result;
    }

    // 7. Trier les candidats par priorité décroissante puis score brut décroissant
    std::sort(candidates.begin(), candidates.end(), [](const IntentCandidate& a, const IntentCandidate& b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.score > b.score;
    });

    for (size_t i = 1; i < candidates.size(); ++i) {
        result.competingIntents.push_back(candidates[i].intent);
    }

    // 8. Évaluation de l'ambiguïté indépendamment de la pénalité fuzzy (COR-07)
    // Conflit entre deux intentions distinctes de priorité et score comparables
    if (candidates.size() >= 2) {
        if (candidates[0].intent != candidates[1].intent) {
            const int priorityDiff = std::abs(static_cast<int>(candidates[0].priority) - static_cast<int>(candidates[1].priority));
            const int diff = std::abs(static_cast<int>(candidates[0].score) - static_cast<int>(candidates[1].score));
            if (priorityDiff <= 5 && diff <= 5 && candidates[0].score >= 80) {
                result.intent = IntentId::AMBIGUOUS;
                result.status = ParseStatus::AMBIGUOUS;
                result.ambiguityDetected = true;
                result.ambiguous = true;
                result.rawScore = candidates[0].score;
                result.fuzzyPenalty = anyFuzzyUsed ? 15 : 0;
                result.adjustedScore = (result.rawScore > result.fuzzyPenalty) ? (result.rawScore - result.fuzzyPenalty) : 0;
                result.score = result.adjustedScore;
                result.executable = false;
                result.rejectionReason = "Conflit entre plusieurs intentions de score comparable";
                result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
                return result;
            }
        }
    }

    // 9. Retenir le meilleur candidat et appliquer la pénalité fuzzy
    const auto& best = candidates[0];
    result.intent = best.intent;
    result.rawScore = best.score;
    result.fuzzyPenalty = anyFuzzyUsed ? 15 : 0;
    result.adjustedScore = (result.rawScore > result.fuzzyPenalty) ? (result.rawScore - result.fuzzyPenalty) : 0;
    result.score = result.adjustedScore;
    result.matchedRule = best.ruleId;
    result.executable = best.executable;

    if (result.score >= result.acceptanceThreshold) {
        result.status = ParseStatus::OK;
    } else {
        result.status = ParseStatus::UNRECOGNIZED;
        result.executable = false;
        result.rejectionReason = "Score inférieur au seuil d'acceptation";
    }

    result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
    return result;
}

} // namespace VoiceJournal::Parser
