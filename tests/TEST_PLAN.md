# Plan de tests automatiques — VoiceJournal v0.2.0.1

## 1. Objectif

Valider de manière déterministe, reproductible et exhaustive toute la logique métier de **VoiceJournal v0.2.0.1** (stockage persistant, machine à états, bascule IA, interface tactile, moteur NLU et moteur de parsing contextuel) en C++17 natif sans matériel physique ESP32-S3.

La suite cumule **505 tests automatiques réussis à 100% (0 échec)** répartis en 3 suites ordonnées.

---

## 2. Exécution des tests natifs

Depuis le répertoire `VoiceJournal_CoreS3/` :

```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror `
    -Itests/mocks -Iinclude -Itests `
    src/JournalStorage.cpp src/CommandRouter.cpp src/VoiceController.cpp src/JournalUI.cpp `
    src/parser/*.cpp tests/parser/*.cpp tests/test_main.cpp `
    -o tests/build/voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

---

## 3. Décomposition de la suite de tests (505 assertions)

### Suite 1 : Tests historiques d'intégration système v0.1.1 (36 assertions)
1. **`JournalStorage` (11 tests)** :
   - Carnet initialement vide.
   - Rotation circulaire 1, 5 puis 6 observations et préservation chronologique.
   - Rejet des textes vides et des notes > 180 octets UTF-8.
   - Préservation des séquences multioctets UTF-8 et troncature sûre.
   - Reprise cohérente après redémarrage simulé.
   - Injections d'échecs ciblées sur l'écriture d'une note, l'index circulaire et le compteur.
   - Effacement persistant `clear()` et intégrité RAM/NVS.
   - Coupures d'alimentation simulées à chaque étape atomique de `addNote()`.
   - Coupures d'alimentation simulées à chaque étape atomique de `clear()`.
   - Échec `begin()` et diagnostic de corruption.
   - Migration automatique non destructive v0.1.0 $\rightarrow$ v0.1.0.1.
2. **`CommandRouter` (3 tests)** :
   - Préfixe `NOTE`, respect de la casse et rejet en milieu de phrase.
   - Commandes obligatoires (`READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`).
   - Confirmation `CLEAR`, expiration de la fenêtre de 15 s et annulation.
3. **`JournalUI` (4 tests)** :
   - Priorité absolue de `STOP` 50 ms et 100 ms après `PARLER`.
   - Anti-rebond tactile de `PARLER` (180 ms).
   - Maintien prolongé sur `STOP` et disponibilité dans tous les états.
   - Sélecteur tactile de fournisseur IA `[IA: OPENAI / GEMINI]` (hit-test, anti-rebond 350 ms).
4. **`VoiceController` (18 tests)** :
   - Tours de transcription partiels vs finaux et exécution unique par `turnId`.
   - Interruption `STOP` en `LISTENING`, `TRANSCRIBING`, `ANALYZING` et `SPEAKING`.
   - Idempotence de deux `STOP` successifs.
   - Déconnexion/reconnexion sans rejeu de commande et NVS intacte.
   - Refus des commandes concurrentes.
   - Synthèse IA bornée par balises sans répétition vocale.
   - Watchdogs de transcription, analyse IA et TTS.
   - Bascule dynamique OpenAI $\leftrightarrow$ Gemini et gestion des clés API manquantes.
   - Formatage d'affichage des transcriptions en minuscules.

### Suite 2 : Tests NLU v0.2.0 (220 tests, 11 catégories)
- **Catégorie 1 : Normalisation UTF-8 (16 tests)** — accents, élisions, apostrophes typographiques, ponctuation.
- **Catégorie 2 : Tokenisation (12 tests)** — découpage robuste, séparateurs multiples, espaces superflus.
- **Catégorie 3 : Dictionnaire de concepts (18 tests)** — recherche binaire en $O(\log N)$, masques de concepts.
- **Catégorie 4 : Reconnaissance des intentions (28 tests)** — synonymes `ADD_NOTE`, commandes exécutables et expérimentales.
- **Catégorie 5 : Extraction des paramètres (22 tests)** — extraction de charge utile, cibles, dates relatives.
- **Catégorie 6 : Tolérance aux erreurs (Fuzzy) (16 tests)** — distance Damerau-Levenshtein $\le 1$ sur lexique autorisé.
- **Catégorie 7 : Négations et sécurité (22 tests)** — isolation de charge utile, refus des effacements ambigus.
- **Catégorie 8 : Gestion des ambiguïtés (16 tests)** — score différentiel $\le 5$ classé `AMBIGUOUS`.
- **Catégorie 9 : Suppression sécurisée (16 tests)** — confirmation en 2 étapes, refus de faux-positifs.
- **Catégorie 10 : Comparaison historique SHADOW (22 tests)** — comparaison miroir Intelligent vs Legacy.
- **Catégorie 11 : Robustesse des entrées (12 tests)** — textes longs, caractères de contrôle, symboles inattendus.

### Suite 3 : Tests contextuels v0.2.0.1 (249 tests, 10 catégories)
- **Catégorie 12 : Locutions multi-mots (25 tests)** — reconnaissance au plus long préfixe (*longest match*), ordre des mots, préservation des rôles.
- **Catégorie 13 : Rôles sémantiques (25 tests)** — étiquetage des 12 rôles sémantiques, construction des `SemanticFrame`.
- **Catégorie 14 : Grammaire contextuelle (40 tests)** — 19 règles contextuelles ordonnées, scores et priorités.
- **Catégorie 15 : Actions et objets (25 tests)** — typage des 8 cibles (`JOURNAL`, `TASK`, `FILE`, `PROJECT`, etc.), rejet des suppressions non-carnet.
- **Catégorie 16 : Portée des négations (30 tests)** — négation d'action vs négation dans la charge utile de note vs locutions d'obligation (*"ne pas oublier de"*).
- **Catégorie 17 : Fuzzy matching et ambiguïtés (21 tests)** — découplage seuil fuzzy / ambiguïté, collision de verbes d'action directs, interdiction fuzzy sur chiffres/versions.
- **Catégorie 18 : Sécurité destructive (30 tests)** — exigences strictes pour `REQUEST_CLEAR` (`hasGlobalScope` et cible carnet obligatoire), blocage `SECURITY_BLOCKED`.
- **Catégorie 19 : Comparateur SHADOW complet (21 tests)** — couverture exhaustive des 7 statuts de comparaison miroir.
- **Catégorie 20 : Robustesse mémoire et limites (16 tests)** — validation RFC 3629 UTF-8 (rejet surlongueurs et surrogates), borne `MAX_INPUT_BYTES = 1024`, borne `MAX_TOKENS = 96`.
- **Catégorie 21 : Protocole série USB (16 tests)** — commandes `PARSE`, `PARSE_SHADOW`, `PARSER_STATUS`, `PARSER_HELP`, validation du format JSON et de l'isolation sans effets de bord.

---

## 4. Dataset de validation contextuelle (`tests/datasets/contextual_parsing_fr.json`)

Le dataset comprend 53 phrases représentatives de l'usage réel du carnet vocal (commandes valides, synonymes, obligations, recherches, suppressions, négations complexes, expressions trompeuses et ambiguïtés).

Exécution via le simulateur :
```powershell
.\simulator\voicejournal_simulator.exe --dataset tests/datasets/contextual_parsing_fr.json
```
Résultat : **53/53 PASS (100% de réussite)**.
