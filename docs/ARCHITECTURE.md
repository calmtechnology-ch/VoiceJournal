# Architecture technique — VoiceJournal v0.1.0

## 1. Vue d'ensemble

VoiceJournal v0.1.0 est une application embarquée modulaire construite **au-dessus de la façade publique `<VoiceKit.h>`** (VoiceKit v0.6.7.2), sans aucune modification de la bibliothèque et sans duplication de ses moteurs audio, WebSocket ou réseau.

```text
+-------------------------------------------------------------------+
|                             main.cpp                              |
|        setup() / loop() : M5.update() + controller.update()       |
+-----------------+----------------------------------+--------------+
                  |                                  |
                  v                                  v
+----------------------------------+   +----------------------------+
|            JournalUI             |   |       VoiceController      |
| - M5GFX paysage 320x240          |   | - Machine a etats app      |
| - Anti-rebond tactile (180 ms)   |   | - Correlation turnId STT   |
| - Rendu conditionnel (dirty)     |<--| - File TTS bornee (4)      |
+----------------------------------+   | - Construction prompt IA   |
                                       | - STOP prioritaire         |
                                       | - Watchdogs non bloquants  |
                                       +---+----------+----------+--+
                                           |          |          |
                              +------------+          |          +------------+
                              v                       v                       v
                 +--------------------+   +--------------------+   +--------------------+
                 |   CommandRouter    |   |   JournalStorage   |   | VoiceKitEngineAdpt |
                 | - Normalisation FR |   | - Preferences NVS  |   | -> <VoiceKit.h>    |
                 | - Prefixe "note"   |   | - Namespace        |   |   listen/stop/say  |
                 | - Confirm CLEAR    |   |   "vjournal"       |   |   askAi/update     |
                 +--------------------+   | - Circulaire 5x180 |   +--------------------+
                                          +--------------------+
```

---

## 2. Responsabilités des modules

| Module | Responsabilité principale | Dépendances |
|---|---|---|
| `AppConfig.h` | Constantes, durées watchdogs, prompt système IA, enum `JournalState` | Standard C++ |
| `JournalStorage` | Tampon circulaire 5 notes × 180 octets UTF-8, persistance NVS `"vjournal"`, intégrité UTF-8 | `Preferences` (via `IKeyValueStore`) |
| `CommandRouter` | Analyse purement déterministe des transcriptions, gestion de l'état de confirmation `CLEAR` | `AppConfig.h` |
| `VoiceController` | Machine à états applicative, corrélation stricte des tours STT (`turnId`), file TTS, analyse Gemini, `STOP` | `IVoiceEngine`, `JournalStorage`, `CommandRouter` |
| `JournalUI` | Rendu M5GFX 320 × 240, gestion tactile avec anti-rebond, translittération d'affichage | `M5Unified` |
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

### Invariants garantis
1. **Pas de double capture** : `PARLER` est refusé hors de `READY` ou si `engine.isBusy()` / `ttsQueue` non vide / tour non clos.
2. **Isolation des callbacks** : `onTranscriptionWithTurnId` ne réalise aucune opération NVS ni graphique bloquante ; la transcription finale valide est déposée dans un emplacement unique et traitée dans `VoiceController::update()`.
3. **Corrélation stricte des tours STT** :
   - `expectedTurnId_` est capturé via `onListeningStartedWithTurnId(turnId)` avec repli sur `getActiveCaptureTurnId()` / `getPendingTranscriptionTurnId()` / `getLastCaptureTurnId()`.
   - Toute transcription partielle (`isFinal == false`) met uniquement à jour l'affichage sans déclencher d'action.
   - Toute transcription finale appartenant à un tour annulé (`invalidatedTurnId_`), déjà traité (`lastProcessedTurnId_`) ou différent de `expectedTurnId_` est ignorée.
4. **STOP prioritaire et non destructif** :
   - Invalide `expectedTurnId_`, annule la confirmation `CLEAR` en cours, vide la file TTS locale, réinitialise `aiRequestActive_`, appelle `cancelListening()` si micro actif puis `stop()`.
   - Ne modifie jamais `JournalStorage`.
5. **Reconnexion sans rejeu** :
   - `onReady()` invalide tout tour antérieur et vide la file TTS locale afin qu'une reconnexion ne redéclenche jamais une ancienne commande.

---

## 4. Échanges avec VoiceKit v0.6.7.2

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

---

## 5. Choix concernant la validation locale des captures (VAD)

VoiceKit v0.6.7.2 fournit `VoiceInteractionController` et `VoiceCaptureValidator`, conçus autour de `VoxCoreVoice` pour le flux conversationnel 2 clics. VoiceJournal v0.1.0 repose sur la façade publique `<VoiceKit.h>` en mode **non conversationnel** (`enableConversation = false`, 3 fonctions distinctes STT / TTS / IA).

Par conséquent, la v0.1.0 **n'intègre pas le filtrage VAD RMS avancé** et applique à la place les garde-fous locaux suivants :
- Rejet des transcriptions vides ou composées uniquement d'espaces.
- Rejet des captures annulées via `STOP`.
- Durée maximale de capture micro : **15 s** (arrêt automatique).
- Watchdog applicatif d'attente de transcription finale : **8 s**.
- Interdiction stricte de toute capture concurrente avant clôture du tour précédent.
