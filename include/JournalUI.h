#pragma once

#include <Arduino.h>
#include <cstdint>
#include "AppConfig.h"
#include "VoiceController.h"

/**
 * @enum TouchAction
 * @brief Actions tactiles filtrees produites par JournalUI.
 */
enum class TouchAction : uint8_t {
    NONE = 0,
    TALK_TOGGLE,    ///< Clic sur [PARLER / TERMINER]
    STOP,           ///< Clic prioritaire sur [STOP]
    SWITCH_PROVIDER,///< Clic sur le selecteur de fournisseur [OPENAI / GEMINI]
    TOGGLE_LAB      ///< Clic sur [LAB] ou [RETOUR] pour basculer la vue diagnostic Parsing Lab
};

/**
 * @struct TouchSample
 * @brief Echantillon tactile independant du materiel pour filtrage et tests natifs.
 */
struct TouchSample {
    bool isPressed = false;  ///< Contact physique actuellement maintenu sur l'ecran
    bool wasPressed = false; ///< Front montant signale par le controleur tactile
    int x = 0;
    int y = 0;
};

/**
 * @class JournalUI
 * @brief Interface graphique M5GFX (paysage 320x240, sans LVGL) pour VoiceJournal v0.1.1.
 *
 * Affiche en permanence l'en-tete, le badge d'etat, le compteur de notes NVS
 * (y compris hors ligne), la derniere transcription, le statut/erreur et les deux
 * boutons [PARLER / TERMINER] et [STOP].
 */
class JournalUI {
public:
    // Geometrie paysage 320 x 240 (exposee pour coherence UI / tests)
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    static constexpr int HEADER_H = 30;

    static constexpr int CARD_X = 8;
    static constexpr int CARD_Y = 34;
    static constexpr int CARD_W = 304;
    static constexpr int CARD_H = 142;

    static constexpr int BTN_PROVIDER_X = 184;
    static constexpr int BTN_PROVIDER_Y = 38;
    static constexpr int BTN_PROVIDER_W = 98;
    static constexpr int BTN_PROVIDER_H = 24;

    static constexpr int BTN_LAB_X = 120;
    static constexpr int BTN_LAB_Y = 4;
    static constexpr int BTN_LAB_W = 48;
    static constexpr int BTN_LAB_H = 22;

    static constexpr int BTN_LAB_RETOUR_X = 224;
    static constexpr int BTN_LAB_RETOUR_Y = 38;
    static constexpr int BTN_LAB_RETOUR_W = 82;
    static constexpr int BTN_LAB_RETOUR_H = 24;

    static constexpr int BTN_Y      = 184;
    static constexpr int BTN_H      = 48;
    static constexpr int BTN_TALK_X = 8;
    static constexpr int BTN_TALK_W = 196;
    static constexpr int BTN_STOP_X = 212;
    static constexpr int BTN_STOP_W = 100;

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
     * @brief Interroge l'ecran tactile M5.Touch et delegue a processTouchSample().
     */
    TouchAction pollTouch(uint32_t nowMs);

    /**
     * @brief Logique de decision tactile pure et testable hors materiel.
     *
     * Garanties :
     * - STOP est evalue en premier et ne depend jamais de l'anti-rebond de PARLER.
     * - L'anti-rebond (TOUCH_DEBOUNCE_MS) s'applique uniquement a PARLER / TERMINER.
     * - Un contact maintenu ne genere qu'un seul evenement jusqu'au relachement.
     * - Deux contacts distincts peuvent produire PARLER puis STOP immediatement.
     */
    TouchAction processTouchSample(const TouchSample& sample, uint32_t nowMs);

    /**
     * @brief Convertit les caracteres accentues UTF-8 en ASCII lisible pour la police
     *        integree M5GFX.
     */
    static String toDisplayAscii(const String& utf8);

private:
    void draw();
    void drawWrappedText(const String& text, int x, int y, int maxWidth, int maxLines, int lineHeight);
    bool isTalkButtonEnabled() const;
    bool isProviderSwitchEnabled() const;
    static bool isInsideRect(int x, int y, int rx, int ry, int rw, int rh);

    JournalViewModel view_;
    bool needsRedraw_ = true;
    bool touchDown_ = false;
    bool hasLastTalkAccepted_ = false;
    uint32_t lastTalkAcceptedMs_ = 0;
    bool hasLastProviderAccepted_ = false;
    uint32_t lastProviderAcceptedMs_ = 0;
};
