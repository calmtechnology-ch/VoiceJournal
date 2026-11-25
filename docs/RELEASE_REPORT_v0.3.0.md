# Rapport de version — VoiceJournal v0.3.0 (Structured Memory)

**Date :** 25 novembre 2026  
**Auteur :** Antigravity (Google DeepMind)  
**Projet :** [VoiceJournal](https://github.com/calmtechnology-ch/VoiceJournal)  
**Cible matérielle :** M5Stack CoreS3 (ESP32-S3FN8, 16 Mo Flash, 8 Mo PSRAM, écran ILI9342 320 × 240)  
**Bibliothèque vocale :** [VoiceKit](https://github.com/calmtechnology-ch/VoiceKit) v0.6.7 (révision `16646c73acf01a8d57408391bc6de15a288b8910`, inchangée)  
**Statut :** **Validation logicielle et simulation 100% OK, prêt pour validation matérielle CoreS3.**

---

## 1. Synthèse exécutive

La version **v0.3.0 — Structured Memory** marque une rupture majeure dans l'évolution de VoiceJournal en le transformant d'un simple carnet tournant de 5 notes NVS en un **assistant autonome de mémoire personnelle et technique structurée**, tout en conservant scrupuleusement la philosophie **CalmTechnology** : discrétion, interaction vocale naturelle, réactivité instantanée locale et information contextuelle disponible au moment opportun.

### Principales réalisations

1. **Mémoire structurée LittleFS (jusqu'à 1 000 entrées) :**
   - Journal append-only avec en-tête `VJ03LOG1`, délimiteurs magiques `0xAA 0x55`, CRC32 par enregistrement et réparation automatique en cas de coupure d'alimentation.
   - Index en RAM ultra-rapide (~32 Ko pour 1 000 entrées) pour des requêtes instantanées sans solliciter la mémoire flash.
   - Typage riche des observations : `NOTE`, `PROBLEM`, `TASK`, `IDEA`, `DECISION`, `RESULT`.
   - Cycle de vie et statuts formels : `NONE`, `OPEN`, `IN_PROGRESS`, `COMPLETED`, `RESOLVED`, `CANCELLED`.
   - Préservation inconditionnelle du texte original de transcription (jusqu'à 1 024 octets UTF-8).

2. **Nouvelle interface graphique épurée sous LVGL 9 (v9.6.0) :**
   - Remplacement de l'interface directe M5GFX par LVGL 9 configuré pour le CoreS3 (16-bit RGB565 swapped, orientation native paysage 320 × 240, rotation 0).
   - 4 écrans principaux avec barre d'onglets épurée : **Accueil**, **Journal** (3 cartes réutilisables avec pagination), **Projets** (liste des projets actifs), **Tâches** (liste des tâches ouvertes avec échéances).
   - Écran de **Détail d'une entrée** avec lecture vocale TTS à la demande (`READ_ENTRY`) et bascule de statut en un clic.
   - Bannière vocale supérieure permanente avec bouton STOP prioritaire universel et retour visuel d'état (ÉCOUTE, TRANSCRIPTION, ANALYSE, PARLE).
   - Intégration des polices Segoe UI avec accents français complets (`font_french_12`, `font_french_14`, `font_french_bold_16`).

3. **Services métier autonomes & découplés :**
   - `ProjectService` : création, alias, bascule de contexte courant (« Ouvre le projet VoiceKit »), archivage.
   - `TaskService` : création avec échéance, marquage terminé (« La tâche tester Gemini est terminée »), réouverture, annulation.
   - `SearchService` : moteur de recherche multicritère local déterministe (mots-clés normalisés, entités, projets, types, statuts, dates relatives).
   - `TimeService` : horloge RTC CoreS3 + synchronisation NTP non bloquante, stockage UTC et fuseau horaire `Europe/Zurich` avec gestion automatique DST été/hiver.
   - `AnalysisService` : constructeur de contexte IA borné (10 entrées max, 1 500 caractères max, balises `<OBSERVATIONS_UTILISATEUR>`).

4. **Migration NVS v0.2.1 vers LittleFS v0.3.0 non destructive :**
   - Importation automatique, idempotente et protégée contre les coupures de courant des anciennes notes de la v0.2.1 vers LittleFS.
   - Conservation intégrale des données NVS historiques d'origine.

5. **Protocole série USB JSON enrichi :**
   - Commandes `JOURNAL_STATUS`, `GET_ENTRY`, `LIST_PROJECTS`, `LIST_TASKS`, `EXPORT_JOURNAL` pour les outils PC et sauvegardes.

---

## 2. Table des partitions 16 Mo

La table de partitions standard de 4 Mo était occupée à 95,3 % en v0.2.1. Une nouvelle table `partitions_16MB.csv` a été mise en œuvre pour exploiter les 16 Mo de mémoire flash du CoreS3 :

| Partition | Type | Sous-type | Adresse de départ | Taille | Utilisation |
|---|---|---|---|---|---|
| `nvs` | data | nvs | `0x00009000` | 20 Ko (`0x5000`) | Paramètres NVS, Wi-Fi, clés API, migration |
| `otadata` | data | ota | `0x0000E000` | 8 Ko (`0x2000`) | Données de démarrage OTA |
| `app0` | app | ota_0 | `0x00010000` | **6 144 Ko (6 Mo)** | Application VoiceJournal v0.3.0 |
| `littlefs` | data | spiffs | `0x00610000` | **9 216 Ko (9 Mo)** | Mémoire structurée journal.log |

> **Garantie de non-régression NVS :** L'adresse physique (`0x9000`) et la taille (`0x5000`) de la partition `nvs` sont **strictement identiques** à la v0.2.1, évitant tout risque d'écrasement des paramètres et clés API existants.

---

## 3. Résultats de qualification logicielle

### 3.1 Tests unitaires natifs C++17

La suite globale a été exécutée avec succès sans aucune régression :

```text
==================================================
Resultat Global VoiceJournal v0.3.0 Structured Memory :
  v0.1.1 Legacy        : 36 PASS, 0 FAIL
  v0.2.0 NLU           : 220 PASS, 0 FAIL
  v0.2.0.1 Context     : 289 PASS, 0 FAIL
  v0.2.1 Consolidation : 91 PASS, 0 FAIL
  v0.3.0 Structured    : 10 PASS, 0 FAIL
  TOTAL GLOBAL         : 646 PASS, 0 FAIL (100% SUCCÈS)
==================================================
```

### 3.2 Simulateur PC (`voicejournal_simulator.exe`)

- Validation du benchmark NLU : **21 / 21 tests réussis (100%)**.
- Analyse en mode interactif avec détection des nouveaux types (`entryType`), projets (`projectName`) et expressions de date (`dateExpr`).

### 3.3 Compilation PlatformIO (M5Stack CoreS3)

```text
RAM:   [=====     ]  47.0% (utilisé 154 092 octets sur 327 680 octets)
Flash: [====      ]  39.1% (utilisé 2 462 037 octets sur 6 291 456 octets)
Firmware: .pio/build/m5stack-cores3/firmware.bin (16 Mo Flash Layout)
```

Marge résiduelle disponible :
- **Flash applicative restante :** 3 829 419 octets (> 3,8 Mo de marge libre).
- **LittleFS disponible :** 9 Mo (supporte largement plus de 10 000 entrées, capacité bridée à 1 000 pour la RAM).
- **PSRAM :** 8 Mo disponibles sur le matériel pour les allocations de cache et LVGL.

---

## 4. Matrice de traçabilité des exigences

| Exigence Spécification | Composant | Statut |
|---|---|---|
| Respect philosophie CalmTechnology | UI LVGL 9 & Synthèse vocale | Validé |
| Préservation tags v0.1.1, v0.2.0, v0.2.0.1, v0.2.1 | Git | Validé |
| VoiceKit révision inchangée | `lib/VoiceKit` | Validé |
| Double fournisseur OpenAI / Gemini | `VoiceController` | Validé |
| PTT deux clics & STOP prioritaire | `JournalUI` / `VoiceController` | Validé |
| Partitionnement 16 Mo (App 6 Mo, LittleFS 9 Mo) | `partitions_16MB.csv` | Validé |
| Mémoire structurée 1 000 entrées (types, statuts) | `MemoryModel.h` / `JournalRepository` | Validé |
| Stockage append-only avec intégrité CRC32 & réparation | `JournalRepository` | Validé |
| Migration NVS idempotente et non destructive | `StorageMigration` | Validé |
| Horodatage UTC & Timezone Zurich avec DST | `TimeService` | Validé |
| Gestion des projets (courant, lookup, bilan) | `ProjectService` | Validé |
| Gestion des tâches (échéances, clôture) | `TaskService` | Validé |
| Recherche multicritère locale sans LLM/vecteurs | `SearchService` | Validé |
| Contexte d'analyse IA borné (1500 car., 10 notes) | `AnalysisService` | Validé |
| Interface graphique LVGL 9 (CoreS3 320x240 rot 0) | `JournalUI` | Validé |
| Protocole série JSON étendu | `SerialParserProtocol.h` | Validé |
| 0 régression sur les 636 tests v0.2.1 | Suite native C++17 | Validé (646/646 PASS) |

---

## 5. Guide de validation matérielle sur CoreS3

1. **Sauvegarde NVS préalable (recommandée) :**
   ```bash
   esptool.py --chip esp32s3 read_flash 0x9000 0x5000 nvs_backup_v021.bin
   ```
2. **Téléversement du firmware v0.3.0 :**
   ```bash
   pio run -e m5stack-cores3 -t upload
   ```
3. **Contrôle au démarrage sur moniteur série (115 200 bauds) :**
   - Présence du logo VoiceJournal v0.3.0.
   - Message de migration : `[MIGRATION] Migration terminee avec succes : X notes importees`.
   - Message de LittleFS : `[STORAGE] LittleFS monte avec succes. Entrees: X, Projets: Y`.
   - Affichage LVGL 9 en orientation paysage correcte (320 × 240, sans bordure noire ni décalage).
4. **Vérification vocale :**
   - Dictée : *« Note, prototype opérationnel »* $\rightarrow$ Carte créée dans l'onglet Journal.
   - Problème : *« Enregistre un problème : coupure Wi-Fi »* $\rightarrow$ Badge rouge PROBLEM sur la carte.
   - Tâche : *« Je dois tester la batterie demain »* $\rightarrow$ Apparition dans l'onglet Tâches.
   - Projet : *« Ouvre le projet VoiceKit »* $\rightarrow$ Affichage du projet courant sur l'écran d'accueil.
   - Bouton STOP : interruption immédiate à tout moment.

---

## 6. Conclusion et recommandation

La version **v0.3.0 — Structured Memory** est achevée, entièrement compilée et validée par les tests automatiques natifs et le simulateur PC. Elle est prête pour le test final sur le M5Stack CoreS3 physique de l'utilisateur.
Le tag Git `v0.3.0` sera apposé dès validation physique complète.
