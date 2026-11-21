# Rapport correctif — VoiceJournal v0.1.0.1 Corrective

**Date :** 21 novembre 2026  
**Dépôt :** `https://github.com/calmtechnology-ch/VoiceJournal`  
**Cible matérielle :** M5Stack CoreS3 (ESP32-S3, M5GFX 320 × 240, PlatformIO, C++17)

---

## 1. Version et commit de départ

- **Version de départ :** `VoiceJournal v0.1.0`
- **Commit de départ (`HEAD`) :** `a6ceccd6e3cc2bcf206cc43652f5fb6a4c9a9ace` (*`feat: implement JournalUI for M5Stack CoreS3 display rendering and touch handling`*)
- **Commit initial :** `23b2bfe6be4c7d529d02766f6e66366889070179` (*`feat: initial release VoiceJournal v0.1.0 for M5Stack CoreS3`*)
- **Version produite :** `VoiceJournal v0.1.0.1 Corrective`

---

## 2. Liste des fichiers modifiés et ajoutés

### Fichiers modifiés
- `platformio.ini` — Épinglage de la dépendance VoiceKit sur la révision Git vérifiée `16646c73acf01a8d57408391bc6de15a288b8910`.
- `include/AppConfig.h` — Passage de `APP_VERSION` / `APP_TITLE` à `0.1.0.1` / `VoiceJournal v0.1.0.1`.
- `include/JournalUI.h` & `src/JournalUI.cpp` — Priorité absolue de `STOP`, séparation de l'anti-rebond de `PARLER`, détection de nouveau contact physique et extraction de `processTouchSample()` testable hors matériel.
- `include/JournalStorage.h` & `src/JournalStorage.cpp` — Double instantané borné A/B avec marqueur de validation FNV-1a écrit en dernier, `isKey()` sur `IKeyValueStore`, contrôle complet des écritures et `remove()`, diagnostic explicite de corruption et migration non destructive depuis v0.1.0.
- `include/VoiceController.h` & `src/VoiceController.cpp` — Suppression du double appel `cancelListening()` + `stop()`, idempotence des `STOP` successifs, `maxInvalidatedTurnId_`, filtrage des `onAssistantText` / `onSpeechStarted` tardifs (`ttsPlaybackActive_`), retour à `READY` conditionné à l'état réel du moteur (`transitionToEngineActualState()`).
- `src/main.cpp` — Mise à jour de l'en-tête vers v0.1.0.1.
- `tests/mocks/MockKeyValueStore.h` — Ajout de `isKey()` et d'injections d'échecs ciblées (par clé, sur `remove()`, et coupures simulées après *k* opérations persistantes).
- `tests/mocks/MockVoiceEngine.h` — Ajout de la simulation de libération asynchrone (`deferReadyAfterCancel`) et de `simulateSpeechStarted()`.
- `tests/test_main.cpp` — Extension à 29 tests natifs C++17 (incluant `JournalUI.cpp`, tests paramétrés de coupure NVS et scénarios `VoiceController`).
- `tests/TEST_PLAN.md`, `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE_VALIDATION.md`.

### Fichiers ajoutés
- `LICENSE` — Licence MIT dédiée au projet VoiceJournal (`Copyright (c) 2026 Philippe AD / calmtechnology-ch (VoiceJournal)`).
- `docs/CORRECTIVE_REPORT_v0.1.0.1.md` — Le présent rapport.

> **Intégrité VoiceKit :** Aucun fichier de la bibliothèque VoiceKit n'a été modifié.

---

## 3. Diagnostic précis de chaque problème

### Problème 1 — Priorité tactile de `STOP` (`src/JournalUI.cpp`)
Dans v0.1.0, `JournalUI::pollTouch()` appliquait un anti-rebond global `lastTouchAcceptedMs_` de 180 ms avant même d'identifier la zone touchée. Un appui sur `STOP` survenant moins de 180 ms après `PARLER` était silencieusement rejeté. Par ailleurs, la logique de décision était couplée à `M5.Touch.getDetail()` et non couverte par les tests natifs.

### Problème 2 — Persistance NVS non vérifiée et sensible aux coupures (`src/JournalStorage.cpp`)
- `clear()` ignorait les valeurs de retour de `store_->remove(keyBuf)`.
- `begin()` ignorait les valeurs de retour des écritures de récupération/recompactage (`putString`, `putUInt`) et retournait `true` même en cas d'échec persistant ou réinitialisait silencieusement des métadonnées hors bornes.
- Le tampon circulaire à jeu unique de clés (`n0..n4`, `head`, `count`) exposait le carnet à une divergence RAM/NVS ou à un état partiellement écrit lors d'une coupure d'alimentation entre l'écriture de la charge utile et celle des métadonnées.

### Problème 3 — Double arrêt `STOP`, callbacks tardifs et état réel du moteur (`src/VoiceController.cpp`)
- Dans `onStopButtonPressed()`, l'enchaînement `engine_.cancelListening()` suivi immédiatement de `engine_.stop()` présentait un défaut critique vis-à-vis de `VoxCoreVoice::stop()` : `cancelListening()` remettait `isListening()` et `isTranscribing()` à faux puis mémorisait `_cancelledCaptureTurnId`, mais l'appel immédiat à `stop()` exécutait alors la branche `else { _cancelledCaptureTurnId = 0; }` et invoquait `_session.requestStop()` inutilement.
- Deux `STOP` consécutifs appelaient `stop()` à répétition.
- `onSpeechStarted` et `VoiceEngineState::SPEAKING` pouvaient perturber un nouveau tour `LISTENING`/`TRANSCRIBING` si un événement audio tardif d'une requête IA annulée arrivait.
- Les watchdogs (`TRANSCRIPTION_WATCHDOG_MS`, `AI_RESPONSE_TIMEOUT_MS`, `TTS_WATCHDOG_MS`) forçaient `DISCONNECTED` si `!engine_.isReady()`, même lorsque le moteur était simplement en cours de libération asynchrone (`STOPPING`).

### Problème 4 — Reproductibilité et gouvernance GitHub
- `platformio.ini` référençait `#main` pour VoiceKit (référence flottante non reproductible).
- Le dépôt ne comportait pas de fichier `LICENSE`.

---

## 4. Modifications apportées

1. **`JournalUI` (Correction 1)** :
   - Extraction de `JournalUI::processTouchSample(const TouchSample&, uint32_t nowMs)`.
   - Verrou de maintien `touchDown_` : un contact physique maintenu n'émet qu'un seul événement jusqu'au relâchement.
   - `STOP` est évalué en premier dans tous les états sans jamais consulter l'anti-rebond de `PARLER`.
   - `PARLER / TERMINER` conserve son anti-rebond dédié (`AppConfig::TOUCH_DEBOUNCE_MS = 180 ms`) et reste bloqué hors `READY` / `LISTENING`.
2. **`JournalStorage` (Correction 2)** :
   - Double instantané borné A/B dans le namespace `"vjournal"` : banques `a` (`a0..a4`, `ah`, `ac`, `ag`, `ak`) et `b` (`b0..b4`, `bh`, `bc`, `bg`, `bk`), référence active `act`, version `v2 = 0x564A3032`.
   - `commitSnapshot()` invalide d'abord le marqueur de la banque inactive (`ak`/`bk = 0`), écrit les notes actives, supprime de manière vérifiée (`isKey()` + `remove()`) les clés inutilisées, écrit `head`, `count`, `gen` et `act`, puis écrit **en dernier** la somme de contrôle FNV-1a (`ak`/`bk`).
   - `begin()` sélectionne la banque valide de génération maximale, diagnostique explicitement toute corruption, refuse de réinitialiser silencieusement ou de retourner `true` si une récupération échoue, et migre de façon non destructive tout carnet v0.1.0 (`magic = 0x564A3031`).
   - `clear()` valide d'abord un instantané vide de génération supérieure dans la banque inactive (empêchant toute résurrection après redémarrage), met à jour la RAM en cohérence, puis vérifie chaque `remove()` de l'ancienne banque.
   - Aucune fausse allégation d'atomicité matérielle multi-clés avec `Preferences`.
3. **`VoiceController` (Correction 3)** :
   - `onStopButtonPressed()` appelle uniquement `cancelListening()` en `LISTENING`/`TRANSCRIBING`, uniquement `stop()` en `ANALYZING`/`SPEAKING`, et n'effectue aucun appel moteur lors d'un `STOP` redondant quand le moteur est déjà libre.
   - `maxInvalidatedTurnId_` rejette tout `turnId` antérieur ou égal au dernier tour invalidé.
   - `ttsPlaybackActive_` + `aiRequestActive_` filtrent tout `onAssistantText`, `onSpeechStarted` ou `VoiceEngineState::SPEAKING` tardif.
   - `transitionToEngineActualState()` garantit que le contrôleur ne passe à `READY` qu'après confirmation effective de `engine_.isReady()`, y compris après `STOP`, reconnexion et timeouts.
4. **Dépendances & documentation (Corrections 4 & 7)** :
   - Épinglage VoiceKit dans `platformio.ini`, ajout de `LICENSE` (MIT), mise à jour de toute la documentation et ajout de `VJ-19` à `VJ-24` dans `docs/HARDWARE_VALIDATION.md`.

---

## 5. Compatibilité des API VoiceKit

Vérifiée par inspection directe de `VoiceKit.h`, `VoiceKit.cpp` et `VoxCoreVoice.cpp` (révision `16646c73acf01a8d57408391bc6de15a288b8910`) et confirmée par la compilation et l'édition de liens PlatformIO :
- Façade publique utilisée exclusivement (`<VoiceKit.h>` via `VoiceKitEngineAdapter`).
- `begin()`, `update()`, `listen()`, `stopListening()`, `cancelListening()`, `say()`, `askAi()`, `stop()`.
- `isReady()`, `isBusy()`, `isListening()`, `isTranscribing()`, `getState()`, `getLastError()`.
- `getActiveCaptureTurnId()`, `getPendingTranscriptionTurnId()`, `getLastCaptureTurnId()`.
- Callbacks : `onReady`, `onError`, `onStateChanged`, `onListeningStartedWithTurnId`, `onListeningFinished`, `onTranscriptionWithTurnId`, `onAssistantText`, `onSpeechStarted`, `onSpeechFinished`.

---

## 6. Résultats des tests natifs

Commande exécutée depuis `VoiceJournal_CoreS3/` :

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Itests\mocks -Iinclude `
    src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp src\JournalUI.cpp tests\test_main.cpp `
    -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

**Résultat : `29 PASS, 0 FAIL`** (zéro avertissement avec `-Wall -Wextra -Werror`) :
- `JournalStorage` : **11 / 11 PASS** (dont injections d'échecs `note`/`head`/`count`/`remove`, coupures simulées à chaque étape `0..12` de `addNote()` et `0..16` de `clear()`, corruption/diagnostic `begin()` et migration v0.1.0 de 5 notes en rotation).
- `CommandRouter` : **3 / 3 PASS** (régression complète).
- `JournalUI` : **3 / 3 PASS** (`STOP` à 50 ms et 100 ms après `PARLER`, rejet second `PARLER` à 50 ms, maintien `STOP` = 1 événement, disponibilité dans tous les états actifs).
- `VoiceController` : **12 / 12 PASS** (`STOP` pendant `LISTENING`/`TRANSCRIBING`/`ANALYZING`/`SPEAKING`, `STOP` puis `PARLER` avec libération asynchrone, deux `STOP` consécutifs, `STOP` puis reconnexion, finale tardive, réponse IA tardive après nouvelle capture, watchdogs STT/IA/TTS).

---

## 7. Résultat de compilation PlatformIO

Commande exécutée depuis `VoiceJournal_CoreS3/` :

```bash
pio run -e m5stack-cores3
```

**Résultat : `BUILD PASS` (`[SUCCESS] Took 69.11 seconds`)**

- **Plateforme :** `espressif32@6.9.0` (`framework-arduinoespressif32 @ 3.20017.241212`, `toolchain-xtensa-esp32s3 @ 8.4.0`)
- **Norme C++ :** `-std=gnu++17`
- **Dépendances résolues :** `M5CoreS3 @ 1.0.0`, `M5Unified @ 0.2.23`, `M5GFX @ 0.3.0`, `VoiceKit @ 0.6.7+2.sha.16646c7`, `Preferences @ 2.0.0`
- **Linkage :** aucune erreur
- **Taille du firmware :**
  - **RAM :** `25.9%` (`84 916 octets` / `327 680 octets`)
  - **Flash :** `82.9%` (`1 739 393 octets` / `2 097 152 octets`, partition `no_ota.csv`)
- **Secrets :** `include/secrets.h` est exclu par `.gitignore` et aucun secret n'est présent dans les sources suivies par Git.

---

## 8. État des tests matériels

- **Statut global :** **`0 PASS` / `0 FAIL` / `24 NOT RUN`** (`VJ-01` à `VJ-24` dans `docs/HARDWARE_VALIDATION.md`).
- **Rappel :** Aucun essai n'a été exécuté sur un M5Stack CoreS3 physique durant cette phase corrective. La compilation réussie et les tests natifs préparent la campagne matérielle mais ne la remplacent pas.

---

## 9. Limitations résiduelles

- **Validation matérielle non encore exécutée** : comportement réel du contrôleur tactile capacitif CoreS3, des temps de libération matérielle micro ES7210 / haut-parleur AW88298 et des coupures secteur réelles sur NVS à confirmer lors de la campagne `VJ-01`..`VJ-24`.
- **Atomicité NVS** : `Preferences` ne dispose pas de transaction multi-clés atomique matérielle ; le schéma A/B protège contre les coupures entre clés successives en s'appuyant sur l'intégrité élémentaire d'une écriture `putUInt` NVS d'ESP-IDF.
- **Empreinte NVS** : le double instantané stocke au maximum 2 × 5 notes (≤ 1,8 Ko de charges utiles + métadonnées), ce qui reste très inférieur à la partition NVS standard (20 Ko), mais suppose que le namespace `"vjournal"` reste dédié à VoiceJournal.
- **Anomalies non reproductibles hors matériel** : rebonds électriques propres à la dalle tactile physique, latences réseau réelles Gemini Live lors d'une coupure Wi-Fi brutale et libération DMA I2S réelle après `cancelListening()` / `stop()`.

---

## 10. Révision Git de VoiceKit utilisée

- **Dépôt :** `https://github.com/calmtechnology-ch/VoiceKit.git`
- **Révision vérifiée et figée :** `16646c73acf01a8d57408391bc6de15a288b8910`
- **Sujet du commit :** `Merge pull request #1 from calmtechnology-ch/release/v0.6.7.2-preparation`
- **Vérification distante :** confirmée accessible via `git ls-remote` et installée avec succès par PlatformIO lors du build `m5stack-cores3`.
