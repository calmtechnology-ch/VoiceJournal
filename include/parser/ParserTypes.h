#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "parser/ParserTables.h"

namespace VoiceJournal::Parser {

enum class ParseStatus : uint8_t {
    OK = 0,
    EMPTY_INPUT,
    UNRECOGNIZED,
    AMBIGUOUS,
    NEGATION_REJECTED,
    CONFIRMATION_INVALID,
    PAYLOAD_MISSING,
    ERROR
};

inline const char* parseStatusToString(ParseStatus status) {
    switch (status) {
        case ParseStatus::OK:                   return "OK";
        case ParseStatus::EMPTY_INPUT:          return "EMPTY_INPUT";
        case ParseStatus::UNRECOGNIZED:         return "UNRECOGNIZED";
        case ParseStatus::AMBIGUOUS:            return "AMBIGUOUS";
        case ParseStatus::NEGATION_REJECTED:    return "NEGATION_REJECTED";
        case ParseStatus::CONFIRMATION_INVALID: return "CONFIRMATION_INVALID";
        case ParseStatus::PAYLOAD_MISSING:      return "PAYLOAD_MISSING";
        case ParseStatus::ERROR:                return "ERROR";
        default:                                return "UNKNOWN";
    }
}

struct Token {
    std::string text;           // Texte normalisé ASCII
    size_t origStart = 0;       // Début dans texte UTF-8 original
    size_t origEnd = 0;         // Fin dans texte UTF-8 original
    size_t normStart = 0;       // Début dans texte normalisé
    size_t normEnd = 0;         // Fin dans texte normalisé
    uint32_t conceptMask = 0;   // Masque binaire des concepts associés
    bool isFuzzy = false;       // Vrai si correspondance approximative
    std::string matchedLexicon; // Mot du dictionnaire si fuzzy
};

struct EntityMatch {
    EntityId id = EntityId::ENT_NONE;
    EntityType type = EntityType::NONE;
    std::string canonical;
    size_t tokenIndex = 0;
    size_t tokenCount = 1;
};

struct SlotValue {
    std::string name;   // "type", "project", "date", "entity", "payload"
    std::string value;
};

struct ParseResult {
    IntentId intent = IntentId::UNKNOWN;
    ParseStatus status = ParseStatus::UNRECOGNIZED;
    uint8_t score = 0;
    bool negationDetected = false;
    bool ambiguous = false;
    bool fuzzyMatchUsed = false;
    bool executable = false;
    uint32_t processingTimeUs = 0;

    std::string originalText;
    std::string normalizedText;
    std::string observationPayload; // Observation brute nettoyée avec accents et casse originaux
    std::string matchedRule;
    std::string rejectionReason;

    std::vector<Token> tokens;
    std::vector<EntityMatch> entities;
    std::vector<SlotValue> slots;
    std::vector<ConceptId> primaryConcepts;
    std::vector<IntentId> competingIntents;

    bool hasConcept(ConceptId targetConcept) const {
        for (auto c : primaryConcepts) {
            if (c == targetConcept) return true;
        }
        return false;
    }

    const SlotValue* getSlot(const std::string& name) const {
        for (const auto& s : slots) {
            if (s.name == name) return &s;
        }
        return nullptr;
    }
};

} // namespace VoiceJournal::Parser
