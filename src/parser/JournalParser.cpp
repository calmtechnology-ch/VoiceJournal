#include "parser/JournalParser.h"
#include "parser/TextNormalizer.h"
#include "parser/Tokenizer.h"
#include "parser/ConceptDictionary.h"
#include "parser/SlotExtractor.h"
#include "parser/NegationDetector.h"
#include "parser/IntentMatcher.h"

#include <chrono>
#include <sstream>

namespace VoiceJournal::Parser {

ParseResult JournalParser::parse(const std::string& rawText, bool confirmationPending) {
    const auto startTime = std::chrono::high_resolution_clock::now();

    // 1. Normalisation avec suivi bijectif des positions
    NormalizedOutput normOut = TextNormalizer::normalize(rawText);

    // 2. Tokenisation
    std::vector<Token> tokens = Tokenizer::tokenize(normOut);

    // 3. Dictionnaire lexical & concepts
    bool anyFuzzyUsed = false;
    ConceptDictionary::annotateTokens(tokens, anyFuzzyUsed);

    // 4. Extraction des entités
    std::vector<EntityMatch> entities = SlotExtractor::extractEntities(tokens);

    // 5. Analyse des négations
    bool startsWithAdd = false;
    if (!tokens.empty()) {
        startsWithAdd = ConceptDictionary::hasConcept(tokens[0].conceptMask, ConceptId::ADD);
    }
    NegationAnalysis negation = NegationDetector::analyze(tokens, startsWithAdd);

    // 6. Correspondance des intentions & extraction des paramètres
    ParseResult result = IntentMatcher::match(
        tokens,
        entities,
        negation,
        confirmationPending,
        anyFuzzyUsed,
        rawText,
        normOut.normalized
    );

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    result.processingTimeUs = static_cast<uint32_t>(durationUs);

    return result;
}

std::string JournalParser::formatDiagnostics(const ParseResult& result, bool maskPrivatePayload) {
    std::ostringstream ss;
    ss << "[VJ][PARSER] Input: " << (maskPrivatePayload ? "<PRIVATE_OBSERVATION>" : result.originalText) << "\n";
    ss << "[VJ][PARSER] Tokens: " << result.tokens.size() << "\n";
    ss << "[VJ][PARSER] Intent: " << intentToString(result.intent) << "\n";

    ss << "[VJ][PARSER] Concepts: ";
    if (result.primaryConcepts.empty()) {
        ss << "NONE";
    } else {
        for (size_t i = 0; i < result.primaryConcepts.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << conceptToString(result.primaryConcepts[i]);
        }
    }
    ss << "\n";

    if (!result.entities.empty()) {
        ss << "[VJ][PARSER] Entity: " << result.entities[0].canonical << "\n";
    }

    ss << "[VJ][PARSER] Score: " << static_cast<unsigned int>(result.score) << "\n";
    ss << "[VJ][PARSER] Negation: " << (result.negationDetected ? "true" : "false") << "\n";
    ss << "[VJ][PARSER] Fuzzy: " << (result.fuzzyMatchUsed ? "true" : "false") << "\n";
    ss << "[VJ][PARSER] Executable: " << (result.executable ? "true" : "false") << "\n";
    ss << "[VJ][PARSER] Duration: " << result.processingTimeUs << " us\n";

    if (!result.matchedRule.empty()) {
        ss << "[VJ][PARSER] Rule: " << result.matchedRule << "\n";
    }
    if (!result.rejectionReason.empty()) {
        ss << "[VJ][PARSER] Rejection: " << result.rejectionReason << "\n";
    }

    return ss.str();
}

} // namespace VoiceJournal::Parser
