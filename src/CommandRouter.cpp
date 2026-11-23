#include "CommandRouter.h"

#include <cctype>
#include <cstring>

CommandRouter::CommandRouter(uint32_t clearConfirmTimeoutMs)
    : clearConfirmTimeoutMs_(clearConfirmTimeoutMs) {}

void CommandRouter::armClearConfirmation(uint32_t nowMs) {
    clearPending_ = true;
    clearArmedAtMs_ = nowMs;
}

void CommandRouter::cancelClearConfirmation() {
    clearPending_ = false;
    clearArmedAtMs_ = 0;
}

bool CommandRouter::isClearConfirmationPending(uint32_t nowMs) const {
    if (!clearPending_) {
        return false;
    }
    return (nowMs - clearArmedAtMs_) <= clearConfirmTimeoutMs_;
}

bool CommandRouter::checkClearConfirmationTimeout(uint32_t nowMs) {
    if (!clearPending_) {
        return false;
    }
    if ((nowMs - clearArmedAtMs_) > clearConfirmTimeoutMs_) {
        clearPending_ = false;
        clearArmedAtMs_ = 0;
        return true;
    }
    return false;
}

String CommandRouter::normalizeText(const String& input) {
    String out;
    out.reserve(input.length());

    const char* s = input.c_str();
    const size_t len = input.length();
    bool lastWasSpace = true; // Supprimer les espaces en debut de chaine

    auto appendChar = [&](char c) {
        if (c == ' ') {
            if (!lastWasSpace) {
                out += ' ';
                lastWasSpace = true;
            }
        } else {
            out += c;
            lastWasSpace = false;
        }
    };

    size_t i = 0;
    while (i < len) {
        const uint8_t b0 = static_cast<uint8_t>(s[i]);

        // ASCII standard
        if (b0 < 0x80u) {
            if (b0 >= 'A' && b0 <= 'Z') {
                appendChar(static_cast<char>(b0 - 'A' + 'a'));
            } else if ((b0 >= 'a' && b0 <= 'z') || (b0 >= '0' && b0 <= '9')) {
                appendChar(static_cast<char>(b0));
            } else {
                // Ponctuation, apostrophe, tiret, espace, etc. -> espace
                appendChar(' ');
            }
            ++i;
            continue;
        }

        // Sequences UTF-8 sur 2 octets (Latin-1 Supplement & Latin Extended-A)
        if ((b0 == 0xC3u || b0 == 0xC5u || b0 == 0xC2u) && (i + 1) < len) {
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            if (b0 == 0xC3u) {
                switch (b1) {
                    // A / a avec accents : À Á Â Ã Ä Å (80..85), à á â ã ä å (A0..A5)
                    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                        appendChar('a'); break;
                    // Æ / æ
                    case 0x86: case 0xA6:
                        appendChar('a'); appendChar('e'); break;
                    // Ç / ç
                    case 0x87: case 0xA7:
                        appendChar('c'); break;
                    // È É Ê Ë (88..8B), è é ê ë (A8..AB)
                    case 0x88: case 0x89: case 0x8A: case 0x8B:
                    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                        appendChar('e'); break;
                    // Ì Í Î Ï (8C..8F), ì í î ï (AC..AF)
                    case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                        appendChar('i'); break;
                    // Ñ / ñ
                    case 0x91: case 0xB1:
                        appendChar('n'); break;
                    // Ò Ó Ô Õ Ö (92..96), ò ó ô õ ö (B2..B6)
                    case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                        appendChar('o'); break;
                    // Ù Ú Û Ü (99..9C), ù ú û ü (B9..BC)
                    case 0x99: case 0x9A: case 0x9B: case 0x9C:
                    case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                        appendChar('u'); break;
                    // Ý / ý / ÿ
                    case 0x9D: case 0xBD: case 0xBF:
                        appendChar('y'); break;
                    default:
                        appendChar(' '); break;
                }
            } else if (b0 == 0xC5u && (b1 == 0x92u || b1 == 0x93u)) {
                // Œ / œ
                appendChar('o');
                appendChar('e');
            } else {
                // Guillemets « » (C2 AB / C2 BB), espace insecable (C2 A0), etc.
                appendChar(' ');
            }
            i += 2;
            continue;
        }

        // Sequences UTF-8 sur 3 octets (ex: apostrophe typographique ’ E2 80 99, tirets)
        if ((b0 & 0xF0u) == 0xE0u && (i + 2) < len) {
            appendChar(' ');
            i += 3;
            continue;
        }

        // Sequences UTF-8 sur 4 octets
        if ((b0 & 0xF8u) == 0xF0u && (i + 3) < len) {
            appendChar(' ');
            i += 4;
            continue;
        }

        ++i;
    }

    out.trim();
    return out;
}

bool CommandRouter::extractNotePayload(const String& rawText, String& outPayload) {
    outPayload = "";
    const char* s = rawText.c_str();
    const size_t len = rawText.length();
    size_t i = 0;

    // Ignorer espaces et guillemets/ponctuation d'ouverture en debut de chaine
    while (i < len) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '\'') {
            ++i;
        } else if (c == 0xC2u && (i + 1) < len &&
                   (static_cast<uint8_t>(s[i + 1]) == 0xABu || static_cast<uint8_t>(s[i + 1]) == 0xA0u)) {
            i += 2;
        } else {
            break;
        }
    }

    // Verifier le prefixe explicite "note" et ses variantes STT francaises
    // frequentes ("note", "notes", "notez", "noter", "note" accentue)
    auto toLowerAscii = [](char ch) -> char {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    };

    if (i + 4 > len) {
        return false;
    }
    if (toLowerAscii(s[i]) != 'n' ||
        toLowerAscii(s[i + 1]) != 'o' ||
        toLowerAscii(s[i + 2]) != 't') {
        return false;
    }
    i += 3;

    if (toLowerAscii(s[i]) == 'e') {
        ++i;
        // Variantes STT frequentes : "notes", "notez", "noter"
        if (i < len) {
            const char suffix = toLowerAscii(s[i]);
            if (suffix == 's' || suffix == 'z' || suffix == 'r') {
                ++i;
            }
        }
    } else if (static_cast<uint8_t>(s[i]) == 0xC3u && (i + 1) < len &&
               (static_cast<uint8_t>(s[i + 1]) == 0xA9u || static_cast<uint8_t>(s[i + 1]) == 0x89u)) {
        // Variante STT "noté" / "NOTÉ" (et accord eventuel "notée" / "notés")
        i += 2;
        if (i < len && toLowerAscii(s[i]) == 'e') {
            ++i;
        }
        if (i < len && toLowerAscii(s[i]) == 's') {
            ++i;
        }
    } else {
        return false;
    }

    auto separatorLengthAt = [&](size_t pos) -> size_t {
        if (pos >= len) {
            return 0;
        }
        const uint8_t b0 = static_cast<uint8_t>(s[pos]);
        if (b0 == ' ' || b0 == ',' || b0 == ':' || b0 == ';' ||
            b0 == '.' || b0 == '-' || b0 == '\t' || b0 == '\n' || b0 == '\r') {
            return 1;
        }
        // Espace insecable UTF-8 (U+00A0 = C2 A0)
        if (b0 == 0xC2u && (pos + 1) < len && static_cast<uint8_t>(s[pos + 1]) == 0xA0u) {
            return 2;
        }
        // Espace fine insecable (U+202F = E2 80 AF) et tirets typographiques (U+2013 / U+2014)
        if (b0 == 0xE2u && (pos + 2) < len && static_cast<uint8_t>(s[pos + 1]) == 0x80u) {
            const uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
            if (b2 == 0xAFu || b2 == 0x93u || b2 == 0x94u) {
                return 3;
            }
        }
        return 0;
    };

    // Apres le prefixe, il doit y avoir fin de chaine ou un separateur explicite
    if (i < len && separatorLengthAt(i) == 0) {
        // Ex: "notebook" ne sont PAS le prefixe "note"
        return false;
    }

    // Sauter les separateurs immediatement apres le prefixe
    while (i < len) {
        const size_t sepLen = separatorLengthAt(i);
        if (sepLen > 0) {
            i += sepLen;
        } else {
            break;
        }
    }

    String payload = rawText.substring(i);
    payload.trim();
    outPayload = payload;
    return true;
}

bool CommandRouter::isExactConfirmPhrase(const String& normalized) {
    return normalized == "confirmer la suppression" ||
           normalized == "confirmez la suppression" ||
           normalized == "confirme la suppression" ||
           normalized == "je confirme la suppression" ||
           normalized == "oui confirmer la suppression" ||
           normalized == "oui confirmez la suppression" ||
           normalized == "oui confirme la suppression" ||
           normalized == "oui je confirme la suppression";
}

bool CommandRouter::isCancelPhrase(const String& normalized) {
    return normalized == "annuler" ||
           normalized == "annulez" ||
           normalized == "annule" ||
           normalized == "annuler la suppression" ||
           normalized == "annulez la suppression" ||
           normalized == "annule la suppression" ||
           normalized == "non";
}

CommandResult CommandRouter::route(const String& rawText, uint32_t nowMs) {
    CommandResult res;
    res.normalizedInput = normalizeText(rawText);

    if (res.normalizedInput.length() == 0) {
        res.type = CommandType::NONE;
        return res;
    }

    // 1. Verifier si une confirmation de suppression est en attente
    const bool pendingClear = isClearConfirmationPending(nowMs);
    if (clearPending_ && !pendingClear) {
        // Expiration silencieuse
        clearPending_ = false;
        clearArmedAtMs_ = 0;
    }

    if (pendingClear) {
        if (isExactConfirmPhrase(res.normalizedInput)) {
            clearPending_ = false;
            clearArmedAtMs_ = 0;
            res.type = CommandType::CONFIRM_CLEAR;
            return res;
        }
        if (isCancelPhrase(res.normalizedInput)) {
            clearPending_ = false;
            clearArmedAtMs_ = 0;
            res.type = CommandType::CANCEL_CLEAR;
            return res;
        }
        // Toute autre phrase invalide la confirmation en attente (securite stricte)
        clearPending_ = false;
        clearArmedAtMs_ = 0;
    }

    // 2. Commande "Note, <observation>" (uniquement comme prefixe explicite)
    String notePayload;
    if (extractNotePayload(rawText, notePayload)) {
        if (notePayload.length() == 0) {
            // Observation vide refusee
            res.type = CommandType::NONE;
            return res;
        }
        res.type = CommandType::ADD_NOTE;
        res.payload = notePayload;
        return res;
    }

    const String& n = res.normalizedInput;

    // 3. Lire la derniere note
    if (n == "lis la derniere note" ||
        n == "lire la derniere note" ||
        n == "relis la derniere note" ||
        n == "relire la derniere note" ||
        n == "lis la derniere observation" ||
        n == "lire la derniere observation" ||
        n == "derniere note") {
        res.type = CommandType::READ_LAST_NOTE;
        return res;
    }

    // 4. Nombre de notes
    if (n == "combien de notes" ||
        n == "combien de note" ||
        n == "combien y a t il de notes" ||
        n == "nombre de notes" ||
        n == "combien d observations" ||
        n == "combien d observation") {
        res.type = CommandType::COUNT_NOTES;
        return res;
    }

    // 5. Analyse IA / Bilan / Synthese
    if (n == "fais le bilan" ||
        n == "faire le bilan" ||
        n == "bilan" ||
        n == "resume les observations" ||
        n == "resumer les observations" ||
        n == "resume les notes" ||
        n == "resumer les notes" ||
        n == "qu avons nous observe" ||
        n == "qu est ce qu on a observe" ||
        n == "analyse les observations" ||
        n == "analyser les observations" ||
        n == "analyse les notes") {
        res.type = CommandType::ANALYZE_NOTES;
        return res;
    }

    // 6. Demande d'effacement de toutes les notes (confirmation obligatoire)
    if (n == "efface toutes les notes" ||
        n == "effacer toutes les notes" ||
        n == "effacez toutes les notes" ||
        n == "supprime toutes les notes" ||
        n == "supprimer toutes les notes" ||
        n == "supprimez toutes les notes" ||
        n == "vide le carnet" ||
        n == "vider le carnet" ||
        n == "videz le carnet") {
        res.type = CommandType::REQUEST_CLEAR;
        return res;
    }

    // 7. Commande non reconnue
    res.type = CommandType::UNKNOWN;
    return res;
}
