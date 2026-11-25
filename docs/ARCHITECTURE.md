# Architecture technique — VoiceJournal v0.2.1

## 1. Vue d'ensemble

VoiceJournal v0.2.1 est une application embarquée modulaire construite **au-dessus de la façade publique `<VoiceKit.h>`** (VoiceKit révision Git `16646c73acf01a8d57408391bc6de15a288b8910`), sans aucune modification de la bibliothèque et sans duplication de ses moteurs audio, WebSocket ou réseau.

Elle prend en charge **OpenAI Realtime** (fournisseur configuré par défaut au démarrage dans `include/AppConfig.h`) et **Google Gemini Live**, avec bascule dynamique depuis l'écran tactile du M5Stack CoreS3.

```text
+-----------------------------------------------------------------------------------------+
|                                        main.cpp                                         |
|      setup() / loop() : M5.update() + controller.update() + processSerialCommands()     |
+---------------------+---------------------------------------------+---------------------+
                      |                                             |
                      v                                             v
+---------------------------------------------+    +--------------------------------------+
|                  JournalUI                  |    |           VoiceController            |
| - M5GFX paysage 320x240 (rotation 0)        |    | - Machine a etats applicative        |
| - STOP prioritaire sans delai               |    | - Bascule OpenAI / Gemini            |
| - Bouton tactile discret [LAB] (3 vues)     |    | - Correlation stricte turnId STT     |
| - Selecteur IA [OPENAI / GEMINI] (350 ms)   |<---| - File TTS bornee (4 messages)       |
| - PTT dedie PARLER / TERMINER (180 ms)      |    | - Armement confirmation CLEAR (30 s) |
| - Verrou de maintien tactile                |    | - Annulation de securite STOP        |
| - Rendu conditionnel (dirty flag)           |    | - Watchdogs non bloquants            |
+---------------------------------------------+    +---+--------------+---------------+---+
                                                       |              |               |
                                          +------------+              |               +------------+
                                          v                           v                            v
                            +---------------------------+   +-------------------+   +--------------------+
                            |       CommandRouter       |   |  JournalStorage   |   | VoiceKitEngineAdpt |
                            | - Modes : INTEL/SHADOW/LEG|   | - Preferences NVS |   | -> <VoiceKit.h>    |
                            | - Validation COR-11       |   | - Double banque   |   |   listen/stop/say  |
                            | - Filtrage executabilite  |   |   A/B + FNV-1a    |   |   cancelListening  |
                            | - Disarm CLEAR si invalide|   | - 5 obs x 180 o.  |   |   applyVoiceProfile|
                            | - Isolation diagnostique  |   +-------------------+   |   askAi/update     |
                            +-------------+-------------+                           +--------------------+
                                          |
                                          v
                            +---------------------------+
                            |       JournalParser       |
                            | - Limites 1024 o. / 96 tok|
                            | - Tokenizer + Normalizer  |
                            | - PhraseRecognizer (51)   |
                            | - SemanticRoleAnalyzer    |
                            | - GrammarEvaluator (19 r.)|
                            | - IntentMatcher & Slots   |
                            | - FuzzyMatcher securise   |
                            +---------------------------+
```

---

## 2. Responsabilités des modules

| Module | Responsabilité principale | Dépendances |
|---|---|---|
| `AppConfig.h` | Configuration globale, version applicative (`0.2.1`), fournisseur par défaut (`DEFAULT_AI_PROVIDER = OPENAI`), profils vocaux, constantes de temps, enums `AiProvider` et `JournalState` | C++ Standard |
| `JournalStorage` | Double instantané circulaire A/B (5 observations × 180 octets UTF-8) en NVS `"vjournal"`, validation par somme de contrôle FNV-1a, contrôle strict des écritures/suppressions | `Preferences` (ESP32) |
| `JournalParser` | Pipeline de compréhension sémantique C++17 : validation UTF-8 (RFC 3629), contrôle de limites (1024 octets, 96 tokens), reconnaissance de 51 locutions, analyse des rôles sémantiques, grammaire de 19 règles, typage d'actions et d'objets, extraction de slots, gestion des négations | C++17 STL |
| `CommandRouter` | Validation centralisée des actions (COR-11), arbitrage entre modes `INTELLIGENT`, `LEGACY` et `SHADOW`, désarmement de sécurité des suppressions en attente | `JournalParser`, `AppConfig.h` |
| `VoiceController` | Machine à états applicative, bascule OpenAI ↔ Gemini avec contrôle de clés API, corrélation stricte des tours STT (`turnId`), file TTS bornée, routage vers stockage ou IA, gestion du cycle de vie de confirmation d'effacement, watchdogs | `IVoiceEngine`, `JournalStorage`, `CommandRouter` |
| `JournalUI` | Affichage M5GFX 320 × 240 (rotation 0), gestion tactile multi-zones avec priorité absolue à `STOP`, bouton `[LAB]` ouvrant le Parsing Lab à 3 vues (Résultat, Contexte, Comparaison), sélecteur de fournisseur IA | `M5Unified` |
| `SerialParserProtocol.h` | Formatage standardisé des réponses JSON pour les commandes de diagnostic série USB (`PARSE`, `PARSE_SHADOW`, `PARSER_STATUS`, `PARSER_HELP`) avec buffer borné à 1152 octets et vidange en cas de dépassement | C++17 STL |
| `main.cpp` | Point d'entrée matériel, initialisation Wi-Fi et audio, liaison VoiceKit, boucle d'événements non bloquante `M5.update()` + `controller.update()` + traitement série non bloquant | Tous les modules + `<VoiceKit.h>` |

---

## 3. Machine à états applicative (`JournalState`)

La machine à états de VoiceJournal est **distincte** de `VoiceState` (VoiceKit). Les transitions autorisées sont :

```text
INITIALIZING ──► CONNECTING ──► READY
                      ▲           │ ▲
                      │           │ │ (fin TTS / fin analyse / timeout / STOP)
       [bascule IA]   │           │ │
   (READY / DISC / ERR)       │ │
                      │           ▼ │
                DISCONNECTED ─────┼─┘
                      │           │
                      │           ├──[clic PARLER]──► LISTENING
                      │           │                      │
                      │           │              [clic TERMINER ou max 15s]
                      │           │                      ▼
                      │           │                 TRANSCRIBING
                      │           │                      │
                      │           │           [transcription finale valide]
                      │           │                      ▼
                      │           │             PROCESSING_COMMAND
                      │           │               │      │      │
                      │           │      [ADD/CLR]│      │[IA]  │[COUNT/READ/UNK]
                      │           │               ▼      │      ▼
                      │           │            SAVING    │   SPEAKING ──► READY
                      │           │               │      │
                      │           │               ▼      ▼
                      │           │            SPEAKING ANALYZING ──► READY
                      │           │
                      └──[tout etat + clic STOP]──► STOPPING ──► READY / DISCONNECTED
                      └──[erreur critique]────────► ERROR_STATE
```

---

## 4. Invariants garantis de la consolidation v0.2.1

1. **Priorité absolue de `STOP`** :
   - `STOP` est évalué avant tout autre bouton tactile et interrompt immédiatement l'audio en cours, la capture micro et toute attente de confirmation.
2. **Rejet strict des entrées invalides (COR-12)** :
   - Toute entrée textuelle dépassant 1024 octets, contenant plus de 96 tokens ou présentant un encodage UTF-8 non conforme est rejetée intégralement sans analyse partielle (`executable = false`).
3. **Validation centralisée des actions (COR-11)** :
   - L'identification d'une intention ne déclenche aucune action métier si le statut est invalide, si le score est insuffisant ou si la commande est ambiguë.
   - Toute commande invalide ou négative prononcée pendant une confirmation d'effacement désarme immédiatement cette confirmation.
4. **Protocole de suppression sécurisé en 2 étapes** :
   - Exige une portée globale explicite (*« toutes les notes »*, *« le carnet »*).
   - Refus strict si la commande cible un fichier, un équipement ou une note unitaire.
   - Fenêtre de confirmation limitée à 30 secondes sans confirmation automatique possible.
5. **Isolation totale du protocole USB (COR-14)** :
   - Les commandes reçues par USB (`PARSE`, `PARSE_SHADOW`, etc.) n'altèrent ni le carnet NVS, ni la session audio, ni l'état de confirmation.
   - Réponse systématique en JSON valide.
6. **Bascule multi-fournisseurs fluide et robuste** :
   - Bascule tactile instantanée entre OpenAI Realtime et Gemini Live sans rejeu et sans perte de notes.
   - OpenAI Realtime actif par défaut à la mise sous tension.
