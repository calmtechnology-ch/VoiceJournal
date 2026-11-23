# Architecture technique — VoiceJournal v0.1.1

## 1. Vue d'ensemble

VoiceJournal v0.1.1 est une application embarquée modulaire construite **au-dessus de la façade publique `<VoiceKit.h>`** (VoiceKit révision Git `16646c73acf01a8d57408391bc6de15a288b8910`), sans aucune modification de la bibliothèque et sans duplication de ses moteurs audio, WebSocket ou réseau. Elle prend en charge **OpenAI Realtime** (fournisseur par défaut) et **Google Gemini Live**, avec sélection initiale centralisée dans `include/AppConfig.h` et bascule dynamique depuis l'écran tactile.

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
| - STOP prioritaire sans delai    |   | - Bascule OpenAI / Gemini  |
| - Selecteur IA [OPENAI / GEMINI] |<--| - Correlation turnId STT   |
| - Anti-rebond dedie PARLER 180ms |   | - File TTS bornee (4)      |
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
                 +--------------------+   | - Migration v0.1.0 |   |   applyVoiceProfile|
                                          +--------------------+   |   askAi/update     |
                                                                   +--------------------+
```

---

## 2. Responsabilités des modules

| Module | Responsabilité principale | Dépendances |
|---|---|---|
| `AppConfig.h` | Constantes, fournisseur par défaut (`DEFAULT_AI_PROVIDER`), profils vocaux (`defaultVoiceForProvider`), durées watchdogs, prompt système IA, enums `AiProvider` et `JournalState` | Standard C++ |
| `JournalStorage` | Double instantané borné A/B (5 notes × 180 octets UTF-8) en NVS `"vjournal"`, validation FNV-1a, contrôle strict des écritures/suppressions, migration depuis v0.1.0 | `Preferences` (via `IKeyValueStore`) |
| `CommandRouter` | Analyse purement déterministe des transcriptions, gestion de l'état de confirmation `CLEAR` | `AppConfig.h` |
| `VoiceController` | Machine à états applicative, bascule OpenAI ↔ Gemini avec vérification de clé API, corrélation stricte des tours STT (`turnId` / `maxInvalidatedTurnId_`), file TTS, analyse IA, `STOP` ciblé non destructif, watchdogs récupérables | `IVoiceEngine`, `JournalStorage`, `CommandRouter` |
| `JournalUI` | Rendu M5GFX 320 × 240 (rotation `0`), sélecteur tactile `[IA: OPENAI / GEMINI]` (anti-rebond 350 ms), filtrage tactile testable (`processTouchSample`) avec priorité absolue de `STOP`, anti-rebond dédié à `PARLER` (180 ms), verrou de maintien | `M5Unified` |
| `main.cpp` | Assemblage des composants, lecture exclusive de `AppConfig::DEFAULT_AI_PROVIDER`, validation de clé API initiale, configuration `VoiceKitConfig`, boucle principale non bloquante, diagnostic mémoire | Tous les modules + `<VoiceKit.h>` |

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

### Invariants garantis (v0.1.1)
1. **Priorité réelle de `STOP` dans `JournalUI`** :
   - `STOP` est testé avant `PARLER` et le sélecteur IA, et n'est jamais bloqué par leurs anti-rebonds (`180 ms` / `350 ms`).
   - Un contact maintenu (`touchDown_`) n'émet qu'un seul événement jusqu'au relâchement.
   - Deux contacts distincts peuvent produire `PARLER` puis `STOP` à 50 ms ou 100 ms d'intervalle.
2. **Bascule multi-fournisseurs sécurisée (`OpenAI` ↔ `Gemini`)** :
   - Autorisée uniquement hors activité vocale (`READY`, `DISCONNECTED`, `ERROR_STATE`) et refusée si le moteur est occupé ou en capture.
   - Vérifie la présence d'une clé API valide pour le fournisseur cible avant toute déconnexion ; en cas d'absence, conserve le fournisseur actif et affiche une erreur explicite.
   - Libère proprement la session précédente via `VoiceKit::applyVoiceProfile()`, passe en `CONNECTING` et ne revient à `READY` qu'après confirmation effective du moteur (`onReady` / `engine_.isReady()`).
   - Ne modifie jamais `JournalStorage`.
3. **Pas de double capture** : `PARLER` est refusé hors de `READY` ou si `engine.isBusy()` / `ttsQueue` non vide / tour non clos.
4. **Isolation des callbacks** : `onTranscriptionWithTurnId` ne réalise aucune opération NVS ni graphique bloquante ; la transcription finale valide est déposée dans un emplacement unique et traitée dans `VoiceController::update()`.
5. **Corrélation stricte des tours STT** :
   - `expectedTurnId_` est capturé via `onListeningStartedWithTurnId(turnId)` avec repli sur `getActiveCaptureTurnId()` / `getPendingTranscriptionTurnId()` / `getLastCaptureTurnId()`.
   - Toute transcription partielle (`isFinal == false`) met uniquement à jour l'affichage sans déclencher d'action.
   - Toute transcription finale appartenant à un tour annulé (`turnId <= maxInvalidatedTurnId_`), déjà traité (`lastProcessedTurnId_`) ou différent de `expectedTurnId_` est ignorée.
6. **STOP ciblé, idempotent et non destructif** :
   - Pendant `LISTENING` ou `TRANSCRIBING` : appelle uniquement `cancelListening()` (sans appeler `stop()` ensuite, préservant `_cancelledCaptureTurnId` dans `VoxCoreVoice` et évitant toute perturbation de session).
   - Pendant `ANALYZING` ou `SPEAKING` : appelle uniquement `stop()`.
   - Sur un second `STOP` consécutif (moteur déjà libre) : aucun appel moteur redondant.
   - Ne revient à `READY` qu'après confirmation de l'état réel du moteur (`engine_.isReady()`) ; reste en `STOPPING` si la libération est asynchrone.
   - Ne modifie jamais `JournalStorage`.
7. **Filtrage des réponses IA et audio tardives** :
   - `onAssistantText` est ignoré si `!aiRequestActive_`.
   - `onSpeechStarted` et `VoiceEngineState::SPEAKING` sont ignorés si ni `aiRequestActive_` ni `ttsPlaybackActive_` n'est actif, ou pendant `LISTENING` / `TRANSCRIBING` / `STOPPING`.
8. **Reconnexion sans rejeu** :
   - `onReady()` invalide tout tour antérieur, annule toute confirmation `CLEAR`, vide la file TTS locale, synchronise le fournisseur actif et ne passe à `READY` qu'après confirmation `engine_.isReady()`.

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
- `provider = VoiceKitEngineAdapter::toVoiceKitProvider(AppConfig::DEFAULT_AI_PROVIDER)` (`VoiceProviderType::OPENAI` par défaut, ou `VoiceProviderType::GEMINI`)
- `openAiApiKey` / `geminiApiKey` renseignées depuis `include/secrets.h` (après filtrage des valeurs modèles), `apiKey` alignée sur le fournisseur initial
- `wifiMode = WiFiMode::MANAGED_WIFI`
- `voice = defaultVoiceForProvider(initialProvider)` (`"alloy"` pour OpenAI, `"Aoede"` pour Gemini), `language = "fr-FR"`, `volume = 70`
- `enableConversation = false` (aucune réponse IA automatique après STT)
- `systemInstruction = AppConfig::VOICE_SYSTEM_INSTRUCTION`
- `aiPromptPrefix = ""` (le contexte laboratoire est porté directement par la requête `askAi()`)
- `queuePolicy = QueuePolicy::REJECT_IF_BUSY` (file courte gérée côté application)

API publiques utilisées sur `<VoiceKit.h>` :
- Cycle de vie : `begin()`, `update()`, `stop()`, `isReady()`, `isBusy()`, `getState()`, `getLastError()`
- Fournisseurs & profils : `getVoiceProfile()`, `applyVoiceProfile()`, `getProviderType()`, `hasProviderApiKey()`
- STT : `listen()`, `stopListening()`, `cancelListening()`, `isListening()`, `isTranscribing()`
- Corrélation : `getActiveCaptureTurnId()`, `getPendingTranscriptionTurnId()`, `getLastCaptureTurnId()`
- TTS / IA : `say()`, `askAi()`
- Callbacks : `onReady()`, `onError()`, `onStateChanged()`, `onListeningStartedWithTurnId()`, `onListeningFinished()`, `onTranscriptionWithTurnId()`, `onAssistantText()`, `onSpeechStarted()`, `onSpeechFinished()`
