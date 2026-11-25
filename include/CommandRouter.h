#pragma once

#include <Arduino.h>
#include <cstdint>
#include "AppConfig.h"
#include "parser/JournalParser.h"

/**
 * @enum CommandType
 * @brief Commandes vocales déterministes reconnues par VoiceJournal.
 */
enum class CommandType : uint8_t {
    NONE = 0,          ///< Transcription vide ou observation vide après "note"
    ADD_NOTE,          ///< Enregistrer une nouvelle observation (préfixe "note")
    READ_LAST_NOTE,    ///< Relire la dernière observation enregistrée
    COUNT_NOTES,       ///< Prononcer le nombre d'observations
    ANALYZE_NOTES,     ///< Demander une synthèse IA via Gemini ou OpenAI
    REQUEST_CLEAR,     ///< Demander l'effacement de toutes les notes (exige confirmation)
    CONFIRM_CLEAR,     ///< Confirmation explicite de suppression
    CANCEL_CLEAR,      ///< Annulation explicite de suppression
    HELP,              ///< Aide vocale et liste des commandes
    EXPERIMENTAL,      ///< Intention expérimentale comprise mais non exécutable
    UNKNOWN            ///< Commande non reconnue
};

inline const char* commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::NONE:           return "NONE";
        case CommandType::ADD_NOTE:       return "ADD_NOTE";
        case CommandType::READ_LAST_NOTE: return "READ_LAST_NOTE";
        case CommandType::COUNT_NOTES:    return "COUNT_NOTES";
        case CommandType::ANALYZE_NOTES:  return "ANALYZE_NOTES";
        case CommandType::REQUEST_CLEAR:  return "REQUEST_CLEAR";
        case CommandType::CONFIRM_CLEAR:  return "CONFIRM_CLEAR";
        case CommandType::CANCEL_CLEAR:   return "CANCEL_CLEAR";
        case CommandType::HELP:           return "HELP";
        case CommandType::EXPERIMENTAL:   return "EXPERIMENTAL";
        case CommandType::UNKNOWN:        return "UNKNOWN";
        default:                          return "UNKNOWN";
    }
}

/**
 * @enum ShadowStatus
 * @brief Statuts complets de comparaison en mode miroir SHADOW (COR-09).
 */
enum class ShadowStatus : uint8_t {
    MATCH = 0,
    NEW_INTENT,
    PAYLOAD_DIVERGENCE,
    PARAMETER_DIVERGENCE,
    ACTION_DIVERGENCE,
    STATE_DIVERGENCE,
    SECURITY_BLOCKED
};

inline const char* shadowStatusToString(ShadowStatus status) {
    switch (status) {
        case ShadowStatus::MATCH:                return "MATCH";
        case ShadowStatus::NEW_INTENT:           return "NEW_INTENT";
        case ShadowStatus::PAYLOAD_DIVERGENCE:   return "PAYLOAD_DIVERGENCE";
        case ShadowStatus::PARAMETER_DIVERGENCE: return "PARAMETER_DIVERGENCE";
        case ShadowStatus::ACTION_DIVERGENCE:    return "ACTION_DIVERGENCE";
        case ShadowStatus::STATE_DIVERGENCE:     return "STATE_DIVERGENCE";
        case ShadowStatus::SECURITY_BLOCKED:     return "SECURITY_BLOCKED";
        default:                                 return "UNKNOWN";
    }
}

struct CommandResult {
    CommandType type = CommandType::NONE;
    String payload;          ///< Contenu brut nettoyé (pour ADD_NOTE, accents préservés)
    String normalizedInput;  ///< Forme normalisée ASCII minuscule utilisée pour le routage
    VoiceJournal::Parser::ParseResult parseResult;  ///< Résultat détaillé du JournalParser v0.2.0.1
    CommandType legacyType = CommandType::NONE;     ///< Type issu du moteur historique
    ShadowStatus shadowStatus = ShadowStatus::MATCH;///< Statut de comparaison SHADOW enrichi (COR-09)
    String comparisonStatus; ///< Représentation textuelle de ShadowStatus ("MATCH", "SECURITY_BLOCKED", etc.)
    bool blockedByDivergence = false;
    bool isExecutable = false;
};

/**
 * @class CommandRouter
 * @brief Routeur de commandes vocales supportant les modes LEGACY, INTELLIGENT et SHADOW (COR-08, COR-09).
 */
class CommandRouter {
public:
    explicit CommandRouter(
        uint32_t clearConfirmTimeoutMs = AppConfig::CLEAR_CONFIRM_TIMEOUT_MS,
        ParserMode mode = AppConfig::DEFAULT_PARSER_MODE
    );

    /**
     * @brief Analyse une transcription finale et retourne la commande selon le mode actif.
     */
    CommandResult route(const String& rawText, uint32_t nowMs);

    /**
     * @brief Analyse avec le moteur historique v0.1.1 (sécurisé contre les fausses confirmations).
     */
    CommandResult routeLegacy(const String& rawText, uint32_t nowMs);

    /**
     * @brief Analyse avec le moteur contextuel JournalParser v0.2.0.1.
     */
    CommandResult routeIntelligent(const String& rawText, uint32_t nowMs);

    /**
     * @brief Analyse en mode SHADOW (comparateur complet COR-09, blocage strict en cas de divergence destructive).
     */
    CommandResult routeShadow(const String& rawText, uint32_t nowMs);

    /**
     * @brief Sélectionne le mode de parsing actif.
     */
    void setParserMode(ParserMode mode) { mode_ = mode; }
    ParserMode getParserMode() const { return mode_; }

    /**
     * @brief Arme l'attente de confirmation de suppression.
     */
    void armClearConfirmation(uint32_t nowMs);

    /**
     * @brief Annule immédiatement toute demande de confirmation en attente (ex: STOP).
     */
    void cancelClearConfirmation();

    /**
     * @brief Indique si une confirmation de suppression est active et non expirée.
     */
    bool isClearConfirmationPending(uint32_t nowMs) const;

    /**
     * @brief Met à jour l'expiration de la confirmation.
     */
    bool checkClearConfirmationTimeout(uint32_t nowMs);

    /**
     * @brief Journalise les diagnostics structurés du parsing.
     */
    void logDiagnostics(const CommandResult& res, bool maskPrivatePayload = AppConfig::LOG_PRIVATE_PAYLOAD) const;

    /**
     * @brief Normalise une chaîne UTF-8 française (pour compatibilité legacy).
     */
    static String normalizeText(const String& input);

    VoiceJournal::Parser::JournalParser& getParser() { return parser_; }

private:
    uint32_t clearConfirmTimeoutMs_;
    ParserMode mode_;
    bool clearPending_ = false;
    uint32_t clearArmedAtMs_ = 0;
    VoiceJournal::Parser::JournalParser parser_;

    static bool extractNotePayload(const String& rawText, String& outPayload);
    static bool isExactConfirmPhrase(const String& normalized);
};
