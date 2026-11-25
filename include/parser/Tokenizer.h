#pragma once

#include <vector>
#include "parser/ParserTypes.h"
#include "parser/TextNormalizer.h"

namespace VoiceJournal::Parser {

class Tokenizer {
public:
    static std::vector<Token> tokenize(const NormalizedOutput& normOut);
};

} // namespace VoiceJournal::Parser
