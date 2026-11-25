#include "parser/JournalParser.h"
#include "parser/TextNormalizer.h"
#include "parser/Tokenizer.h"
#include "parser/ConceptDictionary.h"
#include "parser/PhraseRecognizer.h"
#include "parser/SemanticRoleAnalyzer.h"
#include "parser/SlotExtractor.h"
#include "parser/NegationDetector.h"
#include "parser/IntentMatcher.h"

#include <chrono>
#include <sstream>

namespace VoiceJournal::Parser {

bool JournalParser::isValidUtf8(const std::string& str) {
    const size_t len = str.length();
    size_t i = 0;
    while (i < len) {
        const uint8_t c = static_cast<uint8_t>(str[i]);
        if (c <= 0x7F) {
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false; // Overlong 2 octets (C0, C1)
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len ||
                (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 2]) & 0xC0) != 0x80) return false;
            const uint8_t b1 = static_cast<uint8_t>(str[i + 1]);
            if (c == 0xE0 && b1 < 0xA0) return false; // Overlong 3 octets
            if (c == 0xED && b1 >= 0xA0) return false; // Rejet des surrogates UTF-16 (U+D800 - U+DFFF)
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (c > 0xF4) return false; // Hors espace Unicode (> U+10FFFF)
            if (i + 3 >= len ||
                (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 3]) & 0xC0) != 0x80) return false;
            const uint8_t b1 = static_cast<uint8_t>(str[i + 1]);
            if (c == 0xF0 && b1 < 0x90) return false; // Overlong 4 octets
            if (c == 0xF4 && b1 > 0x8F) return false; // Valeur > U+10FFFF
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

ParseResult JournalParser::parse(const std::string& rawText, bool confirmationPending) {
    const auto startTime = std::chrono::high_resolution_clock::now();

    ParseResult result;
    result.originalText = rawText;

    // 0. Vérification des limites de robustesse mémoire (COR-10)
    if (rawText.size() > MAX_INPUT_BYTES) {
        if (rawText.rfind("Note", 0) == 0 || rawText.rfind("note", 0) == 0 ||
            rawText.rfind("NOTE", 0) == 0 || rawText.rfind("Mémorise", 0) == 0 ||
            rawText.rfind("memorise", 0) == 0 || rawText.rfind("Enregistre", 0) == 0 ||
            rawText.rfind("enregistre", 0) == 0 || rawText.rfind("Retiens", 0) == 0) {
            result.intent = IntentId::ADD_NOTE;
        } else {
            result.intent = IntentId::NO_ACTION;
        }
        result.status = ParseStatus::INPUT_TOO_LONG;
        result.rawScore = 0;
        result.adjustedScore = 0;
        result.score = 0;
        result.executable = false;
        result.rejectionReason = "Entree superieure a MAX_INPUT_BYTES (1024)";
        return result;
    }

    if (!isValidUtf8(rawText)) {
        result.intent = IntentId::NO_ACTION;
        result.status = ParseStatus::INVALID_UTF8;
        result.rawScore = 0;
        result.adjustedScore = 0;
        result.score = 0;
        result.executable = false;
        result.rejectionReason = "Sequence UTF-8 invalide";
        return result;
    }

    // 1. Normalisation UTF-8 avec suivi bijectif des coordonnées d'origine
    NormalizedOutput normOut = TextNormalizer::normalize(rawText);
    result.normalizedText = normOut.normalized;

    // 2. Tokenisation bornée (COR-10)
    std::vector<Token> tokens = Tokenizer::tokenize(normOut);
    const bool tooManyTokens = (tokens.size() > MAX_TOKENS);
    if (tooManyTokens) {
        tokens.resize(MAX_TOKENS);
    }

    // 3. Analyse lexicale & Dictionnaire de concepts (avec fuzzy matching borné)
    bool anyFuzzyUsed = false;
    ConceptDictionary::annotateTokens(tokens, anyFuzzyUsed);

    // 4. Reconnaissance des locutions multi-mots (COR-02)
    std::vector<RecognizedPhrase> phrases = PhraseRecognizer::recognize(tokens, rawText);

    // 5. Extraction des entités techniques
    std::vector<EntityMatch> entities = SlotExtractor::extractEntities(tokens);

    // 6. Détection et analyse de portée des négations (COR-06)
    bool startsWithAdd = false;
    if (!tokens.empty()) {
        startsWithAdd = ConceptDictionary::hasConcept(tokens[0].conceptMask, ConceptId::ADD);
    }
    NegationAnalysis negation = NegationDetector::analyze(tokens, startsWithAdd, phrases);

    // 7. Analyse des rôles sémantiques (COR-01)
    SemanticRoleAnalyzer::annotateRoles(tokens, phrases, entities);

    // 8. Construction du cadre sémantique (SemanticFrame, COR-05)
    SemanticFrame frame = SemanticRoleAnalyzer::buildFrame(tokens, phrases, entities, negation);

    // 9. Grammaire contextuelle & Correspondance d'intention (COR-03, COR-04, COR-07)
    result = IntentMatcher::match(
        tokens,
        phrases,
        entities,
        frame,
        negation,
        confirmationPending,
        anyFuzzyUsed,
        rawText,
        normOut.normalized
    );

    if (tooManyTokens) {
        result.status = ParseStatus::TOO_MANY_TOKENS;
    }

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

    ss << "[VJ][PARSER] Frame: [act=" << (result.semanticFrame.action.empty() ? "-" : result.semanticFrame.action)
       << " | obj=" << (result.semanticFrame.object.empty() ? "-" : result.semanticFrame.object)
       << " | tgt=" << targetCategoryToString(result.semanticFrame.targetCategory)
       << " | ent=" << (result.semanticFrame.entity.empty() ? "-" : result.semanticFrame.entity)
       << " | date=" << (result.semanticFrame.relativeDate.empty() ? "-" : result.semanticFrame.relativeDate)
       << " | negScope=" << result.semanticFrame.negationScope
       << " | glob=" << (result.semanticFrame.hasGlobalScope ? "true" : "false") << "]\n";

    if (!result.phrases.empty()) {
        ss << "[VJ][PARSER] Phrase: " << result.phrases[0].phrase << "\n";
    }

    ss << "[VJ][PARSER] Score: " << static_cast<unsigned int>(result.score)
       << " (raw=" << static_cast<unsigned int>(result.rawScore)
       << ", fuzPen=" << static_cast<unsigned int>(result.fuzzyPenalty) << ")\n";
    ss << "[VJ][PARSER] Ambiguous: " << (result.ambiguityDetected ? "true" : "false") << "\n";
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
