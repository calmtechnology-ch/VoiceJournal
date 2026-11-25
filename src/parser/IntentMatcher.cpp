#include "parser/IntentMatcher.h"
#include "parser/ConceptDictionary.h"
#include "parser/SlotExtractor.h"
#include <algorithm>
#include <cstring>

namespace VoiceJournal::Parser {

bool IntentMatcher::isExactConfirmationPhrase(const std::string& normalizedText) {
    // Vérification stricte contre la liste prédéfinie
    for (size_t i = 0; i < CONFIRM_PHRASE_COUNT; ++i) {
        if (normalizedText == CONFIRM_PHRASES[i]) {
            return true;
        }
    }
    return false;
}

ParseResult IntentMatcher::match(
    const std::vector<Token>& tokens,
    const std::vector<EntityMatch>& entities,
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
    result.entities = entities;
    result.negationDetected = negation.hasAnyNegation;
    result.fuzzyMatchUsed = anyFuzzyUsed;
    result.primaryConcepts = ConceptDictionary::extractPrimaryConcepts(tokens);

    if (tokens.empty()) {
        result.intent = IntentId::NO_ACTION;
        result.status = ParseStatus::EMPTY_INPUT;
        result.score = 0;
        result.executable = false;
        return result;
    }

    std::vector<IntentCandidate> candidates;

    // 1. Détection de CONFIRM_CLEAR (Priorité absolue de sécurité)
    if (confirmationPending || isExactConfirmationPhrase(normalizedText)) {
        // Règle non négociable : si une négation est détectée, REFUS STRICT DE CONFIRMATION
        if (negation.confirmationNegated || NegationDetector::isConfirmationNegated(normalizedText)) {
            result.intent = IntentId::CANCEL_CLEAR;
            result.status = ParseStatus::NEGATION_REJECTED;
            result.score = 100;
            result.executable = true;
            result.matchedRule = "security_reject_negated_confirmation";
            result.rejectionReason = "Négation détectée dans une formulation de confirmation";
            return result;
        }

        // Refus formel de tout rapprochement approximatif pour confirmer
        if (anyFuzzyUsed) {
            result.intent = IntentId::UNKNOWN;
            result.status = ParseStatus::CONFIRMATION_INVALID;
            result.score = 0;
            result.executable = false;
            result.rejectionReason = "Aucun rapprochement approximatif autorise pour une confirmation";
            return result;
        }

        if (isExactConfirmationPhrase(normalizedText)) {
            IntentCandidate cand;
            cand.intent = IntentId::CONFIRM_CLEAR;
            cand.score = 100;
            cand.ruleName = "exact_confirm_phrase";
            cand.executable = true;
            candidates.push_back(cand);
        }
    }

    // 2. Détection de CANCEL_CLEAR
    const bool hasCancelConcept = result.hasConcept(ConceptId::CANCEL);
    const bool hasDeleteConcept = result.hasConcept(ConceptId::DELETE);

    if (hasCancelConcept || (hasDeleteConcept && negation.commandNegated)) {
        IntentCandidate cand;
        cand.intent = IntentId::CANCEL_CLEAR;
        cand.score = 95;
        cand.ruleName = (hasDeleteConcept && negation.commandNegated) ? "negated_delete" : "cancel_direct";
        cand.executable = true;
        candidates.push_back(cand);
    }

    // 3. Détection de ADD_NOTE (Préfixe "note", "enregistre", "memorise", "retiens")
    const bool startsWithAdd = ConceptDictionary::hasConcept(tokens[0].conceptMask, ConceptId::ADD);
    if (startsWithAdd) {
        std::string payload = SlotExtractor::extractObservationPayload(tokens, originalText);
        if (payload.empty() || tokens.size() < 2) {
            result.intent = IntentId::ADD_NOTE;
            result.status = ParseStatus::PAYLOAD_MISSING;
            result.score = 50;
            result.executable = false;
            result.rejectionReason = "Observation vide apres la commande d'enregistrement";
            return result;
        }

        // Règle fondamentale : le préfixe ADD isole le texte dicté.
        // Les éventuels verbes ou concepts contenus dans le texte de l'observation
        // (ex: "Note, ne supprime pas les fichiers", "Note: lis ceci") font partie du contenu
        // et NE DOIVENT PAS entrer en compétition avec la commande d'ajout.
        result.intent = IntentId::ADD_NOTE;
        result.score = 95;
        result.matchedRule = "prefix_add_note";
        result.executable = true;
        result.status = ParseStatus::OK;
        result.observationPayload = payload;
        result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
        return result;
    }

    // 4. Détection de REQUEST_CLEAR (uniquement si ce n'est pas une confirmation exacte)
    const bool isExactConfirm = isExactConfirmationPhrase(normalizedText);
    if (hasDeleteConcept && !negation.commandNegated && !isExactConfirm) {
        // Demande d'effacement du carnet
        IntentCandidate cand;
        cand.intent = IntentId::REQUEST_CLEAR;
        cand.score = 95;
        cand.ruleName = "request_clear_notes";
        cand.executable = true;
        candidates.push_back(cand);
    }

    // 5. Détection de COUNT_NOTES
    if (result.hasConcept(ConceptId::COUNT)) {
        if (!negation.commandNegated) {
            IntentCandidate cand;
            cand.intent = IntentId::COUNT_NOTES;
            cand.score = 90;
            cand.ruleName = "count_notes";
            cand.executable = true;
            candidates.push_back(cand);
        }
    }

    // 6. Détection de READ_LAST_NOTE / LIST_TASKS
    if (result.hasConcept(ConceptId::READ)) {
        if (result.hasConcept(ConceptId::TASK)) {
            // Experimental: LIST_TASKS
            IntentCandidate cand;
            cand.intent = IntentId::LIST_TASKS;
            cand.score = 90;
            cand.ruleName = "list_tasks_experimental";
            cand.executable = false;
            candidates.push_back(cand);
        } else if (!negation.commandNegated) {
            IntentCandidate cand;
            cand.intent = IntentId::READ_LAST_NOTE;
            cand.score = 90;
            cand.ruleName = "read_last_note";
            cand.executable = true;
            candidates.push_back(cand);
        }
    }

    // 7. Détection de ANALYZE_NOTES / ANALYZE_PROJECT
    if (result.hasConcept(ConceptId::ANALYZE) && !negation.commandNegated) {
        if (!entities.empty()) {
            // Experimental: ANALYZE_PROJECT
            IntentCandidate cand;
            cand.intent = IntentId::ANALYZE_PROJECT;
            cand.score = 95;
            cand.ruleName = "analyze_project_experimental";
            cand.executable = false;
            candidates.push_back(cand);
        } else {
            IntentCandidate cand;
            cand.intent = IntentId::ANALYZE_NOTES;
            cand.score = 90;
            cand.ruleName = "analyze_all_notes";
            cand.executable = true;
            candidates.push_back(cand);
        }
    }

    // 8. Détection de HELP
    if (result.hasConcept(ConceptId::HELP)) {
        IntentCandidate cand;
        cand.intent = IntentId::HELP;
        cand.score = 90;
        cand.ruleName = "help_direct";
        cand.executable = true;
        candidates.push_back(cand);
    }

    // 9. Intentions Expérimentales
    // RESOLVE_PROBLEM
    if (result.hasConcept(ConceptId::RESOLVED)) {
        IntentCandidate cand;
        cand.intent = IntentId::RESOLVE_PROBLEM;
        cand.score = 90;
        cand.ruleName = "resolve_problem_experimental";
        cand.executable = false;
        candidates.push_back(cand);
    }

    // SEARCH_NOTES / SEARCH_BY_PROJECT / SEARCH_BY_DATE
    const bool hasSearch = result.hasConcept(ConceptId::SEARCH);
    const bool hasDate = result.hasConcept(ConceptId::DATE_RELATIVE);
    const bool hasNoteTarget = result.hasConcept(ConceptId::NOTE_TARGET);
    const bool hasProblem = result.hasConcept(ConceptId::PROBLEM);
    const bool hasResolved = result.hasConcept(ConceptId::RESOLVED);
    const bool hasProject = result.hasConcept(ConceptId::PROJECT);

    if (hasSearch || (hasDate && hasNoteTarget) || (hasProblem && !hasResolved)) {
        IntentCandidate cand;
        cand.executable = false;
        if (hasProject) {
            cand.intent = IntentId::SEARCH_BY_PROJECT;
            cand.score = 90;
            cand.ruleName = "search_by_project_experimental";
        } else if (hasDate) {
            cand.intent = IntentId::SEARCH_BY_DATE;
            cand.score = 90;
            cand.ruleName = "search_by_date_experimental";
        } else {
            cand.intent = IntentId::SEARCH_NOTES;
            cand.score = 90;
            cand.ruleName = "search_notes_experimental";
        }
        candidates.push_back(cand);
    }

    // ADD_TASK
    if (result.hasConcept(ConceptId::TASK) && !result.hasConcept(ConceptId::READ)) {
        IntentCandidate cand;
        cand.intent = IntentId::ADD_TASK;
        cand.score = 90;
        cand.ruleName = "add_task_experimental";
        cand.executable = false;
        candidates.push_back(cand);
    }

    // Évaluation des candidats et détection d'ambiguïté
    if (candidates.empty()) {
        result.intent = IntentId::UNKNOWN;
        result.status = ParseStatus::UNRECOGNIZED;
        result.score = 0;
        result.executable = false;
        result.rejectionReason = "Aucune règle d'intention correspondante";
        result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
        return result;
    }

    // Application de pénalité fuzzy
    if (anyFuzzyUsed) {
        for (auto& cand : candidates) {
            if (cand.score > 15) cand.score -= 15;
        }
    }

    // Trier les candidats par score décroissant
    std::sort(candidates.begin(), candidates.end(), [](const IntentCandidate& a, const IntentCandidate& b) {
        return a.score > b.score;
    });

    for (size_t i = 1; i < candidates.size(); ++i) {
        result.competingIntents.push_back(candidates[i].intent);
    }

    // Vérifier ambiguïté entre les 2 meilleurs candidats
    if (candidates.size() >= 2) {
        if (candidates[0].intent != candidates[1].intent &&
            (candidates[0].score - candidates[1].score) <= 5 &&
            candidates[0].score >= 80) {
            result.intent = IntentId::AMBIGUOUS;
            result.status = ParseStatus::AMBIGUOUS;
            result.score = candidates[0].score;
            result.ambiguous = true;
            result.executable = false;
            result.rejectionReason = "Conflit entre plusieurs intentions de score comparable";
            return result;
        }
    }

    const IntentCandidate& best = candidates[0];
    result.intent = best.intent;
    result.score = best.score;
    result.matchedRule = best.ruleName;
    result.executable = best.executable;
    result.status = ParseStatus::OK;

    // Extraction des slots pour l'intention retenue
    result.slots = SlotExtractor::extractSlots(tokens, entities, originalText, result.intent);
    if (result.intent == IntentId::ADD_NOTE) {
        result.observationPayload = SlotExtractor::extractObservationPayload(tokens, originalText);
    }

    return result;
}

} // namespace VoiceJournal::Parser
