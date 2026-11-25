# Référence de la Grammaire Sémantique et Contextuelle — VoiceJournal v0.2.0.1

Ce document détaille les concepts, rôles sémantiques, catégories d'objets et règles de grammaire contextuelle régissant l'analyse des commandes dans VoiceJournal v0.2.0.1.

---

## 1. Rôles Sémantiques (`SemanticRole`, COR-01)

Chaque mot ou groupe de mots se voit attribuer un rôle parmi les 12 rôles sémantiques suivants :

```cpp
enum class SemanticRole : uint8_t {
    UNKNOWN = 0,
    SUBJECT,       // Pronoms sujets : "je", "tu", "il", "elle", "on", "nous", "vous", "ils"
    ACTION,        // Verbes d'action principaux : "tester", "supprimer", "lire", "compter", "analyser"
    OBJECT,        // Cibles directes de l'action : "notes", "observations", "carnet", "fichier"
    ENTITY,        // Entités nommées techniques : "Gemini", "VoiceKit", "RuleBot", "CoreS3", "OpenAI"
    QUANTIFIER,    // Quantifieurs : "toutes", "tous", "tout", "chaque", "1", "2"
    QUALIFIER,     // Qualificatifs : "derniere", "precedente", "premier"
    TIME,          // Marqueurs temporels : "demain", "hier", "ce matin", "ce soir", "cette semaine"
    NEGATION,      // Marqueurs de négation : "ne", "pas", "non", "jamais", "rien", "aucun"
    OBLIGATION,    // Modalité d'obligation : "dois", "il faut que", "penser a", "rappelle-moi de"
    QUESTION,      // Mots interrogatifs : "combien", "comment", "pourquoi", "quel"
    CONNECTOR,     // Connecteurs grammaticaux : "de", "du", "des", "a", "au", "sur", "avec", "et"
    PAYLOAD        // Contenu dicté brut d'une observation
};
```

---

## 2. Catégories de Cibles (`TargetCategory`, COR-05)

Pour garantir la sécurité et la précision des actions, les objets sont classifiés en 8 catégories :

```cpp
enum class TargetCategory : uint8_t {
    UNKNOWN_OBJECT = 0, // Objet non répertorié ou absent
    JOURNAL,            // Carnet global : "le carnet", "le journal"
    JOURNAL_NOTE,       // Notes du carnet : "les notes", "les observations", "la derniere note"
    TASK,               // Tâches à accomplir : "la tache", "les taches", "todo"
    PROJECT,            // Projets logiciels : "le projet VoiceKit", "RuleBot"
    PROBLEM,            // Incidents / Anomalies : "les bugs", "les problemes", "les pannes"
    FILE,               // Fichiers système : "le fichier", "les logs", "config"
    DEVICE              // Matériel physique : "le CoreS3", "l'ecran", "le micro"
};
```

---

## 3. Cadre Sémantique (`SemanticFrame`)

La structure intermédiaire `SemanticFrame` synthétise la compréhension contextuelle de la phrase :

```cpp
struct SemanticFrame {
    std::string subject;            // Ex: "je", "nous", "on"
    std::string modality;           // "obligation", "question", "command"
    std::string action;             // Ex: "tester", "effacer", "lis", "compte"
    std::string object;             // Ex: "les notes", "fichier", "bugs"
    TargetCategory targetCategory;  // Catégorie d'objet validée
    std::string entity;             // Ex: "Gemini", "VoiceKit", "RuleBot"
    std::string relativeDate;       // Ex: "demain", "ce matin", "hier"
    std::string negationScope;      // "none", "command", "payload", "obligation", "confirmation"
    bool hasGlobalScope;            // true si "toutes les notes", "le carnet", "les notes"
    std::string matchedPhrase;      // Phrase principale reconnue
};
```

---

## 4. Portée des Négations (`negationScope`, COR-06)

Le moteur classe toute présence de négation selon sa portée exacte :

| Portée (`negationScope`) | Définition | Exemple | Résultat Intent |
|---|---|---|---|
| `none` | Aucune négation ou locution idiomatique positive | *"N'oublie pas de tester Gemini"* | `ADD_TASK` |
| `command` | Négation s'appliquant à l'action principale | *"Ne supprime pas les notes"* | `CANCEL_CLEAR` |
| `payload` | Négation isolée à l'intérieur du texte dicté | *"Note, ne supprime pas les fichiers"* | `ADD_NOTE` |
| `obligation` | Négation de l'obligation modale | *"Je ne dois pas tester Gemini"* | `UNKNOWN` (pas de tâche) |
| `confirmation` | Négation formelle d'une confirmation de suppression | *"Je ne confirme pas la suppression"* | `CANCEL_CLEAR` |

---

## 5. Locutions Multi-Mots Reconnues (`rules/phrases.json`, COR-02)

Une locution multi-mots (*longest-match*) forme une unité sémantique indissociable. 51 locutions sont déclarées, dont :

- **Obligations positives** : `"il faut que"`, `"il faut"`, `"penser a"`, `"pense a"`, `"ne pas oublier de"`, `"n'oublie pas de"`, `"rappelle-moi de"`, `"prevoir de"`.
- **Cibles du carnet avec portée globale** : `"toutes les notes"`, `"toutes les observations"`, `"les notes"`, `"les observations"`, `"le carnet"`, `"le journal"`.
- **Cibles unitaires sans portée globale** : `"la derniere note"`, `"la derniere observation"`, `"mon observation"`.
- **Cibles de fichiers (interdites d'effacement carnet)** : `"le fichier de configuration"`, `"le fichier"`, `"les fichiers"`, `"des fichiers"`, `"les fichiers de log"`.
- **Marqueurs temporels** : `"ce matin"`, `"ce soir"`, `"cette semaine"`, `"la semaine derniere"`, `"la semaine prochaine"`, `"aujourd hui"`, `"apres demain"`, `"avant hier"`.
- **Annulations et refus explicites** : `"ne supprime pas"`, `"je refuse de confirmer"`, `"je refuse"`, `"laisse tomber"`.

---

## 6. Règles Déclaratives de la Grammaire (`rules/grammar.json`, COR-03, COR-04)

Les règles combinatoires définissent les conditions d'activation, scores et priorités :

```json
{
  "rules": [
    {
      "id": "confirm_clear_exact",
      "intent": "CONFIRM_CLEAR",
      "priority": 100,
      "baseScore": 100,
      "executable": true,
      "exactMatchRequired": true,
      "rejectNegation": true,
      "forbiddenTargets": ["FILE", "PROJECT", "DEVICE"]
    },
    {
      "id": "cancel_clear_direct",
      "intent": "CANCEL_CLEAR",
      "priority": 95,
      "baseScore": 95,
      "executable": true,
      "requiredConcepts": ["CANCEL"]
    },
    {
      "id": "cancel_clear_negated_delete",
      "intent": "CANCEL_CLEAR",
      "priority": 95,
      "baseScore": 95,
      "executable": true,
      "requiredConcepts": ["DELETE"],
      "requiresNegation": true
    },
    {
      "id": "add_note_prefix",
      "intent": "ADD_NOTE",
      "priority": 90,
      "baseScore": 95,
      "executable": true,
      "startsWithConcept": "ADD",
      "requireActionPayload": true
    },
    {
      "id": "request_clear_all",
      "intent": "REQUEST_CLEAR",
      "priority": 85,
      "baseScore": 95,
      "executable": true,
      "requiredConcepts": ["DELETE"],
      "allowedTargets": ["JOURNAL", "JOURNAL_NOTE"],
      "forbiddenTargets": ["FILE", "PROJECT", "DEVICE", "TASK", "PROBLEM"],
      "forbiddenConcepts": ["CONFIRM", "CANCEL", "QUESTION"],
      "requireGlobalScope": true,
      "rejectNegatedCommand": true
    },
    {
      "id": "count_notes_query",
      "intent": "COUNT_NOTES",
      "priority": 80,
      "baseScore": 90,
      "executable": true,
      "requiredConcepts": ["COUNT"],
      "rejectNegatedCommand": true
    },
    {
      "id": "read_last_note_cmd",
      "intent": "READ_LAST_NOTE",
      "priority": 80,
      "baseScore": 90,
      "executable": true,
      "requiredConcepts": ["READ"],
      "rejectNegatedCommand": true
    },
    {
      "id": "analyze_notes_all",
      "intent": "ANALYZE_NOTES",
      "priority": 80,
      "baseScore": 90,
      "executable": true,
      "requiredConcepts": ["ANALYZE"],
      "rejectNegatedCommand": true,
      "forbiddenEntities": true
    },
    {
      "id": "analyze_project_cmd",
      "intent": "ANALYZE_PROJECT",
      "priority": 85,
      "baseScore": 95,
      "executable": false,
      "requiredConcepts": ["ANALYZE"],
      "requiresEntity": true,
      "rejectNegatedCommand": true
    },
    {
      "id": "task_obligation",
      "intent": "ADD_TASK",
      "priority": 80,
      "baseScore": 90,
      "executable": false,
      "requiredConcepts": ["OBLIGATION"],
      "optionalConcepts": ["ACTION_VERB", "ENTITY", "DATE_RELATIVE"],
      "forbiddenConcepts": ["DELETE", "CANCEL"],
      "rejectNegatedObligation": true,
      "rejectNegatedCommand": true
    },
    {
      "id": "search_by_project_cmd",
      "intent": "SEARCH_BY_PROJECT",
      "priority": 85,
      "baseScore": 95,
      "executable": false,
      "requiredConcepts": ["SEARCH", "PROJECT"],
      "forbiddenConcepts": ["DELETE", "CANCEL"],
      "rejectNegatedCommand": true
    },
    {
      "id": "search_by_date_cmd",
      "intent": "SEARCH_BY_DATE",
      "priority": 85,
      "baseScore": 95,
      "executable": false,
      "requiredConcepts": ["SEARCH", "DATE_RELATIVE"],
      "forbiddenConcepts": ["DELETE", "CANCEL"],
      "rejectNegatedCommand": true
    },
    {
      "id": "search_date_target",
      "intent": "SEARCH_BY_DATE",
      "priority": 85,
      "baseScore": 95,
      "executable": false,
      "requiredConcepts": ["NOTE_TARGET", "DATE_RELATIVE"],
      "forbiddenConcepts": ["DELETE", "CANCEL"],
      "rejectNegatedCommand": true
    },
    {
      "id": "search_notes_query",
      "intent": "SEARCH_NOTES",
      "priority": 75,
      "baseScore": 85,
      "executable": false,
      "requiredConcepts": ["SEARCH"],
      "optionalConcepts": ["NOTE_TARGET", "PROBLEM", "ENTITY"],
      "forbiddenConcepts": ["DELETE", "CANCEL"],
      "rejectNegatedCommand": true
    }
  ]
}
```

---

## 7. Arbitrage des Ambiguïtés et Pénalités Fuzzy (COR-07)

1. **Calcul des scores** :
   $$\text{adjustedScore} = \max(0, \text{rawScore} - \text{fuzzyPenalty})$$
   où $\text{fuzzyPenalty} = 15$ si au moins un mot approximatif a été corrigé par Levenshtein, $0$ sinon.
2. **Évaluation indépendante de l'ambiguïté** :
   L'ambiguïté est mesurée sur le score brut $\text{rawScore}$ entre les deux meilleurs candidats :
   $$\Delta_{\text{raw}} = |\text{score}_0 - \text{score}_1| \le 5 \quad \text{avec} \quad \text{score}_0 \ge 80$$
   Si cette condition est vérifiée et que les intentions diffèrent, la commande est déclarée `AMBIGUOUS`, même si une pénalité fuzzy abaisse le score final sous le seuil d'acceptation.
