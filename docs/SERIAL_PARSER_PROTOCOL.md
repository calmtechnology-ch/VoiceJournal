# Protocole de Test Série USB — VoiceJournal v0.2.1 (Section 17)

## 1. Présentation

VoiceJournal v0.2.1 intègre un mode de diagnostic et d'évaluation directe via la liaison série USB (port CDC natif ESP32-S3 / CP2104).

Ce mode permet aux développeurs et aux bancs de test automatisés d'injecter du texte directement dans le moteur de parsing contextuel **sans passer par la capture microphone ni la transcription vocale (STT)**, sans latence réseau et **sans aucun effet de bord métier** (aucun enregistrement en NVS, aucun appel aux API d'IA, aucune modification de session vocale).

---

## 2. Configuration Port Série

- **Débit** : 115 200 bauds
- **Bits de données** : 8
- **Parité** : Aucune
- **Bits de stop** : 1
- **Contrôle de flux** : Aucun
- **Fin de ligne** : `\n` (LF) ou `\r\n` (CRLF)
- **Capacité tampon d'entrée** : 1 152 octets (aligné sur la limite max de 1 024 octets de texte avec préfixe de commande). En cas de débordement, le reste de la ligne est vidé sans déclencher de commande partielle.

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
  "intent": "ADD_TASK",
  "status": "OK",
  "action": "tester",
  "entity": "Gemini",
  "relativeDate": "demain",
  "executable": false
}
```

---

### 3.2 Commande `PARSE_SHADOW <texte>`
Soumet une phrase au comparateur miroir SHADOW (moteur historique v0.1.1 vs moteur contextuel v0.2.1).

**Syntaxe :**
```text
PARSE_SHADOW <texte de la commande>
```

**Exemple :**
```text
PARSE_SHADOW Lis la derniere note
```

**Réponse :**
```json
{
  "command": "PARSE_SHADOW",
  "input": "Lis la derniere note",
  "shadowStatus": "MATCH",
  "legacyType": "READ_LAST",
  "intelIntent": "READ_LAST_NOTE",
  "executable": true
}
```

---

### 3.3 Commande `PARSER_STATUS`
Retourne l'état opérationnel et l'empreinte mémoire du système embarqué.

**Syntaxe :**
```text
PARSER_STATUS
```

**Réponse :**
```json
{
  "command": "PARSER_STATUS",
  "version": "0.2.1",
  "mode": "INTELLIGENT",
  "freeHeap": 178240,
  "maxAllocHeap": 114676,
  "freePsram": 16645312,
  "rulesCount": 19,
  "phrasesCount": 51
}
```

---

### 3.4 Commande `PARSER_HELP`
Retourne la liste des commandes série disponibles.

**Syntaxe :**
```text
PARSER_HELP
```

**Réponse :**
```json
{
  "command": "PARSER_HELP",
  "commands": [
    "PARSE <texte>",
    "PARSE_SHADOW <texte>",
    "PARSER_STATUS",
    "PARSER_HELP"
  ]
}
```

---

### 3.5 Gestion des Erreurs et Débordements

En cas de commande non reconnue ou de dépassement de capacité :

```json
{
  "command": "ERROR",
  "status": "INPUT_TOO_LONG",
  "message": "Ligne serie depassant la capacite maximale (1152)"
}
```
ou :
```json
{
  "command": "ERROR",
  "status": "UNKNOWN_COMMAND",
  "message": "Commande inconnue : FOOBAR"
}
```
