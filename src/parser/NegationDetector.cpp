#include "parser/NegationDetector.h"
#include "parser/ConceptDictionary.h"

namespace VoiceJournal::Parser {

NegationAnalysis NegationDetector::analyze(const std::vector<Token>& tokens, bool startsWithAdd) {
    NegationAnalysis res;
    const size_t n = tokens.size();
    if (n == 0) return res;

    bool foundFirst = false;
    for (size_t i = 0; i < n; ++i) {
        if (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::NEGATION) ||
            tokens[i].text == "non") {
            res.hasAnyNegation = true;
            if (!foundFirst) {
                res.firstNegationIndex = i;
                foundFirst = true;
            }
        }
    }

    if (!res.hasAnyNegation) {
        return res;
    }

    // Règle fondamentale : si la phrase commence par une commande d'ajout ("Note, ...", "Mémorise, ...")
    // et possède un contenu subséquent, la négation est contenue dans le texte dicté et
    // NE DOIT PAS être considérée comme une négation de la commande d'enregistrement !
    if (startsWithAdd && n > 1) {
        res.commandNegated = false;
        return res;
    }

    // Dans tous les autres cas (commandes de suppression, confirmation, lecture, etc.) :
    // la présence d'une négation affecte la commande principale.
    res.commandNegated = true;

    // Détection spécifique : confirmation niée
    // Ex: "je ne confirme pas la suppression", "ne confirme pas", "pas de confirmation", "non"
    for (size_t i = 0; i < n; ++i) {
        const bool isNeg = (ConceptDictionary::hasConcept(tokens[i].conceptMask, ConceptId::NEGATION) ||
                            tokens[i].text == "non");
        if (isNeg) {
            // Examiner les tokens voisins pour détecter confirm / delete
            for (size_t j = 0; j < n; ++j) {
                if (ConceptDictionary::hasConcept(tokens[j].conceptMask, ConceptId::CONFIRM)) {
                    res.confirmationNegated = true;
                }
                if (ConceptDictionary::hasConcept(tokens[j].conceptMask, ConceptId::DELETE)) {
                    res.deletionNegated = true;
                }
            }
        }
    }

    return res;
}

bool NegationDetector::isConfirmationNegated(const std::string& normalizedText) {
    // Vérifier les mots complets (pour éviter les faux positifs sur les mots finissant par 'n' comme "suppression ")
    size_t start = 0;
    const size_t len = normalizedText.length();

    while (start < len) {
        while (start < len && normalizedText[start] == ' ') {
            ++start;
        }
        if (start >= len) break;

        size_t end = start;
        while (end < len && normalizedText[end] != ' ') {
            ++end;
        }

        const std::string word = normalizedText.substr(start, end - start);
        if (word == "ne" || word == "n" || word == "pas" || word == "jamais" ||
            word == "rien" || word == "aucun" || word == "aucune" || word == "non") {
            return true;
        }

        start = end;
    }

    return false;
}

} // namespace VoiceJournal::Parser
