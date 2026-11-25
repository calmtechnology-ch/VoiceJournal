#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace VoiceJournal::Parser {

struct NormalizedOutput {
    std::string normalized;
    std::vector<size_t> normToOrigStart;
    std::vector<size_t> normToOrigEnd;
};

class TextNormalizer {
public:
    static NormalizedOutput normalize(const std::string& input);
    static std::string normalizeSimple(const std::string& input);
    static std::string trim(const std::string& str);
    static std::string trimOriginalPayload(const std::string& original, size_t startOffset);
};

} // namespace VoiceJournal::Parser
