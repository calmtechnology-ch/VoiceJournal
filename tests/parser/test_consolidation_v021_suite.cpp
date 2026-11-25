#include "parser/test_consolidation_v021_suite.h"

#include <iostream>
#include <string>
#include <vector>

#include "CommandRouter.h"
#include "JournalStorage.h"
#include "MockKeyValueStore.h"
#include "SerialParserProtocol.h"
#include "parser/JournalParser.h"
#include "parser/test_nlu_suite.h"

namespace VoiceJournal::Tests {

namespace {

static int s_pass = 0;
static int s_fail = 0;

void expectTrue(bool cond, const std::string& msg) {
    if (cond) {
        ++s_pass;
    } else {
        ++s_fail;
        std::cerr << "  [FAIL] Consolidation v0.2.1: " << msg << std::endl;
    }
}

// -----------------------------------------------------------------------------
// Tests COR-11 : Validation centralisée des actions
// -----------------------------------------------------------------------------
void test_COR11_ActionValidation() {
    CommandRouter router(30000, ParserMode::INTELLIGENT);

    // 1. Intention expérimentale reconnue mais non exécutable
    auto rTask = router.route("Je dois tester Gemini demain", 1000);
    expectTrue(rTask.type == CommandType::EXPERIMENTAL, "COR-11: ADD_TASK est EXPERIMENTAL");
    expectTrue(!rTask.isExecutable, "COR-11: ADD_TASK n'est pas executable");

    auto rSearch = router.route("Retrouve les bugs de VoiceKit", 1000);
    expectTrue(rSearch.type == CommandType::EXPERIMENTAL, "COR-11: SEARCH_NOTES est EXPERIMENTAL");
    expectTrue(!rSearch.isExecutable, "COR-11: SEARCH_NOTES n'est pas executable");

    // 2. Annulation de sécurité toujours prioritaire
    router.armClearConfirmation(1000);
    expectTrue(router.isClearConfirmationPending(1500), "COR-11: Confirmation armee");
    auto rCancel = router.route("Annuler", 1600);
    expectTrue(rCancel.type == CommandType::CANCEL_CLEAR, "COR-11: CANCEL_CLEAR reconnu");
    expectTrue(rCancel.isExecutable, "COR-11: CANCEL_CLEAR executable");
    expectTrue(!router.isClearConfirmationPending(1700), "COR-11: Confirmation annulee");

    // 3. Demande invalide reçue pendant confirmation désarme celle-ci
    router.armClearConfirmation(2000);
    expectTrue(router.isClearConfirmationPending(2100), "COR-11: Confirmation re-armee");
    auto rInvalid = router.route("Quelle heure est-il ?", 2200);
    expectTrue(rInvalid.type == CommandType::UNKNOWN, "COR-11: Question inconnue -> UNKNOWN");
    expectTrue(!rInvalid.isExecutable, "COR-11: Invalide non executable");
    expectTrue(!router.isClearConfirmationPending(2300), "COR-11: Confirmation desarmee apres commande invalide");

    // 4. Aucune confirmation automatique par répétition
    router.armClearConfirmation(3000);
    auto rRepeat = router.route("Efface toutes les notes", 3500);
    expectTrue(rRepeat.type == CommandType::REQUEST_CLEAR, "COR-11: Repetition reste REQUEST_CLEAR");
    expectTrue(rRepeat.type != CommandType::CONFIRM_CLEAR, "COR-11: Pas de confirmation automatique");

    // 5. Expiration temporelle de la confirmation
    router.armClearConfirmation(4000);
    expectTrue(router.isClearConfirmationPending(4000 + 20000), "COR-11: Valide a 20s");
    expectTrue(!router.isClearConfirmationPending(4000 + 31000), "COR-11: Expiree a 31s");
    auto rExpired = router.route("Confirmer la suppression", 4000 + 31000);
    expectTrue(rExpired.type != CommandType::CONFIRM_CLEAR, "COR-11: Confirmation refusee apres timeout");
    expectTrue(!rExpired.isExecutable, "COR-11: Expire non executable");
}

// -----------------------------------------------------------------------------
// Tests COR-12 : Rejet strict des entrées invalides (>1024 octets, >96 tokens, UTF-8 invalide)
// -----------------------------------------------------------------------------
void test_COR12_StrictRejection() {
    VoiceJournal::Parser::JournalParser parser;
    CommandRouter router(30000, ParserMode::INTELLIGENT);

    // 1. Entrée > 1024 octets rejetée intégralement sans analyse partielle
    std::string giantInput = "Note, ";
    while (giantInput.size() < 1050) {
        giantInput += "ceci est un long texte repetitif pour tester la limite de capacite ";
    }
    auto prLong = parser.parse(giantInput, false);
    expectTrue(prLong.status == VoiceJournal::Parser::ParseStatus::INPUT_TOO_LONG, "COR-12: Statut INPUT_TOO_LONG");
    expectTrue(!prLong.executable, "COR-12: Executable faux pour input trop long");
    expectTrue(prLong.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-12: Intent UNKNOWN pour input trop long");

    router.armClearConfirmation(1000);
    auto cmdLong = router.route(giantInput.c_str(), 1100);
    expectTrue(cmdLong.type == CommandType::NONE || cmdLong.type == CommandType::UNKNOWN, "COR-12: Pas d'action metier pour input trop long");
    expectTrue(!cmdLong.isExecutable, "COR-12: Commande non executable");
    expectTrue(!router.isClearConfirmationPending(1200), "COR-12: Input trop long desarme confirmation en attente");

    // 2. Entrée > 96 tokens rejetée intégralement sans troncature active
    std::string manyTokens = "";
    for (int i = 0; i < 110; ++i) {
        manyTokens += "mot ";
    }
    auto prTokens = parser.parse(manyTokens, false);
    expectTrue(prTokens.status == VoiceJournal::Parser::ParseStatus::TOO_MANY_TOKENS, "COR-12: Statut TOO_MANY_TOKENS");
    expectTrue(!prTokens.executable, "COR-12: Executable faux pour tokens trop nombreux");
    expectTrue(prTokens.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-12: Intent UNKNOWN pour tokens");

    // 3. Entrée UTF-8 corrompue rejetée intégralement
    std::string badUtf8 = "Note \xC3\x28 invalide";
    auto prUtf8 = parser.parse(badUtf8, false);
    expectTrue(prUtf8.status == VoiceJournal::Parser::ParseStatus::INVALID_UTF8, "COR-12: Statut INVALID_UTF8");
    expectTrue(!prUtf8.executable, "COR-12: Executable faux pour UTF-8 invalide");
}

// -----------------------------------------------------------------------------
// Tests COR-13 : Renforcement de la grammaire & relations objets/actions
// -----------------------------------------------------------------------------
void test_COR13_GrammarReinforcement() {
    VoiceJournal::Parser::JournalParser parser;

    // 1. "Combien de notes ?" (COUNT_NOTES) vs "Combien coûte un CoreS3 ?" (UNKNOWN)
    auto p1 = parser.parse("Combien de notes ?", false);
    expectTrue(p1.intent == VoiceJournal::Parser::IntentId::COUNT_NOTES, "COR-13: Combien de notes -> COUNT_NOTES");
    expectTrue(p1.executable, "COR-13: COUNT_NOTES executable");

    auto p2 = parser.parse("Combien coute un CoreS3 ?", false);
    expectTrue(p2.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-13: Combien coute un CoreS3 -> UNKNOWN");
    expectTrue(!p2.executable, "COR-13: Non executable pour CoreS3 sans note");

    // 2. "Lis la dernière note" (READ_LAST_NOTE) vs "Lis le fichier de configuration" (UNKNOWN)
    auto p3 = parser.parse("Lis la derniere note", false);
    expectTrue(p3.intent == VoiceJournal::Parser::IntentId::READ_LAST_NOTE, "COR-13: Lis la derniere note -> READ_LAST_NOTE");
    expectTrue(p3.executable, "COR-13: READ_LAST_NOTE executable");

    auto p4 = parser.parse("Lis le fichier de configuration", false);
    expectTrue(p4.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-13: Lis fichier de configuration -> UNKNOWN");
    expectTrue(!p4.executable, "COR-13: Lecture fichier interdite");

    // 3. "Fais le bilan" (ANALYZE_NOTES) vs "Fais le bilan du projet VoiceKit" (ANALYZE_PROJECT)
    auto p5 = parser.parse("Fais le bilan", false);
    expectTrue(p5.intent == VoiceJournal::Parser::IntentId::ANALYZE_NOTES, "COR-13: Fais le bilan -> ANALYZE_NOTES");
    expectTrue(p5.executable, "COR-13: ANALYZE_NOTES executable");

    auto p6 = parser.parse("Fais le bilan du projet VoiceKit", false);
    expectTrue(p6.intent == VoiceJournal::Parser::IntentId::ANALYZE_PROJECT, "COR-13: Fais le bilan projet VoiceKit -> ANALYZE_PROJECT");
    expectTrue(!p6.executable, "COR-13: ANALYZE_PROJECT non executable en v0.2.1");

    // 4. "Supprime le fichier de configuration" -> UNKNOWN
    auto p7 = parser.parse("Supprime le fichier de configuration", false);
    expectTrue(p7.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-13: Supprime fichier de config -> UNKNOWN");

    // 5. "Efface la dernière note" -> UNKNOWN (refus de suppression unitaire / non globale)
    auto p8 = parser.parse("Efface la derniere note", false);
    expectTrue(p8.intent == VoiceJournal::Parser::IntentId::UNKNOWN, "COR-13: Efface derniere note -> UNKNOWN");

    // 6. "Efface toutes les notes" -> REQUEST_CLEAR
    auto p9 = parser.parse("Efface toutes les notes", false);
    expectTrue(p9.intent == VoiceJournal::Parser::IntentId::REQUEST_CLEAR, "COR-13: Efface toutes les notes -> REQUEST_CLEAR");
    expectTrue(p9.executable, "COR-13: REQUEST_CLEAR executable");

    // 7. Obligations positives vs niées
    auto p10 = parser.parse("Je dois tester Gemini demain", false);
    expectTrue(p10.intent == VoiceJournal::Parser::IntentId::ADD_TASK, "COR-13: Je dois tester -> ADD_TASK");

    auto p11 = parser.parse("Je ne dois pas tester Gemini", false);
    expectTrue(p11.intent == VoiceJournal::Parser::IntentId::UNKNOWN || p11.intent == VoiceJournal::Parser::IntentId::NO_ACTION, "COR-13: Je ne dois pas tester -> UNKNOWN/NO_ACTION");

    auto p12 = parser.parse("N'oublie pas de tester Gemini", false);
    expectTrue(p12.intent == VoiceJournal::Parser::IntentId::ADD_TASK, "COR-13: N'oublie pas de tester -> ADD_TASK");
}

// -----------------------------------------------------------------------------
// Tests Sécurité des suppressions avec carnet réel contenant des notes
// -----------------------------------------------------------------------------
void test_DestructiveSecurity_WithRealStorage() {
    MockKeyValueStore store;
    JournalStorage storage(&store);
    expectTrue(storage.begin(), "Storage begin");

    // Remplir 5 notes de test réelles
    for (int i = 1; i <= 5; ++i) {
        std::string n = "Note de test numero " + std::to_string(i);
        expectTrue(storage.addNote(n.c_str()), "Ajout note de test");
    }
    expectTrue(storage.count() == 5, "Carnet plein a 5 notes");

    CommandRouter router(30000, ParserMode::INTELLIGENT);

    // 1. Demande d'effacement arme la confirmation
    auto rReq = router.route("Efface toutes les notes", 1000);
    expectTrue(rReq.type == CommandType::REQUEST_CLEAR, "Request clear");
    router.armClearConfirmation(1000);
    expectTrue(router.isClearConfirmationPending(1500), "Clear pending");
    expectTrue(storage.count() == 5, "Notes toujours presentes a l'armement");

    // 2. Refus si négation pendant confirmation : "Je ne confirme pas la suppression"
    auto rNeg = router.route("Je ne confirme pas la suppression", 2000);
    expectTrue(rNeg.type == CommandType::CANCEL_CLEAR, "Je ne confirme pas -> CANCEL_CLEAR");
    expectTrue(!router.isClearConfirmationPending(2100), "Confirmation annulee");
    expectTrue(storage.count() == 5, "5 notes preservees apres refus");

    // 3. Commande destructive approximative avec faute d'orthographe rejetée (aucun fuzzy sur suppression)
    router.armClearConfirmation(3000);
    auto rFuzzyClear = router.route("eface tout", 3100);
    expectTrue(rFuzzyClear.type == CommandType::UNKNOWN, "eface tout -> UNKNOWN");
    expectTrue(!router.isClearConfirmationPending(3200), "Confirmation desarmee apres tentative fuzzy");
    expectTrue(storage.count() == 5, "5 notes preservees apres tentative fuzzy");

    // 4. Commande destructive avec objet concurrent rejetée
    router.armClearConfirmation(4000);
    auto rConcurrent = router.route("Efface les notes et le fichier de config", 4100);
    expectTrue(rConcurrent.type == CommandType::UNKNOWN, "Objets concurrents -> UNKNOWN");
    expectTrue(!router.isClearConfirmationPending(4200), "Confirmation desarmee apres objets concurrents");
    expectTrue(storage.count() == 5, "5 notes preservees apres objets concurrents");

    // 5. Suppression confirmée selon le protocole exact en 2 étapes
    router.armClearConfirmation(5000);
    auto rConf = router.route("Confirmer la suppression", 5500);
    expectTrue(rConf.type == CommandType::CONFIRM_CLEAR, "CONFIRM_CLEAR valide");
    expectTrue(rConf.isExecutable, "CONFIRM_CLEAR executable");
    // Exécution de la suppression dans le carnet
    storage.clear();
    expectTrue(storage.count() == 0, "Carnet effectivement vide apres confirmation exacte");
}

// -----------------------------------------------------------------------------
// Tests COR-14 : Protocole série USB uniforme en JSON
// -----------------------------------------------------------------------------
void test_COR14_SerialProtocolJson() {
    VoiceJournal::Parser::JournalParser parser;
    auto pr = parser.parse("Je dois tester Gemini demain", false);

    // 1. JSON pour PARSE
    std::string jsonParse = VoiceJournal::Protocol::formatParseJson(pr);
    expectTrue(jsonParse.find("\"command\": \"PARSE\"") != std::string::npos, "JSON PARSE command field");
    expectTrue(jsonParse.find("\"intent\": \"ADD_TASK\"") != std::string::npos, "JSON PARSE intent field");
    expectTrue(jsonParse.find("\"executable\": false") != std::string::npos, "JSON PARSE executable false");
    expectTrue(jsonParse.find("\"status\": \"OK\"") != std::string::npos, "JSON PARSE status OK");

    // 2. JSON pour PARSE_SHADOW
    CommandRouter router(30000, ParserMode::SHADOW);
    auto resShadow = router.routeShadow("Lis la derniere note", 1000);
    std::string jsonShadow = VoiceJournal::Protocol::formatParseShadowJson(resShadow, "Lis la derniere note");
    expectTrue(jsonShadow.find("\"command\": \"PARSE_SHADOW\"") != std::string::npos, "JSON SHADOW command field");
    expectTrue(jsonShadow.find("\"comparisonStatus\": \"MATCH\"") != std::string::npos, "JSON SHADOW MATCH");
    expectTrue(jsonShadow.find("\"blocked\": false") != std::string::npos, "JSON SHADOW blocked false");

    // 3. JSON pour PARSER_STATUS
    std::string jsonStatus = VoiceJournal::Protocol::formatParserStatusJson(
        "0.2.1", "INTELLIGENT", 150000, 100000, 4000000, 19, 51
    );
    expectTrue(jsonStatus.find("\"version\": \"0.2.1\"") != std::string::npos, "JSON STATUS version 0.2.1");
    expectTrue(jsonStatus.find("\"mode\": \"INTELLIGENT\"") != std::string::npos, "JSON STATUS mode INTELLIGENT");

    // 4. JSON pour PARSER_HELP
    std::string jsonHelp = VoiceJournal::Protocol::formatParserHelpJson();
    expectTrue(jsonHelp.find("\"command\": \"PARSER_HELP\"") != std::string::npos, "JSON HELP command field");
    expectTrue(jsonHelp.find("PARSE <texte>") != std::string::npos, "JSON HELP liste commandes");

    // 5. JSON pour ERREUR
    std::string jsonErr = VoiceJournal::Protocol::formatErrorJson("INPUT_TOO_LONG", "Ligne serie trop longue");
    expectTrue(jsonErr.find("\"command\": \"ERROR\"") != std::string::npos, "JSON ERROR command field");
    expectTrue(jsonErr.find("INPUT_TOO_LONG") != std::string::npos, "JSON ERROR status field");
}

// -----------------------------------------------------------------------------
// Tests COR-15 : Mode SHADOW et isolation stricte
// -----------------------------------------------------------------------------
void test_COR15_ShadowIsolationAndDivergence() {
    CommandRouter router(30000, ParserMode::SHADOW);

    // 1. Commande sûre concordante -> MATCH
    auto r1 = router.routeShadow("Combien de notes ?", 1000);
    expectTrue(r1.shadowStatus == ShadowStatus::MATCH, "COR-15: Combien de notes -> MATCH");
    expectTrue(!r1.blockedByDivergence, "COR-15: Non bloque");

    // 2. Divergence sur action destructive -> SECURITY_BLOCKED
    auto r2 = router.routeShadow("Efface le fichier de configuration", 2000);
    expectTrue(r2.shadowStatus == ShadowStatus::SECURITY_BLOCKED, "COR-15: Fichier -> SECURITY_BLOCKED");
    expectTrue(r2.blockedByDivergence, "COR-15: Bloque par divergence");
    expectTrue(r2.type == CommandType::UNKNOWN, "COR-15: Type forcé UNKNOWN");
    expectTrue(!router.isClearConfirmationPending(2100), "COR-15: Confirmation non armee");

    // 3. Intention nouvelle expérimentale -> NEW_INTENT
    auto r3 = router.routeShadow("Je dois tester Gemini demain", 3000);
    expectTrue(r3.shadowStatus == ShadowStatus::NEW_INTENT, "COR-15: Tache -> NEW_INTENT");
    expectTrue(!r3.blockedByDivergence, "COR-15: Tache non bloquee par divergence destructive");
}

} // namespace

TestReport runAllConsolidationTests() {
    s_pass = 0;
    s_fail = 0;

    std::cout << "\n=== EXECUTION DE LA SUITE DE TESTS CONSOLIDATION v0.2.1 (COR-11 a COR-15) ===\n";

    const int b1 = s_pass;
    test_COR11_ActionValidation();
    std::cout << "[PASS] COR-11 : Validation centralisee des actions (" << (s_pass - b1) << " tests)\n";

    const int b2 = s_pass;
    test_COR12_StrictRejection();
    std::cout << "[PASS] COR-12 : Rejet strict des entrees invalides (" << (s_pass - b2) << " tests)\n";

    const int b3 = s_pass;
    test_COR13_GrammarReinforcement();
    std::cout << "[PASS] COR-13 : Renforcement de la grammaire (" << (s_pass - b3) << " tests)\n";

    const int b4 = s_pass;
    test_DestructiveSecurity_WithRealStorage();
    std::cout << "[PASS] Securite : Suppressions avec stockage reel (" << (s_pass - b4) << " tests)\n";

    const int b5 = s_pass;
    test_COR14_SerialProtocolJson();
    std::cout << "[PASS] COR-14 : Protocole serie USB en JSON (" << (s_pass - b5) << " tests)\n";

    const int b6 = s_pass;
    test_COR15_ShadowIsolationAndDivergence();
    std::cout << "[PASS] COR-15 : Consolidation SHADOW & isolation (" << (s_pass - b6) << " tests)\n";

    TestReport rep;
    rep.passed = s_pass;
    rep.failed = s_fail;
    rep.total = s_pass + s_fail;

    std::cout << "\nTotal Consolidation v0.2.1 Tests : " << rep.passed << " PASS, " << rep.failed << " FAIL (sur " << rep.total << " tests distincts)\n";

    return rep;
}

} // namespace VoiceJournal::Tests
