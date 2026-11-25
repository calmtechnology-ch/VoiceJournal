#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "parser/ParserTables.h"

namespace VoiceJournal::Parser {

// Limites configurables et explicites du moteur (COR-10)
inline constexpr size_t MAX_INPUT_BYTES = 1024;
inline constexpr size_t MAX_TOKENS = 96;
inline constexpr size_t MAX_PHRASES = 32;
inline constexpr size_t MAX_ENTITIES = 12;
inline constexpr size_t MAX_CANDIDATE_INTENTS = 16;
inline constexpr uint8_t DEFAULT_ACCEPTANCE_THRESHOLD = 75;

enum class ParseStatus : uint8_t {
    OK = 0,
    EMPTY_INPUT,
    UNRECOGNIZED,
    AMBIGUOUS,
    NEGATION_REJECTED,
    CONFIRMATION_INVALID,
    PAYLOAD_MISSING,
    INPUT_TOO_LONG,
    TOO_MANY_TOKENS,
    RESOURCE_LIMIT,
    INVALID_UTF8,
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
        case ParseStatus::INPUT_TOO_LONG:       return "INPUT_TOO_LONG";
        case ParseStatus::TOO_MANY_TOKENS:      return "TOO_MANY_TOKENS";
        case ParseStatus::RESOURCE_LIMIT:       return "RESOURCE_LIMIT";
        case ParseStatus::INVALID_UTF8:         return "INVALID_UTF8";
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
    SemanticRole semanticRole = SemanticRole::UNKNOWN;
    bool isFuzzy = false;       // Vrai si correspondance approximative
    std::string matchedLexicon; // Mot du dictionnaire si fuzzy
};

struct RecognizedPhrase {
    std::string phrase;
    size_t startToken = 0;
    size_t tokenCount = 0;
    size_t origStart = 0;
    size_t origEnd = 0;
    SemanticRole role = SemanticRole::UNKNOWN;
    ConceptId conceptId = ConceptId::NONE;
    TargetCategory targetCategory = TargetCategory::UNKNOWN_OBJECT;
    bool isPositiveObligation = false;
    bool isGlobalScope = false;
};

struct SemanticFrame {
    std::string subject;
    std::string modality;       // "obligation", "question", "statement", "command"
    std::string action;         // "tester", "supprimer", "lire", "compter", "analyser", etc.
    std::string object;         // "Gemini", "notes", "fichier", etc.
    TargetCategory targetCategory = TargetCategory::UNKNOWN_OBJECT;
    std::string entity;
    std::string relativeDate;
    std::string negationScope;  // "none", "command", "payload", "obligation", "confirmation"
    bool hasGlobalScope = false;
    std::string matchedPhrase;
};

struct EntityMatch {
    EntityId id = EntityId::ENT_NONE;
    EntityType type = EntityType::NONE;
    std::string canonical;
    size_t tokenIndex = 0;
    size_t tokenCount = 1;
};

struct SlotValue {
    std::string name;   // "type", "project", "date", "entity", "payload", "action", "target"
    std::string value;
};

struct ParseResult {
    IntentId intent = IntentId::UNKNOWN;
    ParseStatus status = ParseStatus::UNRECOGNIZED;
    uint8_t rawScore = 0;
    uint8_t adjustedScore = 0;
    uint8_t fuzzyPenalty = 0;
    uint8_t score = 0; // rétrocompatibilité v0.2.0 (égal à adjustedScore)
    uint8_t acceptanceThreshold = DEFAULT_ACCEPTANCE_THRESHOLD;
    bool negationDetected = false;
    bool ambiguous = false;
    bool ambiguityDetected = false;
    bool fuzzyMatchUsed = false;
    bool executable = false;
    uint32_t processingTimeUs = 0;

    std::string originalText;
    std::string normalizedText;
    std::string observationPayload; // Observation brute nettoyée avec accents et casse originaux
    std::string matchedRule;
    std::string rejectionReason;

    SemanticFrame semanticFrame;
    std::vector<Token> tokens;
    std::vector<RecognizedPhrase> phrases;
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
