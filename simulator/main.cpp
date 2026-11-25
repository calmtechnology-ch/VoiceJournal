#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "CommandRouter.h"
#include "parser/JournalParser.h"

using namespace VoiceJournal::Parser;

namespace {

void printDivider() {
  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
}

void printHeader(const std::string &title) {
  std::cout << "\n============================================================="
               "===================\n";
  std::cout << "  " << title << "\n";
  std::cout << "==============================================================="
               "=================\n";
}

void evaluatePhrase(CommandRouter &router, const std::string &phrase,
                    bool verbose = true) {
  const String textArduino(phrase.c_str());
  const uint32_t nowMs = 10000;

  const auto start = std::chrono::high_resolution_clock::now();
  CommandResult cmd = router.route(textArduino, nowMs);
  const auto end = std::chrono::high_resolution_clock::now();
  const auto durationUs =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  const ParseResult &pr = cmd.parseResult;

  std::cout << "\n[Original text]      : \"" << pr.originalText << "\"\n";
  std::cout << "[Normalized text]    : \"" << pr.normalizedText << "\"\n";
  std::cout << "[Parser mode]        : "
            << parserModeToLabel(router.getParserMode()) << "\n";

  if (verbose) {
    // Tokens
    std::cout << "[Tokens] (" << pr.tokens.size() << ")        : ";
    for (size_t i = 0; i < pr.tokens.size(); ++i) {
      std::cout << "'" << pr.tokens[i].text << "'";
      if (pr.tokens[i].semanticRole != SemanticRole::UNKNOWN) {
        std::cout << "{" << semanticRoleToString(pr.tokens[i].semanticRole)
                  << "}";
      }
      if (pr.tokens[i].isFuzzy) {
        std::cout << "(fuzzy->" << pr.tokens[i].matchedLexicon << ")";
      }
      std::cout << " ";
    }
    std::cout << "\n";

    // Recognized phrases (COR-02)
    std::cout << "[Recognized phrases] : ";
    if (pr.phrases.empty()) {
      std::cout << "NONE";
    } else {
      for (size_t i = 0; i < pr.phrases.size(); ++i) {
        if (i > 0)
          std::cout << ", ";
        std::cout << "\"" << pr.phrases[i].phrase
                  << "\" (role=" << semanticRoleToString(pr.phrases[i].role)
                  << ", tgt="
                  << targetCategoryToString(pr.phrases[i].targetCategory)
                  << ")";
      }
    }
    std::cout << "\n";

    // Semantic roles (COR-01)
    std::cout << "[Semantic roles]     : ";
    for (size_t i = 0; i < pr.tokens.size(); ++i) {
      if (i > 0)
        std::cout << ", ";
      std::cout << pr.tokens[i].text << ":"
                << semanticRoleToString(pr.tokens[i].semanticRole);
    }
    std::cout << "\n";

    // Semantic frame (COR-05, Section 16)
    const auto &fr = pr.semanticFrame;
    std::cout << "[Semantic frame]     : subject=\"" << fr.subject
              << "\", modality=\"" << fr.modality << "\", action=\""
              << fr.action << "\", object=\"" << fr.object
              << "\", target=" << targetCategoryToString(fr.targetCategory)
              << ", entity=\"" << fr.entity << "\", relativeDate=\""
              << fr.relativeDate << "\", negationScope=\"" << fr.negationScope
              << "\", globalScope=" << (fr.hasGlobalScope ? "true" : "false")
              << "\n";

    // Extracted slots
    std::cout << "[Extracted slots]    : ";
    if (pr.slots.empty()) {
      std::cout << "NONE";
    } else {
      for (size_t i = 0; i < pr.slots.size(); ++i) {
        if (i > 0)
          std::cout << ", ";
        std::cout << pr.slots[i].name << "=\"" << pr.slots[i].value << "\"";
      }
    }
    std::cout << "\n";

    // Matched rule & scores
    std::cout << "[Candidate rule]     : "
              << (pr.matchedRule.empty() ? "NONE" : pr.matchedRule) << "\n";
    std::cout << "[Scores]             : rawScore="
              << static_cast<unsigned>(pr.rawScore)
              << ", adjustedScore=" << static_cast<unsigned>(pr.adjustedScore)
              << ", fuzzyPenalty=" << static_cast<unsigned>(pr.fuzzyPenalty)
              << " (acceptanceThreshold="
              << static_cast<unsigned>(pr.acceptanceThreshold) << ")\n";
    std::cout << "[Negation scope]     : " << fr.negationScope
              << " (detected=" << (pr.negationDetected ? "true" : "false")
              << ")\n";
    std::cout << "[Ambiguity]          : "
              << (pr.ambiguityDetected ? "AMBIGUITY DETECTED" : "none") << "\n";
  }

  std::cout << "[Selected intent]    : " << intentToString(pr.intent) << "\n";
  std::cout << "[Executable]         : " << (pr.executable ? "true" : "false")
            << "\n";
  std::cout << "[Security decision]  : " << commandTypeToString(cmd.type);
  if (cmd.blockedByDivergence) {
    std::cout << " [BLOCKED BY DIVERGENCE]";
  } else if (pr.ambiguityDetected) {
    std::cout << " [BLOCKED BY AMBIGUITY]";
  }
  std::cout << "\n";

  if (!cmd.payload.isEmpty()) {
    std::cout << "[Payload]            : \"" << cmd.payload.c_str() << "\"\n";
  }
  if (cmd.entryType != VoiceJournal::EntryType::NOTE) {
    std::cout << "[Entry Type]         : " << VoiceJournal::entryTypeToString(cmd.entryType) << "\n";
  }
  if (!cmd.projectName.isEmpty()) {
    std::cout << "[Project]            : \"" << cmd.projectName.c_str() << "\"\n";
  }
  if (!cmd.dateExpr.isEmpty()) {
    std::cout << "[Date Expression]    : \"" << cmd.dateExpr.c_str() << "\"\n";
  }

  if (router.getParserMode() == ParserMode::SHADOW) {
    std::cout << "[Shadow Legacy]      : "
              << commandTypeToString(cmd.legacyType) << "\n";
    std::cout << "[Shadow Status]      : " << cmd.comparisonStatus.c_str()
              << "\n";
  }

  std::cout << "[Processing time]    : " << durationUs
            << " us (internal: " << pr.processingTimeUs << " us)\n";
  printDivider();
}

void runTestSuite(CommandRouter &router) {
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
      {"Il faut que je teste Gemini demain", IntentId::ADD_TASK, false},
      {"N'oublie pas de tester Gemini demain", IntentId::ADD_TASK, false},
      {"Fais le bilan du projet RuleBot", IntentId::ANALYZE_PROJECT, false},
      {"Aide", IntentId::HELP, false},
      {"retruve les observations", IntentId::SEARCH_NOTES, false},
      {"problme cores3", IntentId::SEARCH_NOTES, false},
      {"Efface le fichier de configuration", IntentId::UNKNOWN, false},
      {"Je ne dois pas tester Gemini", IntentId::UNKNOWN, false},
  };

  size_t passed = 0;
  for (const auto &tc : cases) {
    if (tc.confirmPending) {
      router.armClearConfirmation(1000);
    } else {
      router.cancelClearConfirmation();
    }

    const auto res = router.getParser().parse(tc.phrase, tc.confirmPending);
    const bool ok = (res.intent == tc.expectedIntent);
    if (ok) {
      ++passed;
      std::cout << "  [PASS] \"" << tc.phrase << "\" -> "
                << intentToString(res.intent) << "\n";
    } else {
      std::cout << "  [FAIL] \"" << tc.phrase
                << "\" -> Obtenu: " << intentToString(res.intent)
                << " | Attendu: " << intentToString(tc.expectedIntent) << "\n";
    }
  }

  std::cout << "\nResultats: " << passed << " / " << cases.size()
            << " tests reussis.\n";
  printDivider();
}

void evaluateDataset(CommandRouter &router, const std::string &path) {
  printHeader(std::string("EVALUATION DU DATASET : ") + path);

  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "Erreur : impossible d'ouvrir le dataset : " << path << "\n";
    return;
  }

  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  f.close();

  // Analyseur JSON léger pour extraire les cas de test du tableau "tests"
  size_t pos = 0;
  size_t total = 0;
  size_t passed = 0;
  size_t failed = 0;
  size_t ambiguousCount = 0;

  auto extractString = [&](const std::string &block,
                           const std::string &key) -> std::string {
    std::string pattern = "\"" + key + "\":";
    size_t p = block.find(pattern);
    if (p == std::string::npos)
      return "";
    p += pattern.length();
    while (p < block.length() && (block[p] == ' ' || block[p] == '\"'))
      ++p;
    size_t endP = block.find('\"', p);
    if (endP == std::string::npos)
      return "";
    return block.substr(p, endP - p);
  };

  while ((pos = content.find("{\"id\":", pos)) != std::string::npos) {
    size_t endObj = content.find("}", pos);
    if (endObj == std::string::npos)
      break;
    std::string block = content.substr(pos, endObj - pos + 1);

    std::string id = extractString(block, "id");
    std::string input = extractString(block, "input");
    std::string expectedIntent = extractString(block, "expectedIntent");

    if (!input.empty() && !expectedIntent.empty()) {
      ++total;
      const auto res = router.getParser().parse(input, false);
      std::string actualIntent = intentToString(res.intent);

      bool ok = (actualIntent == expectedIntent);
      if (res.ambiguityDetected || res.intent == IntentId::AMBIGUOUS) {
        ++ambiguousCount;
      }

      if (ok) {
        ++passed;
        std::cout << "  [PASS] [" << id << "] \"" << input << "\" -> "
                  << actualIntent << "\n";
      } else {
        ++failed;
        std::cout << "  [FAIL] [" << id << "] \"" << input
                  << "\" -> Obtenu: " << actualIntent
                  << " | Attendu: " << expectedIntent << "\n";
      }
    }

    pos = endObj + 1;
  }

  std::cout << "\n============================================================="
               "===================\n";
  std::cout << "  BILAN DU DATASET :\n";
  std::cout << "  Total cas évalues : " << total << "\n";
  std::cout << "  PASS              : " << passed << " ("
            << (total > 0 ? (passed * 100 / total) : 0) << "%)\n";
  std::cout << "  FAIL              : " << failed << "\n";
  std::cout << "  Ambiguites        : " << ambiguousCount << "\n";
  std::cout << "==============================================================="
               "=================\n";
}

} // namespace

int main(int argc, char *argv[]) {
  CommandRouter router(30000, ParserMode::INTELLIGENT);
  router.setStructuredMemoryEnabled(true);

  std::cout << "==============================================================="
               "=================\n";
  std::cout << "  VoiceJournal v0.3.0 — Simulateur NLU & Structured Memory\n";
  std::cout << "==============================================================="
               "=================\n";

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
    } else if (arg1 == "--dataset" && argc > 2) {
      evaluateDataset(router, argv[2]);
      return 0;
    } else if (arg1 == "--help") {
      std::cout << "Usage:\n";
      std::cout << "  simulator.exe                  : Mode interactif REPL\n";
      std::cout << "  simulator.exe --test           : Execute le jeu de "
                   "validation de base\n";
      std::cout << "  simulator.exe --eval <texte>   : Evalue une phrase en "
                   "mode INTELLIGENT (detaille)\n";
      std::cout << "  simulator.exe --shadow <texte> : Evalue en mode "
                   "comparaison SHADOW\n";
      std::cout << "  simulator.exe --dataset <path> : Evalue un fichier JSON "
                   "de validation\n";
      return 0;
    }
  }

  std::cout << "Commandes disponibles dans le simulateur interactif :\n";
  std::cout << "  mode legacy | mode intelligent | mode shadow\n";
  std::cout << "  memory on   | memory off\n";
  std::cout << "  arm_clear   | cancel_clear\n";
  std::cout << "  test\n";
  std::cout << "  quit / exit\n";
  std::cout << "Ou saisissez directement une phrase a analyser.\n";
  printDivider();

  std::string line;
  while (true) {
    std::cout << "VoiceJournal-Parser ("
              << parserModeToLabel(router.getParserMode()) << ")> ";
    if (!std::getline(std::cin, line)) {
      break;
    }
    if (line.empty())
      continue;

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
    } else if (line == "memory on") {
      router.setStructuredMemoryEnabled(true);
      std::cout << "Memoire structuree v0.3.0 : ACTIVEE\n";
    } else if (line == "memory off") {
      router.setStructuredMemoryEnabled(false);
      std::cout << "Memoire structuree v0.3.0 : DESACTIVEE (mode compatibilite)\n";
    } else {
      evaluatePhrase(router, line, true);
    }
  }

  std::cout << "Fin de simulation VoiceJournal.\n";
  return 0;
}
