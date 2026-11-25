cmake_minimum_required(VERSION 3.16)
project(VoiceJournalSimulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/mocks
)

set(PARSER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/TextNormalizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/Tokenizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/FuzzyMatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/ConceptDictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/NegationDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/SlotExtractor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/IntentMatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser/JournalParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CommandRouter.cpp
)

add_executable(voicejournal_simulator
    main.cpp
    ${PARSER_SOURCES}
)
