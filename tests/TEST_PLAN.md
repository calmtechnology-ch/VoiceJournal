# Plan de tests automatiques — VoiceJournal v0.1.1

## 1. Objectif

Valider de manière déterministe et reproductible toute la logique métier de **VoiceJournal v0.1.1** indépendante du matériel ESP32-S3, en compilant les véritables sources de production (`JournalStorage.cpp`, `CommandRouter.cpp`, `VoiceController.cpp`, `JournalUI.cpp`) en C++17 natif avec les mocks de `tests/mocks/`.

## 2. Exécution des tests natifs

Depuis `VoiceJournal_CoreS3/` :

```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror `
    -Itests\mocks -Iinclude `
    src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp src\JournalUI.cpp tests\test_main.cpp `
    -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

## 3. Couverture fonctionnelle (34 scénarios)

### `JournalStorage` (11 tests)
1. Carnet initialement vide.
2. Ajout d'une note, de 5 notes, puis d'une 6e note (rotation circulaire + ordre chronologique).
3. Rejet des textes vides et des notes dépassant 180 octets UTF-8.
4. Préservation des caractères UTF-8 et troncature sans coupure de séquence multioctet.
5. Reprise cohérente après redémarrage simulé.
6. Injections d'échecs ciblées sur l'écriture d'une note, la mise à jour de l'index circulaire (`head`) et la mise à jour du compteur (`count`).
7. Effacement persistant (`clear()`) et contrôle des échecs de `remove()` (avant et après validation de l'instantané vide, cohérence RAM/NVS, non-résurrection au redémarrage).
8. **Test paramétré** : coupure d'alimentation simulée après chaque étape persistante (`0..12`) lors d'un `addNote()` avec rotation circulaire.
9. **Test paramétré** : coupure d'alimentation simulée après chaque étape persistante (`0..16`) lors d'un `clear()`.
10. Échec de `begin()` sur erreur d'écriture, refus de réinitialisation silencieuse sur corruption totale, et récupération diagnostiquée des observations encore valides.
11. Migration non destructive d'un carnet v0.1.0 existant contenant 5 notes après rotation (`head != 0`).

### `CommandRouter` (3 tests)
1. Reconnaissance de `NOTE` uniquement en préfixe explicite, avec préservation des accents/casse et rejet vide/milieu de phrase.
2. Reconnaissance de `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES` (casse, accents, ponctuation) et rejet `UNKNOWN`.
3. Confirmation obligatoire pour `CLEAR` (refus d'un simple « oui »), annulation et expiration temporelle.

### `JournalUI` (4 tests)
1. Priorité de `STOP` 50 ms et 100 ms après un clic `PARLER` (non bloqué par l'anti-rebond de `PARLER`).
2. Anti-rebond de `PARLER` rejetant un second `PARLER` à 50 ms et l'acceptant après 180 ms.
3. Maintien prolongé sur `STOP` générant exactement un seul événement, disponibilité de `STOP` pendant `LISTENING`, `TRANSCRIBING`, `ANALYZING` et `SPEAKING`, et désactivation de `PARLER` hors états autorisés.
4. Sélecteur tactile de fournisseur IA `[IA: OPENAI / GEMINI]` : détection de zone, anti-rebond dédié (`350 ms`), activation en `READY` / `DISCONNECTED` / `ERROR_STATE` et désactivation pendant toute opération vocale active.

### `VoiceController` (16 tests)
1. Transcription partielle vs finale et exécution unique par `turnId`.
2. `STOP` pendant `LISTENING` (appel unique `cancelListening()`, aucun `stop()` redondant, aucune écriture NVS, finale tardive ignorée).
3. `STOP` pendant `TRANSCRIBING` + transcription finale tardive refusée, pas de redéclenchement automatique de l'écoute.
4. `STOP` pendant `ANALYZING` + réponse IA (`onAssistantText` et `onSpeechStarted`) tardive ignorée même après démarrage d'une nouvelle capture.
5. `STOP` pendant `SPEAKING` (interruption TTS, vidage file locale, aucune écriture NVS).
6. `STOP` puis `PARLER` avec libération asynchrone du moteur (`STOPPING` tant que `engine_.isReady()` est faux, puis `READY` et nouvelle capture réussie).
7. Deux `STOP` successifs idempotents (aucun appel moteur redondant, aucune écriture NVS).
8. `STOP` puis déconnexion/reconnexion : aucune commande annulée restaurée, aucun TTS répété, `PARLER` désactivé hors ligne, notes NVS intactes.
9. Refus des requêtes concurrentes (`PARLER` pendant `TRANSCRIBING`).
10. Analyse IA bornée (`<OBSERVATIONS_UTILISATEUR>`) sans double `say()`.
11. Timeout de transcription (`TRANSCRIPTION_WATCHDOG_MS`) puis nouvelle capture réussie.
12. Récupération contrôlée des watchdogs IA (`AI_RESPONSE_TIMEOUT_MS`) et TTS (`TTS_WATCHDOG_MS`).
13. Initialisation OpenAI par défaut, bascule OpenAI → Gemini → OpenAI, reprise après déconnexion et conservation des notes NVS.
14. Refus de bascule si clé API cible manquante ou si une opération vocale est en cours (`LISTENING`).
15. Configuration Gemini comme fournisseur initial (avec profils vocaux `alloy`/`Aoede`, absence de clé secondaire non bloquante, bascule vers OpenAI et nouvelle capture vocale).
16. Fournisseur initial sans clé valide (`ERROR_STATE` explicite conservé face aux événements `CONNECTING`), reprise par bascule vers le fournisseur secondaire puis capture vocale fonctionnelle après reconnexion.
