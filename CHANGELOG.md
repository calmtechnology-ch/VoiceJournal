# Changelog — VoiceJournal

Toutes les évolutions notables de **VoiceJournal** sont consignées dans ce fichier.

## [0.1.1] - 2026-11-23 — Multi-fournisseurs (OpenAI par défaut + Gemini) & Correctifs de configuration

### Ajouté
- **Support multi-fournisseurs OpenAI Realtime (par défaut) et Google Gemini Live** :
  - `AppConfig` : ajout de l'énumération `AiProvider` (`OPENAI` par défaut, voix `alloy` ; `GEMINI` en option, voix `Aoede`), constante unique `AppConfig::DEFAULT_AI_PROVIDER` et `defaultVoiceForProvider()` `constexpr`.
  - `include/secrets.example.h` et `src/main.cpp` : prise en charge de `OPENAI_KEY` / `OPENAI_API_KEY` et `GEMINI_KEY` / `GEMINI_API_KEY`, transmises simultanément à `VoiceKitConfig` (`openAiApiKey` / `geminiApiKey`) avec filtrage des valeurs modèles (`VOTRE_CLE_API_*`, `MA_CLE_OPENAI`, `MA_CLE_GEMINI`).
  - `JournalUI` : ajout d'un sélecteur tactile `[IA: OPENAI]` / `[IA: GEMINI]` dans l'en-tête de carte (paysage 320 × 240, rotation 0), actif hors activité vocale (`READY`, `DISCONNECTED`, `ERROR_STATE`) avec anti-rebond dédié (`350 ms`) sans altérer la priorité de `STOP`.
  - `VoiceController` & `VoiceKitEngineAdapter` : bascule dynamique OpenAI ↔ Gemini (`onSwitchProviderRequested()` / `selectProvider()`), y compris en reprise depuis `DISCONNECTED` ou `ERROR_STATE`, sans jamais toucher aux notes NVS, avec vérification préalable de la présence de la clé API cible.
  - **Tests natifs C++17** : extension à **34 tests** couvrant OpenAI par défaut, Gemini comme fournisseur initial, la sélection du profil vocal, la gestion des clés API (absente au démarrage ou secondaire absente), la bascule tactile/contrôleur OpenAI ↔ Gemini, la reprise après déconnexion/erreur et le refus si clé manquante ou moteur occupé.
  - **Validation matérielle (`docs/HARDWARE_VALIDATION.md`)** : ajout des scénarios `VJ-25` à `VJ-28` (statut `NOT RUN`).

### Corrigé
- **Source unique de configuration fournisseur** : suppression de la surcharge `VOICEJOURNAL_DEFAULT_PROVIDER` dans `src/main.cpp` afin que `AppConfig::DEFAULT_AI_PROVIDER` dans `include/AppConfig.h` soit l'unique point de configuration du fournisseur au démarrage ; alignement de `AppConfig::VOICE_NAME` sur `defaultVoiceForProvider(DEFAULT_AI_PROVIDER)`.
- **Validation explicite de la clé API au démarrage** : `src/main.cpp` signale immédiatement une erreur explicite (`Cle API ... manquante (secrets.h)`) si le fournisseur initial n'a pas de clé valide, tout en permettant le fonctionnement normal si seule la clé secondaire manque et en autorisant la bascule vers le fournisseur secondaire si sa clé est présente.
- **Cohérence du fournisseur affiché au démarrage et à la transition `READY`** : `VoiceKitEngineAdapter::getProvider()` retourne `AppConfig::DEFAULT_AI_PROVIDER` tant que VoiceKit est `UNINITIALIZED`, et `VoiceController::onStateChanged(READY)` synchronise `view_.provider` avec le moteur.
- **Documentation synchronisée sur v0.1.1** : mise à jour de `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE_VALIDATION.md` et `tests/TEST_PLAN.md`.

## [0.1.0.1] - 2026-11-21 — Corrective

Version corrective résolvant les défauts identifiés lors de l'audit du 21 novembre 2026 et préparant la campagne de validation sur **M5Stack CoreS3** réel.

### Corrigé
- **`JournalUI` — Priorité réelle du bouton `STOP`** :
  - Suppression de l'anti-rebond global partagé qui bloquait `STOP` pendant 180 ms après un clic sur `PARLER`.
  - Évaluation de `STOP` en premier, indépendante de l'anti-rebond dédié à `PARLER / TERMINER` (`180 ms`).
  - Détection de nouveau contact physique (`processTouchSample`) empêchant les déclenchements multiples lorsqu'un doigt reste maintenu sur `STOP` tout en autorisant `PARLER` puis `STOP` immédiatement (50 ms / 100 ms).
- **`JournalStorage` — Fiabilisation de la persistance NVS** :
  - Adoption d'un mécanisme à double instantané borné A/B (`a0..a4`/`b0..b4`, métadonnées de banque, compteur de génération monotone et marqueur de validation FNV-1a écrit en dernier) sans prétendre à une atomicité matérielle multi-clés de `Preferences`.
  - Vérification systématique de toutes les écritures et suppressions (`isKey()` + `remove()`) dans `begin()`, `addNote()` et `clear()`.
  - `begin()` ne masque plus les échecs de récupération ni ne réinitialise silencieusement des données existantes, et produit un diagnostic explicite en cas de corruption.
  - `clear()` garantit la cohérence RAM/NVS et empêche toute résurrection de notes supprimées après redémarrage.
  - Migration automatique et non destructive des carnets existants au format v0.1.0 (`magic = 0x564A3031`, `n0..n4`, y compris 5 notes après rotation circulaire).
- **`VoiceController` — Interruption `STOP`, reconnexion et transcriptions tardives** :
  - Suppression de l'appel redondant `engine_.stop()` après `engine_.cancelListening()` lors d'un `STOP` en `LISTENING` / `TRANSCRIBING`, qui effaçait `_cancelledCaptureTurnId` dans `VoxCoreVoice` et sollicitait inutilement `_session.requestStop()`.
  - Idempotence complète de deux `STOP` successifs (aucun appel moteur redondant).
  - Suivi monotone des tours invalidés (`maxInvalidatedTurnId_`) et filtrage strict de `onAssistantText` / `onSpeechStarted` / `VoiceEngineState::SPEAKING` pour ignorer toute réponse IA ou audio tardive après `STOP` ou après le démarrage d'une nouvelle capture.
  - Passage à `READY` uniquement après confirmation de l'état réel du moteur (`transitionToEngineActualState()`), y compris après `STOP`, reconnexion et expiration des watchdogs (`TRANSCRIPTION_WATCHDOG_MS`, `AI_RESPONSE_TIMEOUT_MS`, `TTS_WATCHDOG_MS`).

### Modifié / Ajouté
- **`platformio.ini`** : dépendance VoiceKit figée sur la révision Git vérifiée `16646c73acf01a8d57408391bc6de15a288b8910` (préparation `v0.6.7.2`) pour une reproductibilité stricte.
- **`LICENSE`** : ajout de la licence MIT pour le projet VoiceJournal.
- **Tests natifs C++17 (`tests/test_main.cpp`, `tests/TEST_PLAN.md`)** : extension à 29 scénarios couvrant `JournalStorage` (dont injections d'échec ciblées, coupures simulées à chaque étape persistante et migration v0.1.0), `CommandRouter`, `JournalUI` et `VoiceController`.
- **Validation matérielle (`docs/HARDWARE_VALIDATION.md`)** : ajout des scénarios ciblés `VJ-19` à `VJ-24` (statut `NOT RUN` en attente d'essais sur CoreS3 physique).
- **Rapport correctif** : ajout de `docs/CORRECTIVE_REPORT_v0.1.0.1.md`.

## [0.1.0] - 2026-11-21

### Ajouté
- Projet PlatformIO autonome `VoiceJournal_CoreS3` ciblant **M5Stack CoreS3 (ESP32-S3)** en C++17.
- Intégration de la façade publique `<VoiceKit.h>` (**VoiceKit v0.6.7.2**) avec **Google Gemini Live** en mode non conversationnel (`enableConversation = false`).
- `JournalStorage` : tampon circulaire persistant en NVS (`Preferences`, namespace `"vjournal"`, 5 observations max, 180 octets UTF-8 max, écriture charge utile avant métadonnées, protection UTF-8).
- `CommandRouter` : routeur de commandes vocales déterministe (`NOTE`, `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR` avec confirmation explicite temporisée et annulation).
- `VoiceController` : machine à états applicative explicite (`JournalState`), corrélation stricte des tours STT par `turnId`, file TTS courte, analyse Gemini bornée avec séparation données/instructions, `STOP` prioritaire non destructif et watchdogs non bloquants.
- `JournalUI` : interface tactile M5GFX paysage 320 × 240 (sans LVGL 9) avec anti-rebond tactile, affichage hors ligne du compteur de notes et boutons `PARLER / TERMINER` + `STOP`.
- Diagnostics série structurés à 115 200 bauds (`[VJ][...]`) et diagnostic mémoire périodique configurable.
- Suite de tests unitaires natifs C++17 (`tests/test_main.cpp`, `tests/TEST_PLAN.md`) couvrant `JournalStorage`, `CommandRouter` et `VoiceController`.
- Documentation complète : `README.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE_VALIDATION.md` (grille `VJ-01` à `VJ-18`).
