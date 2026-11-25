#include "CommandRouter.h"

#include <cctype>
#include <cstring>

CommandRouter::CommandRouter(uint32_t clearConfirmTimeoutMs, ParserMode mode)
    : clearConfirmTimeoutMs_(clearConfirmTimeoutMs), mode_(mode) {}

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
    const int32_t diff = static_cast<int32_t>(nowMs - clearArmedAtMs_);
    if (diff < 0) {
        return true;
    }
    return static_cast<uint32_t>(diff) <= clearConfirmTimeoutMs_;
}

bool CommandRouter::checkClearConfirmationTimeout(uint32_t nowMs) {
    if (!clearPending_) {
        return false;
    }
    const int32_t diff = static_cast<int32_t>(nowMs - clearArmedAtMs_);
    if (diff < 0) {
        return false;
    }
    if (static_cast<uint32_t>(diff) > clearConfirmTimeoutMs_) {
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
    bool lastWasSpace = true;

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

        if (b0 < 0x80u) {
            if (b0 >= 'A' && b0 <= 'Z') {
                appendChar(static_cast<char>(b0 - 'A' + 'a'));
            } else if ((b0 >= 'a' && b0 <= 'z') || (b0 >= '0' && b0 <= '9')) {
                appendChar(static_cast<char>(b0));
            } else {
                appendChar(' ');
            }
            ++i;
            continue;
        }

        if ((b0 == 0xC3u || b0 == 0xC5u || b0 == 0xC2u) && (i + 1) < len) {
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            if (b0 == 0xC3u) {
                switch (b1) {
                    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                        appendChar('a'); break;
                    case 0x86: case 0xA6:
                        appendChar('a'); appendChar('e'); break;
                    case 0x87: case 0xA7:
                        appendChar('c'); break;
                    case 0x88: case 0x89: case 0x8A: case 0x8B:
                    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                        appendChar('e'); break;
                    case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                        appendChar('i'); break;
                    case 0x91: case 0xB1:
                        appendChar('n'); break;
                    case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                        appendChar('o'); break;
                    case 0x99: case 0x9A: case 0x9B: case 0x9C:
                    case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                        appendChar('u'); break;
                    case 0x9D: case 0xBD: case 0xBF:
                        appendChar('y'); break;
                    default:
                        appendChar(' '); break;
                }
            } else if (b0 == 0xC5u && (b1 == 0x92u || b1 == 0x93u)) {
                appendChar('o');
                appendChar('e');
            } else {
                appendChar(' ');
            }
            i += 2;
            continue;
        }

        if ((b0 & 0xF0u) == 0xE0u && (i + 2) < len) {
            appendChar(' ');
            i += 3;
            continue;
        }

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
        if (i < len) {
            const char suffix = toLowerAscii(s[i]);
            if (suffix == 's' || suffix == 'z' || suffix == 'r') {
                ++i;
            }
        }
    } else if (static_cast<uint8_t>(s[i]) == 0xC3u && (i + 1) < len &&
               (static_cast<uint8_t>(s[i + 1]) == 0xA9u || static_cast<uint8_t>(s[i + 1]) == 0x89u)) {
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
        if (b0 == 0xC2u && (pos + 1) < len && static_cast<uint8_t>(s[pos + 1]) == 0xA0u) {
            return 2;
        }
        if (b0 == 0xE2u && (pos + 2) < len && static_cast<uint8_t>(s[pos + 1]) == 0x80u) {
            const uint8_t b2 = static_cast<uint8_t>(s[pos + 2]);
            if (b2 == 0xAFu || b2 == 0x93u || b2 == 0x94u) {
                return 3;
            }
        }
        return 0;
    };

    if (i < len && separatorLengthAt(i) == 0) {
        return false;
    }

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
    // 1. Protection de sécurité absolue : refus strict en cas de présence de négation
    if (normalized.indexOf("ne ") >= 0 ||
        normalized.indexOf("n ") >= 0 ||
        normalized.indexOf("pas") >= 0 ||
        normalized.indexOf("jamais") >= 0 ||
        normalized.indexOf("rien") >= 0 ||
        normalized.indexOf("aucun") >= 0 ||
        normalized.indexOf("non") >= 0) {
        return false;
    }

    // 2. Formulations exactes explicitement autorisées
    if (normalized == "confirmer la suppression" ||
        normalized == "confirmer la suppression de toutes les notes" ||
        normalized == "confirmer la suppression des notes" ||
        normalized == "confirmez la suppression" ||
        normalized == "confirme la suppression" ||
        normalized == "je confirme la suppression" ||
        normalized == "oui confirmer la suppression" ||
        normalized == "oui confirmez la suppression" ||
        normalized == "oui confirme la suppression" ||
        normalized == "oui je confirme la suppression" ||
        normalized == "confirmer" ||
        normalized == "confirmez" ||
        normalized == "confirme" ||
        normalized == "je confirme" ||
        normalized == "oui je confirme" ||
        normalized == "oui confirmer" ||
        normalized == "oui confirmez" ||
        normalized == "oui confirme" ||
        normalized == "valider la suppression" ||
        normalized == "valide la suppression" ||
        normalized == "supprimer definitivement" ||
        normalized == "effacer definitivement" ||
        normalized == "oui supprime" ||
        normalized == "oui efface") {
        return true;
    }

    return false;
}

bool CommandRouter::isCancelPhrase(const String& normalized) {
    return normalized == "annuler" ||
           normalized == "annulez" ||
           normalized == "annule" ||
           normalized == "annuler la suppression" ||
           normalized == "annulez la suppression" ||
           normalized == "annule la suppression" ||
           normalized == "non" ||
           normalized == "non annuler" ||
           normalized == "stop" ||
           normalized == "laisse tomber" ||
           normalized == "ne supprime pas" ||
           normalized == "ne supprime rien" ||
           normalized == "n efface pas" ||
           normalized == "n efface rien" ||
           normalized == "ne rien faire" ||
           normalized == "ne fais rien";
}

CommandResult CommandRouter::routeLegacy(const String& rawText, uint32_t nowMs) {
    CommandResult res;
    res.normalizedInput = normalizeText(rawText);

    if (res.normalizedInput.length() == 0) {
        res.type = CommandType::NONE;
        return res;
    }

    const bool pendingClear = isClearConfirmationPending(nowMs);
    if (clearPending_ && !pendingClear) {
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
        // Toute autre phrase (ou négation) invalide la confirmation
        clearPending_ = false;
        clearArmedAtMs_ = 0;
    }

    String notePayload;
    if (extractNotePayload(rawText, notePayload)) {
        if (notePayload.length() == 0) {
            res.type = CommandType::NONE;
            return res;
        }
        res.type = CommandType::ADD_NOTE;
        res.payload = notePayload;
        return res;
    }

    const String& n = res.normalizedInput;

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

    if (n == "combien de notes" ||
        n == "combien de note" ||
        n == "combien y a t il de notes" ||
        n == "nombre de notes" ||
        n == "combien d observations" ||
        n == "combien d observation") {
        res.type = CommandType::COUNT_NOTES;
        return res;
    }

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

    if (n == "efface toutes les notes" ||
        n == "effacer toutes les notes" ||
        n == "effacez toutes les notes" ||
        n == "efface les notes" ||
        n == "effacer les notes" ||
        n == "effacez les notes" ||
        n == "supprime toutes les notes" ||
        n == "supprimer toutes les notes" ||
        n == "supprimez toutes les notes" ||
        n == "supprime les notes" ||
        n == "supprimer les notes" ||
        n == "supprimez les notes" ||
        n == "vide le carnet" ||
        n == "vider le carnet" ||
        n == "videz le carnet") {
        res.type = CommandType::REQUEST_CLEAR;
        return res;
    }

    if (n == "aide" || n == "aide moi" || n == "commandes") {
        res.type = CommandType::HELP;
        return res;
    }

    res.type = CommandType::UNKNOWN;
    return res;
}

CommandResult CommandRouter::routeIntelligent(const String& rawText, uint32_t nowMs) {
    CommandResult res;
    const bool pendingClear = isClearConfirmationPending(nowMs);
    if (clearPending_ && !pendingClear) {
        clearPending_ = false;
        clearArmedAtMs_ = 0;
    }

    const std::string textStd(rawText.c_str());
    res.parseResult = parser_.parse(textStd, pendingClear);
    res.normalizedInput = String(res.parseResult.normalizedText.c_str());

    using namespace VoiceJournal::Parser;

    switch (res.parseResult.intent) {
        case IntentId::ADD_NOTE:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            if (res.parseResult.observationPayload.empty()) {
                res.type = CommandType::NONE;
            } else {
                res.type = CommandType::ADD_NOTE;
                res.payload = String(res.parseResult.observationPayload.c_str());
            }
            break;

        case IntentId::READ_LAST_NOTE:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::READ_LAST_NOTE;
            break;

        case IntentId::COUNT_NOTES:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::COUNT_NOTES;
            break;

        case IntentId::ANALYZE_NOTES:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::ANALYZE_NOTES;
            break;

        case IntentId::REQUEST_CLEAR:
            // Ne pas confirmer automatiquement si déjà en attente
            res.type = CommandType::REQUEST_CLEAR;
            break;

        case IntentId::CONFIRM_CLEAR:
            if (pendingClear && !res.parseResult.negationDetected) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
                res.type = CommandType::CONFIRM_CLEAR;
            } else {
                res.type = CommandType::UNKNOWN;
            }
            break;

        case IntentId::CANCEL_CLEAR:
            clearPending_ = false;
            clearArmedAtMs_ = 0;
            res.type = CommandType::CANCEL_CLEAR;
            break;

        case IntentId::HELP:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::HELP;
            break;

        case IntentId::SEARCH_NOTES:
        case IntentId::ADD_TASK:
        case IntentId::RESOLVE_PROBLEM:
        case IntentId::LIST_TASKS:
        case IntentId::SEARCH_BY_PROJECT:
        case IntentId::SEARCH_BY_DATE:
        case IntentId::ANALYZE_PROJECT:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::EXPERIMENTAL;
            break;

        case IntentId::NO_ACTION:
            res.type = CommandType::NONE;
            break;

        case IntentId::UNKNOWN:
        case IntentId::AMBIGUOUS:
        default:
            if (pendingClear) {
                clearPending_ = false;
                clearArmedAtMs_ = 0;
            }
            res.type = CommandType::UNKNOWN;
            break;
    }

    return res;
}

CommandResult CommandRouter::routeShadow(const String& rawText, uint32_t nowMs) {
    const bool savedClearPending = clearPending_;
    const uint32_t savedClearArmedAtMs = clearArmedAtMs_;

    // 1. Analyse avec le moteur historique
    CommandResult legacyRes = routeLegacy(rawText, nowMs);

    // 2. Restaurer l'état de confirmation pour une analyse équitable
    clearPending_ = savedClearPending;
    clearArmedAtMs_ = savedClearArmedAtMs;

    // 3. Analyse avec le moteur intelligent
    CommandResult intelRes = routeIntelligent(rawText, nowMs);

    CommandResult res = legacyRes;
    res.legacyType = legacyRes.type;
    res.parseResult = intelRes.parseResult;

    // 4. Comparaison
    if (legacyRes.type == intelRes.type) {
        res.comparisonStatus = "MATCH";
    } else if (legacyRes.type == CommandType::UNKNOWN && intelRes.type == CommandType::EXPERIMENTAL) {
        res.comparisonStatus = "NEW_INTENT";
    } else if (legacyRes.type == CommandType::UNKNOWN && intelRes.type != CommandType::UNKNOWN) {
        res.comparisonStatus = "NEW_INTENT";
    } else {
        res.comparisonStatus = "DIVERGENCE";
    }

    // 5. Règle de sécurité non négociable : blocage de toute action destructive en cas de divergence
    const bool legacyDestructive = (legacyRes.type == CommandType::CONFIRM_CLEAR || legacyRes.type == CommandType::REQUEST_CLEAR);
    const bool intelDestructive = (intelRes.type == CommandType::CONFIRM_CLEAR || intelRes.type == CommandType::REQUEST_CLEAR);

    if (res.comparisonStatus == "DIVERGENCE" && (legacyDestructive || intelDestructive)) {
        res.type = CommandType::UNKNOWN;
        res.blockedByDivergence = true;
        res.comparisonStatus = "SECURITY_BLOCKED";
        cancelClearConfirmation();
    }

    return res;
}

CommandResult CommandRouter::route(const String& rawText, uint32_t nowMs) {
    switch (mode_) {
        case ParserMode::LEGACY:
            return routeLegacy(rawText, nowMs);
        case ParserMode::INTELLIGENT:
            return routeIntelligent(rawText, nowMs);
        case ParserMode::SHADOW:
            return routeShadow(rawText, nowMs);
        default:
            return routeIntelligent(rawText, nowMs);
    }
}

void CommandRouter::logDiagnostics(const CommandResult& res, bool maskPrivatePayload) const {
    const auto& pr = res.parseResult;
    String displayInput = (maskPrivatePayload && res.type == CommandType::ADD_NOTE)
        ? "<OBSERVATION_PRIVEE>"
        : String(pr.originalText.c_str());

    Serial.printf("[VJ][PARSER] Input: %s\n", displayInput.c_str());
    Serial.printf("[VJ][PARSER] Tokens: %u\n", static_cast<unsigned int>(pr.tokens.size()));
    Serial.printf("[VJ][PARSER] Intent: %s\n", VoiceJournal::Parser::intentToString(pr.intent));

    std::string conceptsStr;
    if (pr.primaryConcepts.empty()) {
        conceptsStr = "NONE";
    } else {
        for (size_t i = 0; i < pr.primaryConcepts.size(); ++i) {
            if (i > 0) conceptsStr += ", ";
            conceptsStr += VoiceJournal::Parser::conceptToString(pr.primaryConcepts[i]);
        }
    }
    Serial.printf("[VJ][PARSER] Concepts: %s\n", conceptsStr.c_str());

    if (!pr.entities.empty()) {
        Serial.printf("[VJ][PARSER] Entity: %s\n", pr.entities[0].canonical.c_str());
    }

    Serial.printf("[VJ][PARSER] Score: %u\n", static_cast<unsigned int>(pr.score));
    Serial.printf("[VJ][PARSER] Negation: %s\n", pr.negationDetected ? "true" : "false");
    Serial.printf("[VJ][PARSER] Executable: %s\n", pr.executable ? "true" : "false");

    if (mode_ == ParserMode::SHADOW) {
        Serial.printf("[VJ][PARSER] Legacy: %s\n", commandTypeToString(res.legacyType));
        Serial.printf("[VJ][PARSER] Comparison: %s\n", res.comparisonStatus.c_str());
    }
}
