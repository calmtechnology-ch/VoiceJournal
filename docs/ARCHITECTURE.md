# Architecture technique — VoiceJournal v0.1.0.1 Corrective

## 1. Vue d'ensemble

VoiceJournal v0.1.0.1 Corrective est une application embarquée modulaire construite **au-dessus de la façade publique `<VoiceKit.h>`** (VoiceKit révision Git `16646c73acf01a8d57408391bc6de15a288b8910`), sans aucune modification de la bibliothèque et sans duplication de ses moteurs audio, WebSocket ou réseau.

```text
+-------------------------------------------------------------------+
|                             main.cpp                              |
|        setup() / loop() : M5.update() + controller.update()       |
+-----------------+----------------------------------+--------------+
                  |                                  |
                  v                                  v
+----------------------------------+   +----------------------------+
|            JournalUI             |   |       VoiceController      |
| - M5GFX paysage 320x240 (rot 0)  |   | - Machine a etats app      |
| - STOP prioritaire sans delai    |   | - Correlation turnId STT   |
| - Anti-rebond dedie PARLER 180ms |<--| - File TTS bornee (4)      |
| - Verrou maintien tactile        |   | - Construction prompt IA   |
| - Rendu conditionnel (dirty)     |   | - STOP cible & idempotent  |
+----------------------------------+   | - Watchdogs non bloquants  |
                                       +---+----------+----------+--+
                                           |          |          |
                              +------------+          |          +------------+
                              v                       v                       v
                 +--------------------+   +--------------------+   +--------------------+
                 |   CommandRouter    |   |   JournalStorage   |   | VoiceKitEngineAdpt |
                 | - Normalisation FR |   | - Preferences NVS  |   | -> <VoiceKit.h>    |
                 | - Prefixe "note"   |   | - Double banque    |   |   listen/stop/say  |
                 | - Confirm CLEAR    |   |   A/B + FNV-1a     |   |   cancelListening  |
                 +--------------------+   | - Migration v0.1.0 |   |   askAi/update     |
                                          +--------------------+   +--------------------+
```

---

## 2. Responsabilités des modules

| Module | Responsabilité principale | Dépendances |
|---|---|---|
| `AppConfig.h` | Constantes, durées watchdogs, prompt système IA, enum `JournalState` | Standard C++ |
| `JournalStorage` | Double instantané borné A/B (5 notes × 180 octets UTF-8) en NVS `"vjournal"`, validation FNV-1a, contrôle strict des écritures/suppressions, migration depuis v0.1.0 | `Preferences` (via `IKeyValueStore`) |
| `CommandRouter` | Analyse purement déterministe des transcriptions, gestion de l'état de confirmation `CLEAR` | `AppConfig.h` |
| `VoiceController` | Machine à états applicative, corrélation stricte des tours STT (`turnId` / `maxInvalidatedTurnId_`), file TTS, analyse Gemini, `STOP` ciblé non destructif, watchdogs récupérables | `IVoiceEngine`, `JournalStorage`, `CommandRouter` |
| `JournalUI` | Rendu M5GFX 320 × 240 (rotation `0`), filtrage tactile testable (`processTouchSample`) avec priorité absolue de `STOP`, anti-rebond dédié à `PARLER`, verrou de maintien | `M5Unified` |
| `main.cpp` | Assemblage des composants, configuration `VoiceKitConfig`, boucle principale non bloquante, diagnostic mémoire | Tous les modules + `<VoiceKit.h>` |

---

## 3. Machine à états applicative (`JournalState`)

La machine à états de VoiceJournal est **distincte** de `VoiceState` (VoiceKit). Les transitions autorisées sont :

```text
INITIALIZING ──► CONNECTING ──► READY
                      │           │ ▲
                      ▼           │ │ (fin TTS / fin analyse / timeout / STOP)
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

### Invariants garantis (v0.1.0.1)
1. **Priorité réelle de `STOP` dans `JournalUI`** :
   - `STOP` est testé avant `PARLER` et n'est jamais bloqué par l'anti-rebond de `PARLER` (`180 ms`).
   - Un contact maintenu (`touchDown_`) n'émet qu'un seul événement jusqu'au relâchement.
   - Deux contacts distincts peuvent produire `PARLER` puis `STOP` à 50 ms ou 100 ms d'intervalle.
2. **Pas de double capture** : `PARLER` est refusé hors de `READY` ou si `engine.isBusy()` / `ttsQueue` non vide / tour non clos.
3. **Isolation des callbacks** : `onTranscriptionWithTurnId` ne réalise aucune opération NVS ni graphique bloquante ; la transcription finale valide est déposée dans un emplacement unique et traitée dans `VoiceController::update()`.
4. **Corrélation stricte des tours STT** :
   - `expectedTurnId_` est capturé via `onListeningStartedWithTurnId(turnId)` avec repli sur `getActiveCaptureTurnId()` / `getPendingTranscriptionTurnId()` / `getLastCaptureTurnId()`.
   - Toute transcription partielle (`isFinal == false`) met uniquement à jour l'affichage sans déclencher d'action.
   - Toute transcription finale appartenant à un tour annulé (`turnId <= maxInvalidatedTurnId_`), déjà traité (`lastProcessedTurnId_`) ou différent de `expectedTurnId_` est ignorée.
5. **STOP ciblé, idempotent et non destructif** :
   - Pendant `LISTENING` ou `TRANSCRIBING` : appelle uniquement `cancelListening()` (sans appeler `stop()` ensuite, préservant `_cancelledCaptureTurnId` dans `VoxCoreVoice` et évitant toute perturbation de session).
   - Pendant `ANALYZING` ou `SPEAKING` : appelle uniquement `stop()`.
   - Sur un second `STOP` consécutif (moteur déjà libre) : aucun appel moteur redondant.
   - Ne revient à `READY` qu'après confirmation de l'état réel du moteur (`engine_.isReady()`) ; reste en `STOPPING` si la libération est asynchrone.
   - Ne modifie jamais `JournalStorage`.
6. **Filtrage des réponses IA et audio tardives** :
   - `onAssistantText` est ignoré si `!aiRequestActive_`.
   - `onSpeechStarted` et `VoiceEngineState::SPEAKING` sont ignorés si ni `aiRequestActive_` ni `ttsPlaybackActive_` n'est actif, ou pendant `LISTENING` / `TRANSCRIBING` / `STOPPING`.
7. **Reconnexion sans rejeu** :
   - `onReady()` invalide tout tour antérieur, annule toute confirmation `CLEAR`, vide la file TTS locale et ne passe à `READY` qu'après confirmation `engine_.isReady()`.

---

## 4. Fiabilisation NVS (Double instantané A/B)

`Preferences` (NVS ESP-IDF) ne fournit pas de transaction atomique matérielle multi-clés. Pour empêcher toute divergence RAM/NVS ou corruption lors d'une coupure d'alimentation :

- Deux banques indépendantes **A** (`a0..a4`, `ah`, `ac`, `ag`, `ak`) et **B** (`b0..b4`, `bh`, `bc`, `bg`, `bk`) coexistent dans le namespace `"vjournal"`, accompagnées de la clé de version `v2 = 0x564A3032` et de la référence active `act`.
- Lors d'un `addNote()` ou d'un `clear()`, seule la banque **inactive** est écrite :
  1. Invalidation immédiate de son marqueur de validation (`ak`/`bk = 0`).
  2. Écriture des notes actives et suppression vérifiée (`isKey()` + `remove()`) des emplacements inutilisés.
  3. Écriture de `head`, `count`, `gen` (monotone) et `act`.
  4. Écriture en dernier du marqueur de validation FNV-1a (`ak`/`bk`).
- Au redémarrage (`begin()`), les deux banques sont contrôlées (bornes + UTF-8 + somme FNV-1a) et la banque valide de génération la plus élevée est retenue.
- Si un espace v0.1.0 (`magic = 0x564A3031`) est détecté, `migrateFromV1()` transfère les observations existantes dans leur ordre chronologique vers la banque A avant d'écrire `v2`.

---

## 5. Échanges avec VoiceKit (révision `16646c73acf01a8d57408391bc6de15a288b8910`)

Configuration appliquée dans `main.cpp` :
- `provider = VoiceProviderType::GEMINI`
- `wifiMode = WiFiMode::MANAGED_WIFI`
- `voice = "Aoede"`, `language = "fr-FR"`, `volume = 70`
- `enableConversation = false` (aucune réponse IA automatique après STT)
- `systemInstruction = VOICEKIT_DEFAULT_INTEGRATED_INSTRUCTION`
- `aiPromptPrefix = ""` (le contexte laboratoire est porté directement par la requête `askAi()`)
- `queuePolicy = QueuePolicy::REJECT_IF_BUSY` (file courte gérée côté application)

API publiques utilisées sur `<VoiceKit.h>` :
- Cycle de vie : `begin()`, `update()`, `stop()`, `isReady()`, `isBusy()`, `getState()`, `getLastError()`
- STT : `listen()`, `stopListening()`, `cancelListening()`, `isListening()`, `isTranscribing()`
- Corrélation : `getActiveCaptureTurnId()`, `getPendingTranscriptionTurnId()`, `getLastCaptureTurnId()`
- TTS / IA : `say()`, `askAi()`
- Callbacks : `onReady()`, `onError()`, `onStateChanged()`, `onListeningStartedWithTurnId()`, `onListeningFinished()`, `onTranscriptionWithTurnId()`, `onAssistantText()`, `onSpeechStarted()`, `onSpeechFinished()`
