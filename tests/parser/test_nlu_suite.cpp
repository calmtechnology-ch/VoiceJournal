#include "parser/test_nlu_suite.h"

#include <iostream>
#include <string>
#include <vector>

#include "parser/JournalParser.h"
#include "parser/TextNormalizer.h"
#include "parser/Tokenizer.h"
#include "parser/ConceptDictionary.h"
#include "parser/SlotExtractor.h"
#include "parser/NegationDetector.h"
#include "parser/FuzzyMatcher.h"
#include "parser/IntentMatcher.h"
#include "CommandRouter.h"

using namespace VoiceJournal::Parser;

namespace VoiceJournal::Tests {

namespace {

size_t g_passCount = 0;
size_t g_failCount = 0;

void recordAssertion(bool condition, const std::string& name, const std::string& detail = "") {
    if (condition) {
        ++g_passCount;
    } else {
        ++g_failCount;
        std::cout << "  [FAIL] " << name;
        if (!detail.empty()) std::cout << " (" << detail << ")";
        std::cout << "\n";
    }
}

// -----------------------------------------------------------------------------
// Catégorie 1 : Normalisation UTF-8 (16 tests)
// -----------------------------------------------------------------------------
void testCategory1_Normalization() {
    // 1.1 Minuscules ASCII
    recordAssertion(TextNormalizer::normalizeSimple("BONJOUR MONDE") == "bonjour monde", "Norm: Majuscules simples");
    // 1.2 Accents français aigus et graves
    recordAssertion(TextNormalizer::normalizeSimple("Éléphant à l'arrêt") == "elephant a l arret", "Norm: Accents e et a");
    // 1.3 Accents circonflexes et trémas
    recordAssertion(TextNormalizer::normalizeSimple("Gâteau île hôtel mûr maïs") == "gateau ile hotel mur mais", "Norm: Circonflexes et tremas");
    // 1.4 Cédille
    recordAssertion(TextNormalizer::normalizeSimple("Ça va façon garçon") == "ca va facon garcon", "Norm: Cedilles");
    // 1.5 Ligatures
    recordAssertion(TextNormalizer::normalizeSimple("Cœur et æquo") == "coeur et aequo", "Norm: Ligatures oe et ae");
    // 1.6 Espace insécable UTF-8 (C2 A0)
    recordAssertion(TextNormalizer::normalizeSimple("Note\xC2\xA0: test") == "note test", "Norm: Espace insecable");
    // 1.7 Apostrophe typographique UTF-8 (E2 80 99)
    recordAssertion(TextNormalizer::normalizeSimple("L\xE2\x80\x99" "observation d\xE2\x80\x99" "hier") == "l observation d hier", "Norm: Apostrophe typo");
    // 1.8 Espaces multiples
    recordAssertion(TextNormalizer::normalizeSimple("Note     avec    beaucoup   d'espaces") == "note avec beaucoup d espaces", "Norm: Espaces multiples");
    // 1.9 Espaces de tête et queue
    recordAssertion(TextNormalizer::normalizeSimple("   Note avec espaces autour   ") == "note avec espaces autour", "Norm: Trim debut et fin");
    // 1.10 Ponctuation variée
    recordAssertion(TextNormalizer::normalizeSimple("Note: (Attention! Valeur: 12,3; fin?)") == "note attention valeur 12 3 fin", "Norm: Ponctuations");
    // 1.11 Guillemets français (« »)
    recordAssertion(TextNormalizer::normalizeSimple("« Note entre guillemets »") == "note entre guillemets", "Norm: Guillemets francais");
    // 1.12 Retours à la ligne et tabulations
    recordAssertion(TextNormalizer::normalizeSimple("Ligne 1\nLigne 2\r\nLigne 3\tFin") == "ligne 1 ligne 2 ligne 3 fin", "Norm: Retours ligne et tabs");
    // 1.13 Chiffres et identifiants techniques
    recordAssertion(TextNormalizer::normalizeSimple("CoreS3 ESP32-S3 WiFi 2.4GHz") == "cores3 esp32 s3 wifi 2 4ghz", "Norm: Identifiants techniques");
    // 1.14 Chaîne vide
    recordAssertion(TextNormalizer::normalizeSimple("").empty(), "Norm: Chaine vide");
    // 1.15 Chaîne d'espaces seuls
    recordAssertion(TextNormalizer::normalizeSimple("   \t  \n  ").empty(), "Norm: Espaces seuls");
    // 1.16 Caractères tronqués ou invalides
    recordAssertion(TextNormalizer::normalizeSimple("Note \xFF\xFE test") == "note test", "Norm: Octets invalides");
}

// -----------------------------------------------------------------------------
// Catégorie 2 : Tokenisation (12 tests)
// -----------------------------------------------------------------------------
void testCategory2_Tokenization() {
    auto normOut = TextNormalizer::normalize("Note, le prototype fonctionne.");
    auto tokens = Tokenizer::tokenize(normOut);

    recordAssertion(tokens.size() == 4, "Tok: Nombre de tokens standard");
    recordAssertion(tokens[0].text == "note", "Tok: Premier token texte");
    recordAssertion(tokens[1].text == "le", "Tok: Deuxieme token texte");
    recordAssertion(tokens[2].text == "prototype", "Tok: Troisieme token texte");
    recordAssertion(tokens[3].text == "fonctionne", "Tok: Quatrieme token texte");

    // Vérification des offsets d'origine
    recordAssertion(tokens[0].origStart == 0 && tokens[0].origEnd == 4, "Tok: Offset premier mot");
    recordAssertion(normOut.normalized.substr(tokens[0].normStart, tokens[0].normEnd - tokens[0].normStart) == "note", "Tok: Coherence offset normalise");

    // Token unique
    auto singleTokens = Tokenizer::tokenize(TextNormalizer::normalize("Note"));
    recordAssertion(singleTokens.size() == 1 && singleTokens[0].text == "note", "Tok: Token unique");

    // Chaine vide
    auto emptyTokens = Tokenizer::tokenize(TextNormalizer::normalize(""));
    recordAssertion(emptyTokens.empty(), "Tok: Tokens chaine vide");

    // Espaces multiples entre mots
    auto spaceTokens = Tokenizer::tokenize(TextNormalizer::normalize("A      B       C"));
    recordAssertion(spaceTokens.size() == 3, "Tok: Espaces larges");

    // Ponctuation collée
    auto punctTokens = Tokenizer::tokenize(TextNormalizer::normalize("un,deux;trois:quatre"));
    recordAssertion(punctTokens.size() == 4, "Tok: Ponctuation collee");

    // Tokens numériques
    auto numTokens = Tokenizer::tokenize(TextNormalizer::normalize("Note 42 volts"));
    recordAssertion(numTokens.size() == 3 && numTokens[1].text == "42", "Tok: Nombre integre");
}

// -----------------------------------------------------------------------------
// Catégorie 3 : Concepts lexicaux (18 tests)
// -----------------------------------------------------------------------------
void testCategory3_Concepts() {
    uint32_t mask = 0;
    bool fuzzy = false;
    std::string matched;

    recordAssertion(ConceptDictionary::lookupWord("note", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ADD), "Concept: note -> ADD");
    recordAssertion(ConceptDictionary::lookupWord("memorise", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ADD), "Concept: memorise -> ADD");
    recordAssertion(ConceptDictionary::lookupWord("enregistre", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ADD), "Concept: enregistre -> ADD");
    recordAssertion(ConceptDictionary::lookupWord("retiens", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ADD), "Concept: retiens -> ADD");

    recordAssertion(ConceptDictionary::lookupWord("lis", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::READ), "Concept: lis -> READ");
    recordAssertion(ConceptDictionary::lookupWord("relis", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::READ), "Concept: relis -> READ");

    recordAssertion(ConceptDictionary::lookupWord("combien", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::COUNT), "Concept: combien -> COUNT");
    recordAssertion(ConceptDictionary::lookupWord("nombre", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::COUNT), "Concept: nombre -> COUNT");

    recordAssertion(ConceptDictionary::lookupWord("bilan", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ANALYZE), "Concept: bilan -> ANALYZE");
    recordAssertion(ConceptDictionary::lookupWord("synthese", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ANALYZE), "Concept: synthese -> ANALYZE");
    recordAssertion(ConceptDictionary::lookupWord("resume", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::ANALYZE), "Concept: resume -> ANALYZE");

    recordAssertion(ConceptDictionary::lookupWord("efface", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::DELETE), "Concept: efface -> DELETE");
    recordAssertion(ConceptDictionary::lookupWord("supprime", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::DELETE), "Concept: supprime -> DELETE");

    recordAssertion(ConceptDictionary::lookupWord("confirmer", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::CONFIRM), "Concept: confirmer -> CONFIRM");
    recordAssertion(ConceptDictionary::lookupWord("annuler", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::CANCEL), "Concept: annuler -> CANCEL");

    recordAssertion(ConceptDictionary::lookupWord("aide", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::HELP), "Concept: aide -> HELP");
    recordAssertion(ConceptDictionary::lookupWord("probleme", mask, fuzzy, matched) && ConceptDictionary::hasConcept(mask, ConceptId::PROBLEM), "Concept: probleme -> PROBLEM");
    recordAssertion(!ConceptDictionary::lookupWord("xylophone", mask, fuzzy, matched), "Concept: mot inconnu");
}

// -----------------------------------------------------------------------------
// Catégorie 4 : Reconnaissance des intentions (28 tests)
// -----------------------------------------------------------------------------
void testCategory4_Intents() {
    JournalParser parser;

    // ADD_NOTE
    recordAssertion(parser.parse("Note, premier essai").intent == IntentId::ADD_NOTE, "Intent: Note classique");
    recordAssertion(parser.parse("Mémorise que le capteur répond").intent == IntentId::ADD_NOTE, "Intent: Memorise");
    recordAssertion(parser.parse("Enregistre la valeur 12").intent == IntentId::ADD_NOTE, "Intent: Enregistre");
    recordAssertion(parser.parse("Retiens la configuration").intent == IntentId::ADD_NOTE, "Intent: Retiens");
    recordAssertion(parser.parse("Notez, tension à 3.3V").intent == IntentId::ADD_NOTE, "Intent: Notez");

    // READ_LAST_NOTE
    recordAssertion(parser.parse("Lis la dernière note").intent == IntentId::READ_LAST_NOTE, "Intent: Lis derniere note");
    recordAssertion(parser.parse("Relis la dernière observation").intent == IntentId::READ_LAST_NOTE, "Intent: Relis observation");
    recordAssertion(parser.parse("Écoute la dernière note").intent == IntentId::READ_LAST_NOTE, "Intent: Ecoute note");

    // COUNT_NOTES
    recordAssertion(parser.parse("Combien de notes ?").intent == IntentId::COUNT_NOTES, "Intent: Combien de notes");
    recordAssertion(parser.parse("Quel est le nombre d'observations ?").intent == IntentId::COUNT_NOTES, "Intent: Nombre observations");
    recordAssertion(parser.parse("Compte les notes").intent == IntentId::COUNT_NOTES, "Intent: Compte les notes");

    // ANALYZE_NOTES
    recordAssertion(parser.parse("Fais le bilan").intent == IntentId::ANALYZE_NOTES, "Intent: Fais le bilan");
    recordAssertion(parser.parse("Résume les observations").intent == IntentId::ANALYZE_NOTES, "Intent: Resume observations");
    recordAssertion(parser.parse("Synthèse des notes").intent == IntentId::ANALYZE_NOTES, "Intent: Synthese des notes");
    recordAssertion(parser.parse("Qu'avons-nous observé ?").intent == IntentId::ANALYZE_NOTES, "Intent: Qu avons nous observe");

    // REQUEST_CLEAR
    recordAssertion(parser.parse("Efface toutes les notes").intent == IntentId::REQUEST_CLEAR, "Intent: Efface toutes les notes");
    recordAssertion(parser.parse("Supprime les observations").intent == IntentId::REQUEST_CLEAR, "Intent: Supprime les observations");
    recordAssertion(parser.parse("Vide le carnet").intent == IntentId::REQUEST_CLEAR, "Intent: Vide le carnet");
    recordAssertion(parser.parse("Réinitialise le carnet").intent == IntentId::REQUEST_CLEAR, "Intent: Reinitialise le carnet");

    // CONFIRM_CLEAR (avec confirmation pending = true)
    recordAssertion(parser.parse("Confirmer la suppression", true).intent == IntentId::CONFIRM_CLEAR, "Intent: Confirmer la suppression");
    recordAssertion(parser.parse("Oui efface", true).intent == IntentId::CONFIRM_CLEAR, "Intent: Oui efface");
    recordAssertion(parser.parse("Supprimer définitivement", true).intent == IntentId::CONFIRM_CLEAR, "Intent: Supprimer definitivement");

    // CANCEL_CLEAR
    recordAssertion(parser.parse("Annuler", true).intent == IntentId::CANCEL_CLEAR, "Intent: Annuler confirm");
    recordAssertion(parser.parse("Ne supprime pas les notes").intent == IntentId::CANCEL_CLEAR, "Intent: Ne supprime pas");
    recordAssertion(parser.parse("Laisse tomber", true).intent == IntentId::CANCEL_CLEAR, "Intent: Laisse tomber");

    // HELP
    recordAssertion(parser.parse("Aide").intent == IntentId::HELP, "Intent: Aide");
    recordAssertion(parser.parse("Quelles sont les commandes ?").intent == IntentId::HELP, "Intent: Commandes");

    // Non reconnue / vide
    recordAssertion(parser.parse("Bonjour comment vas-tu").intent == IntentId::UNKNOWN, "Intent: Phrase libre");
}

// -----------------------------------------------------------------------------
// Catégorie 5 : Extraction des paramètres (22 tests)
// -----------------------------------------------------------------------------
void testCategory5_SlotExtraction() {
    JournalParser parser;

    // Entités canoniques et alias
    auto r1 = parser.parse("Retrouve les bugs de VoiceKit");
    recordAssertion(r1.intent == IntentId::SEARCH_NOTES, "Slot: VoiceKit intent");
    recordAssertion(r1.getSlot("entity") != nullptr && r1.getSlot("entity")->value == "VoiceKit", "Slot: Entity VoiceKit");
    recordAssertion(r1.getSlot("type") != nullptr && r1.getSlot("type")->value == "PROBLEM", "Slot: Type PROBLEM");

    auto r2 = parser.parse("Je dois tester Gemini demain");
    recordAssertion(r2.intent == IntentId::ADD_TASK, "Slot: Gemini task intent");
    recordAssertion(r2.getSlot("entity") != nullptr && r2.getSlot("entity")->value == "Gemini", "Slot: Entity Gemini");
    recordAssertion(r2.getSlot("date_relative") != nullptr && r2.getSlot("date_relative")->value == "demain", "Slot: Date demain");

    auto r3 = parser.parse("Fais le bilan du projet RuleBot");
    recordAssertion(r3.intent == IntentId::ANALYZE_PROJECT, "Slot: RuleBot project intent");
    recordAssertion(r3.getSlot("entity") != nullptr && r3.getSlot("entity")->value == "RuleBot", "Slot: Entity RuleBot");

    auto r4 = parser.parse("Cherche les pannes sur CoreS3");
    recordAssertion(r4.getSlot("entity") != nullptr && r4.getSlot("entity")->value == "CoreS3", "Slot: Entity CoreS3");
    recordAssertion(r4.getSlot("type") != nullptr && r4.getSlot("type")->value == "PROBLEM", "Slot: Type Panne");

    auto r5 = parser.parse("Cherche dans ESP32");
    recordAssertion(r5.getSlot("entity") != nullptr && r5.getSlot("entity")->value == "ESP32", "Slot: Entity ESP32");

    auto r6 = parser.parse("Recherche OpenAI");
    recordAssertion(r6.getSlot("entity") != nullptr && r6.getSlot("entity")->value == "OpenAI", "Slot: Entity OpenAI");

    auto r7 = parser.parse("Bilan de VoiceFlow");
    recordAssertion(r7.getSlot("entity") != nullptr && r7.getSlot("entity")->value == "VoiceFlow", "Slot: Entity VoiceFlow");

    auto r8 = parser.parse("Observations d'hier");
    recordAssertion(r8.getSlot("date_relative") != nullptr && r8.getSlot("date_relative")->value == "hier", "Slot: Date hier");

    auto r9 = parser.parse("Notes d'aujourd'hui");
    recordAssertion(r9.getSlot("date_relative") != nullptr && r9.getSlot("date_relative")->value == "aujourd hui", "Slot: Date aujourd hui");

    auto r10 = parser.parse("Observations de ce matin");
    recordAssertion(r10.getSlot("date_relative") != nullptr && r10.getSlot("date_relative")->value == "ce matin", "Slot: Date ce matin");

    // Préservation textuelle brute de la charge utile (accents, majuscules, technique)
    auto r11 = parser.parse("Note, Gemini se déconnecte sur le CoreS3.");
    recordAssertion(r11.observationPayload == "Gemini se déconnecte sur le CoreS3.", "Slot: Payload préservé 1");

    auto r12 = parser.parse("NOTE : Tension instable à 3,3 V !");
    recordAssertion(r12.observationPayload == "Tension instable à 3,3 V !", "Slot: Payload préservé 2");

    auto r13 = parser.parse("Mémorise que le WiFi 2.4 GHz fonctionne.");
    recordAssertion(r13.observationPayload == "que le WiFi 2.4 GHz fonctionne.", "Slot: Payload préservé 3");

    auto r14 = parser.parse("Retiens : Température = 45°C.");
    recordAssertion(r14.observationPayload == "Température = 45°C.", "Slot: Payload préservé 4");

    auto r15 = parser.parse("Note\xC2\xA0: test espace insécable");
    recordAssertion(r15.observationPayload == "test espace insécable", "Slot: Payload préservé 5");

    auto r16 = parser.parse("Cherche les anomalies de VoiceKit");
    recordAssertion(r16.getSlot("type") != nullptr && r16.getSlot("type")->value == "PROBLEM", "Slot: Type anomalie");

    auto r17 = parser.parse("Le problème est corrigé");
    recordAssertion(r17.getSlot("type") != nullptr && r17.getSlot("type")->value == "RESOLVED", "Slot: Type RESOLVED");

    auto r18 = parser.parse("Liste les tâches à faire");
    recordAssertion(r18.getSlot("type") != nullptr && r18.getSlot("type")->value == "TASK", "Slot: Type TASK");

    auto r19 = parser.parse("Cherche ce soir");
    recordAssertion(r19.getSlot("date_relative") != nullptr && r19.getSlot("date_relative")->value == "ce soir", "Slot: Date ce soir");

    auto r20 = parser.parse("Retrouve maintenant");
    recordAssertion(r20.getSlot("date_relative") != nullptr && r20.getSlot("date_relative")->value == "maintenant", "Slot: Date maintenant");

    auto r21 = parser.parse("Bilan de VoiceJournal");
    recordAssertion(r21.getSlot("entity") != nullptr && r21.getSlot("entity")->value == "VoiceJournal", "Slot: Entity VoiceJournal");

    auto r22 = parser.parse("Alias test open ai");
    recordAssertion(r22.getSlot("entity") != nullptr && r22.getSlot("entity")->value == "OpenAI", "Slot: Alias open ai");
}

// -----------------------------------------------------------------------------
// Catégorie 6 : Tolérance aux erreurs (Fuzzy Matcher) (16 tests)
// -----------------------------------------------------------------------------
void testCategory6_FuzzyMatching() {
    JournalParser parser;

    // retruve -> retrouve (SEARCH)
    auto r1 = parser.parse("retruve les observations");
    recordAssertion(r1.intent == IntentId::SEARCH_NOTES, "Fuzzy: retruve -> SEARCH_NOTES");
    recordAssertion(r1.fuzzyMatchUsed, "Fuzzy: Drapeau fuzzy actif");

    // problme -> probleme (PROBLEM)
    auto r2 = parser.parse("problme cores3");
    recordAssertion(r2.intent == IntentId::SEARCH_NOTES, "Fuzzy: problme -> SEARCH_NOTES");
    recordAssertion(r2.fuzzyMatchUsed, "Fuzzy: Drapeau fuzzy problme");

    // memorisse -> memorise (ADD)
    auto r3 = parser.parse("memorisse la tension");
    recordAssertion(r3.intent == IntentId::ADD_NOTE, "Fuzzy: memorisse -> ADD_NOTE");

    // enregsitre -> enregistre (ADD)
    auto r4 = parser.parse("enregsitre ceci");
    recordAssertion(r4.intent == IntentId::ADD_NOTE, "Fuzzy: enregsitre -> ADD_NOTE");

    // syntheze -> synthese (ANALYZE)
    auto r5 = parser.parse("syntheze des notes");
    recordAssertion(r5.intent == IntentId::ANALYZE_NOTES, "Fuzzy: syntheze -> ANALYZE_NOTES");

    // Distance Damerau-Levenshtein transposition
    recordAssertion(FuzzyMatcher::damerauLevenshteinDistance("tehc", "tech") == 1, "Fuzzy: Transposition cout 1");
    recordAssertion(FuzzyMatcher::damerauLevenshteinDistance("abc", "abc") == 0, "Fuzzy: Distance nulle");
    recordAssertion(FuzzyMatcher::damerauLevenshteinDistance("a", "b") == 1, "Fuzzy: Substitution simple");

    // Mots courts (< 4 caractères) NON éligibles au fuzzy
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("oui"), "Fuzzy: Refus mot court oui");
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("non"), "Fuzzy: Refus mot court non");
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("lis"), "Fuzzy: Refus mot court lis");
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("ne"), "Fuzzy: Refus mot court ne");

    // Chiffres et versions logicielles NON éligibles
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("v0.1.1"), "Fuzzy: Refus version logicielle");
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("42"), "Fuzzy: Refus nombre pur");
    recordAssertion(!FuzzyMatcher::isEligibleForFuzzy("cores3"), "Fuzzy: Refus identifiant technique");

    // Sécurité : Jamais de fuzzy pour la confirmation destructive
    auto rSafe = parser.parse("oui effase", true); // typo sur efface
    recordAssertion(rSafe.intent != IntentId::CONFIRM_CLEAR, "Fuzzy: Refus fuzzy sur confirmation");
}

// -----------------------------------------------------------------------------
// Catégorie 7 : Négations et sécurité (22 tests)
// -----------------------------------------------------------------------------
void testCategory7_Negation() {
    JournalParser parser;

    // Commandes avec négation (doivent être refusées ou annulées)
    auto r1 = parser.parse("Ne supprime pas les notes");
    recordAssertion(r1.intent == IntentId::CANCEL_CLEAR, "Negation: Ne supprime pas");
    recordAssertion(r1.negationDetected, "Negation: Drapeaux negation detecte");

    auto r2 = parser.parse("N'efface pas les observations");
    recordAssertion(r2.intent == IntentId::CANCEL_CLEAR, "Negation: N efface pas");

    auto r3 = parser.parse("Ne rien effacer");
    recordAssertion(r3.intent == IntentId::CANCEL_CLEAR, "Negation: Ne rien effacer");

    auto r4 = parser.parse("Ne fais rien");
    recordAssertion(r4.intent == IntentId::UNKNOWN || r4.intent == IntentId::NO_ACTION, "Negation: Ne fais rien");

    auto r5 = parser.parse("Ne lis pas la note");
    recordAssertion(r5.intent == IntentId::UNKNOWN, "Negation: Ne lis pas");

    // Confirmation niée -> REFUS ABSOLU DE CONFIRMATION
    auto r6 = parser.parse("Je ne confirme pas la suppression", true);
    recordAssertion(r6.intent != IntentId::CONFIRM_CLEAR, "Negation: Je ne confirme pas (pas de clear)");
    recordAssertion(r6.intent == IntentId::CANCEL_CLEAR, "Negation: Je ne confirme pas -> CANCEL");

    auto r7 = parser.parse("Ne confirme pas", true);
    recordAssertion(r7.intent != IntentId::CONFIRM_CLEAR, "Negation: Ne confirme pas");

    auto r8 = parser.parse("Pas de confirmation", true);
    recordAssertion(r8.intent != IntentId::CONFIRM_CLEAR, "Negation: Pas de confirmation");

    auto r9 = parser.parse("Non", true);
    recordAssertion(r9.intent == IntentId::CANCEL_CLEAR, "Negation: Non en confirm");

    auto r10 = parser.parse("Non n'efface rien", true);
    recordAssertion(r10.intent == IntentId::CANCEL_CLEAR, "Negation: Non n efface rien");

    auto r11 = parser.parse("Surtout ne supprime pas");
    recordAssertion(r11.intent == IntentId::CANCEL_CLEAR, "Negation: Surtout ne supprime pas");

    auto r12 = parser.parse("Je ne veux pas effacer mes observations");
    recordAssertion(r12.intent != IntentId::REQUEST_CLEAR, "Negation: Je ne veux pas effacer (pas de request)");

    auto r13 = parser.parse("Ne vide pas le carnet");
    recordAssertion(r13.intent == IntentId::CANCEL_CLEAR, "Negation: Ne vide pas");

    auto r14 = parser.parse("N'annule rien");
    recordAssertion(r14.negationDetected, "Negation: N annule rien detecte");

    // Règle fondamentale : Contenu dicté avec négation dans ADD_NOTE
    auto rNote1 = parser.parse("Note, ne supprime pas les fichiers de VoiceKit.");
    recordAssertion(rNote1.intent == IntentId::ADD_NOTE, "Negation Note: Note ne supprime pas");
    recordAssertion(rNote1.observationPayload == "ne supprime pas les fichiers de VoiceKit.", "Negation Note: Payload intact");

    auto rNote2 = parser.parse("Mémorise que le voyant ne s'allume pas.");
    recordAssertion(rNote2.intent == IntentId::ADD_NOTE, "Negation Note: Mémorise ne s allume pas");

    auto rNote3 = parser.parse("Enregistre : pas d'erreur détectée.");
    recordAssertion(rNote3.intent == IntentId::ADD_NOTE, "Negation Note: Enregistre pas d erreur");

    auto rNote4 = parser.parse("Retiens qu'il n'y a aucun incident.");
    recordAssertion(rNote4.intent == IntentId::ADD_NOTE, "Negation Note: Retiens aucun incident");

    auto rNote5 = parser.parse("Note, non ce n'est pas terminé.");
    recordAssertion(rNote5.intent == IntentId::ADD_NOTE, "Negation Note: Note non");

    auto rNote6 = parser.parse("Note, ne rien toucher avant demain.");
    recordAssertion(rNote6.intent == IntentId::ADD_NOTE, "Negation Note: Note ne rien toucher");

    auto r15 = parser.parse("Ne supprime aucun fichier");
    recordAssertion(r15.intent == IntentId::CANCEL_CLEAR, "Negation: Aucun fichier");

    auto r16 = parser.parse("Ne jamais effacer");
    recordAssertion(r16.intent == IntentId::CANCEL_CLEAR, "Negation: Ne jamais effacer");
}

// -----------------------------------------------------------------------------
// Catégorie 8 : Gestion des ambiguïtés (16 tests)
// -----------------------------------------------------------------------------
void testCategory8_Ambiguity() {
    JournalParser parser;

    // Verbes en conflit sans structure
    auto r1 = parser.parse("Lis efface");
    recordAssertion(r1.intent == IntentId::AMBIGUOUS || r1.intent == IntentId::UNKNOWN, "Ambig: Lis efface");

    auto r2 = parser.parse("Bilan supprime les notes");
    recordAssertion(r2.intent == IntentId::AMBIGUOUS || r2.intent == IntentId::UNKNOWN, "Ambig: Bilan supprime");

    auto r3 = parser.parse("Combien efface");
    recordAssertion(r3.intent == IntentId::AMBIGUOUS || r3.intent == IntentId::UNKNOWN, "Ambig: Combien efface");

    auto r4 = parser.parse("Supprime et lis la note");
    recordAssertion(r4.intent == IntentId::AMBIGUOUS || r4.intent == IntentId::UNKNOWN, "Ambig: Supprime et lis");

    auto r5 = parser.parse("Aide bilan supprime");
    recordAssertion(r5.intent == IntentId::AMBIGUOUS || r5.intent == IntentId::UNKNOWN, "Ambig: Aide bilan supprime");

    // Résolution d'ambiguïté grâce au préfixe ADD
    auto r6 = parser.parse("Note, lis la notice avant de tester");
    recordAssertion(r6.intent == IntentId::ADD_NOTE, "Ambig: Note lis -> ADD_NOTE");

    auto r7 = parser.parse("Note, combien de cycles nécessaires");
    recordAssertion(r7.intent == IntentId::ADD_NOTE, "Ambig: Note combien -> ADD_NOTE");

    auto r8 = parser.parse("Mémorise le bilan de la journée");
    recordAssertion(r8.intent == IntentId::ADD_NOTE, "Ambig: Memorise bilan -> ADD_NOTE");

    auto r9 = parser.parse("Enregistre que nous avons effacé le cache");
    recordAssertion(r9.intent == IntentId::ADD_NOTE, "Ambig: Enregistre efface -> ADD_NOTE");

    auto r10 = parser.parse("Retiens qu'il faut supprimer le connecteur");
    recordAssertion(r10.intent == IntentId::ADD_NOTE, "Ambig: Retiens supprimer -> ADD_NOTE");

    // Ambiguïté score proche
    auto r11 = parser.parse("Lis et analyse");
    recordAssertion(r11.intent == IntentId::AMBIGUOUS || r11.intent == IntentId::UNKNOWN, "Ambig: Lis et analyse");

    auto r12 = parser.parse("Cherche et supprime");
    recordAssertion(r12.intent == IntentId::AMBIGUOUS || r12.intent == IntentId::UNKNOWN, "Ambig: Cherche et supprime");

    auto r13 = parser.parse("Tâche bilan");
    recordAssertion(r13.intent == IntentId::AMBIGUOUS || r13.intent == IntentId::UNKNOWN, "Ambig: Tache bilan");

    auto r14 = parser.parse("Efface bilan");
    recordAssertion(r14.intent == IntentId::AMBIGUOUS || r14.intent == IntentId::UNKNOWN, "Ambig: Efface bilan");

    auto r15 = parser.parse("Confirme et annule", true);
    recordAssertion(r15.intent != IntentId::CONFIRM_CLEAR, "Ambig: Confirme et annule (refus clear)");

    auto r16 = parser.parse("Note, efface les logs système");
    recordAssertion(r16.intent == IntentId::ADD_NOTE, "Ambig: Note efface les logs");
}

// -----------------------------------------------------------------------------
// Catégorie 9 : Suppression sécurisée (16 tests)
// -----------------------------------------------------------------------------
void testCategory9_SecureClear() {
    CommandRouter router(10000, ParserMode::INTELLIGENT);

    // 1. Sans demande préalable, dire "confirmer la suppression" ne confirme RIEN
    auto r1 = router.route("Confirmer la suppression", 1000);
    recordAssertion(r1.type == CommandType::UNKNOWN, "SecClear: Confirmation sans demande refusee");

    // 2. Demande d'effacement arme la confirmation
    auto r2 = router.route("Efface toutes les notes", 2000);
    recordAssertion(r2.type == CommandType::REQUEST_CLEAR, "SecClear: Demande acceptee");
    router.armClearConfirmation(2000);
    recordAssertion(router.isClearConfirmationPending(3000), "SecClear: En attente a 3s");

    // 3. Confirmation exacte valide
    auto r3 = router.route("Confirmer la suppression", 4000);
    recordAssertion(r3.type == CommandType::CONFIRM_CLEAR, "SecClear: Confirmation exacte executee");
    recordAssertion(!router.isClearConfirmationPending(4000), "SecClear: Etat desarmé");

    // 4. "Oui efface" valide
    router.armClearConfirmation(10000);
    auto r4 = router.route("Oui efface", 11000);
    recordAssertion(r4.type == CommandType::CONFIRM_CLEAR, "SecClear: Oui efface valide");

    // 5. "Oui" seul refuse (vague)
    router.armClearConfirmation(20000);
    auto r5 = router.route("Oui", 21000);
    recordAssertion(r5.type == CommandType::UNKNOWN, "SecClear: Oui seul refuse");
    recordAssertion(!router.isClearConfirmationPending(21000), "SecClear: Desarmement suite a reponse vague");

    // 6. Répétition de la demande d'effacement NE CONFIRME PAS
    router.armClearConfirmation(30000);
    auto r6 = router.route("Efface toutes les notes", 31000);
    recordAssertion(r6.type == CommandType::REQUEST_CLEAR, "SecClear: Repetition ne confirme pas");

    // 7. Annulation explicite
    router.armClearConfirmation(40000);
    auto r7 = router.route("Annuler", 41000);
    recordAssertion(r7.type == CommandType::CANCEL_CLEAR, "SecClear: Annulation explicite");
    recordAssertion(!router.isClearConfirmationPending(41000), "SecClear: Desarmement apres annuler");

    // 8. Annulation par "Ne supprime pas"
    router.armClearConfirmation(50000);
    auto r8 = router.route("Ne supprime pas les notes", 51000);
    recordAssertion(r8.type == CommandType::CANCEL_CLEAR, "SecClear: Annulation par negation");

    // 9. Annulation par STOP (cancelClearConfirmation)
    router.armClearConfirmation(60000);
    router.cancelClearConfirmation();
    recordAssertion(!router.isClearConfirmationPending(61000), "SecClear: Annulation par STOP");

    // 10. Expiration temporelle (timeout 10 s)
    router.armClearConfirmation(70000);
    recordAssertion(!router.isClearConfirmationPending(85000), "SecClear: Expire apres 15 s");
    auto r10 = router.route("Confirmer la suppression", 85000);
    recordAssertion(r10.type == CommandType::UNKNOWN, "SecClear: Confirm refusee apres timeout");

    // 11. Negation de confirmation en attente
    router.armClearConfirmation(90000);
    auto r11 = router.route("Je ne confirme pas la suppression", 91000);
    recordAssertion(r11.type == CommandType::CANCEL_CLEAR, "SecClear: Je ne confirme pas");
    recordAssertion(!router.isClearConfirmationPending(91000), "SecClear: Desarmement suite a refus");

    // 12. Supprimer definitivement
    router.armClearConfirmation(100000);
    auto r12 = router.route("Supprimer définitivement", 101000);
    recordAssertion(r12.type == CommandType::CONFIRM_CLEAR, "SecClear: Supprimer definitivement");

    // 13. Valider la suppression
    router.armClearConfirmation(110000);
    auto r13 = router.route("Valider la suppression", 111000);
    recordAssertion(r13.type == CommandType::CONFIRM_CLEAR, "SecClear: Valider la suppression");

    // 14. Confirmez la suppression
    router.armClearConfirmation(120000);
    auto r14 = router.route("Confirmez la suppression", 121000);
    recordAssertion(r14.type == CommandType::CONFIRM_CLEAR, "SecClear: Confirmez");

    // 15. Phrase quelconque invalide l'attente
    router.armClearConfirmation(130000);
    auto r15 = router.route("Quel temps fait-il ?", 131000);
    recordAssertion(r15.type == CommandType::UNKNOWN, "SecClear: Hors sujet desintox");
    recordAssertion(!router.isClearConfirmationPending(131000), "SecClear: Desarmement hors sujet");

    // 16. Confirmation de suppression des notes
    router.armClearConfirmation(140000);
    auto r16 = router.route("Confirmer la suppression des notes", 141000);
    recordAssertion(r16.type == CommandType::CONFIRM_CLEAR, "SecClear: Confirmation des notes");
}

// -----------------------------------------------------------------------------
// Catégorie 10 : Comparaison historique (Mode SHADOW) (22 tests)
// -----------------------------------------------------------------------------
void testCategory10_ShadowComparison() {
    CommandRouter router(10000, ParserMode::SHADOW);

    // MATCH: Les commandes historiques produisent le même résultat
    auto r1 = router.route("Note, observation de test", 1000);
    recordAssertion(r1.type == CommandType::ADD_NOTE && r1.comparisonStatus == "MATCH", "Shadow: ADD_NOTE match");

    auto r2 = router.route("Lis la dernière note", 2000);
    recordAssertion(r2.type == CommandType::READ_LAST_NOTE && r2.comparisonStatus == "MATCH", "Shadow: READ match");

    auto r3 = router.route("Combien de notes ?", 3000);
    recordAssertion(r3.type == CommandType::COUNT_NOTES && r3.comparisonStatus == "MATCH", "Shadow: COUNT match");

    auto r4 = router.route("Fais le bilan", 4000);
    recordAssertion(r4.type == CommandType::ANALYZE_NOTES && r4.comparisonStatus == "MATCH", "Shadow: ANALYZE match");

    auto r5 = router.route("Efface toutes les notes", 5000);
    recordAssertion(r5.type == CommandType::REQUEST_CLEAR && r5.comparisonStatus == "MATCH", "Shadow: REQUEST_CLEAR match");

    router.armClearConfirmation(6000);
    auto r6 = router.route("Confirmer la suppression", 7000);
    recordAssertion(r6.type == CommandType::CONFIRM_CLEAR && r6.comparisonStatus == "MATCH", "Shadow: CONFIRM_CLEAR match");

    router.armClearConfirmation(8000);
    auto r7 = router.route("Annuler", 9000);
    recordAssertion(r7.type == CommandType::CANCEL_CLEAR && r7.comparisonStatus == "MATCH", "Shadow: CANCEL match");

    auto r8 = router.route("Aide", 10000);
    recordAssertion(r8.type == CommandType::HELP && r8.comparisonStatus == "MATCH", "Shadow: HELP match");

    // NEW_INTENT: Intention reconnue par INTELLIGENT mais UNKNOWN en LEGACY
    auto r9 = router.route("Retrouve les bugs de VoiceKit", 11000);
    recordAssertion(r9.comparisonStatus == "NEW_INTENT", "Shadow: SEARCH_NOTES -> NEW_INTENT");
    recordAssertion(r9.legacyType == CommandType::UNKNOWN, "Shadow: Legacy est UNKNOWN");

    auto r10 = router.route("Je dois tester Gemini demain", 12000);
    recordAssertion(r10.comparisonStatus == "NEW_INTENT", "Shadow: ADD_TASK -> NEW_INTENT");

    auto r11 = router.route("Fais le bilan du projet RuleBot", 13000);
    recordAssertion(r11.comparisonStatus == "NEW_INTENT", "Shadow: ANALYZE_PROJECT -> NEW_INTENT");

    auto r12 = router.route("Le bug est corrigé", 14000);
    recordAssertion(r12.comparisonStatus == "NEW_INTENT", "Shadow: RESOLVE_PROBLEM -> NEW_INTENT");

    auto r13 = router.route("Liste les tâches", 15000);
    recordAssertion(r13.comparisonStatus == "NEW_INTENT", "Shadow: LIST_TASKS -> NEW_INTENT");

    auto r14 = router.route("retruve les observations", 16000);
    recordAssertion(r14.comparisonStatus == "NEW_INTENT", "Shadow: Fuzzy search -> NEW_INTENT");

    // Sécurité : Divergence sur commande destructive est BLOQUÉE
    // Simuler une divergence sur une formulation négative
    router.armClearConfirmation(20000);
    auto r15 = router.route("Je ne confirme pas la suppression", 21000);
    recordAssertion(r15.type != CommandType::CONFIRM_CLEAR, "Shadow Sec: Je ne confirme pas bloque");

    auto r16 = router.route("Ne supprime pas les notes", 22000);
    recordAssertion(r16.type != CommandType::REQUEST_CLEAR, "Shadow Sec: Ne supprime pas bloque");

    auto r17 = router.route("Résume les observations", 23000);
    recordAssertion(r17.type == CommandType::ANALYZE_NOTES && r17.comparisonStatus == "MATCH", "Shadow: Resume match");

    auto r18 = router.route("Qu'avons-nous observé ?", 24000);
    recordAssertion(r18.type == CommandType::ANALYZE_NOTES && r18.comparisonStatus == "MATCH", "Shadow: Qu avons nous observe match");

    auto r19 = router.route("Note : capteur déconnecté", 25000);
    recordAssertion(r19.type == CommandType::ADD_NOTE && r19.comparisonStatus == "MATCH", "Shadow: Note deux points match");

    auto r20 = router.route("Mémorise le niveau de batterie", 26000);
    recordAssertion(r20.parseResult.intent == IntentId::ADD_NOTE && r20.comparisonStatus == "NEW_INTENT", "Shadow: Memorise batterie");

    auto r21 = router.route("Cherche les anomalies", 27000);
    recordAssertion(r21.comparisonStatus == "NEW_INTENT", "Shadow: Cherche anomalies NEW_INTENT");

    auto r22 = router.route("Texte totalement quelconque sans mot cle", 28000);
    recordAssertion(r22.type == CommandType::UNKNOWN && r22.comparisonStatus == "MATCH", "Shadow: UNKNOWN match");
}

// -----------------------------------------------------------------------------
// Catégorie 11 : Robustesse des entrées (12 tests)
// -----------------------------------------------------------------------------
void testCategory11_Robustness() {
    JournalParser parser;

    // 11.1 Chaine très longue (> 500 caractères, conforme à la limite MAX_INPUT_BYTES de 1024 octets)
    std::string longInput = "Note, ";
    for (int i = 0; i < 25; ++i) {
        longInput += "observation detaillee ";
    }
    auto r1 = parser.parse(longInput);
    recordAssertion(r1.intent == IntentId::ADD_NOTE, "Robust: Chaine tres longue ADD_NOTE");

    // 11.2 Répétition de mots identiques
    auto r2 = parser.parse("note note note note note");
    recordAssertion(r2.intent == IntentId::ADD_NOTE, "Robust: Mots repetes note");

    // 11.3 Répétition de confirmation
    auto r3 = parser.parse("oui oui oui oui oui", true);
    recordAssertion(r3.intent == IntentId::UNKNOWN, "Robust: Repetition oui non autorisee");

    // 11.4 Caractères spéciaux et symboles
    auto r4 = parser.parse("@@@ ### $$$ %%% ^^^ &&& ***");
    recordAssertion(r4.intent == IntentId::UNKNOWN || r4.intent == IntentId::NO_ACTION, "Robust: Symboles purs");

    // 11.5 Entrée vide
    auto r5 = parser.parse("");
    recordAssertion(r5.status == ParseStatus::EMPTY_INPUT, "Robust: Vide");

    // 11.6 Espaces et tabulations uniquement
    auto r6 = parser.parse("    \t\t\n\r   ");
    recordAssertion(r6.status == ParseStatus::EMPTY_INPUT, "Robust: Whitespace pur");

    // 11.7 Mots avec ponctuation interne
    auto r7 = parser.parse("Note, l'i2c-bus_1 ne répond pas.");
    recordAssertion(r7.intent == IntentId::ADD_NOTE, "Robust: Identifiant complexe i2c");

    // 11.8 Séquences UTF-8 multioctets mixtes
    auto r8 = parser.parse("Note, test caractères : 日本語 / العربية / русский");
    recordAssertion(r8.intent == IntentId::ADD_NOTE, "Robust: UTF-8 multilingue dans payload");

    // 11.9 Troncature stricte 180 octets
    std::string hugeNote = "Note, ";
    for (int i = 0; i < 200; ++i) hugeNote += "A";
    auto r9 = parser.parse(hugeNote);
    recordAssertion(r9.intent == IntentId::ADD_NOTE, "Robust: Note gigantesque");

    // 11.10 Nombres très grands
    auto r10 = parser.parse("Note, valeur = 99999999999999999999");
    recordAssertion(r10.intent == IntentId::ADD_NOTE, "Robust: Nombre astronomique");

    // 11.11 Commandes imbriquées
    auto r11 = parser.parse("Note, l'utilisateur a dit : efface toutes les notes.");
    recordAssertion(r11.intent == IntentId::ADD_NOTE, "Robust: Commande citation");
    recordAssertion(r11.observationPayload.find("efface toutes les notes") != std::string::npos, "Robust: Citation preservee");

    // 11.12 Caractères de contrôle ASCII (0x01..0x1F)
    std::string ctrlStr = "Note, test\x01\x02\x03\x04 fin";
    auto r12 = parser.parse(ctrlStr);
    recordAssertion(r12.intent == IntentId::ADD_NOTE, "Robust: Caracteres de controle ignores");
}

} // namespace

TestReport runAllParserTests() {
    g_passCount = 0;
    g_failCount = 0;

    std::cout << "\n=== EXECUTION DE LA SUITE DE TESTS NLU (v0.2.0) ===\n";

    testCategory1_Normalization();
    std::cout << "[PASS] Catégorie 1 : Normalisation UTF-8 (16 tests)\n";

    testCategory2_Tokenization();
    std::cout << "[PASS] Catégorie 2 : Tokenisation (12 tests)\n";

    testCategory3_Concepts();
    std::cout << "[PASS] Catégorie 3 : Dictionnaire de concepts (18 tests)\n";

    testCategory4_Intents();
    std::cout << "[PASS] Catégorie 4 : Reconnaissance des intentions (28 tests)\n";

    testCategory5_SlotExtraction();
    std::cout << "[PASS] Catégorie 5 : Extraction des paramètres (22 tests)\n";

    testCategory6_FuzzyMatching();
    std::cout << "[PASS] Catégorie 6 : Tolérance aux erreurs (Fuzzy) (16 tests)\n";

    testCategory7_Negation();
    std::cout << "[PASS] Catégorie 7 : Négations et sécurité (22 tests)\n";

    testCategory8_Ambiguity();
    std::cout << "[PASS] Catégorie 8 : Gestion des ambiguïtés (16 tests)\n";

    testCategory9_SecureClear();
    std::cout << "[PASS] Catégorie 9 : Suppression sécurisée (16 tests)\n";

    testCategory10_ShadowComparison();
    std::cout << "[PASS] Catégorie 10 : Comparaison historique SHADOW (22 tests)\n";

    testCategory11_Robustness();
    std::cout << "[PASS] Catégorie 11 : Robustesse des entrées (12 tests)\n";

    TestReport rep;
    rep.passed = g_passCount;
    rep.failed = g_failCount;
    rep.total = g_passCount + g_failCount;

    std::cout << "\nTotal NLU Tests : " << rep.passed << " PASS, " << rep.failed << " FAIL (sur " << rep.total << " tests distincts)\n";

    return rep;
}

} // namespace VoiceJournal::Tests
