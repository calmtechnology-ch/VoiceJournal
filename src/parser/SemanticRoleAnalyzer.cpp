#include "parser/SemanticRoleAnalyzer.h"
#include "parser/ConceptDictionary.h"

namespace VoiceJournal::Parser {

void SemanticRoleAnalyzer::annotateRoles(
    std::vector<Token>& tokens,
    const std::vector<RecognizedPhrase>& phrases,
    const std::vector<EntityMatch>& entities
) {
    (void)phrases;
    const size_t n = tokens.size();

    for (size_t i = 0; i < n; ++i) {
        // Si le rôle a déjà été assigné par PhraseRecognizer, le respecter
        if (tokens[i].semanticRole != SemanticRole::UNKNOWN) {
            continue;
        }

        // 1. Entités techniques
        for (const auto& em : entities) {
            if (i >= em.tokenIndex && i < (em.tokenIndex + em.tokenCount)) {
                tokens[i].semanticRole = SemanticRole::ENTITY;
                goto next_token;
            }
        }

        // 2. Pronoms sujets
        if (tokens[i].text == "je" || tokens[i].text == "tu" || tokens[i].text == "il" ||
            tokens[i].text == "elle" || tokens[i].text == "on" || tokens[i].text == "nous" ||
            tokens[i].text == "vous" || tokens[i].text == "ils" || tokens[i].text == "j") {
            tokens[i].semanticRole = SemanticRole::SUBJECT;
            continue;
        }

        // 3. Négations
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::NEGATION) ||
            tokens[i].text == "non") {
            tokens[i].semanticRole = SemanticRole::NEGATION;
            continue;
        }

        // 4. Obligation modale
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::OBLIGATION)) {
            tokens[i].semanticRole = SemanticRole::OBLIGATION;
            continue;
        }

        // 5. Verbes d'action
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::ACTION_VERB) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::ADD) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::DELETE) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::READ) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::COUNT) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::ANALYZE) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::SEARCH) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::RESOLVED)) {
            tokens[i].semanticRole = SemanticRole::ACTION;
            continue;
        }

        // 6. Cibles et Objets
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::NOTE_TARGET) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::FILE_TARGET) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::DEVICE_TARGET) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::TASK) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::PROBLEM) ||
            ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::PROJECT)) {
            tokens[i].semanticRole = SemanticRole::OBJECT;
            continue;
        }

        // 7. Quantifieurs
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::ALL_QUANTIFIER) ||
            tokens[i].text == "chaque" || tokens[i].text == "1" || tokens[i].text == "2" ||
            tokens[i].text == "3" || tokens[i].text == "4" || tokens[i].text == "5") {
            tokens[i].semanticRole = SemanticRole::QUANTIFIER;
            continue;
        }

        // 8. Temps et dates
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::DATE_RELATIVE)) {
            tokens[i].semanticRole = SemanticRole::TIME;
            continue;
        }

        // 9. Questions
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::QUESTION)) {
            tokens[i].semanticRole = SemanticRole::QUESTION;
            continue;
        }

        // 10. Connecteurs grammaticaux
        if (tokens[i].text == "de" || tokens[i].text == "d" || tokens[i].text == "du" ||
            tokens[i].text == "des" || tokens[i].text == "a" || tokens[i].text == "au" ||
            tokens[i].text == "aux" || tokens[i].text == "que" || tokens[i].text == "qu" ||
            tokens[i].text == "pour" || tokens[i].text == "avec" || tokens[i].text == "sans" ||
            tokens[i].text == "sur" || tokens[i].text == "dans" || tokens[i].text == "et" ||
            tokens[i].text == "ou") {
            tokens[i].semanticRole = SemanticRole::CONNECTOR;
            continue;
        }

    next_token:;
    }
}

SemanticFrame SemanticFrameFromTokens(
    const std::vector<Token>& tokens,
    const std::vector<RecognizedPhrase>& phrases,
    const std::vector<EntityMatch>& entities,
    const NegationAnalysis& negation
) {
    SemanticFrame frame;
    frame.negationScope = "none";

    // 1. Détection du sujet (COR-01)
    for (const auto& tok : tokens) {
        if (tok.semanticRole == SemanticRole::SUBJECT) {
            frame.subject = tok.text;
            break;
        }
    }
    if (frame.subject.empty() && !tokens.empty()) {
        const std::string& first = tokens[0].text;
        if (first == "je" || first == "tu" || first == "il" || first == "elle" ||
            first == "on" || first == "nous" || first == "vous" || first == "ils") {
            frame.subject = first;
        }
    }

    // 2. Détection de la modalité
    bool hasObligation = false;
    bool hasQuestion = false;
    for (const auto& tok : tokens) {
        if (tok.semanticRole == SemanticRole::OBLIGATION) hasObligation = true;
        if (tok.semanticRole == SemanticRole::QUESTION) hasQuestion = true;
    }
    for (const auto& p : phrases) {
        if (p.role == SemanticRole::OBLIGATION) hasObligation = true;
        if (p.role == SemanticRole::QUESTION) hasQuestion = true;
    }

    if (hasObligation) {
        frame.modality = "obligation";
    } else if (hasQuestion) {
        frame.modality = "question";
    } else {
        frame.modality = "command";
    }

    // 3. Détection de l'action principale
    // Chercher d'abord dans les phrases (ex: "fais le bilan")
    for (const auto& p : phrases) {
        if (p.role == SemanticRole::ACTION) {
            frame.action = p.phrase;
            break;
        }
    }
    if (frame.action.empty()) {
        for (const auto& tok : tokens) {
            if (tok.semanticRole == SemanticRole::ACTION) {
                frame.action = tok.text;
                break;
            }
        }
    }

    // 4. Détection de la cible et catégorie d'objet (COR-05)
    // Examiner les locutions d'abord
    for (const auto& p : phrases) {
        if (p.targetCategory != TargetCategory::UNKNOWN_OBJECT) {
            frame.targetCategory = p.targetCategory;
            frame.object = p.phrase;
            if (p.isGlobalScope) {
                frame.hasGlobalScope = true;
            }
            break;
        }
    }

    // Si non trouvé dans les phrases, examiner les tokens
    if (frame.targetCategory == TargetCategory::UNKNOWN_OBJECT) {
        for (const auto& tok : tokens) {
            if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::FILE_TARGET)) {
                frame.targetCategory = TargetCategory::FILE;
                frame.object = tok.text;
                break;
            } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::NOTE_TARGET)) {
                const std::string& word = tok.isFuzzy ? tok.matchedLexicon : tok.text;
                frame.targetCategory = (word == "carnet" || word == "journal")
                                           ? TargetCategory::JOURNAL
                                           : TargetCategory::JOURNAL_NOTE;
                frame.object = word;
                if (word == "carnet" || word == "journal") {
                    frame.hasGlobalScope = true;
                }
                break;
            } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::TASK)) {
                frame.targetCategory = TargetCategory::TASK;
                frame.object = tok.text;
                break;
            } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::PROBLEM)) {
                frame.targetCategory = TargetCategory::PROBLEM;
                frame.object = tok.text;
                break;
            } else if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::DEVICE_TARGET)) {
                frame.targetCategory = TargetCategory::DEVICE;
                frame.object = tok.text;
                break;
            }
        }
    }

    // Vérifier la portée globale ("toutes les notes", "tous les", etc.)
    for (const auto& tok : tokens) {
        if (ConceptDictionary::hasConcept(tok.conceptMask, ConceptId::ALL_QUANTIFIER)) {
            frame.hasGlobalScope = true;
            break;
        }
    }
    for (const auto& p : phrases) {
        if (p.isGlobalScope) {
            frame.hasGlobalScope = true;
            break;
        }
    }

    // 5. Entité
    if (!entities.empty()) {
        frame.entity = entities[0].canonical;
        if (frame.object.empty()) {
            frame.object = entities[0].canonical;
        }
    }

    // 6. Date relative
    for (const auto& p : phrases) {
        if (p.role == SemanticRole::TIME) {
            frame.relativeDate = p.phrase;
            break;
        }
    }
    if (frame.relativeDate.empty()) {
        for (const auto& tok : tokens) {
            if (tok.semanticRole == SemanticRole::TIME) {
                frame.relativeDate = tok.text;
                break;
            }
        }
    }

    // 7. Locution reconnue principale
    if (!phrases.empty()) {
        frame.matchedPhrase = phrases[0].phrase;
    }

    // 8. Portée de la négation (COR-06)
    bool hasPositiveObligationPhrase = false;
    for (const auto& p : phrases) {
        if (p.isPositiveObligation) {
            hasPositiveObligationPhrase = true;
            break;
        }
    }

    if (hasPositiveObligationPhrase) {
        // Locution du type "ne pas oublier de", "n'oublie pas de" : c'est une obligation positive !
        frame.negationScope = "none";
    } else if (negation.confirmationNegated) {
        frame.negationScope = "confirmation";
    } else if (negation.hasAnyNegation) {
        if (!tokens.empty() && ConceptDictionary::hasConcept(tokens[0].conceptMask, ConceptId::ADD) && tokens.size() > 1) {
            frame.negationScope = "payload";
        } else if (hasObligation) {
            // Ex: "je ne dois pas tester Gemini"
            frame.negationScope = "obligation";
        } else {
            frame.negationScope = "command";
        }
    } else {
        frame.negationScope = "none";
    }

    return frame;
}

SemanticFrame SemanticRoleAnalyzer::buildFrame(
    const std::vector<Token>& tokens,
    const std::vector<RecognizedPhrase>& phrases,
    const std::vector<EntityMatch>& entities,
    const NegationAnalysis& negation
) {
    return SemanticFrameFromTokens(tokens, phrases, entities, negation);
}

} // namespace VoiceJournal::Parser
