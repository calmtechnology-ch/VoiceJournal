#include "parser/TextNormalizer.h"
#include <cstdint>
#include <cctype>

namespace VoiceJournal::Parser {

NormalizedOutput TextNormalizer::normalize(const std::string& input) {
    NormalizedOutput out;
    const size_t len = input.length();
    if (len == 0) {
        return out;
    }

    out.normalized.reserve(len);
    out.normToOrigStart.reserve(len);
    out.normToOrigEnd.reserve(len);

    bool lastWasSpace = true; // Pour supprimer les espaces de tête

    auto appendChar = [&](char c, size_t origStart, size_t origEnd) {
        if (c == ' ') {
            if (!lastWasSpace) {
                out.normalized.push_back(' ');
                out.normToOrigStart.push_back(origStart);
                out.normToOrigEnd.push_back(origEnd);
                lastWasSpace = true;
            }
        } else {
            out.normalized.push_back(c);
            out.normToOrigStart.push_back(origStart);
            out.normToOrigEnd.push_back(origEnd);
            lastWasSpace = false;
        }
    };

    size_t i = 0;
    while (i < len) {
        const uint8_t b0 = static_cast<uint8_t>(input[i]);

        // 1. ASCII standard
        if (b0 < 0x80u) {
            const size_t start = i;
            const size_t end = i + 1;
            if (b0 >= 'A' && b0 <= 'Z') {
                appendChar(static_cast<char>(b0 - 'A' + 'a'), start, end);
            } else if ((b0 >= 'a' && b0 <= 'z') || (b0 >= '0' && b0 <= '9')) {
                appendChar(static_cast<char>(b0), start, end);
            } else {
                // Ponctuation, apostrophe, tiret, espace -> espace
                appendChar(' ', start, end);
            }
            ++i;
            continue;
        }

        // 2. Séquences UTF-8 sur 2 octets
        if ((b0 == 0xC3u || b0 == 0xC5u || b0 == 0xC2u) && (i + 1 < len)) {
            const uint8_t b1 = static_cast<uint8_t>(input[i + 1]);
            const size_t start = i;
            const size_t end = i + 2;

            if (b0 == 0xC3u) {
                switch (b1) {
                    // A / a avec accents : À Á Â Ã Ä Å / à á â ã ä å
                    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                        appendChar('a', start, end); break;
                    // Æ / æ
                    case 0x86: case 0xA6:
                        appendChar('a', start, end);
                        appendChar('e', start, end); break;
                    // Ç / ç
                    case 0x87: case 0xA7:
                        appendChar('c', start, end); break;
                    // È É Ê Ë / è é ê ë
                    case 0x88: case 0x89: case 0x8A: case 0x8B:
                    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                        appendChar('e', start, end); break;
                    // Ì Í Î Ï / ì í î ï
                    case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                        appendChar('i', start, end); break;
                    // Ñ / ñ
                    case 0x91: case 0xB1:
                        appendChar('n', start, end); break;
                    // Ò Ó Ô Õ Ö / ò ó ô õ ö
                    case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                        appendChar('o', start, end); break;
                    // Ù Ú Û Ü / ù ú û ü
                    case 0x99: case 0x9A: case 0x9B: case 0x9C:
                    case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                        appendChar('u', start, end); break;
                    // Ý / ý / ÿ
                    case 0x9D: case 0xBD: case 0xBF:
                        appendChar('y', start, end); break;
                    default:
                        appendChar(' ', start, end); break;
                }
            } else if (b0 == 0xC5u) {
                // œ / Œ
                if (b1 == 0x92 || b1 == 0x93) {
                    appendChar('o', start, end);
                    appendChar('e', start, end);
                } else {
                    appendChar(' ', start, end);
                }
            } else if (b0 == 0xC2u) {
                // Espace insécable (0xA0), guillemets français « (0xAB) » (0xBB)
                appendChar(' ', start, end);
            }
            i += 2;
            continue;
        }

        // 3. Séquences UTF-8 sur 3 octets (ex: apostrophe typographique ’ 0xE2 0x80 0x99)
        if (b0 == 0xE2u && (i + 2 < len)) {
            const size_t start = i;
            const size_t end = i + 3;
            appendChar(' ', start, end);
            i += 3;
            continue;
        }

        // 4. Autre caractère UTF-8 (sur 3 ou 4 octets) ou octet invalide
        size_t seqLen = 1;
        if ((b0 & 0xE0u) == 0xC0u) seqLen = 2;
        else if ((b0 & 0xF0u) == 0xE0u) seqLen = 3;
        else if ((b0 & 0xF8u) == 0xF0u) seqLen = 4;

        if (i + seqLen > len) {
            seqLen = 1; // Octet tronqué/invalide
        }
        appendChar(' ', i, i + seqLen);
        i += seqLen;
    }

    // Supprimer l'espace final éventuel
    while (!out.normalized.empty() && out.normalized.back() == ' ') {
        out.normalized.pop_back();
        out.normToOrigStart.pop_back();
        out.normToOrigEnd.pop_back();
    }

    return out;
}

std::string TextNormalizer::normalizeSimple(const std::string& input) {
    return normalize(input).normalized;
}

std::string TextNormalizer::trim(const std::string& str) {
    size_t first = 0;
    while (first < str.size() && static_cast<unsigned char>(str[first]) <= ' ') {
        ++first;
    }
    if (first == str.size()) return "";
    size_t last = str.size() - 1;
    while (last > first && static_cast<unsigned char>(str[last]) <= ' ') {
        --last;
    }
    return str.substr(first, last - first + 1);
}

std::string TextNormalizer::trimOriginalPayload(const std::string& original, size_t startOffset) {
    if (startOffset >= original.size()) {
        return "";
    }
    size_t i = startOffset;
    // Ignorer les espaces, espaces insécables et signes de ponctuation séparateurs au début
    while (i < original.size()) {
        const uint8_t b0 = static_cast<uint8_t>(original[i]);
        if (b0 == ' ' || b0 == '\t' || b0 == ',' || b0 == ':' || b0 == '-' || b0 == ';' || b0 == '.' || b0 == '\r' || b0 == '\n') {
            ++i;
        } else if (b0 == 0xC2u && (i + 1 < original.size()) && static_cast<uint8_t>(original[i + 1]) == 0xA0u) {
            i += 2; // Espace insécable UTF-8
        } else {
            break;
        }
    }
    if (i >= original.size()) {
        return "";
    }

    // Trouver la fin en nettoyant les espaces de queue
    size_t end = original.size();
    while (end > i) {
        const char c = original[end - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            --end;
        } else {
            break;
        }
    }
    return original.substr(i, end - i);
}

} // namespace VoiceJournal::Parser
