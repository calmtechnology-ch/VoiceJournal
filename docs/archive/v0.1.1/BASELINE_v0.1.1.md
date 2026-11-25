# Référence de base — VoiceJournal v0.1.1

Ce document consigne l'état complet, vérifié et reproductible de **VoiceJournal v0.1.1** avant toute modification liée à la version v0.2.0.

---

## 1. Référence Git et traçabilité

- **Dépôt Git :** `VoiceJournal_CoreS3`
- **Branche de référence :** `main`
- **Commit Git de référence :** `e0ca1d6e7b2784d70d6f7ac6d8cdd655a01ff996`
- **Tag Git :** `v0.1.1`
- **Auteur :** Philippe <52742879+Philippe-AD@users.noreply.github.com>
- **Date du commit :** Mon Nov 23 17:21:26 2026 +0100
- **Message de commit :** `feat: add core application configuration, main entry point, and platform configuration`
- **Branche de développement v0.2.0 :** `feature/v0.2.0-intelligent-parsing`

---

## 2. Version exacte des dépendances

- **VoiceKit :** Révision Git vérifiée `16646c73acf01a8d57408391bc6de15a288b8910` (préparation `v0.6.7.2`). Façade publique `<VoiceKit.h>`.
- **M5Unified :** `0.2.23`
- **M5CoreS3 :** `1.0.0`
- **Framework Arduino :** `framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b`
- **Platform :** `espressif32 @ 6.9.0`
- **Standard C++ :** C++17 (`-std=gnu++17`)

---

## 3. Architecture existante (v0.1.1)

L'architecture v0.1.1 repose sur un découpage en 5 modules applicatifs au-dessus de VoiceKit :

1. **`AppConfig.h`** : Configuration centralisée (`DEFAULT_AI_PROVIDER`, profils vocaux, timers, watchdogs, enums `AiProvider`, `JournalState`).
2. **`JournalStorage`** : Persistance NVS (`"vjournal"`) à double banque A/B (`a0..a4` / `b0..b4`), contrôle FNV-1a, 5 notes circulaires maximum de 180 octets UTF-8 max.
3. **`CommandRouter`** : Routage vocal déterministe par préfixe et sous-chaînes normalisées ASCII. Gestion de l'état de confirmation `CLEAR`.
4. **`VoiceController`** : Machine à états applicative (`JournalState`), filtrage strict des tours STT par `turnId`, file TTS bornée (capacité 4), bascule dynamique OpenAI ↔ Gemini, watchdogs non bloquants, priorité absolue et idempotence de STOP.
5. **`JournalUI`** : Interface graphique M5GFX 320 × 240 (orientation paysage, rotation `0`), bouton tactile PTT (`PARLER / TERMINER`), bouton tactile `STOP` prioritaire avec verrou de maintien, sélecteur tactile fournisseur `[IA: OPENAI / GEMINI]`.
6. **`main.cpp`** : Assemblage, validation au boot de la clé API du fournisseur sélectionné, boucle principale `M5.update()` + `controller.update()`, diagnostic mémoire périodique.

---

## 4. Fonctionnalités disponibles (v0.1.1)

- Dictée et enregistrement persistant d'observations de laboratoire en NVS (jusqu'à 5 notes de 180 octets, écrasement circulaire du plus ancien).
- Relecture vocale de la dernière observation enregistrée via TTS.
- Dénombrement vocal du nombre d'observations présentes.
- Demande d'analyse / synthèse par intelligence artificielle (Gemini Live ou OpenAI Realtime).
- Effacement sécurisé du carnet avec protocole en 2 étapes (demande + confirmation explicite temporisée sous 30 s).
- Annulation explicite de l'effacement (« annuler », « non »).
- Interruption immédiate par bouton physique tactile `STOP` à tout instant.
- Bascule dynamique à chaud entre OpenAI Realtime et Gemini Live via appui tactile.
- Mode hors-ligne conservant l'accès aux notes et à l'interface en cas de déconnexion réseau.

---

## 5. Commandes vocales reconnues (v0.1.1)

Dans la v0.1.1, le moteur `CommandRouter` utilise une normalisation ASCII simple (suppression accents, mise en minuscules, ponctuation convertie en espaces) et des correspondances de chaînes :

| Commande | Déclencheur vocal reconnu | Action exécutée |
|---|---|---|
| `ADD_NOTE` | Commence par le mot `note` (ex: *« Note, température 23 degrés »*) | Enregistrement de l'observation en NVS (charge utile nettoyée, accents préservés) |
| `READ_LAST_NOTE` | Contient *« lis la derniere note »*, *« relis la derniere note »*, *« lis la derniere observation »*, *« relire la derniere »*, etc. | Restitution TTS de la dernière note |
| `COUNT_NOTES` | Contient *« combien de notes »*, *« combien d observations »*, *« nombre de notes »*, *« quel est le nombre »*, etc. | Restitution TTS du décompte des notes |
| `ANALYZE_NOTES` | Contient *« fais le bilan »*, *« resume les notes »*, *« synthese des observations »*, *« analyse les notes »*, etc. | Requête d'analyse IA envoyée à VoiceKit (`askAi`) |
| `REQUEST_CLEAR` | Contient *« efface toutes les notes »*, *« supprime toutes les notes »*, *« vide le carnet »*, *« reinitialise »*, etc. | Armement de la confirmation de suppression pour 30 s |
| `CONFIRM_CLEAR` | Égal exactement à *« oui efface »*, *« confirmer »*, *« je confirme la suppression »*, *« supprimer definitivement »*, etc. | Effacement NVS effectif et réinitialisation du compteur à 0 |
| `CANCEL_CLEAR` | Contient *« annuler »*, *« ne supprime pas »*, *« laisse tomber »*, *« non »* | Annulation immédiate de la demande d'effacement |

---

## 6. Résultats des tests unitaires natifs existants

Commande exécutée :
```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Itests\mocks -Iinclude src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp src\JournalUI.cpp tests\test_main.cpp -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

Résultats constatés :
```text
=== VoiceJournal v0.1.1 — Tests unitaires natifs C++17 ===
[PASS] JournalStorage: carnet initialement vide
[PASS] JournalStorage: 1, 5 puis 6 notes (rotation circulaire + ordre)
[PASS] JournalStorage: rejet vide et > 180 octets
[PASS] JournalStorage: preservation UTF-8 et troncature sure
[PASS] JournalStorage: reprise apres redemarrage simule
[PASS] JournalStorage: echecs cibles note, head et count
[PASS] JournalStorage: clear persistant et controle remove()
[PASS] JournalStorage: coupure simulee a chaque etape de addNote()
[PASS] JournalStorage: coupure simulee a chaque etape de clear()
[PASS] JournalStorage: echec begin() et diagnostic corruption
[PASS] JournalStorage: migration v0.1.0 -> v0.1.0.1 (5 notes en rotation)
[PASS] CommandRouter: prefixe NOTE, casse et rejet vide/milieu
[PASS] CommandRouter: commandes obligatoires, casse et accents
[PASS] CommandRouter: confirmation CLEAR, annulation et expiration
[PASS] JournalUI: STOP prioritaire 50 ms et 100 ms apres PARLER
[PASS] JournalUI: anti-rebond PARLER rejette un second PARLER a 50 ms
[PASS] JournalUI: STOP maintenu (1 evenement) et actif dans tous les etats
[PASS] VoiceController: partiel vs final et execution unique
[PASS] VoiceController: STOP pendant LISTENING sans stop() redondant
[PASS] VoiceController: STOP pendant TRANSCRIBING + finale tardive refusee
[PASS] VoiceController: STOP pendant ANALYZING + reponse IA tardive refusee
[PASS] VoiceController: STOP pendant SPEAKING
[PASS] VoiceController: STOP puis PARLER + liberation asynchrone moteur
[PASS] VoiceController: deux STOP successifs idempotents
[PASS] VoiceController: STOP puis reconnexion sans rejeu et NVS intact
[PASS] VoiceController: refus des requetes concurrentes
[PASS] VoiceController: analyse IA bornee sans double say()
[PASS] VoiceController: timeout transcription puis nouvelle capture
[PASS] VoiceController: watchdogs IA et TTS recuperables
[PASS] JournalUI: selecteur fournisseur IA (hit-test, anti-rebond, etats)
[PASS] VoiceController: Gemini par defaut (models/gemini-3.8-live) + bascule Gemini <-> OpenAI + reprise apres deconnexion
[PASS] VoiceController: refus bascule si cle manquante ou moteur occupe
[PASS] VoiceController: Gemini initial, profils vocaux et capture apres bascule
[PASS] VoiceController: cle initiale manquante (ERROR_STATE) puis reprise par bascule
[PASS] VoiceController: confirmation suppression en majuscules et fenetre apres TTS
[PASS] VoiceController: formatDisplayTranscript convertit les majuscules integrales

Resultat : 36 PASS, 0 FAIL
```

---

## 7. Consommation mémoire mesurée (PlatformIO CoreS3)

Compilation cible : `pio run -e m5stack-cores3`

- **RAM interne :** `84 948 octets / 327 680 octets` (**25.9%** utilisée, **242 732 octets** libres)
- **Mémoire Flash :** `1 750 833 octets / 2 097 152 octets` (**83.5%** utilisée, **346 319 octets** libres)
- **PSRAM :** 8 Mo disponibles sur ESP32-S3 (utilisée par VoiceKit pour les tampons audio).

---

## 8. Limitations connues de la v0.1.1

1. **Fragilité de la détection de confirmation `isExactConfirmPhrase()`** :
   - Basée sur des égalités strictes ou des comparaisons incomplètes.
   - Ne traite pas les négations complexes : une phrase telle que *« Je ne confirme pas la suppression »* pourrait être mal interprétée si elle contient une sous-chaîne de confirmation.
2. **Absence d'extraction sémantique structurée** :
   - Le moteur ne distingue ni entités (projets, matériels, composants), ni intentions secondaires, ni concepts combinatoires.
   - Une phrase telle que *« Note, ne supprime pas les fichiers de VoiceKit »* risque d'être routée ou rejetée de manière ambiguë si les mots clés de suppression sont détectés.
3. **Absence de tolérance aux fautes de transcription STT** :
   - La transcription vocale génère fréquemment de légères variations phonétiques ou d'accord (*« retruve »*, *« memorisse »*, *« problme »*), qui provoquent un rejet `UNKNOWN` dans la v0.1.1.
4. **Pas d'extensibilité déclarative** :
   - Les règles de parsing sont codées en dur avec des chaînes `strstr()` dans `src/CommandRouter.cpp`.
5. **Pas de diagnostic explicable du parsing** :
   - Pas de score, pas de traces détaillées des concepts/tokens reconnus, pas d'écran dédié sur le CoreS3 pour inspecter le parsing.

---

## 9. État réel des validations matérielles sur M5Stack CoreS3

Comme documenté dans `docs/HARDWARE_VALIDATION.md` (v0.1.1) :
- Scénarios `VJ-01` à `VJ-24` (fonctionnalités de base, NVS, UI, STOP, watchdogs) : **NOT RUN** (en attente de campagne physique).
- Scénarios `VJ-25` à `VJ-28` (multi-fournisseurs OpenAI Realtime / Gemini Live) : **NOT RUN** (en attente de campagne physique).
- Bilan matériel officiel de la v0.1.1 : **0 PASS / 0 FAIL / 28 NOT RUN**.
