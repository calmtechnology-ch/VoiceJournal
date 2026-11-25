#include "parser/Tokenizer.h"

namespace VoiceJournal::Parser {

std::vector<Token> Tokenizer::tokenize(const NormalizedOutput& normOut) {
    std::vector<Token> tokens;
    const std::string& norm = normOut.normalized;
    const size_t len = norm.length();
    if (len == 0) {
        return tokens;
    }

    size_t i = 0;
    while (i < len) {
        while (i < len && norm[i] == ' ') {
            ++i;
        }
        if (i >= len) break;

        const size_t wordStart = i;
        while (i < len && norm[i] != ' ') {
            ++i;
        }
        const size_t wordEnd = i;

        Token tok;
        tok.text = norm.substr(wordStart, wordEnd - wordStart);
        tok.normStart = wordStart;
        tok.normEnd = wordEnd;

        if (wordStart < normOut.normToOrigStart.size()) {
            tok.origStart = normOut.normToOrigStart[wordStart];
        } else {
            tok.origStart = wordStart;
        }

        if (wordEnd > 0 && (wordEnd - 1) < normOut.normToOrigEnd.size()) {
            tok.origEnd = normOut.normToOrigEnd[wordEnd - 1];
        } else {
            tok.origEnd = wordEnd;
        }

        tok.conceptMask = 0;
        tok.isFuzzy = false;

        tokens.push_back(tok);
    }

    return tokens;
}

} // namespace VoiceJournal::Parser
