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

String CommandRouter::normalizeText(const String &input) {
  String out;
  out.reserve(input.length());

  const char *s = input.c_str();
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
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
          appendChar('a');
          break;
        case 0x86:
        case 0xA6:
          appendChar('a');
          appendChar('e');
          break;
        case 0x87:
        case 0xA7:
          appendChar('c');
          break;
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
          appendChar('e');
          break;
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
          appendChar('i');
          break;
        case 0x91:
        case 0xB1:
          appendChar('n');
          break;
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB6:
          appendChar('o');
          break;
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
          appendChar('u');
          break;
        case 0x9D:
        case 0xBD:
        case 0xBF:
          appendChar('y');
          break;
        default:
          appendChar(' ');
          break;
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

bool CommandRouter::extractNotePayload(const String &rawText,
                                       String &outPayload) {
  outPayload = "";
  const char *s = rawText.c_str();
  const size_t len = rawText.length();
  size_t i = 0;

  while (i < len) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' ||
        c == '\'') {
      ++i;
    } else if (c == 0xC2u && (i + 1) < len &&
               (static_cast<uint8_t>(s[i + 1]) == 0xABu ||
                static_cast<uint8_t>(s[i + 1]) == 0xA0u)) {
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
  if (toLowerAscii(s[i]) != 'n' || toLowerAscii(s[i + 1]) != 'o' ||
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
             (static_cast<uint8_t>(s[i + 1]) == 0xA9u ||
              static_cast<uint8_t>(s[i + 1]) == 0x89u)) {
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
    if (b0 == ' ' || b0 == ',' || b0 == ':' || b0 == ';' || b0 == '.' ||
        b0 == '-' || b0 == '\t' || b0 == '\n' || b0 == '\r') {
      return 1;
    }
    if (b0 == 0xC2u && (pos + 1) < len &&
        static_cast<uint8_t>(s[pos + 1]) == 0xA0u) {
      return 2;
    }
    if (b0 == 0xE2u && (pos + 2) < len &&
        static_cast<uint8_t>(s[pos + 1]) == 0x80u) {
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

bool CommandRouter::isExactConfirmPhrase(const String &normalized) {
  if (normalized.indexOf("ne ") >= 0 || normalized.indexOf("n ") >= 0 ||
      normalized.indexOf("pas") >= 0 || normalized.indexOf("jamais") >= 0 ||
      normalized.indexOf("rien") >= 0 || normalized.indexOf("aucun") >= 0 ||
      normalized.indexOf("non") >= 0) {
    return false;
  }

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
      normalized == "oui je confirme" || normalized == "oui confirmer" ||
      normalized == "oui confirmez" || normalized == "oui confirme" ||
      normalized == "oui efface" || normalized == "oui supprime" ||
      normalized == "supprimer definitivement" ||
      normalized == "effacer definitivement" ||
      normalized == "valider la suppression" ||
      normalized == "valide la suppression" || normalized == "je confirme" ||
      normalized == "confirmer" || normalized == "confirmez" ||
      normalized == "confirme") {
    return true;
  }

  return false;
}

CommandResult CommandRouter::routeLegacy(const String &rawText,
                                         uint32_t nowMs) {
  CommandResult res;
  res.normalizedInput = normalizeText(rawText);

  if (res.normalizedInput.length() == 0) {
    res.type = CommandType::NONE;
    res.isExecutable = false;
    return res;
  }

  const bool pendingClear = isClearConfirmationPending(nowMs);
  if (clearPending_ && !pendingClear) {
    clearPending_ = false;
    clearArmedAtMs_ = 0;
  }

  if (pendingClear) {
    if (res.normalizedInput.indexOf("annuler") >= 0 ||
        res.normalizedInput.indexOf("ne supprime pas") >= 0 ||
        res.normalizedInput.indexOf("laisse tomber") >= 0 ||
        res.normalizedInput.indexOf("non") >= 0 ||
        res.normalizedInput.indexOf("pas") >= 0) {
      clearPending_ = false;
      clearArmedAtMs_ = 0;
      res.type = CommandType::CANCEL_CLEAR;
      res.isExecutable = true;
      return res;
    }

    if (isExactConfirmPhrase(res.normalizedInput)) {
      clearPending_ = false;
      clearArmedAtMs_ = 0;
      res.type = CommandType::CONFIRM_CLEAR;
      res.isExecutable = true;
      return res;
    }

    clearPending_ = false;
    clearArmedAtMs_ = 0;
    res.type = CommandType::UNKNOWN;
    res.isExecutable = false;
    return res;
  }

  String payload;
  if (extractNotePayload(rawText, payload)) {
    if (payload.length() == 0) {
      res.type = CommandType::NONE;
      res.isExecutable = false;
      return res;
    }
    res.type = CommandType::ADD_NOTE;
    res.payload = payload;
    res.isExecutable = true;
    return res;
  }

  const String &n = res.normalizedInput;

  if (n == "lis la derniere note" || n == "lire la derniere note" ||
      n == "relis la derniere note" || n == "relire la derniere note" ||
      n == "lis la derniere observation" ||
      n == "lire la derniere observation" || n == "derniere note") {
    res.type = CommandType::READ_LAST_NOTE;
    res.isExecutable = true;
    return res;
  }

  if (n == "combien de notes" || n == "combien de note" ||
      n == "combien y a t il de notes" || n == "nombre de notes" ||
      n == "combien d observations" || n == "combien d observation") {
    res.type = CommandType::COUNT_NOTES;
    res.isExecutable = true;
    return res;
  }

  if (n == "fais le bilan" || n == "faire le bilan" || n == "bilan" ||
      n == "resume les observations" || n == "resumer les observations" ||
      n == "resume les notes" || n == "resumer les notes" ||
      n == "qu avons nous observe" || n == "qu est ce qu on a observe" ||
      n == "analyse les observations" || n == "analyser les observations" ||
      n == "analyse les notes") {
    res.type = CommandType::ANALYZE_NOTES;
    res.isExecutable = true;
    return res;
  }

  if (n == "efface toutes les notes" || n == "effacer toutes les notes" ||
      n == "effacez toutes les notes" || n == "efface les notes" ||
      n == "effacer les notes" || n == "effacez les notes" ||
      n == "supprime toutes les notes" || n == "supprimer toutes les notes" ||
      n == "supprimez toutes les notes" || n == "supprime les notes" ||
      n == "supprimer les notes" || n == "supprimez les notes" ||
      n == "vide le carnet" || n == "vider le carnet" ||
      n == "videz le carnet" || n.startsWith("efface") ||
      n.startsWith("supprime") || n.startsWith("vide")) {
    res.type = CommandType::REQUEST_CLEAR;
    res.isExecutable = true;
    return res;
  }

  if (n == "aide" || n == "aide moi" || n == "commandes") {
    res.type = CommandType::HELP;
    res.isExecutable = true;
    return res;
  }

  if (n == "annule" || n == "annuler" || n == "laisse tomber" ||
      n == "ne supprime pas les notes") {
    res.type = CommandType::CANCEL_CLEAR;
    res.isExecutable = true;
    return res;
  }

  res.type = CommandType::UNKNOWN;
  res.isExecutable = false;
  return res;
}

CommandResult CommandRouter::routeIntelligent(const String &rawText,
                                              uint32_t nowMs) {
  CommandResult res;
  const bool pendingClear = isClearConfirmationPending(nowMs);
  if (clearPending_ && !pendingClear) {
    clearPending_ = false;
    clearArmedAtMs_ = 0;
  }

  const std::string textStd(rawText.c_str());
  res.parseResult = parser_.parse(textStd, pendingClear);
  res.normalizedInput = String(res.parseResult.normalizedText.c_str());
  res.isExecutable = res.parseResult.executable;

  if (structuredMemoryEnabled_) {
    String norm = res.normalizedInput;
    norm.trim();

    if (norm.startsWith("cree le projet ") ||
        norm.startsWith("creer le projet ")) {
      res.type = CommandType::CREATE_PROJECT;
      int idx = rawText.indexOf("projet");
      res.projectName = (idx >= 0) ? rawText.substring(idx + 6) : "";
      res.projectName.trim();
      res.isExecutable = !res.projectName.isEmpty();
      return res;
    }
    if (norm.startsWith("ouvre le projet ") ||
        norm.startsWith("travaille sur le projet ")) {
      res.type = CommandType::OPEN_PROJECT;
      int idx = rawText.indexOf("projet");
      res.projectName = (idx >= 0) ? rawText.substring(idx + 6) : "";
      res.projectName.trim();
      res.isExecutable = !res.projectName.isEmpty();
      return res;
    }
    if (norm == "ferme le projet courant" || norm == "ferme le projet") {
      res.type = CommandType::CLOSE_PROJECT;
      res.isExecutable = true;
      return res;
    }
    if (norm == "quels sont mes projets" || norm == "affiche mes projets" ||
        norm == "liste les projets" || norm == "mes projets") {
      res.type = CommandType::LIST_PROJECTS;
      res.isExecutable = true;
      return res;
    }
    if (norm.startsWith("la tache ") && norm.indexOf("est terminee") != -1) {
      res.type = CommandType::COMPLETE_TASK;
      int start = rawText.indexOf("tache");
      int end = rawText.indexOf("est terminee");
      if (start >= 0 && end > start + 5) {
        res.payload = rawText.substring(start + 5, end);
      } else {
        res.payload = rawText;
      }
      res.payload.trim();
      res.isExecutable = true;
      return res;
    }
    if (norm.startsWith("annule la tache ")) {
      res.type = CommandType::CANCEL_TASK;
      int start = rawText.indexOf("tache");
      res.payload = (start >= 0) ? rawText.substring(start + 5) : "";
      res.payload.trim();
      res.isExecutable = true;
      return res;
    }
  }

  using namespace VoiceJournal::Parser;

  // COR-11 : Validation centralisée des actions
  // 1. Annulation de sécurité toujours possible et prioritaire, même en cas de
  // statut invalide
  if (res.parseResult.intent == IntentId::CANCEL_CLEAR) {
    cancelClearConfirmation();
    res.type = CommandType::CANCEL_CLEAR;
    res.isExecutable = true;
    return res;
  }

  // 2. Rejet strict si ambiguïté détectée
  if (res.parseResult.ambiguous || res.parseResult.ambiguityDetected ||
      res.parseResult.status == ParseStatus::AMBIGUOUS) {
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::UNKNOWN;
    res.isExecutable = false;
    return res;
  }

  // 3. Statut invalide, entrée tronquée/rejetée, ou score inférieur au seuil :
  // aucune action métier autorisée
  if (res.parseResult.status != ParseStatus::OK ||
      res.parseResult.score < res.parseResult.acceptanceThreshold) {
    if (pendingClear) {
      cancelClearConfirmation();
    }
    if (res.parseResult.status == ParseStatus::PAYLOAD_MISSING) {
      res.type = CommandType::NONE;
    } else {
      res.type = CommandType::UNKNOWN;
    }
    res.isExecutable = false;
    return res;
  }

  // 4. Intentions expérimentales si mémoire structurée désactivée (v0.2.1)
  if (!structuredMemoryEnabled_) {
    switch (res.parseResult.intent) {
    case IntentId::SEARCH_NOTES:
    case IntentId::ADD_TASK:
    case IntentId::RESOLVE_PROBLEM:
    case IntentId::LIST_TASKS:
    case IntentId::SEARCH_BY_PROJECT:
    case IntentId::SEARCH_BY_DATE:
    case IntentId::ANALYZE_PROJECT:
      if (pendingClear) {
        cancelClearConfirmation();
      }
      res.type = CommandType::EXPERIMENTAL;
      res.isExecutable = false;
      return res;
    default:
      break;
    }
  } else {
    // En mode v0.3.0 mémoire structurée activée
    switch (res.parseResult.intent) {
    case IntentId::ADD_TASK:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::ADD_TASK;
      res.entryType = VoiceJournal::EntryType::TASK;
      res.payload = res.parseResult.observationPayload.empty()
                        ? rawText
                        : String(res.parseResult.observationPayload.c_str());
      res.isExecutable = true;
      return res;

    case IntentId::LIST_TASKS:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::LIST_TASKS;
      res.isExecutable = true;
      return res;

    case IntentId::RESOLVE_PROBLEM:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::RESOLVE_PROBLEM;
      res.payload = String(res.parseResult.observationPayload.c_str());
      res.isExecutable = true;
      return res;

    case IntentId::SEARCH_NOTES:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::SEARCH_NOTES;
      res.payload = String(res.parseResult.observationPayload.c_str());
      res.isExecutable = true;
      return res;

    case IntentId::SEARCH_BY_PROJECT:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::SEARCH_BY_PROJECT;
      res.projectName = String(res.parseResult.semanticFrame.entity.c_str());
      res.isExecutable = true;
      return res;

    case IntentId::SEARCH_BY_DATE:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::SEARCH_BY_DATE;
      res.dateExpr = String(res.parseResult.semanticFrame.relativeDate.c_str());
      res.isExecutable = true;
      return res;

    case IntentId::ANALYZE_PROJECT:
      if (pendingClear)
        cancelClearConfirmation();
      res.type = CommandType::ANALYZE_PROJECT;
      res.projectName = String(res.parseResult.semanticFrame.entity.c_str());
      res.isExecutable = true;
      return res;

    default:
      break;
    }
  }

  // 5. Gestion des commandes opérationnelles autorisées
  switch (res.parseResult.intent) {
  case IntentId::ADD_NOTE:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    if (res.parseResult.observationPayload.empty()) {
      res.type = CommandType::NONE;
      res.isExecutable = false;
    } else {
      res.type = CommandType::ADD_NOTE;
      res.payload = String(res.parseResult.observationPayload.c_str());
      res.isExecutable = true;

      if (structuredMemoryEnabled_) {
        String normPayload = normalizeText(res.payload);
        if (normPayload.startsWith("probleme") ||
            normPayload.startsWith("bug") ||
            normPayload.startsWith("enregistre un probleme") ||
            normPayload.indexOf("se deconnecte") != -1) {
          res.entryType = VoiceJournal::EntryType::PROBLEM;
        } else if (normPayload.startsWith("tache") ||
                   normPayload.startsWith("je dois") ||
                   normPayload.startsWith("ajoute une tache")) {
          res.entryType = VoiceJournal::EntryType::TASK;
        } else if (normPayload.startsWith("idee") ||
                   normPayload.startsWith("j ai une idee")) {
          res.entryType = VoiceJournal::EntryType::IDEA;
        } else if (normPayload.startsWith("decision") ||
                   normPayload.startsWith("on decide")) {
          res.entryType = VoiceJournal::EntryType::DECISION;
        } else if (normPayload.startsWith("resultat") ||
                   normPayload.startsWith("mesure")) {
          res.entryType = VoiceJournal::EntryType::RESULT;
        } else {
          res.entryType = VoiceJournal::EntryType::NOTE;
        }
      }
    }
    break;

  case IntentId::READ_LAST_NOTE:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::READ_LAST_NOTE;
    res.isExecutable = true;
    break;

  case IntentId::COUNT_NOTES:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::COUNT_NOTES;
    res.isExecutable = true;
    break;

  case IntentId::ANALYZE_NOTES:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::ANALYZE_NOTES;
    res.isExecutable = true;
    break;

  case IntentId::REQUEST_CLEAR:
    // Sécurité destructive : refus si fuzzy matching ou négation
    if (res.parseResult.fuzzyMatchUsed || res.parseResult.negationDetected) {
      if (pendingClear) {
        cancelClearConfirmation();
      }
      res.type = CommandType::UNKNOWN;
      res.isExecutable = false;
      break;
    }
    res.type = CommandType::REQUEST_CLEAR;
    res.isExecutable = true;
    break;

  case IntentId::CONFIRM_CLEAR:
    if (pendingClear && !res.parseResult.negationDetected &&
        !res.parseResult.fuzzyMatchUsed && res.parseResult.executable) {
      cancelClearConfirmation();
      res.type = CommandType::CONFIRM_CLEAR;
      res.isExecutable = true;
    } else {
      if (pendingClear) {
        cancelClearConfirmation();
      }
      res.type = CommandType::UNKNOWN;
      res.isExecutable = false;
    }
    break;

  case IntentId::CANCEL_CLEAR:
    cancelClearConfirmation();
    res.type = CommandType::CANCEL_CLEAR;
    res.isExecutable = true;
    break;

  case IntentId::HELP:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::HELP;
    res.isExecutable = true;
    break;

  case IntentId::NO_ACTION:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::NONE;
    res.isExecutable = false;
    break;

  case IntentId::UNKNOWN:
  default:
    if (pendingClear) {
      cancelClearConfirmation();
    }
    res.type = CommandType::UNKNOWN;
    res.isExecutable = false;
    break;
  }

  return res;
}

CommandResult CommandRouter::routeShadow(const String &rawText,
                                         uint32_t nowMs) {
  const bool savedClearPending = clearPending_;
  const uint32_t savedClearArmedAtMs = clearArmedAtMs_;

  // 1. Analyse avec le moteur historique
  CommandResult legacyRes = routeLegacy(rawText, nowMs);
  const bool legacyPendingAfter = clearPending_;

  // 2. Restaurer l'état de confirmation initial pour une analyse miroir
  // équitable
  clearPending_ = savedClearPending;
  clearArmedAtMs_ = savedClearArmedAtMs;

  // 3. Analyse avec le moteur contextuel intelligent
  CommandResult intelRes = routeIntelligent(rawText, nowMs);
  const bool intelPendingAfter = clearPending_;

  // 4. L'état effectif de confirmation suit le moteur historique en mode SHADOW
  clearPending_ = legacyPendingAfter;
  clearArmedAtMs_ = savedClearArmedAtMs;

  CommandResult res = legacyRes;
  res.legacyType = legacyRes.type;
  res.parseResult = intelRes.parseResult;
  res.isExecutable = intelRes.isExecutable;

  // 5. Comparateur SHADOW complet (COR-09, COR-15)
  const bool legacyDestructive =
      (legacyRes.type == CommandType::CONFIRM_CLEAR ||
       legacyRes.type == CommandType::REQUEST_CLEAR);
  const bool intelDestructive = (intelRes.type == CommandType::CONFIRM_CLEAR ||
                                 intelRes.type == CommandType::REQUEST_CLEAR);

  // Priorité absolue : Sécurité destructive (SECURITY_BLOCKED)
  if ((legacyDestructive || intelDestructive) &&
      (legacyRes.type != intelRes.type ||
       legacyRes.isExecutable != intelRes.isExecutable)) {
    res.type = CommandType::UNKNOWN;
    res.isExecutable = false;
    res.blockedByDivergence = true;
    res.shadowStatus = ShadowStatus::SECURITY_BLOCKED;
    res.comparisonStatus = shadowStatusToString(ShadowStatus::SECURITY_BLOCKED);
    cancelClearConfirmation();
    return res;
  }

  if (legacyRes.type == intelRes.type) {
    // Même type : vérifier divergences d'autorisation, de payload ou d'état
    if (legacyRes.isExecutable != intelRes.isExecutable) {
      res.shadowStatus = ShadowStatus::ACTION_DIVERGENCE;
      res.comparisonStatus =
          shadowStatusToString(ShadowStatus::ACTION_DIVERGENCE);
    } else if (legacyRes.type == CommandType::ADD_NOTE &&
               legacyRes.payload != intelRes.payload) {
      res.shadowStatus = ShadowStatus::PAYLOAD_DIVERGENCE;
      res.comparisonStatus =
          shadowStatusToString(ShadowStatus::PAYLOAD_DIVERGENCE);
    } else if (legacyPendingAfter != intelPendingAfter) {
      res.shadowStatus = ShadowStatus::STATE_DIVERGENCE;
      res.comparisonStatus =
          shadowStatusToString(ShadowStatus::STATE_DIVERGENCE);
    } else {
      res.shadowStatus = ShadowStatus::MATCH;
      res.comparisonStatus = shadowStatusToString(ShadowStatus::MATCH);
    }
  } else if (legacyPendingAfter != intelPendingAfter) {
    res.shadowStatus = ShadowStatus::STATE_DIVERGENCE;
    res.comparisonStatus = shadowStatusToString(ShadowStatus::STATE_DIVERGENCE);
  } else if (legacyRes.type == CommandType::UNKNOWN &&
             (intelRes.type == CommandType::EXPERIMENTAL ||
              intelRes.type != CommandType::UNKNOWN)) {
    res.shadowStatus = ShadowStatus::NEW_INTENT;
    res.comparisonStatus = shadowStatusToString(ShadowStatus::NEW_INTENT);
  } else {
    res.shadowStatus = ShadowStatus::ACTION_DIVERGENCE;
    res.comparisonStatus =
        shadowStatusToString(ShadowStatus::ACTION_DIVERGENCE);
  }

  return res;
}

CommandResult CommandRouter::route(const String &rawText, uint32_t nowMs) {
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

void CommandRouter::logDiagnostics(const CommandResult &res,
                                   bool maskPrivatePayload) const {
  const auto &pr = res.parseResult;
  String displayInput =
      (maskPrivatePayload && res.type == CommandType::ADD_NOTE)
          ? "<OBSERVATION_PRIVEE>"
          : String(pr.originalText.c_str());

  Serial.printf("[VJ][PARSER] Input: %s\n", displayInput.c_str());
  Serial.printf("[VJ][PARSER] Tokens: %u\n",
                static_cast<unsigned int>(pr.tokens.size()));
  Serial.printf("[VJ][PARSER] Intent: %s\n",
                VoiceJournal::Parser::intentToString(pr.intent));

  const auto &fr = pr.semanticFrame;
  Serial.printf(
      "[VJ][PARSER] Frame: act=%s obj=%s tgt=%s ent=%s date=%s neg=%s\n",
      fr.action.c_str(), fr.object.c_str(),
      VoiceJournal::Parser::targetCategoryToString(fr.targetCategory),
      fr.entity.c_str(), fr.relativeDate.c_str(), fr.negationScope.c_str());

  Serial.printf("[VJ][PARSER] Score: %u (raw=%u)\n",
                static_cast<unsigned int>(pr.score),
                static_cast<unsigned int>(pr.rawScore));
  Serial.printf("[VJ][PARSER] Ambiguous: %s\n",
                pr.ambiguityDetected ? "true" : "false");
  Serial.printf("[VJ][PARSER] Executable: %s\n",
                pr.executable ? "true" : "false");

  if (mode_ == ParserMode::SHADOW) {
    Serial.printf("[VJ][PARSER] Legacy: %s\n",
                  commandTypeToString(res.legacyType));
    Serial.printf("[VJ][PARSER] Comparison: %s\n",
                  res.comparisonStatus.c_str());
  }
}
