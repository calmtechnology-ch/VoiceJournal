# VoiceJournal v0.3.0 — Storage & Display Recovery

## 1. Cause identifiée

Le symptôme observé sur M5Stack CoreS3 physique (*l'écran d'accueil affichait 2 notes en mémoire alors que l'onglet Journal n'affichait aucune entrée*) provenait de quatre défauts combinés :

1. **Double source de vérité NVS / LittleFS (COR-ST01 & COR-ST03)** : `VoiceController::executeCommand(ADD_NOTE)` écrivait d'abord dans `JournalStorage` (NVS) puis appelait `repo_->addEntry(entry)` sans vérifier le retour. Parallèlement, `VoiceController::setState()` et `VoiceController::publishView()` écrasaient systématiquement `view_.noteCount = storage_.count()` (NVS). Lorsque LittleFS n'était pas monté ou que `JournalRepository` n'était pas initialisé, le compteur de l'accueil affichait le nombre de notes NVS (`2`) tandis que l'écran Journal interrogeait `JournalRepository` (`0`).
2. **Initialisation LittleFS non contrôlée (COR-ST05)** : `main.cpp` activait les services structurés et les commandes associées même lorsque `s_fs.begin()` (avec `formatOnFail = false`) ou `s_repo.begin()` avait échoué, sans afficher d'état explicite (*LittleFS indisponible* / *Journal endommagé*).
3. **Commandes de lecture/comptage/analyse/suppression restées sur NVS (COR-ST04)** : `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR`, `CONFIRM_CLEAR` et `buildAnalysisPrompt()` lisaient ou effaçaient `JournalStorage` (NVS) au lieu de `JournalRepository` (LittleFS), et `ADD_NOTE` appliquait encore la limite historique de 180 octets au lieu de `MAX_ENTRY_TEXT_BYTES` (1024 octets).
4. **Affichage Journal et intégrité stockage (COR-ST06 & COR-ST07)** : `JournalUI::updateJournalWidgets()` n'affichait pas d'erreur explicite lors d'un échec `getEntry()`, ne distinguait pas un journal vide d'un filtre sans résultat, n'exposait pas le bouton de réinitialisation de filtre (`btnJournalClearFilter_`), et `LittleFSFileHandle::truncate()` retournait `true` sans effectuer de troncature réelle.

---

## 2. Fichiers modifiés

- `include/IFileSystem.h`
- `include/LittleFSFileSystem.h`
- `include/JournalRepository.h`
- `src/JournalRepository.cpp`
- `src/StorageMigration.cpp`
- `include/AnalysisService.h`
- `include/VoiceController.h`
- `src/VoiceController.cpp`
- `include/JournalUI.h`
- `src/JournalUI.cpp`
- `include/SerialParserProtocol.h`
- `src/main.cpp`
- `tests/test_v030_structured_memory_suite.cpp`
- `docs/STORAGE_RECOVERY.md`

Composants préservés sans modification : VoiceKit (`16646c73...`), connexion OpenAI/Gemini, corrections tactiles LVGL, boutons PARLER/TERMINER et STOP, navigation, `partitions_16MB.csv`, moteur de parsing, version `v0.3.0`.

---

## 3. Corrections réalisées

- **COR-ST01 (Source unique)** : En mode mémoire structurée v0.3.0, `JournalRepository` + LittleFS constitue l'unique source de vérité. Suppression de la double écriture NVS/LittleFS dans `ADD_NOTE` et interdiction de tout basculement silencieux vers NVS en cas d'échec LittleFS. `JournalStorage` est conservé uniquement pour la migration historique, les tests historiques v0.1.1 et le mode diagnostic legacy explicite (`VJ_DIAG_ENABLE_STRUCTURED_SERVICES=0`).
- **COR-ST02 (Enregistrement `ADD_NOTE`)** : Vérification stricte de l'initialisation de `JournalRepository`, validation UTF-8 sur `AppConfig::MAX_ENTRY_TEXT_BYTES` (1024 octets), contrôle du retour de `repo_->addEntry(entry)`, vérification par relecture immédiate `repo_->getEntry(entry.id, verifyEntry)`, mise à jour du compteur, rafraîchissement du Journal et confirmation vocale uniquement si toutes les étapes réussissent.
- **COR-ST03 (Compteur cohérent)** : Centralisation via `VoiceController::getActiveNoteCount()` utilisée dans le constructeur, `attachCallbacks()`, `setState()`, `publishView()` et les commandes. Le compteur ne change plus lors des transitions d'état vocal.
- **COR-ST04 (Lecture, comptage, analyse, suppression)** : Adaptation de `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `buildAnalysisPrompt()`, `REQUEST_CLEAR` et `CONFIRM_CLEAR` au stockage actif. En mode structuré, `CONFIRM_CLEAR` supprime uniquement les observations (hors `EntryType::TASK`) sans jamais supprimer les tâches ni les projets.
- **COR-ST05 (Initialisation LittleFS)** : Contrôle explicite de `s_fs.begin()` et `s_repo.begin()`, sans formatage automatique (`formatOnFail = false`). Distinction claire de *LittleFS disponible*, *LittleFS indisponible*, *Journal vide* et *Journal endommagé* dans les logs série, `JOURNAL_STATUS` et l'IHM. Procédure explicite et sécurisée de première initialisation sur partition vierge via la commande série `FORMAT_LITTLEFS_CONFIRM` (refusée si un fichier `/journal.log` existe déjà).
- **COR-ST06 (Affichage Journal)** : Refonte de `JournalUI::updateJournalWidgets()` autour de `buildJournalSnapshot()` avec troncature UTF-8 sûre, affichage d'erreur explicite si `getEntry()` échoue (sans jamais laisser une carte vide ou d'anciennes données), distinction *Journal vide* vs *Aucun résultat pour ce filtre*, activation du bouton `× Filtre` pour réinitialiser les filtres et rafraîchissement immédiat après enregistrement/suppression.
- **COR-ST07 (Intégrité stockage)** : Implémentation réelle et sécurisée de `LittleFSFileHandle::truncate()` (copie vérifiée vers `.trunc`, sauvegarde `.bak`, remplacement et restauration sur erreur), vérification de toutes les écritures dans `JournalRepository::appendRecord()` et `begin()`, validation intégrale (en-tête + CRC32 + comptages) du fichier temporaire avant tout remplacement dans `compact()`, et vérification par relecture lors de la migration NVS.
- **Diagnostics série ciblés** : Traces complètes `[VJ][STORAGE]` (`Backend`, `Mounted`, `RepositoryReady`, `Write`, `ReadBack`, `Count`) et `[VJ][UI]` (`Page=JOURNAL`, `QueryResults`, `VisibleCards`), causes détaillées d'échec dans `getEntry()`, et enrichissement de `JOURNAL_STATUS` (`backend`, `legacyNvsReady`, `legacyNvsCount`, `littleFsMounted`, `repositoryReady`, `entryCount`, `readErrors`, `writeErrors`, `lastStorageError`) sans exposition des clés API.

---

## 4. Résultats des tests natifs (C++17)

Exécution de `tests/build/voicejournal_tests.exe` :

| Suite | Résultat |
|---|---|
| v0.1.1 Legacy | 36 PASS, 0 FAIL |
| v0.2.0 NLU | 220 PASS, 0 FAIL |
| v0.2.0.1 Context | 289 PASS, 0 FAIL |
| v0.2.1 Consolidation | 91 PASS, 0 FAIL |
| v0.3.0 Structured Memory + Recovery | 15 PASS, 0 FAIL |
| **TOTAL GLOBAL** | **651 PASS, 0 FAIL** |

Les 5 nouveaux tests de reprise couvrent notamment : ajout 1/2/3 notes + compteur + cartes visibles, lecture dernière note, stabilité du compteur au changement d'état vocal, navigation aller-retour, reprise après redémarrage simulé, cohérence `LIST_ENTRIES` / Journal, carnet > 5 observations et > 180 octets indépendant de NVS, échec de montage LittleFS, échec d'écriture simulé, échec `getEntry()` simulé, suppression confirmée préservant tâches et projets, réinitialisation de filtre, et préservation du journal original en cas d'échec `truncate()` ou `compact()`.

> **Statut validation matérielle physique** : **EN ATTENTE D'EXÉCUTION SUR CORES3 PHYSIQUE** (non exécutée dans cet environnement).

---

## 5. Résultat de compilation PlatformIO (`m5stack-cores3`)

Commande exécutée : `pio run -e m5stack-cores3`

- **Statut** : `SUCCESS` (code retour 0)
- **RAM** : 32.1% (105 072 / 327 680 octets)
- **Flash** : 39.5% (2 483 693 / 6 291 456 octets)

---

## 6. Procédure de validation matérielle (M5Stack CoreS3)

1. **Téléverser et ouvrir le moniteur série (115200 bauds)** :
   ```powershell
   cd VoiceJournal_CoreS3
   pio run -e m5stack-cores3 -t upload -t monitor
   ```
2. **Vérifier l'état initial du stockage** en envoyant `JOURNAL_STATUS` sur le port série :
   - Si `littleFsMounted: true` et `repositoryReady: true` : le journal LittleFS est prêt (si des notes existaient en NVS historique, la migration idempotente les importe une seule fois).
   - Si `littleFsMounted: false` (partition LittleFS vierge jamais formatée) : envoyer explicitement `FORMAT_LITTLEFS_CONFIRM` puis revérifier `JOURNAL_STATUS`.
3. **Exécuter la grille de tests physiques** :
   - Ajouter une 1re, 2e puis 3e note vocale : vérifier les traces `[VJ][STORAGE] Write=OK` / `ReadBack=OK` / `Count=1..3` et `[VJ][UI] Page=JOURNAL` / `VisibleCards=1..3`, ainsi que l'affichage des cartes sur l'écran Journal.
   - Dire *« Lis la dernière note »* et vérifier la restitution exacte.
   - Appuyer sur **PARLER** puis **STOP** et vérifier que le compteur d'accueil reste inchangé.
   - Naviguer Accueil → Projets → Journal et vérifier que les cartes restent visibles.
   - Envoyer `LIST_ENTRIES` et `GET_ENTRY 1` via USB série et comparer avec l'écran Journal.
   - Ajouter plus de 5 observations (dont une longue > 180 caractères) et vérifier que le compteur dépasse 5 et que les 3 plus récentes sont visibles.
   - Redémarrer le CoreS3 (bouton Reset / coupure alimentation) et vérifier la persistance intégrale.
   - Tester *« Efface toutes les notes »* puis *« Confirmer la suppression »* et vérifier la cohérence du compteur, du Journal, et la préservation des tâches et projets existants.
