#pragma once

#include <Arduino.h>
#include <cstdint>
#include "AppConfig.h"
#include "VoiceController.h"

/**
 * @enum TouchAction
 * @brief Actions tactiles filtrees (avec anti-rebond) produites par JournalUI.
 */
enum class TouchAction : uint8_t {
    NONE = 0,
    TALK_TOGGLE, ///< Clic sur [PARLER / TERMINER]
    STOP         ///< Clic sur [STOP]
};

/**
 * @class JournalUI
 * @brief Interface graphique M5GFX (paysage 320x240, sans LVGL) pour VoiceJournal v0.1.0.
 *
 * Affiche en permanence l'en-tete, le badge d'etat, le compteur de notes NVS
 * (y compris hors ligne), la derniere transcription, le statut/erreur et les deux
 * boutons [PARLER / TERMINER] et [STOP].
 */
class JournalUI {
public:
    JournalUI() = default;

    /**
     * @brief Initialise l'ecran en mode paysage 320x240 et effectue le premier rendu.
     */
    void begin(const JournalViewModel& initialView);

    /**
     * @brief Met a jour les donnees affichees et marque l'ecran a redessiner si besoin.
     */
    void updateView(const JournalViewModel& view);

    /**
     * @brief Redessine l'ecran uniquement si l'etat ou les textes ont change.
     */
    void renderIfNeeded();

    /**
     * @brief Interroge l'ecran tactile M5.Touch avec anti-rebond et filtre les zones boutons.
     */
    TouchAction pollTouch(uint32_t nowMs);

    /**
     * @brief Convertit les caracteres accentues UTF-8 en ASCII lisible pour la police
     *        integree M5GFX.
     */
    static String toDisplayAscii(const String& utf8);

private:
    void draw();
    void drawWrappedText(const String& text, int x, int y, int maxWidth, int maxLines, int lineHeight);
    bool isTalkButtonEnabled() const;

    // Geometrie paysage 320 x 240
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    static constexpr int HEADER_H = 30;

    static constexpr int CARD_X = 8;
    static constexpr int CARD_Y = 34;
    static constexpr int CARD_W = 304;
    static constexpr int CARD_H = 142;

    static constexpr int BTN_Y      = 184;
    static constexpr int BTN_H      = 48;
    static constexpr int BTN_TALK_X = 8;
    static constexpr int BTN_TALK_W = 196;
    static constexpr int BTN_STOP_X = 212;
    static constexpr int BTN_STOP_W = 100;

    JournalViewModel view_;
    bool needsRedraw_ = true;
    uint32_t lastTouchAcceptedMs_ = 0;
};
