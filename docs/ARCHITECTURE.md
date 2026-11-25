# Architecture technique — VoiceJournal v0.3.0 (Structured Memory)

## 1. Vue d'ensemble

VoiceJournal v0.3.0 est une application embarquée modulaire en C++17 conçue pour le **M5Stack CoreS3** (ESP32-S3FN8, 16 Mo Flash, 8 Mo PSRAM).

Elle repose sur la séparation stricte des responsabilités prescrite par la spécification v0.3.0 :

```text
               +-------------------------------------------+
               |                 VoiceKit                  |
               | (OpenAI Realtime / Google Gemini Live)    |
               +---------------------+---------------------+
                                     |
                                     v
               +-------------------------------------------+
               |               JournalParser               |
               | (NLU contextuel deterministe hors-ligne)  |
               +---------------------+---------------------+
                                     |
                                     v
               +-------------------------------------------+
               |               CommandRouter               |
               | (Validation centralisee & classification) |
               +---------------------+---------------------+
                                     |
                                     v
               +-------------------------------------------+
               |             VoiceController               |
               |  (Machine a etats & coordinateur vocal)   |
               +---------------------+---------------------+
                                     |
             +-----------------------+-----------------------+
             |                       |                       |
             v                       v                       v
    +-----------------+     +-----------------+     +-----------------+
    |  ProjectService |     |   TaskService   |     |  SearchService  |
    +--------+--------+     +--------+--------+     +--------+--------+
             |                       |                       |
             |                       v                       |
             |              +-----------------+              |
             +------------->| AnalysisService |<-------------+
                            +--------+--------+
                                     |
                                     v
                            +-----------------+
                            |   TimeService   |
                            +--------+--------+
                                     |
                                     v
    +-----------------------------------------------------------------+
    |                       JournalRepository                         |
    |      (Journal binaire append-only LittleFS + Index RAM 32 Ko)   |
    +--------------------------------+--------------------------------+
                                     |
                 +-------------------+-------------------+
                 |                                       |
                 v                                       v
    +-------------------------+             +-------------------------+
    |   LittleFS (9 Mo Flash) |             |    NVS (20 Ko Flash)    |
    | - /journal.log          |             | - Cles API / Wi-Fi      |
    | - Sauvegardes / tmp     |             | - Flag migrated_v030    |
    +-------------------------+             +-------------------------+
```

---

## 2. Découplage de la couche Interface Graphique (LVGL 9)

L'interface graphique est complètement isolée de la logique métier et des fournisseurs d'IA :

```text
                         +-----------------------------+
                         |       Interface LVGL 9      |
                         |   (320 x 240, Rotation 0)   |
                         +--------------+--------------+
                                        |
         +------------------+-----------+-----------+------------------+
         |                  |                       |                  |
         v                  v                       v                  v
+-----------------+ +-----------------+   +-----------------+ +-----------------+
|  Onglet Accueil | |  Onglet Journal |   |  Onglet Projets | |  Onglet Taches  |
| - Statut vocal  | | - 3 cartes/page |   | - Liste projets | | - Taches OPEN   |
| - Projet courant| | - Pagination    |   | - Projets actifs| | - Echeances     |
| - PTT / IA switch | - Vue Detail   |   | - Selection ctx | | - Clic Terminer |
+-----------------+ +-----------------+   +-----------------+ +-----------------+
```

---

## 3. Responsabilités des composants métier

| Service / Composant | Responsabilité | Dépendances |
|---|---|---|
| `JournalRepository` | Persistance append-only LittleFS avec CRC32, indexation RAM (~32 Ko pour 1 000 entrées), compactage sécurisé et résilience aux coupures d'alimentation. | `IFileSystem` (`LittleFSFileSystem` / `NativeFileSystem`) |
| `ProjectService` | Création de projets, alias phonétiques, gestion du projet de travail courant et archivage. | `JournalRepository`, `TimeService` |
| `TaskService` | Ajout de tâches (`ADD_TASK`), suivi des échéances, clôture (`COMPLETE_TASK`), réouverture et annulation. | `JournalRepository`, `TimeService` |
| `SearchService` | Recherche multicritère déterministe (mots-clés normalisés, entités, projets, types d'entrées, statuts, dates relatives). | `JournalRepository`, `TimeService` |
| `TimeService` | Horodatage interne en UTC, gestion du fuseau `Europe/Zurich` avec heure d'été/hiver automatique, analyse des expressions de dates relatives en français. | RTC CoreS3 (BM8563), SNTP |
| `AnalysisService` | Synthèse ciblée et bornée pour les LLM : sélection des 10 entrées les plus pertinentes (< 1 500 octets) encadrées par `<OBSERVATIONS_UTILISATEUR>`. | `JournalRepository`, `SearchService` |
| `StorageMigration` | Importation automatique, non destructive et idempotente des anciennes notes de la v0.2.1 (NVS) vers LittleFS au premier démarrage. | `JournalStorage` (NVS), `JournalRepository` |
| `JournalUI` | Moteur graphique LVGL 9 (v9.6.0) sur dalle ILI9342 CoreS3 (paysage natif 320 × 240, rotation 0), typographie Segoe UI française, bannière vocale supérieure permanente avec bouton STOP prioritaire. | `lvgl`, `M5GFX` |
| `VoiceController` | Machine à états asynchrone, intégration VoiceKit, priorisation STOP, bascule OpenAI ↔ Gemini, déclenchement des actions métier. | `IVoiceEngine`, `JournalRepository`, services métier |

---

## 4. Table des partitions 16 Mo (`partitions_16MB.csv`)

```text
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x600000,
littlefs, data, spiffs,  0x610000,0x900000,
```
- **Partition applicative (`app0`) :** 6 Mo (firmware actuel = 2,46 Mo / 39,1%).
- **Partition de stockage (`littlefs`) :** 9 Mo (supporte jusqu'à 1 000 entrées indexées en RAM).
- **Partition NVS (`nvs`) :** Maintenue à `0x9000` (20 Ko), garantissant la préservation absolue des clés API, identifiants Wi-Fi et historique v0.2.1.
