# Plan de tests automatiques — VoiceJournal v0.1.0

## 1. Objectif

Valider de manière déterministe et reproductible toute la logique métier de **VoiceJournal v0.1.0** qui ne dépend pas du matériel ESP32-S3, en compilant les véritables sources de production (`JournalStorage.cpp`, `CommandRouter.cpp`, `VoiceController.cpp`) en C++17 natif avec des mocks dédiés situés dans `tests/mocks/`.

## 2. Exécution des tests natifs

Depuis `VoiceJournal_CoreS3/` :

```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror `
    -Itests\mocks -Iinclude `
    src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp tests\test_main.cpp `
    -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

## 3. Couverture fonctionnelle

### `JournalStorage`
1. Carnet initialement vide.
2. Ajout d'une note, de 5 notes, puis d'une 6e note (rotation circulaire).
3. Conservation de l'ordre chronologique.
4. Lecture de la dernière note.
5. Rejet des textes vides ou composés uniquement d'espaces.
6. Rejet des notes dépassant 180 octets UTF-8.
7. Préservation des caractères UTF-8 et troncature sans coupure de séquence multioctet.
8. Reprise cohérente après redémarrage simulé.
9. Préservation de l'état en cas d'échec simulé d'écriture NVS.
10. Effacement complet persistant (`clear()`).

### `CommandRouter`
1. Reconnaissance de `NOTE` uniquement en préfixe explicite, avec préservation des accents/casse du contenu.
2. Rejet d'une observation vide après `NOTE` et non-confusion avec le mot « note » en milieu de phrase.
3. Reconnaissance de `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES` (y compris variantes « Fais le bilan », « Résume les observations », « Qu'avons-nous observé ? »).
4. Rejet des commandes inconnues (`UNKNOWN`).
5. Gestion de la casse, des accents et de la ponctuation.
6. Confirmation obligatoire pour `CLEAR` (refus d'un simple « oui »).
7. Annulation de `CLEAR` et expiration automatique après délai.

### `VoiceController`
1. Une transcription finale entraîne au maximum une action (anti-doublon par `turnId`).
2. Une transcription partielle ne déclenche aucune action métier.
3. Une transcription annulée ou obsolète est ignorée.
4. `STOP` invalide le tour en attente, coupe le micro/audio et conserve les notes NVS.
5. Une reconnexion ne redéclenche jamais la dernière commande.
6. Refus des requêtes concurrentes (ex. clic `PARLER` pendant `TRANSCRIBING` ou hors ligne).
7. Analyse IA via `askAi()` avec séparation données/instructions (`<OBSERVATIONS_UTILISATEUR>`) et sans rappel `say()` sur `onAssistantText`.
8. Reprise propre après timeout de transcription sans blocage permanent.
