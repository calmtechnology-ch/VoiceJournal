#pragma once

#include <Arduino.h>
#include <cstdint>
#include "AppConfig.h"
#include "parser/JournalParser.h"

/**
 * @enum CommandType
 * @brief Commandes vocales deterministes reconnues par VoiceJournal.
 */
enum class CommandType : uint8_t {
    NONE = 0,          ///< Transcription vide ou observation vide apres "note"
    ADD_NOTE,          ///< Enregistrer une nouvelle observation (prefixe "note")
    READ_LAST_NOTE,    ///< Relire la derniere observation enregistree
    COUNT_NOTES,       ///< Prononcer le nombre d'observations
    ANALYZE_NOTES,     ///< Demander une synthese IA via Gemini ou OpenAI
    REQUEST_CLEAR,     ///< Demander l'effacement de toutes les notes (exige confirmation)
    CONFIRM_CLEAR,     ///< Confirmation explicite de suppression
    CANCEL_CLEAR,      ///< Annulation explicite de suppression
    HELP,              ///< Aide vocale et liste des commandes
    EXPERIMENTAL,      ///< Intention experimentale comprise mais non executable
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

struct CommandResult {
    CommandType type = CommandType::NONE;
    String payload;          ///< Contenu brut nettoye (pour ADD_NOTE, accents preserves)
    String normalizedInput;  ///< Forme normalisee ASCII minuscule utilisee pour le routage
    VoiceJournal::Parser::ParseResult parseResult;  ///< Resultat detaille du JournalParser v0.2.0
    CommandType legacyType = CommandType::NONE;     ///< Type issu du moteur historique
    String comparisonStatus; ///< "MATCH", "NEW_INTENT", "DIVERGENCE", "SECURITY_BLOCKED"
    bool blockedByDivergence = false;
};

/**
 * @class CommandRouter
 * @brief Routeur de commandes vocales supportant les modes LEGACY, INTELLIGENT et SHADOW.
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
     * @brief Analyse avec le moteur historique v0.1.1 (securise contre les fausses confirmations).
     */
    CommandResult routeLegacy(const String& rawText, uint32_t nowMs);

    /**
     * @brief Analyse avec le moteur intelligent JournalParser v0.2.0.
     */
    CommandResult routeIntelligent(const String& rawText, uint32_t nowMs);

    /**
     * @brief Analyse en mode SHADOW (compare les deux moteurs, bloque en cas de divergence destructive).
     */
    CommandResult routeShadow(const String& rawText, uint32_t nowMs);

    /**
     * @brief Selectionne le mode de parsing actif.
     */
    void setParserMode(ParserMode mode) { mode_ = mode; }
    ParserMode getParserMode() const { return mode_; }

    /**
     * @brief Arme l'attente de confirmation de suppression.
     */
    void armClearConfirmation(uint32_t nowMs);

    /**
     * @brief Annule immediatement toute demande de confirmation en attente (ex: STOP).
     */
    void cancelClearConfirmation();

    /**
     * @brief Indique si une confirmation de suppression est active et non expiree.
     */
    bool isClearConfirmationPending(uint32_t nowMs) const;

    /**
     * @brief Met a jour l'expiration de la confirmation.
     */
    bool checkClearConfirmationTimeout(uint32_t nowMs);

    /**
     * @brief Journalise les diagnostics structures du parsing.
     */
    void logDiagnostics(const CommandResult& res, bool maskPrivatePayload = AppConfig::LOG_PRIVATE_PAYLOAD) const;

    /**
     * @brief Normalise une chaine UTF-8 francaise (pour compatibilite legacy).
     */
    static String normalizeText(const String& input);

    VoiceJournal::Parser::JournalParser& getParser() { return parser_; }
    const VoiceJournal::Parser::JournalParser& getParser() const { return parser_; }

private:
    static bool extractNotePayload(const String& rawText, String& outPayload);
    static bool isExactConfirmPhrase(const String& normalized);
    static bool isCancelPhrase(const String& normalized);

    uint32_t clearConfirmTimeoutMs_;
    bool clearPending_ = false;
    uint32_t clearArmedAtMs_ = 0;
    ParserMode mode_ = AppConfig::DEFAULT_PARSER_MODE;
    VoiceJournal::Parser::JournalParser parser_;
};
