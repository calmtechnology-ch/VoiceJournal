#include "JournalUI.h"

#ifdef ARDUINO
#include <M5Unified.h>
#endif

void JournalUI::begin(const JournalViewModel& initialView) {
    view_ = initialView;
    needsRedraw_ = true;
#ifdef ARDUINO
    // M5Stack CoreS3 (ILI9342) est nativement en paysage 320x240 a la rotation 0
    // (rotation 1 = portrait 240x320).
    M5.Display.setRotation(0);
    M5.Display.setTextWrap(false);
    draw();
#endif
}

void JournalUI::updateView(const JournalViewModel& view) {
    if (view_.state != view.state ||
        view_.noteCount != view.noteCount ||
        view_.lastTranscript != view.lastTranscript ||
        view_.statusMessage != view.statusMessage ||
        view_.connected != view.connected ||
        view_.clearConfirmPending != view.clearConfirmPending) {
        view_ = view;
        needsRedraw_ = true;
    }
}

void JournalUI::renderIfNeeded() {
    if (!needsRedraw_) {
        return;
    }
#ifdef ARDUINO
    draw();
#endif
    needsRedraw_ = false;
}

bool JournalUI::isTalkButtonEnabled() const {
    return (view_.state == JournalState::READY || view_.state == JournalState::LISTENING);
}

TouchAction JournalUI::pollTouch(uint32_t nowMs) {
#ifdef ARDUINO
    auto touch = M5.Touch.getDetail();
    if (!touch.wasPressed()) {
        return TouchAction::NONE;
    }

    // Anti-rebond tactile
    if ((nowMs - lastTouchAcceptedMs_) < AppConfig::TOUCH_DEBOUNCE_MS) {
        return TouchAction::NONE;
    }

    const int x = touch.x;
    const int y = touch.y;

    // 1. Bouton STOP (prioritaire, toujours actif)
    if (x >= BTN_STOP_X && x <= (BTN_STOP_X + BTN_STOP_W) &&
        y >= BTN_Y && y <= (BTN_Y + BTN_H)) {
        lastTouchAcceptedMs_ = nowMs;
        return TouchAction::STOP;
    }

    // 2. Bouton PARLER / TERMINER (actif uniquement si READY ou LISTENING)
    if (x >= BTN_TALK_X && x <= (BTN_TALK_X + BTN_TALK_W) &&
        y >= BTN_Y && y <= (BTN_Y + BTN_H)) {
        if (!isTalkButtonEnabled()) {
            return TouchAction::NONE;
        }
        lastTouchAcceptedMs_ = nowMs;
        return TouchAction::TALK_TOGGLE;
    }
#else
    (void)nowMs;
#endif
    return TouchAction::NONE;
}

String JournalUI::toDisplayAscii(const String& utf8) {
    String out;
    out.reserve(utf8.length());

    const char* s = utf8.c_str();
    const size_t len = utf8.length();
    size_t i = 0;

    while (i < len) {
        const uint8_t b0 = static_cast<uint8_t>(s[i]);
        if (b0 < 0x80u) {
            if (b0 >= 0x20u && b0 <= 0x7Eu) {
                out += static_cast<char>(b0);
            } else if (b0 == '\n' || b0 == '\r' || b0 == '\t') {
                out += ' ';
            }
            ++i;
            continue;
        }

        if (b0 == 0xC3u && (i + 1) < len) {
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            switch (b1) {
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: out += 'A'; break;
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: out += 'a'; break;
                case 0x87: out += 'C'; break;
                case 0xA7: out += 'c'; break;
                case 0x88: case 0x89: case 0x8A: case 0x8B: out += 'E'; break;
                case 0xA8: case 0xA9: case 0xAA: case 0xAB: out += 'e'; break;
                case 0x8C: case 0x8D: case 0x8E: case 0x8F: out += 'I'; break;
                case 0xAC: case 0xAD: case 0xAE: case 0xAF: out += 'i'; break;
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: out += 'O'; break;
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: out += 'o'; break;
                case 0x99: case 0x9A: case 0x9B: case 0x9C: out += 'U'; break;
                case 0xB9: case 0xBA: case 0xBB: case 0xBC: out += 'u'; break;
                default: out += ' '; break;
            }
            i += 2;
            continue;
        }

        if (b0 == 0xC5u && (i + 1) < len) {
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            if (b1 == 0x92u) out += "OE";
            else if (b1 == 0x93u) out += "oe";
            else out += ' ';
            i += 2;
            continue;
        }

        if (b0 == 0xE2u && (i + 2) < len) {
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t b2 = static_cast<uint8_t>(s[i + 2]);
            if (b1 == 0x80u && (b2 == 0x98u || b2 == 0x99u)) {
                out += '\'';
            } else {
                out += ' ';
            }
            i += 3;
            continue;
        }

        if ((b0 & 0xE0u) == 0xC0u)      i += 2;
        else if ((b0 & 0xF0u) == 0xE0u) i += 3;
        else if ((b0 & 0xF8u) == 0xF0u) i += 4;
        else                            ++i;
    }

    return out;
}

void JournalUI::drawWrappedText(const String& text,
                                int x,
                                int y,
                                int maxWidth,
                                int maxLines,
                                int lineHeight) {
#ifdef ARDUINO
    String ascii = toDisplayAscii(text);
    ascii.trim();
    if (ascii.length() == 0) {
        return;
    }

    const int charWidth = 6; // Police par defaut M5GFX size=1 (6x8 px)
    const size_t maxCharsPerLine = static_cast<size_t>(maxWidth / charWidth);
    if (maxCharsPerLine == 0) {
        return;
    }

    size_t pos = 0;
    const size_t total = ascii.length();
    for (int line = 0; line < maxLines && pos < total; ++line) {
        while (pos < total && ascii[pos] == ' ') {
            ++pos;
        }
        if (pos >= total) {
            break;
        }

        size_t take = total - pos;
        if (take > maxCharsPerLine) {
            take = maxCharsPerLine;
            // Chercher le dernier espace pour couper proprement entre deux mots
            size_t spacePos = take;
            while (spacePos > 0 && ascii[pos + spacePos] != ' ') {
                --spacePos;
            }
            if (spacePos > 0) {
                take = spacePos;
            }
        }

        String lineStr = ascii.substring(pos, pos + take);
        if (line == (maxLines - 1) && (pos + take) < total && lineStr.length() > 3) {
            lineStr = lineStr.substring(0, lineStr.length() - 3) + "...";
        }
        M5.Display.drawString(lineStr.c_str(), x, y + (line * lineHeight));
        pos += take;
    }
#else
    (void)text; (void)x; (void)y; (void)maxWidth; (void)maxLines; (void)lineHeight;
#endif
}

void JournalUI::draw() {
#ifdef ARDUINO
    const uint16_t bgColor     = M5.Display.color565(14, 18, 26);
    const uint16_t headerBg    = M5.Display.color565(22, 28, 40);
    const uint16_t cardBg      = M5.Display.color565(24, 32, 46);
    const uint16_t cardBorder  = M5.Display.color565(56, 72, 98);

    M5.Display.startWrite();
    M5.Display.fillScreen(bgColor);

    // =========================================================================
    // 1. En-tete : Titre + Badge d'etat
    // =========================================================================
    M5.Display.fillRect(0, 0, SCREEN_W, HEADER_H, headerBg);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(TFT_WHITE, headerBg);
    M5.Display.drawString(AppConfig::APP_TITLE, 10, HEADER_H / 2);

    uint16_t badgeColor = TFT_DARKGRAY;
    uint16_t badgeText  = TFT_WHITE;
    switch (view_.state) {
        case JournalState::READY:
            badgeColor = M5.Display.color565(30, 170, 90);
            badgeText  = TFT_BLACK;
            break;
        case JournalState::LISTENING:
            badgeColor = M5.Display.color565(220, 45, 55);
            badgeText  = TFT_WHITE;
            break;
        case JournalState::TRANSCRIBING:
            badgeColor = M5.Display.color565(0, 175, 210);
            badgeText  = TFT_BLACK;
            break;
        case JournalState::SAVING:
        case JournalState::PROCESSING_COMMAND:
            badgeColor = M5.Display.color565(45, 120, 220);
            badgeText  = TFT_WHITE;
            break;
        case JournalState::ANALYZING:
            badgeColor = M5.Display.color565(155, 70, 220);
            badgeText  = TFT_WHITE;
            break;
        case JournalState::SPEAKING:
            badgeColor = M5.Display.color565(220, 165, 30);
            badgeText  = TFT_BLACK;
            break;
        case JournalState::DISCONNECTED:
            badgeColor = M5.Display.color565(120, 95, 60);
            badgeText  = TFT_WHITE;
            break;
        case JournalState::ERROR_STATE:
            badgeColor = M5.Display.color565(200, 35, 35);
            badgeText  = TFT_WHITE;
            break;
        default:
            badgeColor = M5.Display.color565(180, 120, 30);
            badgeText  = TFT_BLACK;
            break;
    }

    const int badgeW = 126;
    const int badgeH = 20;
    const int badgeX = SCREEN_W - badgeW - 8;
    const int badgeY = (HEADER_H - badgeH) / 2;
    M5.Display.fillRoundRect(badgeX, badgeY, badgeW, badgeH, 4, badgeColor);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(badgeText, badgeColor);
    M5.Display.drawString(journalStateToUiLabel(view_.state),
                          badgeX + (badgeW / 2),
                          badgeY + (badgeH / 2));

    // =========================================================================
    // 2. Zone centrale : Compteur notes + Derniere transcription + Statut
    // =========================================================================
    M5.Display.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 8, cardBg);
    M5.Display.drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 8, cardBorder);

    // Ligne superieure : compteur de notes (toujours visible meme hors ligne)
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(M5.Display.color565(130, 210, 255), cardBg);
    char noteHdr[32];
    snprintf(noteHdr, sizeof(noteHdr), "Notes : %u / %u",
             static_cast<unsigned>(view_.noteCount),
             static_cast<unsigned>(AppConfig::MAX_NOTES));
    M5.Display.drawString(noteHdr, CARD_X + 10, CARD_Y + 8);

    // Pastille de connectivite
    const uint16_t netColor = view_.connected ? M5.Display.color565(40, 200, 100)
                                              : M5.Display.color565(220, 120, 40);
    M5.Display.fillCircle(CARD_X + CARD_W - 16, CARD_Y + 16, 5, netColor);

    M5.Display.drawFastHLine(CARD_X + 8, CARD_Y + 32, CARD_W - 16, cardBorder);

    // Zone transcription / synthese
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(150, 165, 190), cardBg);
    M5.Display.drawString("Derniere transcription :", CARD_X + 10, CARD_Y + 38);

    M5.Display.setTextColor(TFT_WHITE, cardBg);
    if (view_.lastTranscript.length() > 0) {
        drawWrappedText(view_.lastTranscript, CARD_X + 10, CARD_Y + 52, CARD_W - 20, 4, 14);
    } else {
        M5.Display.setTextColor(M5.Display.color565(110, 125, 145), cardBg);
        M5.Display.drawString("(Aucune dictee pour le moment)", CARD_X + 10, CARD_Y + 56);
    }

    // Ligne de statut ou d'erreur en bas de carte
    M5.Display.drawFastHLine(CARD_X + 8, CARD_Y + CARD_H - 26, CARD_W - 16, cardBorder);
    uint16_t statusColor = M5.Display.color565(175, 225, 185);
    if (view_.state == JournalState::ERROR_STATE) {
        statusColor = M5.Display.color565(255, 110, 110);
    } else if (view_.clearConfirmPending || view_.state == JournalState::DISCONNECTED) {
        statusColor = M5.Display.color565(255, 205, 90);
    }
    M5.Display.setTextColor(statusColor, cardBg);
    drawWrappedText(view_.statusMessage, CARD_X + 10, CARD_Y + CARD_H - 19, CARD_W - 20, 1, 12);

    // =========================================================================
    // 3. Zone inferieure : [ PARLER / TERMINER ] [ STOP ]
    // =========================================================================
    const bool talkEnabled = isTalkButtonEnabled();
    uint16_t talkBg = M5.Display.color565(55, 62, 74); // Desactive
    uint16_t talkFg = M5.Display.color565(140, 150, 165);
    const char* talkLabel = "PARLER";

    if (view_.state == JournalState::LISTENING) {
        talkBg = M5.Display.color565(215, 45, 45);
        talkFg = TFT_WHITE;
        talkLabel = "TERMINER";
    } else if (talkEnabled) {
        talkBg = M5.Display.color565(28, 115, 215);
        talkFg = TFT_WHITE;
        talkLabel = "PARLER";
    }

    M5.Display.fillRoundRect(BTN_TALK_X, BTN_Y, BTN_TALK_W, BTN_H, 8, talkBg);
    M5.Display.drawRoundRect(BTN_TALK_X, BTN_Y, BTN_TALK_W, BTN_H, 8,
                             talkEnabled ? TFT_WHITE : cardBorder);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(talkFg, talkBg);
    M5.Display.drawString(talkLabel, BTN_TALK_X + (BTN_TALK_W / 2), BTN_Y + (BTN_H / 2));

    // Bouton STOP
    const uint16_t stopBg = M5.Display.color565(180, 35, 40);
    M5.Display.fillRoundRect(BTN_STOP_X, BTN_Y, BTN_STOP_W, BTN_H, 8, stopBg);
    M5.Display.drawRoundRect(BTN_STOP_X, BTN_Y, BTN_STOP_W, BTN_H, 8, TFT_WHITE);
    M5.Display.setTextColor(TFT_WHITE, stopBg);
    M5.Display.drawString("STOP", BTN_STOP_X + (BTN_STOP_W / 2), BTN_Y + (BTN_H / 2));

    M5.Display.endWrite();
#endif
}
