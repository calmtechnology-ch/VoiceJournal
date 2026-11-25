# VoiceJournal v0.2.0.1 — Carnet de laboratoire vocal intelligent & Moteur NLU Contextuel

Application embarquée C++17 pour **M5Stack CoreS3 (ESP32-S3)** construite sur la façade publique **`<VoiceKit.h>`** (**VoiceKit**, révision Git vérifiée `16646c73acf01a8d57408391bc6de15a288b8910`), compatible **OpenAI Realtime** (fournisseur par défaut) et **Google Gemini Live**.

La version **v0.2.0.1 — Contextual Parsing Corrective** dote VoiceJournal d'un **moteur de compréhension du langage naturel contextuel**, local, déterministe, explicable et optimisé pour l'ESP32-S3. Le moteur comprend désormais les relations fines entre les mots, les concepts, les actions et leurs objets, tout en restant strictement hors-ligne pour la phase d'analyse sémantique.

> **Statut de validation :**
> - **505 tests natifs C++17 réussis à 100%** (36 assertions historiques v0.1.1 + 220 tests NLU v0.2.0 + 249 tests contextuels v0.2.0.1).
> - **Dataset de validation contextuelle 53/53 cas réussis à 100%** sur le simulateur console PC.
> - **Compilation PlatformIO (`m5stack-cores3`) validée sans avertissement** :
>   - **RAM : 26,3%** (86 276 octets utilisés / 327 680 octets) — seulement **+360 octets statiques** par rapport à la v0.2.0 (85 916 octets), largement en-dessous du plafond de +16 Ko.
>   - **Flash : 87,9%** (1 844 049 octets utilisés / 2 097 152 octets).
> - **Référence matérielle conservée** : le rapport matériel de la v0.2.0 (8 PASS, 0 FAIL, 7 NOT RUN) est consigné dans [`docs/baseline/BASELINE_v0.2.0.md`](docs/baseline/BASELINE_v0.2.0.md) et sert de référence de non-régression.

---

## 1. Présentation de VoiceJournal v0.2.0.1

**VoiceJournal** transforme un **M5Stack CoreS3** autonome (sans aucun capteur ni module externe) en carnet de laboratoire vocal. L'utilisateur dicte ses observations techniques en français, les stocke localement dans la mémoire non volatile (NVS), les relit à la voix et demande à l'IA (**OpenAI Realtime** par défaut ou **Google Gemini Live**) d'en produire une synthèse factuelle courte.

En version **v0.2.0.1**, le moteur dépasse la simple reconnaissance par mots-clés et construit une véritable représentation structurée de l'intention grâce à un pipeline contextuel en 11 étapes :

```text
Texte original
       │
Normalisation UTF-8 (RFC 3629)
       │
Tokenisation (max 96 tokens)
       │
Analyse lexicale (231 concepts en Flash)
       │
Reconnaissance des locutions (longest match, 51 expressions)
       │
Analyse des rôles sémantiques (12 rôles)
       │
Construction des relations & analyse d'objets (8 catégories de cibles)
       │
Grammaire contextuelle (19 règles déclaratives ordonnées)
       │
Extraction des paramètres & portée des négations
       │
Validation de sécurité centralisée
       │
ParseResult ──► CommandRouter (SHADOW / INTELLIGENT / LEGACY)
```

---

## 2. Fonctionnalités majeures (v0.2.0.1)

### Pipeline sémantique contextuel
- **Rôles sémantiques formels (COR-01)** : typage des unités lexicales en 12 rôles (`UNKNOWN`, `SUBJECT`, `ACTION`, `OBJECT`, `ENTITY`, `QUANTIFIER`, `QUALIFIER`, `TIME`, `NEGATION`, `OBLIGATION`, `QUESTION`, `CONNECTOR`, `PAYLOAD`) et construction de `SemanticFrame`.
- **Reconnaissance de locutions au plus long préfixe (COR-02)** : reconnaissance déterministe de 51 locutions (*"je dois"*, *"il faut que"*, *"penser à"*, *"ne pas oublier de"*, *"la dernière note"*, *"toutes les observations"*, *"ce matin"*, etc.) avec masques binaires sans allocation dynamique.
- **Grammaire contextuelle légère (COR-03 & COR-04)** : 19 règles combinatoires ordonnées dans `rules/grammar.json`, générées en tables `constexpr` C++ (`ParserTables.h`) par `tools/generate_parser_tables.py`.
- **Typage strict des actions et de leurs objets (COR-05)** : distinction catégorielle (`TargetCategory`) entre `JOURNAL`, `JOURNAL_NOTE`, `TASK`, `PROJECT`, `PROBLEM`, `FILE`, `DEVICE`, `UNKNOWN_OBJECT`. Rejet strict en `UNKNOWN` de toute suppression ciblant un fichier, un projet, un équipement ou une note singulière sans portée globale.
- **Portée fine des négations (COR-06)** : distinction entre négation de l'action principale, négation d'obligation, annulation et négation au sein d'une note dictée (*"Note, ne supprime pas les fichiers"* $\rightarrow$ `ADD_NOTE`).
- **Découplage fuzzy matching et ambiguïtés (COR-07)** : ambiguïté calculée sur le score brut avant pénalité fuzzy ; détection des collisions directes entre verbes d'action concurrents ; exclusion formelle du fuzzy sur les chiffres, versions, identifiants et confirmations.
- **Validation centralisée et sécurité destructive (COR-08)** : validation centralisée dans `CommandRouter` ; verrouillage automatique `SECURITY_BLOCKED` en mode `SHADOW` en cas de divergence sur une intention destructive.
- **Comparateur SHADOW complet (COR-09)** : 7 statuts formels (`MATCH`, `SEMANTIC_EQUIVALENT`, `SMART_UPGRADE`, `TARGET_MISMATCH`, `DIVERGENT`, `SECURITY_BLOCKED`, `SYNTAX_ERROR`).
- **Limites bornées et conformité UTF-8 (COR-10)** : `MAX_INPUT_BYTES = 1024`, `MAX_TOKENS = 96`, validation UTF-8 conforme RFC 3629 rejetant les surlongueurs et les surrogates.

### Nouveaux outils d'exploitation et de diagnostic
- **Mode diagnostic USB série (sans capture audio)** :
  - Commandes série textuelles directes : `PARSE <texte>`, `PARSE_SHADOW <texte>`, `PARSER_STATUS`, `PARSER_HELP`.
  - Format de réponse standardisé en JSON compact.
  - Isolation stricte : n'altère ni le stockage NVS ni la machine à états vocale.
- **Parsing Lab tactile CoreS3 à 3 vues** :
  - Bouton `[LAB]` permettant de cycler entre :
    - **Vue 1 : Résultat** (Intention, score, statut, cible, portée, négation, timing µs).
    - **Vue 2 : Contexte sémantique** (Transcription défilante, rôles sémantiques token par token, entités détectées).
    - **Vue 3 : Comparaison Shadow** (Statut Shadow, intention Legacy vs Contextuelle, scores respectifs).
- **Simulateur autonome PC étendu** :
  - Support de `--dataset <path>` avec exécution automatisée du jeu de validation contextuel (`tests/datasets/contextual_parsing_fr.json`).

---

## 3. Matériel nécessaire

- **M5Stack CoreS3** (ESP32-S3, 16 Mo Flash, 8 Mo PSRAM, écran tactile 320 × 240, microphones ES7210 et haut-parleur AW88298 intégrés).
- Câble USB-C données + alimentation (≥ 5 V / 1 A).
- Réseau **Wi-Fi 2.4 GHz** (l'ESP32-S3 ne supporte pas le 5 GHz).
- Clé API **OpenAI Realtime** ([OpenAI Platform](https://platform.openai.com/api-keys)) et/ou clé API **Google Gemini Live** ([Google AI Studio](https://aistudio.google.com/)).
- Aucun capteur ou module supplémentaire requis.

---

## 4. Dépendances et licences

Déclarées dans `platformio.ini` :

- Plateforme : `espressif32@6.9.0` (framework Arduino-ESP32, C++17 `-std=gnu++17`, licence Apache-2.0 / LGPL-2.1).
- `m5stack/M5CoreS3@1.0.0` (licence MIT).
- `m5stack/M5Unified@0.2.23` (inclut M5GFX, licence MIT).
- `Preferences` (bibliothèque standard Arduino-ESP32 pour la NVS).
- **VoiceKit** (`https://github.com/calmtechnology-ch/VoiceKit.git#16646c73acf01a8d57408391bc6de15a288b8910`, licence MIT).

Le projet **VoiceJournal** est distribué sous licence **MIT** (voir [LICENSE](LICENSE)).

---

## 5. Configuration et Compilation

### Configuration Wi-Fi et Clés API (`include/secrets.h`)
1. Copiez `include/secrets.example.h` vers `include/secrets.h` :
   ```powershell
   Copy-Item include\secrets.example.h include\secrets.h
   ```
2. Renseignez vos identifiants :
   ```cpp
   #pragma once
   #define WIFI_SSID     "MON_WIFI"
   #define WIFI_PASSWORD "MON_MOT_DE_PASSE"

   #define OPENAI_KEY    "MA_CLE_OPENAI"
   #define GEMINI_KEY    "MA_CLE_GEMINI"
   ```

### Compilation et téléversement PlatformIO
Depuis le dossier `VoiceJournal_CoreS3/` :

```powershell
# Compilation du firmware
pio run -e m5stack-cores3

# Téléversement sur M5Stack CoreS3 (USB-C)
pio run -e m5stack-cores3 -t upload

# Moniteur série (115 200 bauds)
pio device monitor -b 115200
```

---

## 6. Utilisation du mode diagnostic série USB

Le CoreS3 intègre un interpréteur de commandes de test sur le port série USB (115 200 bauds), permettant de valider le parsing sans connexion réseau ni enregistrement vocal :

```text
PARSE Je dois tester Gemini demain.
{"status":"OK","intent":"ADD_TASK","score":95,"executable":false,"slots":{"action":"tester","entity":"gemini","time":"demain"}}

PARSE_SHADOW Supprime le fichier.
{"status":"OK","contextual_intent":"UNKNOWN","legacy_intent":"UNKNOWN","shadow_status":"MATCH"}

PARSER_STATUS
{"parser_version":"0.2.0.1","rules_count":19,"phrases_count":51,"lexicon_size":231,"max_input_bytes":1024,"max_tokens":96}

PARSER_HELP
{"commands":["PARSE <texte>","PARSE_SHADOW <texte>","PARSER_STATUS","PARSER_HELP"]}
```

---

## 7. Exemples de commandes vocales

| Formulation prononcée | Intention reconnue | Type | Action système |
|---|---|---|---|
| *« Note, le prototype redémarre »* | `ADD_NOTE` | Exécutable | Enregistre l'observation en NVS |
| *« Mémorise que le WiFi fonctionne »* | `ADD_NOTE` | Exécutable | Enregistre l'observation en NVS |
| *« Note, ne supprime pas les fichiers »* | `ADD_NOTE` | Exécutable | Enregistre l'observation en NVS (négation préservée dans la charge utile) |
| *« Lis la dernière note »* | `READ_LAST_NOTE` | Exécutable | Lit la dernière note par TTS |
| *« Combien de notes ? »* | `COUNT_NOTES` | Exécutable | Annonce le nombre d'observations |
| *« Fais le bilan »* | `ANALYZE_NOTES` | Exécutable | Synthèse IA globale des notes |
| *« Efface toutes les notes »* | `REQUEST_CLEAR` | Exécutable | Déclenche la fenêtre de confirmation (15 s) |
| *« Confirmer la suppression »* | `CONFIRM_CLEAR` | Exécutable | Efface définitivement le carnet |
| *« Ne supprime pas les notes »* | `CANCEL_CLEAR` | Exécutable | Annule la suppression en attente |
| *« Je dois tester Gemini demain »* | `ADD_TASK` | Expérimentale | Reconnue (obligation + action + entité + date) |
| *« N'oublie pas de tester Gemini »* | `ADD_TASK` | Expérimentale | Reconnue (obligation positive par locution) |
| *« Cherche les bugs de VoiceKit »* | `SEARCH_NOTES` | Expérimentale | Reconnue (recherche + problème + entité) |
| *« Donne-moi une synthèse de RuleBot »* | `ANALYZE_PROJECT` | Expérimentale | Reconnue (synthèse + projet) |
| *« Efface le fichier »* | `UNKNOWN` | Rejet | Rejeté : cible `FILE` non autorisée |
| *« Supprime le projet VoiceKit »* | `UNKNOWN` | Rejet | Rejeté : cible `PROJECT` non autorisée |
| *« Lis la dernière note et fais le bilan »* | `AMBIGUOUS` | Ambiguïté | Détectée : deux verbes d'action concurrents |

---

## 8. Organisation du code

```text
VoiceJournal_CoreS3/
├── LICENSE                 # Licence MIT
├── platformio.ini          # Configuration PlatformIO (m5stack-cores3, C++17, VoiceKit fige)
├── no_ota.csv              # Table de partitions 2 Mo app0
├── README.md
├── CHANGELOG.md
├── rules/                  # Définitions déclaratives de la grammaire et du lexique
│   ├── concepts.json       # Lexique et concepts sémantiques (231 entrées)
│   ├── phrases.json        # 51 locutions ordonnées (longest match)
│   ├── grammar.json        # 19 règles contextuelles déclaratives
│   ├── entities.json       # Entités reconnues (gemini, voicekit, rulebot, etc.)
│   └── aliases.json        # Alias et synonymes
├── tools/
│   └── generate_parser_tables.py # Générateur Python des tables C++ PROGMEM
├── include/
│   ├── secrets.example.h   # Modèle de configuration des secrets
│   ├── AppConfig.h         # Configuration globale, profils IA et modes de parsing
│   ├── JournalStorage.h    # Persistance NVS A/B fiabilisée
│   ├── CommandRouter.h     # Routeur de commandes, validation d'action et mode SHADOW
│   ├── VoiceController.h   # Machine à états applicative et orchestration vocale
│   ├── JournalUI.h         # Interface tactile M5GFX 320x240 et Parsing Lab 3 vues
│   └── parser/             # Moteur de compréhension contextuelle C++17
│       ├── ParserTypes.h          # Rôles sémantiques, catégories de cibles, statuts
│       ├── ParserTables.h         # Tables compilées en mémoire Flash
│       ├── JournalParser.h        # Façade principale du pipeline de parsing
│       ├── PhraseRecognizer.h     # Reconnaissance de locutions au plus long préfixe
│       ├── SemanticRoleAnalyzer.h # Étiquetage des rôles sémantiques et frames
│       ├── GrammarEvaluator.h     # Évaluateur de la grammaire contextuelle
│       ├── NegationDetector.h     # Détecteur contextuel de portée des négations
│       ├── SlotExtractor.h        # Extracteur générique de slots
│       └── IntentMatcher.h        # Décision d'intention et détection d'ambiguïtés
├── src/
│   ├── main.cpp            # Boucle principale, gestion tactile et protocole série USB
│   ├── JournalStorage.cpp
│   ├── CommandRouter.cpp
│   ├── VoiceController.cpp
│   ├── JournalUI.cpp
│   └── parser/
│       ├── JournalParser.cpp
│       ├── PhraseRecognizer.cpp
│       ├── SemanticRoleAnalyzer.cpp
│       ├── GrammarEvaluator.cpp
│       ├── NegationDetector.cpp
│       ├── SlotExtractor.cpp
│       └── IntentMatcher.cpp
├── simulator/
│   ├── main.cpp            # Simulateur console PC (--eval, --shadow, --dataset)
│   └── CMakeLists.txt
├── tests/
│   ├── TEST_PLAN.md        # Plan de test exhaustif (505 scénarios)
│   ├── mocks/              # Mocks NVS, VoiceKit et Arduino pour tests natifs PC
│   ├── datasets/
│   │   └── contextual_parsing_fr.json # Dataset de référence contextuel (53 cas)
│   ├── parser/
│   │   ├── test_nlu_suite.cpp         # 220 tests NLU v0.2.0
│   │   └── test_contextual_suite.cpp  # 249 tests contextuels v0.2.0.1
│   └── test_main.cpp       # Lanceur de la suite de tests complète (505 assertions)
└── docs/
    ├── baseline/
    │   ├── BASELINE_v0.1.1.md         # Baseline v0.1.1
    │   └── BASELINE_v0.2.0.md         # Baseline v0.2.0 (8 PASS, 0 FAIL, 7 NOT RUN)
    ├── CONTEXTUAL_PARSING.md          # Architecture complète du pipeline contextuel
    ├── GRAMMAR_REFERENCE.md           # Référence formelle de la grammaire (rôles, cibles, règles)
    ├── PARSER_USER_GUIDE.md           # Guide utilisateur du parser et Parsing Lab
    ├── SERIAL_PARSER_PROTOCOL.md      # Protocole USB série de diagnostic
    ├── REGRESSION_REPORT_v0.2.0.1.md  # Rapport complet des 505 tests natifs
    ├── CORRECTIVE_REPORT_v0.2.0.1.md  # Bilan correctif COR-01 à COR-10
    └── HARDWARE_VALIDATION.md         # Grille de validation matérielle CoreS3
```

---

## 9. Procédure de test et validation

### Tests unitaires natifs PC (505 tests, sans matériel)
```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror -Itests/mocks -Iinclude -Itests `
    src/JournalStorage.cpp src/CommandRouter.cpp src/VoiceController.cpp src/JournalUI.cpp `
    src/parser/*.cpp tests/parser/*.cpp tests/test_main.cpp `
    -o tests/build/voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

### Évaluation du dataset contextuel sur simulateur PC
```powershell
.\simulator\voicejournal_simulator.exe --dataset tests/datasets/contextual_parsing_fr.json
```

### Tests matériels sur M5Stack CoreS3
Consultez la grille de validation dans [`docs/HARDWARE_VALIDATION.md`](docs/HARDWARE_VALIDATION.md). Les tests de référence v0.2.0 (8 PASS) sont conservés sans régression.
