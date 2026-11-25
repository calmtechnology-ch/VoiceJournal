# Protocole de Test Série USB — VoiceJournal v0.2.0.1 (Section 17)

## 1. Présentation

VoiceJournal v0.2.0.1 intègre un mode de diagnostic et d'évaluation directe via la liaison série USB (port CDC natif ESP32-S3 / CP2104).

Ce mode permet aux développeurs et aux bancs de test automatisés d'injecter du texte directement dans le moteur de parsing contextuel **sans passer par la capture microphone ni la transcription vocale (STT)**, sans latence réseau et **sans aucun effet de bord métier** (aucun enregistrement en NVS, aucun appel aux API d'IA).

---

## 2. Configuration Port Série

- **Débit** : 115 200 bauds
- **Bits de données** : 8
- **Parité** : Aucune
- **Bits de stop** : 1
- **Contrôle de flux** : Aucun
- **Fin de ligne** : `\n` (LF) ou `\r\n` (CRLF)

---

## 3. Commandes Disponibles

### 3.1 Commande `PARSE <texte>`
Soumet une phrase textuelle au moteur contextuel en mode `INTELLIGENT`.

**Syntaxe :**
```text
PARSE <texte de la commande>
```

**Exemple :**
```text
PARSE Je dois tester Gemini demain
```

**Réponse structurée retournée :**
```json
{
  "command": "PARSE",
  "input": "Je dois tester Gemini demain",
  "normalized": "je dois tester gemini demain",
  "intent": "ADD_TASK",
  "executable": false,
  "status": "OK",
  "scores": {
    "raw": 94,
    "adjusted": 94,
    "fuzzy_penalty": 0
  },
  "rule": "task_obligation",
  "roles": {
    "je": "SUBJECT",
    "dois": "OBLIGATION",
    "tester": "ACTION",
    "gemini": "ENTITY",
    "demain": "TIME"
  },
  "frame": {
    "subject": "je",
    "modality": "obligation",
    "action": "tester",
    "object": "Gemini",
    "target": "UNKNOWN_OBJECT",
    "entity": "Gemini",
    "relative_date": "demain",
    "negation_scope": "none",
    "global_scope": false
  },
  "slots": {
    "entity": "Gemini",
    "date_relative": "demain"
  },
  "duration_us": 216
}
```

---

### 3.2 Commande `PARSE_SHADOW <texte>`
Soumet une phrase au comparateur miroir SHADOW (moteur historique v0.1.1 vs moteur contextuel v0.2.0.1).

**Syntaxe :**
```text
PARSE_SHADOW <texte de la commande>
```

**Exemple 1 (Nouvelle intention) :**
```text
PARSE_SHADOW Retrouve les bugs de VoiceKit
```

**Réponse :**
```json
{
  "command": "PARSE_SHADOW",
  "input": "Retrouve les bugs de VoiceKit",
  "shadow_status": "NEW_INTENT",
  "legacy_type": "UNKNOWN",
  "intel_intent": "SEARCH_NOTES",
  "blocked_by_divergence": false,
  "duration_us": 347
}
```

**Exemple 2 (Sécurité destructive bloquée) :**
```text
PARSE_SHADOW Efface le fichier de configuration
```

**Réponse :**
```json
{
  "command": "PARSE_SHADOW",
  "input": "Efface le fichier de configuration",
  "shadow_status": "SECURITY_BLOCKED",
  "legacy_type": "REQUEST_CLEAR",
  "intel_intent": "UNKNOWN",
  "blocked_by_divergence": true,
  "executed_type": "UNKNOWN",
  "duration_us": 312
}
```

---

### 3.3 Commande `PARSER_STATUS`
Retourne l'état opérationnel et l'empreinte mémoire du moteur de parsing.

**Syntaxe :**
```text
PARSER_STATUS
```

**Réponse :**
```json
{
  "version": "0.2.0.1",
  "mode": "INTELLIGENT",
  "lexicon_entries": 231,
  "phrase_entries": 51,
  "grammar_rules": 19,
  "max_tokens": 96,
  "max_input_bytes": 1024,
  "free_heap_bytes": 241404,
  "uptime_ms": 128450
}
```

---

### 3.4 Commande `PARSER_HELP`
Affiche le résumé d'aide du protocole série en texte clair.

**Syntaxe :**
```text
PARSER_HELP
```

---

## 4. Garanties d'Isolation et de Sécurité

1. **Aucun effet de bord métier** : Les commandes exécutées via `PARSE` ou `PARSE_SHADOW` n'arment jamais la confirmation réelle de suppression de l'appareil (`clearPending_` n'est pas modifié).
2. **Pas d'interaction audio** : Le haut-parleur, le microphone et le module audio VoiceKit restent inactifs.
3. **Zéro consommation de quota API** : Aucune communication réseau n'est initiée avec OpenAI ou Google Gemini.
4. **Idempotence** : L'exécution répétée de commandes via le protocole série ne modifie pas l'état du carnet stocké en NVS.
