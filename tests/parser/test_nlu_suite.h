#pragma once

#include <cstddef>

namespace VoiceJournal::Tests {

struct TestReport {
    size_t total = 0;
    size_t passed = 0;
    size_t failed = 0;
};

TestReport runAllParserTests();

} // namespace VoiceJournal::Tests
