#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>

#include "parser/JournalParser.h"
#include "CommandRouter.h"

using namespace VoiceJournal::Parser;

namespace {

void printDivider() {
    std::cout << "--------------------------------------------------------------------------------\n";
}

void printHeader(const std::string& title) {
    std::cout << "\n================================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "================================================================================\n";
}

void evaluatePhrase(CommandRouter& router, const std::string& phrase, bool verbose = true) {
    const String textArduino(phrase.c_str());
    const uint32_t nowMs = 10000;

    const auto start = std::chrono::high_resolution_clock::now();
    CommandResult cmd = router.route(textArduino, nowMs);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    const ParseResult& pr = cmd.parseResult;

    std::cout << "\n[ENTREE]      : \"" << phrase << "\"\n";
    std::cout << "[NORMALISE]   : \"" << pr.normalizedText << "\"\n";
    std::cout << "[MODE]        : " << parserModeToLabel(router.getParserMode()) << "\n";

    if (verbose) {
        std::cout << "[TOKENS] (" << pr.tokens.size() << ") : ";
        for (size_t i = 0; i < pr.tokens.size(); ++i) {
            std::cout << "'" << pr.tokens[i].text << "' ";
            if (pr.tokens[i].isFuzzy) {
                std::cout << "(fuzzy->" << pr.tokens[i].matchedLexicon << ") ";
            }
        }
        std::cout << "\n";

        std::cout << "[CONCEPTS]    : ";
        if (pr.primaryConcepts.empty()) {
            std::cout << "AUCUN";
        } else {
            for (size_t i = 0; i < pr.primaryConcepts.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << conceptToString(pr.primaryConcepts[i]);
            }
        }
        std::cout << "\n";

        std::cout << "[ENTITES]     : ";
        if (pr.entities.empty()) {
            std::cout << "AUCUNE";
        } else {
            for (size_t i = 0; i < pr.entities.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << pr.entities[i].canonical << " (" << pr.entities[i].tokenCount << " tok)";
            }
        }
        std::cout << "\n";

        std::cout << "[SLOTS]       : ";
        if (pr.slots.empty()) {
            std::cout << "AUCUN";
        } else {
            for (size_t i = 0; i < pr.slots.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << pr.slots[i].name << "=\"" << pr.slots[i].value << "\"";
            }
        }
        std::cout << "\n";

        std::cout << "[REGLE]       : " << (pr.matchedRule.empty() ? "AUCUNE" : pr.matchedRule) << "\n";
        std::cout << "[SCORE]       : " << static_cast<unsigned>(pr.score) << " / 100\n";
        std::cout << "[NEGATION]    : " << (pr.negationDetected ? "OUI" : "NON") << "\n";
        std::cout << "[FUZZY UTIL]  : " << (pr.fuzzyMatchUsed ? "OUI" : "NON") << "\n";
        std::cout << "[AMBIGU]      : " << (pr.ambiguous ? "OUI" : "NON") << "\n";
    }

    std::cout << "[INTENTION]   : " << intentToString(pr.intent) << "\n";
    std::cout << "[EXECUTABLE]  : " << (pr.executable ? "OUI" : "NON (EXPERIMENTAL / OBS)") << "\n";
    std::cout << "[COMMANDE]    : " << commandTypeToString(cmd.type) << "\n";

    if (cmd.type == CommandType::ADD_NOTE && !cmd.payload.isEmpty()) {
        std::cout << "[PAYLOAD NOTE]: \"" << cmd.payload.c_str() << "\"\n";
    }

    if (router.getParserMode() == ParserMode::SHADOW) {
        std::cout << "[SHADOW LEG]  : " << commandTypeToString(cmd.legacyType) << "\n";
        std::cout << "[SHADOW COMP] : " << cmd.comparisonStatus.c_str() << "\n";
        if (cmd.blockedByDivergence) {
            std::cout << "[SECURITE]    : ACTION DESTRUCTIVE BLOQUEE EN RAISON DE LA DIVERGENCE !\n";
        }
    }

    std::cout << "[DUREE PARSE] : " << durationUs << " us (" << pr.processingTimeUs << " us mesuré interne)\n";
    printDivider();
}

void runTestSuite(CommandRouter& router) {
    printHeader("EXECUTION DU JEU DE TESTS INTELLIGENT PARSER");

    struct TestCase {
        std::string phrase;
        IntentId expectedIntent;
        bool confirmPending;
    };

    const std::vector<TestCase> cases = {
        {"Note, test rapide", IntentId::ADD_NOTE, false},
        {"NOTE : Gemini se déconnecte sur CoreS3.", IntentId::ADD_NOTE, false},
        {"Lis la dernière note", IntentId::READ_LAST_NOTE, false},
        {"Combien de notes ?", IntentId::COUNT_NOTES, false},
        {"Fais le bilan", IntentId::ANALYZE_NOTES, false},
        {"Efface toutes les notes", IntentId::REQUEST_CLEAR, false},
        {"Confirmer la suppression", IntentId::CONFIRM_CLEAR, true},
        {"Oui efface", IntentId::CONFIRM_CLEAR, true},
        {"Je ne confirme pas la suppression", IntentId::CANCEL_CLEAR, true},
        {"Ne supprime pas les notes", IntentId::CANCEL_CLEAR, false},
        {"Note, ne supprime pas les fichiers", IntentId::ADD_NOTE, false},
        {"Retrouve les bugs de VoiceKit", IntentId::SEARCH_NOTES, false},
        {"Je dois tester Gemini demain", IntentId::ADD_TASK, false},
        {"Fais le bilan du projet RuleBot", IntentId::ANALYZE_PROJECT, false},
        {"Aide", IntentId::HELP, false},
        {"retruve les observations", IntentId::SEARCH_NOTES, false},
        {"problme cores3", IntentId::SEARCH_NOTES, false},
    };

    size_t passed = 0;
    for (const auto& tc : cases) {
        if (tc.confirmPending) {
            router.armClearConfirmation(1000);
        } else {
            router.cancelClearConfirmation();
        }

        const auto res = router.getParser().parse(tc.phrase, tc.confirmPending);
        const bool ok = (res.intent == tc.expectedIntent);
        if (ok) {
            ++passed;
            std::cout << "  [PASS] \"" << tc.phrase << "\" -> " << intentToString(res.intent) << "\n";
        } else {
            std::cout << "  [FAIL] \"" << tc.phrase << "\" -> Obtenu: " << intentToString(res.intent)
                      << " | Attendu: " << intentToString(tc.expectedIntent) << "\n";
        }
    }

    std::cout << "\nResultats: " << passed << " / " << cases.size() << " tests reussis.\n";
    printDivider();
}

} // namespace

int main(int argc, char* argv[]) {
    CommandRouter router(30000, ParserMode::INTELLIGENT);

    std::cout << "================================================================================\n";
    std::cout << "  VoiceJournal v0.2.0 — Simulateur Console NLU & Intelligent Parsing\n";
    std::cout << "================================================================================\n";

    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--test") {
            runTestSuite(router);
            return 0;
        } else if (arg1 == "--eval" && argc > 2) {
            evaluatePhrase(router, argv[2], true);
            return 0;
        } else if (arg1 == "--shadow" && argc > 2) {
            router.setParserMode(ParserMode::SHADOW);
            evaluatePhrase(router, argv[2], true);
            return 0;
        } else if (arg1 == "--help") {
            std::cout << "Usage:\n";
            std::cout << "  simulator.exe                : Mode interactif REPL\n";
            std::cout << "  simulator.exe --test         : Execute le jeu de validation de base\n";
            std::cout << "  simulator.exe --eval <texte> : Evalue une phrase en mode INTELLIGENT\n";
            std::cout << "  simulator.exe --shadow <txt> : Evalue en mode comparaison SHADOW\n";
            return 0;
        }
    }

    std::cout << "Commandes disponibles dans le simulateur interactif :\n";
    std::cout << "  mode legacy | mode intelligent | mode shadow\n";
    std::cout << "  arm_clear   | cancel_clear\n";
    std::cout << "  test\n";
    std::cout << "  quit / exit\n";
    std::cout << "Ou saisissez directement une phrase a analyser.\n";
    printDivider();

    std::string line;
    while (true) {
        std::cout << "VoiceJournal-Parser (" << parserModeToLabel(router.getParserMode()) << ")> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) continue;

        if (line == "exit" || line == "quit") {
            break;
        } else if (line == "test") {
            runTestSuite(router);
        } else if (line == "mode legacy") {
            router.setParserMode(ParserMode::LEGACY);
            std::cout << "Mode bascule sur : LEGACY\n";
        } else if (line == "mode intelligent") {
            router.setParserMode(ParserMode::INTELLIGENT);
            std::cout << "Mode bascule sur : INTELLIGENT\n";
        } else if (line == "mode shadow") {
            router.setParserMode(ParserMode::SHADOW);
            std::cout << "Mode bascule sur : SHADOW\n";
        } else if (line == "arm_clear") {
            router.armClearConfirmation(10000);
            std::cout << "Confirmation de suppression armee (delai 30 s).\n";
        } else if (line == "cancel_clear") {
            router.cancelClearConfirmation();
            std::cout << "Confirmation de suppression annulee.\n";
        } else {
            evaluatePhrase(router, line, true);
        }
    }

    std::cout << "Fin de simulation VoiceJournal.\n";
    return 0;
}
