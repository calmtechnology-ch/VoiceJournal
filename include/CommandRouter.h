#pragma once

#include <Arduino.h>
#include <cstdint>
#include "AppConfig.h"

/**
 * @enum CommandType
 * @brief Commandes vocales deterministes reconnues par VoiceJournal v0.1.0.
 */
enum class CommandType : uint8_t {
    NONE = 0,          ///< Transcription vide ou observation vide apres "note"
    ADD_NOTE,          ///< Enregistrer une nouvelle observation (prefixe "note")
    READ_LAST_NOTE,    ///< Relire la derniere observation enregistree
    COUNT_NOTES,       ///< Prononcer le nombre d'observations
    ANALYZE_NOTES,     ///< Demander une synthese IA via Gemini
    REQUEST_CLEAR,     ///< Demander l'effacement de toutes les notes (exige confirmation)
    CONFIRM_CLEAR,     ///< Confirmation explicite de suppression
    CANCEL_CLEAR,      ///< Annulation explicite de suppression
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
        case CommandType::UNKNOWN:        return "UNKNOWN";
        default:                          return "UNKNOWN";
    }
}

struct CommandResult {
    CommandType type = CommandType::NONE;
    String payload;          ///< Contenu brut nettoye (pour ADD_NOTE, accents preserves)
    String normalizedInput;  ///< Forme normalisee ASCII minuscule utilisee pour le routage
};

/**
 * @class CommandRouter
 * @brief Routeur de commandes vocales purement deterministe (aucune decision IA).
 *
 * Gere egalement l'etat de confirmation obligatoire pour l'effacement de toutes
 * les notes, avec expiration temporelle et annulation explicite.
 */
class CommandRouter {
public:
    explicit CommandRouter(uint32_t clearConfirmTimeoutMs = AppConfig::CLEAR_CONFIRM_TIMEOUT_MS);

    /**
     * @brief Analyse une transcription finale et retourne la commande deterministe.
     * @param rawText Texte transcrit brut (UTF-8).
     * @param nowMs Horodatage courant en millisecondes (millis()).
     */
    CommandResult route(const String& rawText, uint32_t nowMs);

    /**
     * @brief Arme l'attente de confirmation de suppression pour clearConfirmTimeoutMs_.
     */
    void armClearConfirmation(uint32_t nowMs);

    /**
     * @brief Annule immediatement toute demande de confirmation en attente (ex: STOP).
     */
    void cancelClearConfirmation();

    /**
     * @brief Indique si une confirmation de suppression est actuellement active et non expiree.
     */
    bool isClearConfirmationPending(uint32_t nowMs) const;

    /**
     * @brief Met a jour l'expiration de la confirmation. Retourne true si elle vient d'expirer.
     */
    bool checkClearConfirmationTimeout(uint32_t nowMs);

    /**
     * @brief Normalise une chaine UTF-8 francaise :
     *        accents -> ASCII, minuscules, ponctuation -> espace, espaces reduits.
     */
    static String normalizeText(const String& input);

private:
    static bool extractNotePayload(const String& rawText, String& outPayload);
    static bool isExactConfirmPhrase(const String& normalized);
    static bool isCancelPhrase(const String& normalized);

    uint32_t clearConfirmTimeoutMs_;
    bool clearPending_ = false;
    uint32_t clearArmedAtMs_ = 0;
};
