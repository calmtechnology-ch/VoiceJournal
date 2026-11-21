# Changelog — VoiceJournal

Toutes les évolutions notables de **VoiceJournal** sont consignées dans ce fichier.

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
