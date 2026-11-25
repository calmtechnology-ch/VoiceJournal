# Protocole de Test et Diagnostic Série USB — VoiceJournal v0.3.0

## 1. Présentation

VoiceJournal v0.3.0 intègre un protocole série étendu via le port USB (CDC natif ESP32-S3).

Ce mode permet aux développeurs, bancs de test et futures interfaces compagnon PC de :
1. Tester le moteur de compréhension du langage naturel (NLU) sans passer par l'audio (`PARSE`, `PARSE_SHADOW`).
2. Consulter l'état de la mémoire structurée en temps réel (`JOURNAL_STATUS`).
3. Interroger les entrées (`GET_ENTRY <id>`), projets (`LIST_PROJECTS`) et tâches (`LIST_TASKS`).
4. Exporter l'intégralité du carnet au format JSON (`EXPORT_JOURNAL`).
5. Obtenir l'aide des commandes série (`PARSER_HELP`).

---

## 2. Configuration Port Série

- **Débit** : 115 200 bauds
- **Format** : 8 bits, sans parité, 1 bit de stop (8N1)
- **Fin de ligne** : `\n` (LF) ou `\r\n` (CRLF)
- **Capacité tampon d'entrée** : 1 152 octets avec purge automatique de ligne en cas de débordement.

---

## 3. Commandes Disponibles

### 3.1 `PARSE <texte>`
Évalue une phrase textuelle dans le moteur NLU sans effet de bord audio.
```text
PARSE Je dois tester Gemini demain
```
**Réponse JSON :**
```json
{
  "command": "PARSE",
  "input": "Je dois tester Gemini demain",
  "intent": "ADD_TASK",
  "status": "OK",
  "action": "tester",
  "entity": "Gemini",
  "relativeDate": "demain",
  "executable": true
}
```

### 3.2 `PARSE_SHADOW <texte>`
Compare l'analyse en miroir entre l'ancien moteur legacy et le moteur contextuel.

### 3.3 `JOURNAL_STATUS`
Retourne les métriques complètes de la mémoire structurée LittleFS.
**Exemple de réponse :**
```json
{
  "command": "JOURNAL_STATUS",
  "status": "OK",
  "entriesCount": 42,
  "maxCapacity": 1000,
  "projectsCount": 3,
  "openTasksCount": 5,
  "openProblemsCount": 2,
  "usedBytes": 12840,
  "totalBytes": 9437184,
  "timeReliable": true,
  "currentUtc": 1795617000
}
```

### 3.4 `GET_ENTRY <id>`
Récupère une entrée précise par son identifiant persistant.
```text
GET_ENTRY 42
```
**Exemple de réponse :**
```json
{
  "command": "GET_ENTRY",
  "status": "OK",
  "entry": {
    "id": 42,
    "type": "PROBLEM",
    "projectId": 3,
    "createdAt": 1795617000,
    "updatedAt": 0,
    "status": "OPEN",
    "dueAt": 0,
    "text": "Gemini se déconnecte après quelques secondes.",
    "entities": ["Gemini", "CoreS3"],
    "tags": ["connexion"]
  }
}
```

### 3.5 `LIST_PROJECTS`
Liste tous les projets actifs ou archivés enregistrés en mémoire.
```json
{
  "command": "LIST_PROJECTS",
  "status": "OK",
  "count": 2,
  "projects": [
    {"id": 1, "name": "VoiceKit", "status": "ACTIVE", "createdAt": 1795600000, "aliases": ["vk", "voice-kit"]},
    {"id": 2, "name": "RuleBot", "status": "ACTIVE", "createdAt": 1795610000, "aliases": []}
  ]
}
```

### 3.6 `LIST_TASKS`
Liste les tâches ouvertes en mémoire structurée.
```json
{
  "command": "LIST_TASKS",
  "status": "OK",
  "count": 1,
  "tasks": [
    {"id": 12, "text": "Tester la connexion WebSocket", "projectId": 1, "status": "OPEN", "dueAt": 1795703400}
  ]
}
```

### 3.7 `EXPORT_JOURNAL`
Exporte l'ensemble des entrées du journal au format JSON pour sauvegarde ou analyse externe sur PC.

### 3.8 `PARSER_STATUS` et `PARSER_HELP`
Affiche le mode actif du parseur (`INTELLIGENT`, `SHADOW`, `LEGACY`) et la liste d'aide des commandes série.
