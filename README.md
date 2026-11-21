# VoiceJournal v0.1.0 — Carnet de laboratoire vocal intelligent

Application embarquée C++17 pour **M5Stack CoreS3 (ESP32-S3)** construite sur la façade publique **`<VoiceKit.h>`** (**VoiceKit v0.6.7.2**) et **Google Gemini Live**.

Conforme au troisième article de la série VoiceKit : *« Créer une application embarquée intelligente avec VoiceKit : du premier enregistrement vocal à l'analyse par IA »*.

---

## 1. Présentation de VoiceJournal

**VoiceJournal** transforme un **M5Stack CoreS3** autonome (sans aucun capteur ni module externe) en carnet de laboratoire vocal. L'utilisateur dicte ses observations techniques en français, les stocke localement dans la mémoire non volatile (NVS), les relit à la voix et demande à **Gemini Live** d'en produire une synthèse factuelle courte.

---

## 2. Fonctionnalités disponibles (v0.1.0)

- **Dictée d'observations techniques (STT)** en Push-to-Talk 2 clics (`PARLER` puis `TERMINER`).
- **Stockage local persistant en NVS** (espace `"vjournal"`, tampon circulaire de 5 observations, 180 octets UTF-8 max chacune).
- **Consultation vocale (TTS)** : relecture de la dernière note, comptage des notes.
- **Effacement sécurisé** : suppression complète du carnet uniquement après confirmation vocale explicite dans un délai de 15 s (annulable par `STOP`).
- **Synthèse IA à la demande** via `voice.askAi(...)` (Gemini Live) avec séparation stricte entre consignes système et données utilisateur.
- **Interface tactile M5GFX** (paysage 320 × 240, sans LVGL 9) affichant en permanence le nombre de notes, même hors ligne.
- **Interruption immédiate `STOP`** coupant écoute, synthèse ou analyse IA sans jamais effacer les notes NVS.
- **Diagnostics série** structurés à 115 200 bauds (`[VJ][...]`) sans jamais exposer Wi-Fi ni clé API.

---

## 3. Matériel nécessaire

- **M5Stack CoreS3** (ESP32-S3, 16 Mo Flash, 8 Mo PSRAM, écran tactile 320 × 240, microphones ES7210 et haut-parleur AW88298 intégrés).
- Câble USB-C données + alimentation (≥ 5 V / 1 A).
- Réseau **Wi-Fi 2.4 GHz** (l'ESP32-S3 ne supporte pas le 5 GHz).
- Clé API **Google Gemini Live** ([Google AI Studio](https://aistudio.google.com/)).
- Aucun capteur ou module supplémentaire requis.

---

## 4. Dépendances

Déclarées dans `platformio.ini` :

- Plateforme : `espressif32@6.9.0` (framework Arduino, C++17 `-std=gnu++17`).
- `m5stack/M5CoreS3@1.0.0`
- `m5stack/M5Unified@0.2.23` (inclut M5GFX).
- `Preferences` (bibliothèque standard Arduino-ESP32 pour la NVS).
- **VoiceKit v0.6.7.2** (`https://github.com/calmtechnology-ch/VoiceKit.git`).

---

## 5. Installation de VoiceKit

VoiceKit est récupéré automatiquement par PlatformIO via `lib_deps` dans `platformio.ini` :

```ini
lib_deps =
    m5stack/M5CoreS3@1.0.0
    m5stack/M5Unified@0.2.23
    https://github.com/calmtechnology-ch/VoiceKit.git#main
```

> **Note :** Remplacez `#main` par `#v0.6.7.2` dès la publication du tag Git officiel. Aucun fichier de VoiceKit n'est modifié par VoiceJournal.

---

## 6. Configuration du Wi-Fi et de Gemini

1. Copiez `include/secrets.example.h` vers `include/secrets.h` :
   ```powershell
   Copy-Item include\secrets.example.h include\secrets.h
   ```
2. Éditez `include/secrets.h` avec votre réseau Wi-Fi 2.4 GHz et votre clé API Gemini :
   ```cpp
   #pragma once
   #define WIFI_SSID     "VOTRE_RESEAU_WIFI_2_4GHZ"
   #define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"
   #define GEMINI_KEY    "AIzaSy..."
   ```

> **Sécurité :** `include/secrets.h` est exclu par `.gitignore`. Aucune clé API ni mot de passe Wi-Fi n'est affiché sur le port série ou stocké en NVS.

---

## 7. Compilation PlatformIO

Depuis le dossier `VoiceJournal_CoreS3/` :

```bash
pio run
```

La table de partitions `no_ota.csv` (2 Mo `app0`) est utilisée automatiquement.

---

## 8. Téléversement sur CoreS3

Branchez le M5Stack CoreS3 en USB-C puis exécutez :

```bash
pio run -t upload
pio device monitor -b 115200
```

---

## 9. Utilisation de l'interface

L'écran paysage (320 × 240) présente :

- **En-tête** : `VoiceJournal v0.1.0` + badge d'état (`PRET`, `ECOUTE`, `TRANSCRIPTION`, `ENREGISTREMENT`, `ANALYSE IA`, `REPONSE AUDIO`, `DECONNECTE`, `ERREUR`).
- **Zone centrale** : compteur `Notes : X / 5` (toujours visible, y compris hors ligne), dernière transcription ou réponse IA, et ligne de statut/erreur.
- **Zone inférieure** :
  - **`PARLER` / `TERMINER`** : 1er clic démarre le micro (`ECOUTE`, libellé `TERMINER`), 2e clic arrête le micro (`TRANSCRIPTION`). Désactivé hors ligne ou lorsqu'une opération est en cours.
  - **`STOP`** : interrompt immédiatement l'écoute, la restitution TTS ou l'analyse IA, annule une confirmation en attente et conserve toutes les notes.

---

## 10. Exemples de commandes vocales

| Commande prononcée | Action exécutée |
|---|---|
| *« Note, le prototype redémarre »* | Enregistre l'observation en NVS puis confirme par TTS |
| *« Lis la dernière note »* | Lit la dernière observation enregistrée |
| *« Combien de notes ? »* | Annonce le nombre d'observations enregistrées |
| *« Fais le bilan »* | Envoie toutes les notes à Gemini pour une synthèse orale |
| *« Résume les observations »* | Produit une synthèse IA des notes |
| *« Qu'avons-nous observé ? »* | Produit une synthèse IA des notes |
| *« Efface toutes les notes »* | Demande confirmation explicite (valide 15 s) |
| *« Confirmer la suppression »* | Efface définitivement les notes (uniquement si demandé avant) |

---

## 11. Organisation du code

```text
VoiceJournal_CoreS3/
├── platformio.ini          # Configuration PlatformIO (m5stack-cores3, C++17)
├── no_ota.csv              # Table de partitions 2 Mo app0
├── include/
│   ├── secrets.example.h   # Modele Wi-Fi / Gemini (sans identifiants reels)
│   ├── AppConfig.h         # Constantes, prompt IA et enum JournalState
│   ├── JournalStorage.h    # Persistance NVS (tampon circulaire 5 x 180 B)
│   ├── CommandRouter.h     # Routeur deterministe + confirmation suppression
│   ├── VoiceController.h   # Integration VoiceKit, machine a etats, watchdogs
│   └── JournalUI.h         # Affichage M5GFX 320x240 et tactile anti-rebond
├── src/
│   ├── main.cpp            # Initialisation et boucle principale non bloquante
│   ├── JournalStorage.cpp
│   ├── CommandRouter.cpp
│   ├── VoiceController.cpp
│   └── JournalUI.cpp
├── tests/
│   ├── TEST_PLAN.md        # Plan de tests natifs C++17
│   ├── mocks/              # Mocks NVS, VoiceKit et Arduino pour tests natifs
│   └── test_main.cpp       # Suite de tests unitaires automatiques
└── docs/
    ├── ARCHITECTURE.md         # Architecture detaillee et transitions d'etats
    └── HARDWARE_VALIDATION.md  # Grille de validation materielle VJ-01..VJ-18
```

---

## 12. Gestion de la mémoire NVS

- Espace NVS dédié : `prefs.begin("vjournal", false)`.
- Capacité : **5 observations**, **180 octets UTF-8 maximum** par observation.
- Tampon circulaire : clés `"n0"` à `"n4"`, métadonnées `"magic"` (`0x564A3031`), `"head"` et `"count"`.
- **Stratégie anti-corruption** : la charge utile `"n<slot>"` est toujours écrite *avant* la mise à jour de `"head"` / `"count"`. En cas d'échec NVS, la RAM n'est pas modifiée et aucune confirmation vocale « Note enregistrée » n'est émise.
- Les notes ne sont jamais supprimées lors d'une reconnexion Wi-Fi ou d'un appui sur `STOP`.
- **Limite connue** : NVS ne fournit pas de transaction multi-clés atomique matérielle ; une coupure d'alimentation exactement entre l'écriture de `"head"` et `"count"` est rattrapée au démarrage suivant par la validation de bornes et le rechargement des emplacements valides.

---

## 13. Fonctionnement de l'analyse IA

L'analyse IA (`voice.askAi(prompt)`) n'est jamais déclenchée automatiquement après une simple transcription (`cfg.enableConversation = false`). Elle s'exécute uniquement sur commande explicite (*« Fais le bilan »*, *« Résume les observations »*, *« Qu'avons-nous observé ? »*).

Le prompt assemblé :
1. Vérifie la présence d'au moins une note et la disponibilité de VoiceKit.
2. Inclut les règles de laboratoire (français, 2 à 4 phrases, aucune invention, distinction observations/hypothèses).
3. Encadre les observations dans `<OBSERVATIONS_UTILISATEUR> ... </OBSERVATIONS_UTILISATEUR>` pour les isoler comme données non fiables (protection contre l'injection de prompt).
4. Restitue directement la réponse audio fournie par Gemini Live sans appeler `say()` une seconde fois.

---

## 14. Procédure de test

### Tests unitaires natifs (sans matériel)
Consultez [tests/TEST_PLAN.md](tests/TEST_PLAN.md) :

```powershell
mkdir tests\build -Force
g++ -std=c++17 -Wall -Wextra -Werror -Itests\mocks -Iinclude `
    src\JournalStorage.cpp src\CommandRouter.cpp src\VoiceController.cpp tests\test_main.cpp `
    -o tests\build\voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

### Tests matériels sur M5Stack CoreS3
Complétez la grille de 18 scénarios (`VJ-01` à `VJ-18`) dans [docs/HARDWARE_VALIDATION.md](docs/HARDWARE_VALIDATION.md).

---

## 15. Limitations connues (v0.1.0)

- **Pas de VAD local avancé** : `VoiceInteractionController` / `VoiceCaptureValidator` de VoiceKit ciblent `VoxCoreVoice` en mode conversationnel ; VoiceJournal v0.1.0 utilise exclusivement la façade publique `<VoiceKit.h>` en mode non conversationnel (`enableConversation = false`) et applique une validation locale de base (rejet des transcriptions vides, rejet des captures annulées, limite de durée 15 s, watchdog transcription 8 s, blocage des doubles captures).
- **Police M5GFX intégrée** : les caractères accentués sont conservés intégralement en UTF-8 en NVS et en TTS/IA, et translittérés en ASCII lisible pour l'affichage sur l'écran 320 × 240.
- **Périmètre volontairement borné** : pas de LVGL 9, pas d'horodatage, pas de serveur Web, pas de MQTT/Home Assistant, pas de mot de réveil ni d'écoute permanente, pas de changement dynamique de fournisseur IA.
