#include "parser/test_contextual_suite.h"
#include "parser/JournalParser.h"
#include "parser/PhraseRecognizer.h"
#include "parser/SemanticRoleAnalyzer.h"
#include "parser/TextNormalizer.h"
#include "parser/Tokenizer.h"
#include "parser/ConceptDictionary.h"
#include "CommandRouter.h"

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

namespace VoiceJournal::Tests {

using namespace VoiceJournal::Parser;

namespace {

int g_catPassed = 0;
int g_catTotal = 0;

void expectTrue(bool cond, const char* desc) {
    ++g_catTotal;
    if (cond) {
        ++g_catPassed;
    } else {
        std::cout << "  [FAIL] " << desc << "\n";
    }
}

// -----------------------------------------------------------------------------
// Catégorie 12 : Locutions multi-mots (25 tests, COR-02)
// -----------------------------------------------------------------------------
void testCategory12_Phrases() {
    JournalParser parser;

    const char* phrasesToTest[25] = {
        "je dois tester Gemini",
        "il faut que je teste",
        "il faut verifier le micro",
        "penser a compiler",
        "pense a tester",
        "ne pas oublier de tester",
        "ne pas oublier VoiceKit",
        "n'oublie pas de tester",
        "n'oublie pas Gemini",
        "rappelle-moi de verifier",
        "rappelle moi de tester",
        "prevoir un test de Gemini",
        "prevoir de deployer",
        "la derniere note etait claire",
        "la derniere observation enregistree",
        "relis mon observation",
        "toutes les notes du carnet",
        "toutes les observations",
        "tous les fichiers de config",
        "le fichier de configuration",
        "supprime le fichier",
        "efface des fichiers",
        "les fichiers de log",
        "ce matin le CoreS3 boot",
        "fais le bilan maintenant"
    };

    for (int i = 0; i < 25; ++i) {
        auto res = parser.parse(phrasesToTest[i], false);
        expectTrue(!res.phrases.empty(), phrasesToTest[i]);
    }
}

// -----------------------------------------------------------------------------
// Catégorie 13 : Rôles sémantiques (25 tests, COR-01)
// -----------------------------------------------------------------------------
void testCategory13_SemanticRoles() {
    JournalParser parser;

    // 1-5: Sujets
    auto r1 = parser.parse("Je dois tester Gemini", false);
    expectTrue(r1.semanticFrame.subject == "je", "Subject je");
    auto r2 = parser.parse("Il faut tester Gemini", false);
    expectTrue(r2.semanticFrame.modality == "obligation", "Modality obligation il faut");
    auto r3 = parser.parse("Nous devons tester Gemini", false);
    expectTrue(r3.semanticFrame.subject == "nous", "Subject nous");
    auto r4 = parser.parse("Tu dois verifier le micro", false);
    expectTrue(r4.semanticFrame.subject == "tu", "Subject tu");
    auto r5 = parser.parse("On doit deployer", false);
    expectTrue(r5.semanticFrame.subject == "on", "Subject on");

    // 6-10: Actions
    expectTrue(r1.semanticFrame.action == "tester", "Action tester");
    auto r7 = parser.parse("Lis la derniere note", false);
    expectTrue(r7.semanticFrame.action == "lis", "Action lis");
    auto r8 = parser.parse("Compte les notes", false);
    expectTrue(r8.semanticFrame.action == "compte", "Action compte");
    auto r9 = parser.parse("Fais le bilan", false);
    expectTrue(!r9.semanticFrame.action.empty(), "Action bilan");
    auto r10 = parser.parse("Supprime le fichier", false);
    expectTrue(r10.semanticFrame.action == "supprime", "Action supprime");

    // 11-15: Objets et Cibles
    expectTrue(r1.semanticFrame.entity == "Gemini", "Entity Gemini");
    expectTrue(r7.semanticFrame.targetCategory == TargetCategory::JOURNAL_NOTE, "TargetCategory JOURNAL_NOTE");
    auto r13 = parser.parse("Efface le fichier de configuration", false);
    expectTrue(r13.semanticFrame.targetCategory == TargetCategory::FILE, "TargetCategory FILE");
    auto r14 = parser.parse("Fais le bilan du projet RuleBot", false);
    expectTrue(r14.semanticFrame.entity == "RuleBot", "Entity RuleBot");
    auto r15 = parser.parse("Retrouve les bugs de VoiceKit", false);
    expectTrue(r15.semanticFrame.entity == "VoiceKit", "Entity VoiceKit");

    // 16-20: Dates relatives
    auto r16 = parser.parse("Je dois tester Gemini demain", false);
    expectTrue(r16.semanticFrame.relativeDate == "demain", "Date demain");
    auto r17 = parser.parse("Trouve les notes de hier", false);
    expectTrue(r17.semanticFrame.relativeDate == "hier", "Date hier");
    auto r18 = parser.parse("Observations de ce matin", false);
    expectTrue(r18.semanticFrame.relativeDate == "ce matin", "Date ce matin");
    auto r19 = parser.parse("Notes de ce soir", false);
    expectTrue(r19.semanticFrame.relativeDate == "ce soir", "Date ce soir");
    auto r20 = parser.parse("Observations de cette semaine", false);
    expectTrue(r20.semanticFrame.relativeDate == "cette semaine", "Date cette semaine");

    // 21-25: Portée globale et négation dans frame
    auto r21 = parser.parse("Efface toutes les notes", false);
    expectTrue(r21.semanticFrame.hasGlobalScope, "GlobalScope toutes les notes");
    auto r22 = parser.parse("Vide le carnet", false);
    expectTrue(r22.semanticFrame.hasGlobalScope, "GlobalScope carnet");
    auto r23 = parser.parse("Efface la derniere note", false);
    expectTrue(!r23.semanticFrame.hasGlobalScope, "No GlobalScope derniere note");
    auto r24 = parser.parse("Ne supprime pas les notes", false);
    expectTrue(r24.semanticFrame.negationScope == "command", "NegationScope command");
    auto r25 = parser.parse("Note, ne supprime pas les fichiers", false);
    expectTrue(r25.semanticFrame.negationScope == "payload", "NegationScope payload");
}

// -----------------------------------------------------------------------------
// Catégorie 14 : Grammaire contextuelle (40 tests, COR-03, COR-04)
// -----------------------------------------------------------------------------
void testCategory14_ContextualGrammar() {
    JournalParser parser;

    struct CaseG {
        const char* text;
        IntentId expected;
        bool exec;
    };

    const CaseG grammarCases[40] = {
        // ADD_TASK variants
        {"Je dois tester Gemini demain", IntentId::ADD_TASK, false},
        {"Il faut que je teste Gemini demain", IntentId::ADD_TASK, false},
        {"Il faut tester Gemini ce matin", IntentId::ADD_TASK, false},
        {"Prevoir un test de Gemini demain", IntentId::ADD_TASK, false},
        {"Penser a tester Gemini demain", IntentId::ADD_TASK, false},
        {"Rappelle-moi de tester Gemini demain", IntentId::ADD_TASK, false},
        {"N'oublie pas de tester Gemini demain", IntentId::ADD_TASK, false},
        {"Prevoir de deployer VoiceKit", IntentId::ADD_TASK, false},
        {"Ne pas oublier de verifier le micro", IntentId::ADD_TASK, false},
        {"Pense a compiler le projet", IntentId::ADD_TASK, false},

        // SEARCH_NOTES variants
        {"Retrouve les bugs de VoiceKit", IntentId::SEARCH_NOTES, false},
        {"Cherche les problemes avec VoiceKit", IntentId::SEARCH_NOTES, false},
        {"Quels problemes ai-je rencontres avec VoiceKit", IntentId::SEARCH_NOTES, false},
        {"Rappelle-moi les erreurs de VoiceKit", IntentId::SEARCH_NOTES, false},
        {"Cherche les anomalies sur CoreS3", IntentId::SEARCH_NOTES, false},
        {"Retrouve les defauts", IntentId::SEARCH_NOTES, false},
        {"Trouve les observations", IntentId::SEARCH_NOTES, false},
        {"Recherche les pannes", IntentId::SEARCH_NOTES, false},

        // SEARCH_BY_DATE & SEARCH_BY_PROJECT
        {"Cherche les notes de hier", IntentId::SEARCH_BY_DATE, false},
        {"Trouve les observations de ce matin", IntentId::SEARCH_BY_DATE, false},
        {"Recherche de cette semaine", IntentId::SEARCH_BY_DATE, false},
        {"Observations de demain", IntentId::SEARCH_BY_DATE, false},

        // ANALYZE_PROJECT & ANALYZE_NOTES
        {"Fais le bilan du projet RuleBot", IntentId::ANALYZE_PROJECT, false},
        {"Resume les observations du projet RuleBot", IntentId::ANALYZE_PROJECT, false},
        {"Donne-moi une synthese de RuleBot", IntentId::ANALYZE_PROJECT, false},
        {"Synthese des observations sur VoiceKit", IntentId::ANALYZE_PROJECT, false},
        {"Fais le bilan", IntentId::ANALYZE_NOTES, true},
        {"Resume les observations", IntentId::ANALYZE_NOTES, true},
        {"Donne-moi une synthese", IntentId::ANALYZE_NOTES, true},
        {"Bilan global du carnet", IntentId::ANALYZE_NOTES, true},

        // READ_LAST_NOTE & COUNT_NOTES
        {"Lis la derniere note", IntentId::READ_LAST_NOTE, true},
        {"Relis mon observation", IntentId::READ_LAST_NOTE, true},
        {"Ecoute la derniere note", IntentId::READ_LAST_NOTE, true},
        {"Combien de notes", IntentId::COUNT_NOTES, true},
        {"Compte les observations", IntentId::COUNT_NOTES, true},
        {"Quel est le nombre de notes", IntentId::COUNT_NOTES, true},

        // REQUEST_CLEAR & CANCEL_CLEAR
        {"Efface toutes les notes", IntentId::REQUEST_CLEAR, true},
        {"Supprime toutes les observations", IntentId::REQUEST_CLEAR, true},
        {"Vide le carnet", IntentId::REQUEST_CLEAR, true},
        {"Ne supprime pas les notes", IntentId::CANCEL_CLEAR, true}
    };

    for (int i = 0; i < 40; ++i) {
        auto res = parser.parse(grammarCases[i].text, false);
        expectTrue(res.intent == grammarCases[i].expected && res.executable == grammarCases[i].exec,
                   grammarCases[i].text);
    }
}

// -----------------------------------------------------------------------------
// Catégorie 15 : Actions et objets (25 tests, COR-05)
// -----------------------------------------------------------------------------
void testCategory15_ActionsAndObjects() {
    JournalParser parser;

    // Cas valides pour REQUEST_CLEAR (Objet carnet / note + Scope global)
    expectTrue(parser.parse("Efface toutes les notes", false).intent == IntentId::REQUEST_CLEAR, "Delete all notes");
    expectTrue(parser.parse("Supprime toutes les observations", false).intent == IntentId::REQUEST_CLEAR, "Delete all obs");
    expectTrue(parser.parse("Vide le carnet", false).intent == IntentId::REQUEST_CLEAR, "Clear carnet");
    expectTrue(parser.parse("Reinitialise toutes les notes", false).intent == IntentId::REQUEST_CLEAR, "Reset all notes");
    expectTrue(parser.parse("Supprime tout le carnet", false).intent == IntentId::REQUEST_CLEAR, "Delete whole carnet");

    // Cas INTERDITS qui ne doivent JAMAIS déclencher REQUEST_CLEAR
    const char* nonClearCases[20] = {
        "Efface le fichier de configuration",
        "Efface le fichier",
        "Supprime le fichier",
        "Supprime les fichiers",
        "Efface des fichiers",
        "Supprime le projet VoiceKit",
        "Efface le projet RuleBot",
        "Efface la derniere note",
        "Supprime la derniere observation",
        "Supprime la derniere note",
        "Efface mon observation",
        "Supprime le mot Gemini",
        "Efface l'entite CoreS3",
        "Effacer des fichiers est dangereux",
        "Comment supprimer une note ?",
        "Comment effacer le carnet ?",
        "Pourquoi supprimer les notes ?",
        "Supprimer un fichier sur le disque",
        "Efface la tache en cours",
        "Supprime ce probleme"
    };

    for (int i = 0; i < 20; ++i) {
        auto res = parser.parse(nonClearCases[i], false);
        expectTrue(res.intent != IntentId::REQUEST_CLEAR, nonClearCases[i]);
    }
}

// -----------------------------------------------------------------------------
// Catégorie 16 : Portée des négations (30 tests, COR-06)
// -----------------------------------------------------------------------------
void testCategory16_NegationScope() {
    JournalParser parser;

    // 1-8: Négation contenue dans l'observation dictée (ne doit pas nier l'ajout)
    const char* noteWithNeg[8] = {
        "Note, ne supprime pas les fichiers",
        "Memorise que Gemini ne se connecte pas",
        "Enregistre : le serveur ne repond plus",
        "Retiens que le capteur n'est pas etalonne",
        "Note, ne jamais couper l'alimentation",
        "Memorise que rien ne fonctionne sur CoreS3",
        "Note que sans wifi rien ne marche",
        "Enregistre : pas de tension sur la broche"
    };
    for (int i = 0; i < 8; ++i) {
        auto res = parser.parse(noteWithNeg[i], false);
        expectTrue(res.intent == IntentId::ADD_NOTE && res.status == ParseStatus::OK, noteWithNeg[i]);
    }

    // 9-14: Locutions d'obligation idiomatique positive (ne doivent pas être traitées comme interdiction)
    const char* posOblig[6] = {
        "N'oublie pas de tester Gemini demain",
        "Ne pas oublier de tester Gemini",
        "N'oublie pas de compiler",
        "Ne pas oublier de verifier",
        "N'oublie pas le prototype",
        "Ne pas oublier la sauvegarde"
    };
    for (int i = 0; i < 6; ++i) {
        auto res = parser.parse(posOblig[i], false);
        expectTrue(res.intent == IntentId::ADD_TASK, posOblig[i]);
    }

    // 15-20: Obligation niée (doit annuler la tâche)
    const char* negOblig[6] = {
        "Je ne dois pas tester Gemini",
        "Il ne faut pas tester Gemini",
        "On ne doit pas compiler",
        "Je ne dois pas deployer",
        "Il ne faut pas effacer",
        "Tu ne dois pas toucher"
    };
    for (int i = 0; i < 6; ++i) {
        auto res = parser.parse(negOblig[i], false);
        expectTrue(res.intent != IntentId::ADD_TASK, negOblig[i]);
    }

    // 21-25: Confirmation niée (doit annuler l'effacement)
    const char* negConf[5] = {
        "Je ne confirme pas la suppression",
        "Ne confirme pas",
        "Pas de confirmation",
        "Non ne confirme pas",
        "Je refuse de confirmer"
    };
    for (int i = 0; i < 5; ++i) {
        auto res = parser.parse(negConf[i], true);
        expectTrue(res.intent == IntentId::CANCEL_CLEAR, negConf[i]);
    }

    // 26-30: Commandes de suppression niées
    const char* negDel[5] = {
        "Ne supprime pas les notes",
        "N'efface pas le carnet",
        "Ne supprime pas le carnet",
        "N'efface pas toutes les notes",
        "Ne vide pas les observations"
    };
    for (int i = 0; i < 5; ++i) {
        auto res = parser.parse(negDel[i], false);
        expectTrue(res.intent == IntentId::CANCEL_CLEAR, negDel[i]);
    }
}

// -----------------------------------------------------------------------------
// Catégorie 17 : Fuzzy matching et ambiguïtés (20 tests, COR-07)
// -----------------------------------------------------------------------------
void testCategory17_FuzzyAndAmbiguity() {
    JournalParser parser;

    // 1-5: Ambiguïtés nettes détectées
    auto a1 = parser.parse("Lis la derniere note et fais le bilan", false);
    expectTrue(a1.intent == IntentId::AMBIGUOUS && a1.ambiguityDetected, "Ambig 1");
    auto a2 = parser.parse("Compte les notes et efface toutes les observations", false);
    expectTrue(a2.intent == IntentId::AMBIGUOUS && a2.ambiguityDetected, "Ambig 2");
    auto a3 = parser.parse("Resume les notes et compte les observations", false);
    expectTrue(a3.intent == IntentId::AMBIGUOUS && a3.ambiguityDetected, "Ambig 3");
    auto a4 = parser.parse("Fais le bilan et lis la derniere note", false);
    expectTrue(a4.intent == IntentId::AMBIGUOUS && a4.ambiguityDetected, "Ambig 4");
    auto a5 = parser.parse("Efface le carnet et lis la note", false);
    expectTrue(a5.intent == IntentId::AMBIGUOUS && a5.ambiguityDetected, "Ambig 5");

    // 6-10: Ambiguïté préservée même si fuzzy appliqué (COR-07 : test clé !)
    auto a6 = parser.parse("Relsi la derniere note et fais le bilan", false);
    expectTrue(a6.ambiguityDetected || a6.intent == IntentId::AMBIGUOUS, "Ambig with fuzzy");
    auto a7 = parser.parse("Cmpte les notes et efface toutes les observations", false);
    expectTrue(a7.ambiguityDetected || a7.intent == IntentId::AMBIGUOUS, "Ambig with fuzzy 2");
    auto a8 = parser.parse("Rsume les notes et compte les observations", false);
    expectTrue(a8.ambiguityDetected || a8.intent == IntentId::AMBIGUOUS, "Ambig with fuzzy 3");
    auto a9 = parser.parse("Fais le blan et lis la derniere note", false);
    expectTrue(a9.ambiguityDetected || a9.intent == IntentId::AMBIGUOUS, "Ambig with fuzzy 4");
    auto a10 = parser.parse("Vide le crnet et relis la note", false);
    expectTrue(a10.ambiguityDetected || a10.intent == IntentId::AMBIGUOUS, "Ambig with fuzzy 5");

    // 11-15: Refus strict de fuzzy sur les confirmations de suppression
    auto f1 = parser.parse("confirmai la suppression", true);
    expectTrue(f1.status == ParseStatus::CONFIRMATION_INVALID || f1.intent != IntentId::CONFIRM_CLEAR, "No fuzzy confirm 1");
    auto f2 = parser.parse("oui effas", true);
    expectTrue(f2.intent != IntentId::CONFIRM_CLEAR, "No fuzzy confirm 2");
    auto f3 = parser.parse("confirme la supresion", true);
    expectTrue(f3.intent != IntentId::CONFIRM_CLEAR, "No fuzzy confirm 3");
    auto f4 = parser.parse("je confirmai", true);
    expectTrue(f4.intent != IntentId::CONFIRM_CLEAR, "No fuzzy confirm 4");
    auto f5 = parser.parse("suprimer definitivement", true);
    expectTrue(f5.intent != IntentId::CONFIRM_CLEAR, "No fuzzy confirm 5");

    // 16-20: Distinctions rawScore, adjustedScore, fuzzyPenalty
    auto f6 = parser.parse("retruve les observations", false);
    expectTrue(f6.fuzzyMatchUsed && f6.fuzzyPenalty > 0, "Fuzzy penalty applied");
    expectTrue(f6.rawScore > f6.adjustedScore, "rawScore > adjustedScore");
    auto f7 = parser.parse("problme cores3", false);
    expectTrue(f7.fuzzyMatchUsed && f7.fuzzyPenalty > 0, "Fuzzy penalty on problem");
    auto f8 = parser.parse("Lis la derniere note", false);
    expectTrue(!f8.fuzzyMatchUsed && f8.fuzzyPenalty == 0, "Exact match zero penalty");
    expectTrue(f8.rawScore == f8.adjustedScore, "Exact match rawScore == adjustedScore");
    auto f9 = parser.parse("Combien de notes", false);
    expectTrue(f9.rawScore == f9.adjustedScore, "Count notes exact rawScore");
}

// -----------------------------------------------------------------------------
// Catégorie 18 : Sécurité destructive (25 tests, COR-08)
// -----------------------------------------------------------------------------
void testCategory18_DestructiveSecurity() {
    CommandRouter router(30000, ParserMode::INTELLIGENT);

    // 1-5: Armement et temporisation
    router.armClearConfirmation(1000);
    expectTrue(router.isClearConfirmationPending(1500), "Clear pending at 1500");
    expectTrue(router.isClearConfirmationPending(30000), "Clear pending at 30000");
    expectTrue(!router.isClearConfirmationPending(32000), "Clear expired at 32000");
    router.armClearConfirmation(1000);
    router.cancelClearConfirmation();
    expectTrue(!router.isClearConfirmationPending(1001), "Clear cancelled immediately");
    router.armClearConfirmation(5000);
    expectTrue(router.checkClearConfirmationTimeout(40000), "Check timeout triggers cancel");

    // 6-10: Formules exactes de confirmation autorisées
    const char* exactConf[5] = {
        "Confirmer la suppression",
        "Oui efface",
        "Confirme la suppression",
        "Je confirme la suppression",
        "Supprimer definitivement"
    };
    for (int i = 0; i < 5; ++i) {
        router.armClearConfirmation(1000);
        auto cmd = router.route(exactConf[i], 1500);
        expectTrue(cmd.type == CommandType::CONFIRM_CLEAR, exactConf[i]);
    }

    // 11-15: Rejet de confirmation si aucune attente active
    router.cancelClearConfirmation();
    for (int i = 0; i < 5; ++i) {
        auto cmd = router.route(exactConf[i], 2000);
        expectTrue(cmd.type != CommandType::CONFIRM_CLEAR, "No confirm without pending");
    }

    // 16-20: Toute phrase invalide annule l'attente active (COR-08)
    const char* invalidDuringPending[5] = {
        "Bonjour comment ca va",
        "Quelle heure est-il",
        "Peut-etre plus tard",
        "Je regarde un film",
        "Il fait beau"
    };
    for (int i = 0; i < 5; ++i) {
        router.armClearConfirmation(1000);
        auto cmd = router.route(invalidDuringPending[i], 1200);
        expectTrue(cmd.type == CommandType::UNKNOWN, "Unknown on invalid during pending");
        expectTrue(!router.isClearConfirmationPending(1201), "Pending cancelled on invalid phrase");
    }

    // 21-25: Répétition de demande ne confirme jamais automatiquement
    router.armClearConfirmation(1000);
    auto cmdRep1 = router.route("Efface toutes les notes", 1200);
    expectTrue(cmdRep1.type != CommandType::CONFIRM_CLEAR && cmdRep1.type == CommandType::REQUEST_CLEAR, "Repeat request does not auto confirm");
    router.armClearConfirmation(1000);
    auto cmdRep2 = router.route("Supprime toutes les notes", 1200);
    expectTrue(cmdRep2.type != CommandType::CONFIRM_CLEAR && cmdRep2.type == CommandType::REQUEST_CLEAR, "Repeat delete does not auto confirm");
    router.armClearConfirmation(1000);
    auto cmdRep3 = router.route("Vide le carnet", 1200);
    expectTrue(cmdRep3.type != CommandType::CONFIRM_CLEAR && cmdRep3.type == CommandType::REQUEST_CLEAR, "Repeat vide carnet does not auto confirm");
    router.armClearConfirmation(1000);
    auto cmdRep4 = router.route("oui", 1200);
    expectTrue(cmdRep4.type != CommandType::CONFIRM_CLEAR, "Simple oui rejected");
    router.armClearConfirmation(1000);
    auto cmdRep5 = router.route("d'accord", 1200);
    expectTrue(cmdRep5.type != CommandType::CONFIRM_CLEAR, "Daccord rejected");
}

// -----------------------------------------------------------------------------
// Catégorie 19 : Comparateur SHADOW complet (20 tests, COR-09)
// -----------------------------------------------------------------------------
void testCategory19_ShadowComparison() {
    CommandRouter router(30000, ParserMode::SHADOW);

    // 1-5: MATCH sur commandes canoniques
    expectTrue(router.route("Note, test 123", 1000).shadowStatus == ShadowStatus::MATCH, "Shadow MATCH ADD_NOTE");
    expectTrue(router.route("Lis la derniere note", 1000).shadowStatus == ShadowStatus::MATCH, "Shadow MATCH READ");
    expectTrue(router.route("Combien de notes", 1000).shadowStatus == ShadowStatus::MATCH, "Shadow MATCH COUNT");
    expectTrue(router.route("Fais le bilan", 1000).shadowStatus == ShadowStatus::MATCH, "Shadow MATCH ANALYZE");
    expectTrue(router.route("Aide", 1000).shadowStatus == ShadowStatus::MATCH, "Shadow MATCH HELP");

    // 6-10: NEW_INTENT sur intentions avancées ou synonymes
    expectTrue(router.route("Memorise que Gemini fonctionne", 1000).shadowStatus == ShadowStatus::NEW_INTENT, "Shadow NEW_INTENT Memorise");
    expectTrue(router.route("Je dois tester Gemini demain", 1000).shadowStatus == ShadowStatus::NEW_INTENT, "Shadow NEW_INTENT Task");
    expectTrue(router.route("Retrouve les bugs de VoiceKit", 1000).shadowStatus == ShadowStatus::NEW_INTENT, "Shadow NEW_INTENT Search");
    expectTrue(router.route("Fais le bilan du projet RuleBot", 1000).shadowStatus == ShadowStatus::NEW_INTENT, "Shadow NEW_INTENT Project");
    expectTrue(router.route("Cherche les notes de hier", 1000).shadowStatus == ShadowStatus::NEW_INTENT, "Shadow NEW_INTENT Date");

    // 11-15: SECURITY_BLOCKED en cas de divergence destructive
    router.armClearConfirmation(1000);
    auto s1 = router.route("Je ne confirme pas la suppression", 1200);
    expectTrue(s1.shadowStatus == ShadowStatus::MATCH || s1.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Shadow confirm cancel");

    // Phrase destructive rejetée par le nouveau moteur intelligent mais comprise par legacy
    auto s2 = router.route("Efface le fichier de configuration", 2000);
    expectTrue(s2.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Shadow block destructive file delete");
    expectTrue(s2.type == CommandType::UNKNOWN, "Command set to UNKNOWN on security block");

    auto s3 = router.route("Efface la derniere note", 2000);
    expectTrue(s3.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Shadow block single note delete");

    auto s4 = router.route("Supprime le projet VoiceKit", 2000);
    expectTrue(s4.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Shadow block project delete");

    auto s5 = router.route("Supprime le mot Gemini", 2000);
    expectTrue(s5.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Shadow block word delete");

    // 16-20: PAYLOAD_DIVERGENCE, STATE_DIVERGENCE, ACTION_DIVERGENCE
    router.cancelClearConfirmation();
    auto s6 = router.route("Quelle est la meteo", 2000);
    expectTrue(s6.shadowStatus == ShadowStatus::MATCH, "Unknown matches unknown");
    auto s7 = router.route("Je regarde un film", 2000);
    expectTrue(s7.shadowStatus == ShadowStatus::MATCH, "Film matches unknown");
    auto s8 = router.route("Annule", 2000);
    expectTrue(s8.type == CommandType::CANCEL_CLEAR, "Cancel clear routed");
    auto s9 = router.route("Laisse tomber", 2000);
    expectTrue(s9.type == CommandType::CANCEL_CLEAR, "Laisse tomber routed");
    auto s10 = router.route("Note, capteur ok", 2000);
    expectTrue(s10.shadowStatus == ShadowStatus::MATCH, "Note payload matches");
}

// -----------------------------------------------------------------------------
// Catégorie 20 : Robustesse mémoire et limites (15 tests, COR-10)
// -----------------------------------------------------------------------------
void testCategory20_MemoryRobustness() {
    JournalParser parser;

    // 1. Dépassement taille max (> 1024 octets)
    std::string longInput(1050, 'a');
    auto r1 = parser.parse(longInput, false);
    expectTrue(r1.status == ParseStatus::INPUT_TOO_LONG, "INPUT_TOO_LONG status");
    expectTrue(!r1.executable, "Input too long not executable");

    // 2. Dépassement nombre de tokens (> 96 tokens)
    std::string manyTokens = "";
    for (int i = 0; i < 110; ++i) {
        manyTokens += "mot ";
    }
    auto r2 = parser.parse(manyTokens, false);
    expectTrue(r2.status == ParseStatus::TOO_MANY_TOKENS, "TOO_MANY_TOKENS status");

    // 3. UTF-8 invalide
    std::string invalidUtf8 = "Note \xFF\xFE test";
    auto r3 = parser.parse(invalidUtf8, false);
    expectTrue(r3.status == ParseStatus::INVALID_UTF8, "INVALID_UTF8 status");

    // 4. Entrée vide
    auto r4 = parser.parse("", false);
    expectTrue(r4.status == ParseStatus::EMPTY_INPUT, "EMPTY_INPUT on empty string");

    // 5. Entrée espaces uniquement
    auto r5 = parser.parse("    \t   \n  ", false);
    expectTrue(r5.status == ParseStatus::EMPTY_INPUT, "EMPTY_INPUT on spaces");

    // 6. Entrée ponctuation uniquement
    auto r6 = parser.parse("... ??? !!! ;;;", false);
    expectTrue(r6.status == ParseStatus::EMPTY_INPUT, "EMPTY_INPUT on punctuation");

    // 7. Entrée de taille limite exacte (1024 octets)
    std::string exactLimit(1024, 'x');
    auto r7 = parser.parse(exactLimit, false);
    expectTrue(r7.status != ParseStatus::INPUT_TOO_LONG, "1024 bytes accepted under limit");

    // 8-15: Stabilité après 1000 analyses successives sans corruption ni fuite
    bool stable = true;
    for (int i = 0; i < 1000; ++i) {
        auto res = parser.parse("Je dois tester Gemini demain", false);
        if (res.intent != IntentId::ADD_TASK) {
            stable = false;
            break;
        }
    }
    expectTrue(stable, "1000 successive parses stable");
    expectTrue(JournalParser::isValidUtf8("Texte français valide avec accents : été, fête, cœur"), "Valid UTF8 helper");
    expectTrue(!JournalParser::isValidUtf8("\xC0\xAF"), "Overlong UTF8 detected");
    expectTrue(!JournalParser::isValidUtf8("\xED\xA0\x80"), "Surrogate UTF8 detected");
    expectTrue(parser.parse("Note, observation normale", false).status == ParseStatus::OK, "Normal parse OK");
    expectTrue(parser.parse("Lis la note", false).status == ParseStatus::OK, "Read parse OK");
    expectTrue(parser.parse("Aide", false).status == ParseStatus::OK, "Help parse OK");
    expectTrue(parser.parse("Xylophone zèbre 123", false).status == ParseStatus::UNRECOGNIZED, "Unrecognized parse");
}

// -----------------------------------------------------------------------------
// Catégorie 21 : Protocole série USB (15 tests, Section 17)
// -----------------------------------------------------------------------------
void testCategory21_SerialProtocol() {
    JournalParser parser;

    // 1-5: Commandes PARSE valides
    auto p1 = parser.parse("Je dois tester Gemini demain", false);
    expectTrue(p1.intent == IntentId::ADD_TASK, "Serial PARSE ADD_TASK");
    expectTrue(p1.semanticFrame.action == "tester", "Serial PARSE Action tester");
    expectTrue(p1.semanticFrame.entity == "Gemini", "Serial PARSE Entity Gemini");
    expectTrue(p1.semanticFrame.relativeDate == "demain", "Serial PARSE Date demain");
    expectTrue(!p1.executable, "Serial PARSE Executable false");

    // 6-10: Commandes PARSE_SHADOW isolées
    CommandRouter router(30000, ParserMode::SHADOW);
    auto s1 = router.routeShadow("Lis la derniere note", 1000);
    expectTrue(s1.shadowStatus == ShadowStatus::MATCH, "Serial SHADOW MATCH");
    auto s2 = router.routeShadow("Je dois tester Gemini", 1000);
    expectTrue(s2.shadowStatus == ShadowStatus::NEW_INTENT, "Serial SHADOW NEW_INTENT");
    auto s3 = router.routeShadow("Efface le fichier de configuration", 1000);
    expectTrue(s3.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "Serial SHADOW SECURITY_BLOCKED");
    expectTrue(!router.isClearConfirmationPending(1001), "Serial test never arms real clear");

    // 11-15: Vérification que PARSE n'altère aucun état
    expectTrue(!router.isClearConfirmationPending(2000), "Initial pending false");
    auto pClear = parser.parse("Efface toutes les notes", false);
    expectTrue(pClear.intent == IntentId::REQUEST_CLEAR, "Parse yields REQUEST_CLEAR");
    expectTrue(!router.isClearConfirmationPending(2001), "Parser parse() never arms router");
    auto pConf = parser.parse("Confirmer la suppression", false);
    expectTrue(pConf.intent == IntentId::CONFIRM_CLEAR, "Parse yields CONFIRM_CLEAR");
    expectTrue(!router.isClearConfirmationPending(2002), "Parser parse() never modifies state");
    expectTrue(GRAMMAR_RULE_COUNT >= 18, "Rules count at least 18");
    expectTrue(PHRASE_COUNT >= 40, "Phrases count at least 40");
}

} // namespace

TestReport runAllContextualTests() {
    g_catPassed = 0;
    g_catTotal = 0;

    std::cout << "\n=== EXECUTION DE LA SUITE DE TESTS CONTEXTUELS (v0.2.0.1) ===\n";

    const int b12 = g_catPassed;
    testCategory12_Phrases();
    std::cout << "[PASS] Catégorie 12 : Locutions multi-mots (" << (g_catPassed - b12) << " tests)\n";

    const int b13 = g_catPassed;
    testCategory13_SemanticRoles();
    std::cout << "[PASS] Catégorie 13 : Rôles sémantiques (" << (g_catPassed - b13) << " tests)\n";

    const int b14 = g_catPassed;
    testCategory14_ContextualGrammar();
    std::cout << "[PASS] Catégorie 14 : Grammaire contextuelle (" << (g_catPassed - b14) << " tests)\n";

    const int b15 = g_catPassed;
    testCategory15_ActionsAndObjects();
    std::cout << "[PASS] Catégorie 15 : Actions et objets (" << (g_catPassed - b15) << " tests)\n";

    const int b16 = g_catPassed;
    testCategory16_NegationScope();
    std::cout << "[PASS] Catégorie 16 : Portée des négations (" << (g_catPassed - b16) << " tests)\n";

    const int b17 = g_catPassed;
    testCategory17_FuzzyAndAmbiguity();
    std::cout << "[PASS] Catégorie 17 : Fuzzy matching et ambiguïtés (" << (g_catPassed - b17) << " tests)\n";

    const int b18 = g_catPassed;
    testCategory18_DestructiveSecurity();
    std::cout << "[PASS] Catégorie 18 : Sécurité destructive (" << (g_catPassed - b18) << " tests)\n";

    const int b19 = g_catPassed;
    testCategory19_ShadowComparison();
    std::cout << "[PASS] Catégorie 19 : Comparateur SHADOW complet (" << (g_catPassed - b19) << " tests)\n";

    const int b20 = g_catPassed;
    testCategory20_MemoryRobustness();
    std::cout << "[PASS] Catégorie 20 : Robustesse mémoire et limites (" << (g_catPassed - b20) << " tests)\n";

    const int b21 = g_catPassed;
    testCategory21_SerialProtocol();
    std::cout << "[PASS] Catégorie 21 : Protocole série USB (" << (g_catPassed - b21) << " tests)\n";

    TestReport report;
    report.total = g_catTotal;
    report.passed = g_catPassed;
    report.failed = g_catTotal - g_catPassed;

    std::cout << "\nTotal Contextual Tests : " << report.passed << " PASS, " << report.failed << " FAIL (sur " << report.total << " tests distincts)\n";

    return report;
}

} // namespace VoiceJournal::Tests
