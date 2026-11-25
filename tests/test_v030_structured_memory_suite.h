#pragma once

#include <cstddef>

namespace VoiceJournal::Tests {

struct V030Report {
    size_t passed = 0;
    size_t failed = 0;
};

V030Report runV030StructuredMemorySuite();

} // namespace VoiceJournal::Tests
