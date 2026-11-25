# VoiceJournal v0.2.0 — Carnet de laboratoire vocal intelligent & Moteur NLU Embarqué

Application embarquée C++17 pour **M5Stack CoreS3 (ESP32-S3)** construite sur la façade publique **`<VoiceKit.h>`** (**VoiceKit**, révision Git vérifiée `16646c73acf01a8d57408391bc6de15a288b8910`), compatible **OpenAI Realtime** (fournisseur par défaut) et **Google Gemini Live**.

La version **v0.2.0 — Intelligent Parsing Foundation** dote VoiceJournal d'un moteur de compréhension du langage naturel (**NLU**) local, déterministe, explicable et optimisé pour l'ESP32-S3, sans dépendance cloud ni modèle probabiliste lourd.

> **Statut de validation :** La version **v0.2.0** valide la totalité de sa logique métier par **256 tests natifs C++17 réussis à 100%** (34 tests historiques v0.1.1 avec 36 assertions + 220 tests NLU v0.2.0) et compile avec PlatformIO (`m5stack-cores3` : RAM 26,1%, Flash 87,2%). **Les tests matériels sur M5Stack CoreS3 réel (`VJ-01` à `VJ-44`) restent `NOT RUN` tant qu'ils n'ont pas été exécutés sur un appareil physique.**

---

## 1. Présentation de VoiceJournal v0.2.0

**VoiceJournal** transforme un **M5Stack CoreS3** autonome (sans aucun capteur ni module externe) en carnet de laboratoire vocal. L'utilisateur dicte ses observations techniques en français, les stocke localement dans la mémoire non volatile (NVS), les relit à la voix et demande à l'IA (**OpenAI Realtime** par défaut ou **Google Gemini Live**) d'en produire une synthèse factuelle courte.

En version **v0.2.0**, le moteur NLU comprend désormais la richesse du langage naturel : synonymes d'ajout (*"Mémorise"*, *"Enregistre"*, *"Retiens"*), tolérance aux fautes de transcription (Damerau-Levenshtein borné), détection sémantique des négations avec isolation du contenu dicté, gestion explicite des ambiguïtés et protection absolue contre les effacements accidentels.

---

## 2. Fonctionnalités disponibles (v0.2.0)

- **Moteur NLU local déterministe C++17 (`JournalParser`)** :
  - Normalisation UTF-8 complète (accents français, apostrophes, élisions, ponctuation).
  - Préservation intégrale et bijective de la charge utile originale (accents, majuscules, acronymes techniques conservés en NVS).
  - Dictionnaire compact de 179 entrées lexicales constantes en mémoire Flash (`constexpr` / `PROGMEM`) avec masques binaires 32 bits et recherche en $O(\log N)$.
  - Tolérance aux fautes d'inattention (Damerau-Levenshtein distance $\le 1$) avec exclusions strictes (mots courts, chiffres, versions, identifiants techniques et confirmations).
  - Analyse fine des négations et isolation stricte du contenu de note dicté (*"Note, ne supprime pas..."* $\rightarrow$ `ADD_NOTE`).
  - Détection déterministe d'ambiguïté entre commandes concurrentes.
- **Trois modes de parsing configurables (`AppConfig::DEFAULT_PARSER_MODE`)** :
  - `INTELLIGENT` (défaut) : moteur NLU v0.2.0 actif.
  - `LEGACY` : comportement strict v0.1.1 pour compatibilité totale.
  - `SHADOW` : exécution legacy avec analyse miroir intelligente en parallèle et verrouillage de sécurité en cas de divergence (`SECURITY_BLOCKED`).
- **Parsing Lab sur écran tactile CoreS3** :
  - Bouton `[LAB]` / `[RETOUR]` affichant la carte diagnostique en direct (transcription, intention, score, concepts, entités, négation, fuzzy, temps en microsecondes, comparatif Shadow).
  - Bouton physique/tactile `STOP` rouge prioritaire maintenu en permanence.
- **Simulateur autonome PC (`simulator/`)** :
  - Exécutable console permettant de tester des phrases en mode REPL interactif (`--eval`, `--shadow`, `--test`).
- **Deux fournisseurs IA compatibles** : **OpenAI Realtime** (fournisseur par défaut au démarrage, voix `alloy`) et **Google Gemini Live** (voix `Aoede`).
- **Configuration centralisée du fournisseur initial** dans `include/AppConfig.h` via `AppConfig::DEFAULT_AI_PROVIDER`.
- **Sélecteur tactile de fournisseur IA** (`[IA: OPENAI]` / `[IA: GEMINI]`) directement sur l'écran du CoreS3 pour basculer à chaud hors activité vocale.
- **Stockage local persistant fiabilisé en NVS** (espace `"vjournal"`, double instantané A/B de 5 observations, 180 octets UTF-8 max chacune, marqueur FNV-1a).
- **Consultation vocale (TTS)** : relecture de la dernière note, comptage des notes.
- **Effacement sécurisé** : confirmation destructive stricte en 2 étapes avec fenêtre de 15 s, insensible aux négations trompeuses.
- **Synthèse IA à la demande** via `voice.askAi(...)` avec séparation stricte consignes système / données utilisateur.
- **Interruption immédiate `STOP` réellement prioritaire** dans tous les états du système.

---

## 3. Matériel nécessaire

- **M5Stack CoreS3** (ESP32-S3, 16 Mo Flash, 8 Mo PSRAM, écran tactile 320 × 240, microphones ES7210 et haut-parleur AW88298 intégrés).
- Câble USB-C données + alimentation (≥ 5 V / 1 A).
- Réseau **Wi-Fi 2.4 GHz** (l'ESP32-S3 ne supporte pas le 5 GHz).
- Clé API **OpenAI Realtime** ([OpenAI Platform](https://platform.openai.com/api-keys)) et/ou clé API **Google Gemini Live** ([Google AI Studio](https://aistudio.google.com/)).
- Aucun capteur ou module supplémentaire requis.

---

## 4. Dépendances et licences

Déclarées dans `platformio.ini` :

- Plateforme : `espressif32@6.9.0` (framework Arduino-ESP32, C++17 `-std=gnu++17`, licence Apache-2.0 / LGPL-2.1).
- `m5stack/M5CoreS3@1.0.0` (licence MIT).
- `m5stack/M5Unified@0.2.23` (inclut M5GFX, licence MIT).
- `Preferences` (bibliothèque standard Arduino-ESP32 pour la NVS).
- **VoiceKit** (`https://github.com/calmtechnology-ch/VoiceKit.git#16646c73acf01a8d57408391bc6de15a288b8910`, licence MIT).

Le projet **VoiceJournal** est distribué sous licence **MIT** (voir [LICENSE](LICENSE)).

---

## 5. Installation de VoiceKit

VoiceKit est récupéré automatiquement par PlatformIO via `lib_deps` dans `platformio.ini`, figé sur une révision Git vérifiée pour garantir la reproductibilité :

```ini
lib_deps =
    m5stack/M5CoreS3@1.0.0
    m5stack/M5Unified@0.2.23
    https://github.com/calmtechnology-ch/VoiceKit.git#16646c73acf01a8d57408391bc6de15a288b8910
```

> **Note :** Aucun fichier de VoiceKit n'est modifié par VoiceJournal.

---

## 6. Configuration du fournisseur IA au démarrage (`include/AppConfig.h`)

Par défaut, VoiceJournal v0.1.1 démarre sur **OpenAI Realtime** (`AiProvider::OPENAI`).

Le choix du fournisseur initial est centralisé exclusivement dans `include/AppConfig.h` :

```cpp
namespace AppConfig {
// Fournisseur IA utilise au demarrage
// Valeurs : AiProvider::OPENAI ou AiProvider::GEMINI
static constexpr AiProvider DEFAULT_AI_PROVIDER =
    AiProvider::OPENAI;
} // namespace AppConfig
```

Pour démarrer directement sur **Google Gemini Live**, il suffit de remplacer dans `include/AppConfig.h` :

```cpp
static constexpr AiProvider DEFAULT_AI_PROVIDER =
    AiProvider::OPENAI;
```

par :

```cpp
static constexpr AiProvider DEFAULT_AI_PROVIDER =
    AiProvider::GEMINI;
```

**Aucune modification de `src/main.cpp` n'est nécessaire.** Le profil vocal par défaut (`alloy` pour OpenAI, `Aoede` pour Gemini) et la clé API correspondante sont sélectionnés automatiquement.

---

## 7. Configuration du Wi-Fi et des clés API (`include/secrets.h`)

1. Copiez `include/secrets.example.h` vers `include/secrets.h` :
   ```powershell
   Copy-Item include\secrets.example.h include\secrets.h
   ```
2. Éditez `include/secrets.h` avec votre réseau Wi-Fi 2.4 GHz et vos clés API :
   ```cpp
   #pragma once
   #define WIFI_SSID     "MON_WIFI"
   #define WIFI_PASSWORD "MON_MOT_DE_PASSE"

   #define OPENAI_KEY    "MA_CLE_OPENAI"
   #define GEMINI_KEY    "MA_CLE_GEMINI"
   ```

Comportement des clés API :
- Les deux clés peuvent être configurées simultanément pour activer la bascule dynamique depuis l'écran tactile.
- Si seule la clé du fournisseur initial est renseignée, VoiceJournal fonctionne normalement sur ce fournisseur ; une tentative de bascule vers l'autre fournisseur est refusée proprement avec un message explicite tout en conservant le fournisseur actif.
- Si le fournisseur initial sélectionné dans `AppConfig.h` n'a pas de clé valide, VoiceJournal affiche un message d'erreur explicite (`Cle API ... manquante (secrets.h)`).

> **Sécurité :** `include/secrets.h` est exclu par `.gitignore`. Aucune clé API ni mot de passe Wi-Fi n'est affiché sur le port série ou stocké en NVS.

---

## 8. Compilation et téléversement PlatformIO

Depuis le dossier `VoiceJournal_CoreS3/` :

```powershell
# Compilation du firmware
pio run -e m5stack-cores3

# Téléversement sur M5Stack CoreS3 (USB-C)
pio run -e m5stack-cores3 -t upload

# Moniteur série (115 200 bauds)
pio device monitor -b 115200
```

La table de partitions `no_ota.csv` (2 Mo `app0`) est utilisée automatiquement.

---

## 9. Utilisation de l'interface et bascule tactile de fournisseur

L'écran paysage (320 × 240, rotation `0` native CoreS3 ILI9342) présente :

- **En-tête** : `VoiceJournal v0.1.1` + badge d'état (`CONNEXION`, `PRET`, `ECOUTE`, `TRANSCRIPTION`, `ENREGISTREMENT`, `ANALYSE IA`, `REPONSE AUDIO`, `ARRET`, `DECONNECTE`, `ERREUR`).
- **Zone centrale** :
  - Compteur `Notes : X / 5` (toujours visible, y compris hors ligne).
  - **Sélecteur tactile `[IA: OPENAI]` / `[IA: GEMINI]`** : un appui bascule entre OpenAI Realtime et Google Gemini Live. La bascule est autorisée uniquement hors activité vocale (`PRET`, `DECONNECTE`, `ERREUR`), protégée par un anti-rebond dédié de 350 ms, libère proprement la session précédente, applique le nouveau profil VoiceKit et attend la confirmation réelle `READY` du moteur. Les notes NVS sont intégralement conservées.
  - Pastille de connectivité, dernière transcription ou réponse IA, et ligne de statut/erreur.
- **Zone inférieure** :
  - **`PARLER` / `TERMINER`** : 1er clic démarre le micro (`ECOUTE`, libellé `TERMINER`), 2e clic arrête le micro (`TRANSCRIPTION`). Anti-rebond dédié de 180 ms. Désactivé hors ligne ou lorsqu'une opération est en cours.
  - **`STOP`** : évalué en priorité et indépendant de l'anti-rebond de `PARLER`, protégé contre la répétition en cas de maintien. Interrompt immédiatement l'écoute, la transcription, la restitution TTS ou l'analyse IA, annule une confirmation en attente et conserve toutes les notes.

---

## 10. Exemples de commandes vocales

| Commande prononcée | Action exécutée |
|---|---|
| *« Note, le prototype redémarre »* | Enregistre l'observation en NVS puis confirme par TTS |
| *« Lis la dernière note »* | Lit la dernière observation enregistrée |
| *« Combien de notes ? »* | Annonce le nombre d'observations enregistrées |
| *« Fais le bilan »* | Envoie toutes les notes au fournisseur IA actif pour une synthèse orale |
| *« Résume les observations »* | Produit une synthèse IA des notes |
| *« Qu'avons-nous observé ? »* | Produit une synthèse IA des notes |
| *« Efface toutes les notes »* | Demande confirmation explicite (valide 15 s) |
| *« Confirmer la suppression »* | Efface définitivement les notes (uniquement si demandé avant) |

---

## 11. Organisation du code

```text
VoiceJournal_CoreS3/
├── LICENSE                 # Licence MIT
├── platformio.ini          # Configuration PlatformIO (m5stack-cores3, C++17, VoiceKit fige)
├── no_ota.csv              # Table de partitions 2 Mo app0
├── README.md
├── CHANGELOG.md
├── include/
│   ├── secrets.example.h   # Modele Wi-Fi / OpenAI / Gemini (sans identifiants reels)
│   ├── AppConfig.h         # Constantes, DEFAULT_AI_PROVIDER, prompt IA, AiProvider et JournalState
│   ├── JournalStorage.h    # Persistance NVS A/B (5 x 180 B + migration v0.1.0)
│   ├── CommandRouter.h     # Routeur deterministe + confirmation suppression
│   ├── VoiceController.h   # Integration VoiceKit, machine a etats, bascule IA, watchdogs
│   └── JournalUI.h         # Affichage M5GFX 320x240, selecteur IA et filtrage tactile prioritaire STOP
├── src/
│   ├── main.cpp            # Initialisation et boucle principale non bloquante
│   ├── JournalStorage.cpp
│   ├── CommandRouter.cpp
│   ├── VoiceController.cpp
│   └── JournalUI.cpp
├── tests/
│   ├── TEST_PLAN.md        # Plan de tests natifs C++17 (34 scenarios)
│   ├── mocks/              # Mocks NVS (injections d'echecs/coupures), VoiceKit et Arduino
│   └── test_main.cpp       # Suite de 34 tests unitaires automatiques
└── docs/
    ├── ARCHITECTURE.md               # Architecture detaillee et transitions d'etats
    ├── HARDWARE_VALIDATION.md        # Grille de validation materielle VJ-01..VJ-28
    └── CORRECTIVE_REPORT_v0.1.0.1.md # Rapport correctif v0.1.0.1
```

---

## 12. Gestion de la mémoire NVS

- Espace NVS dédié : `prefs.begin("vjournal", false)`.
- Capacité : **5 observations**, **180 octets UTF-8 maximum** par observation.
- **Double instantané borné A/B** :
  - Banque A (`a0`..`a4`, `ah`, `ac`, `ag`, `ak`) et Banque B (`b0`..`b4`, `bh`, `bc`, `bg`, `bk`), référence active `act` et marqueur de version `v2` (`0x564A3032`).
  - `Preferences` ne fournissant pas de transaction atomique matérielle multi-clés, chaque écriture (`addNote`, `clear`) cible exclusivement la banque inactive après avoir invalidé son marqueur (`ak`/`bk = 0`), écrit les notes et métadonnées, puis écrit en dernier la somme de contrôle FNV-1a (`ak`/`bk`).
  - En cas de coupure d'alimentation à n'importe quelle étape, le redémarrage sélectionne la banque valide de génération la plus élevée — garantissant un retour exact au dernier état validé, sans corruption silencieuse ni résurrection de notes supprimées.
  - Toutes les écritures et suppressions (`isKey()` + `remove()`) sont vérifiées ; `begin()` ne masque pas une erreur de récupération et produit un diagnostic explicite.
- **Migration transparente depuis v0.1.0** : un carnet existant au format `magic = 0x564A3031` (`n0`..`n4`, `head`, `count`) est migré automatiquement vers le format A/B sans perte de notes ni modification de l'ordre chronologique.

---

## 13. Fonctionnement de l'analyse IA

L'analyse IA (`voice.askAi(prompt)`) n'est jamais déclenchée automatiquement après une simple transcription (`cfg.enableConversation = false`). Elle s'exécute uniquement sur commande explicite (*« Fais le bilan »*, *« Résume les observations »*, *« Qu'avons-nous observé ? »*).

Le prompt assemblé :
1. Vérifie la présence d'au moins une note et la disponibilité de VoiceKit.
2. Inclut les règles de laboratoire (français, 2 à 4 phrases, aucune invention, distinction observations/hypothèses).
3. Encadre les observations dans `<OBSERVATIONS_UTILISATEUR> ... </OBSERVATIONS_UTILISATEUR>` pour les isoler comme données non fiables (protection contre l'injection de prompt).
4. Restitue directement la réponse audio fournie par le fournisseur actif (OpenAI Realtime ou Gemini Live) sans appeler `say()` une seconde fois, et ignore toute réponse tardive après `STOP` ou nouvelle capture.

---

## 14. Procédure de test

### Tests unitaires natifs (sans matériel)
Consultez [tests/TEST_PLAN.md](tests/TEST_PLAN.md) :

```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror -Itests\mocks -Iinclude `
    src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp src\JournalUI.cpp tests\test_main.cpp `
    -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

### Tests matériels sur M5Stack CoreS3
Complétez la grille de 28 scénarios (`VJ-01` à `VJ-28`) dans [docs/HARDWARE_VALIDATION.md](docs/HARDWARE_VALIDATION.md).

---

## 15. Limitations connues et état réel des validations (v0.1.1)

- **Validation matérielle en attente (`NOT RUN`)** : les essais `VJ-01` à `VJ-28` (y compris démarrage OpenAI, démarrage Gemini, bascule OpenAI ↔ Gemini et reprise après déconnexion) doivent être exécutés sur un M5Stack CoreS3 physique avant toute déclaration de validation matérielle.
- **Pas d'atomicité matérielle multi-clés NVS** : le mécanisme A/B protège contre les coupures d'alimentation entre écritures individuelles de clés `Preferences`, mais repose sur l'intégrité élémentaire d'une écriture `putUInt` NVS sous-jacente (garantie par la journalisation de pages NVS d'ESP-IDF).
- **Pas de VAD local avancé** : VoiceJournal utilise exclusivement la façade publique `<VoiceKit.h>` en mode non conversationnel (`enableConversation = false`) et applique des garde-fous locaux (rejet des transcriptions vides/annulées, limite de durée 15 s, watchdogs 8 s / 20 s, blocage des doubles captures).
- **Police M5GFX intégrée** : les caractères accentués sont conservés intégralement en UTF-8 en NVS et en TTS/IA, et translittérés en ASCII lisible pour l'affichage sur l'écran 320 × 240.
- **Périmètre volontairement borné** : pas de LVGL 9, pas d'horodatage, pas de serveur Web, pas de MQTT/Home Assistant, pas de mot de réveil ni d'écoute permanente.
