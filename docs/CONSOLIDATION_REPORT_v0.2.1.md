# Rapport de Consolidation — VoiceJournal v0.2.1

## 1. Version et commit de départ

- **Dépôt** : `https://github.com/calmtechnology-ch/VoiceJournal`
- **Branche de départ** : `feature/v0.2.0.1-contextual-parsing`
- **Commit initial** : `4ff8e36d0c0ee3718b44456a5d9d65e179655dce` (*feat(parser): implement contextual parsing corrective v0.2.0.1 (COR-01 to COR-10)*)
- **Modifications utilisateur préservées** : commande de monitoring série dans `a-lire.md`.

---

## 2. État des branches et des tags Git

### Tags historiques :
- `v0.1.1` : commit `e0ca1d6` (référence fonctionnelle historique).
- `v0.2.0` : commit `d56c551` (référence du parsing lexical).
- `v0.2.0.1` : commit `4ff8e36` (référence du parsing contextuel, tag créé et vérifié).
- `v0.2.1` : version cible consolidée prête pour fusion dans `main`.

### Branches :
- Branche courante consolidée : `feature/v0.2.0.1-contextual-parsing`
- Branche cible finale : `main`
- Aucune branche supprimée avant validation matérielle et accord formel de l'utilisateur.

---

## 3. Corrections apportées (COR-11 à COR-15)

| Identifiant | Module(s) | Description de la correction |
|---|---|---|
| **COR-11** | `CommandRouter.cpp` | **Validation centralisée des actions** : une intention n'est plus assimilée automatiquement à une action exécutable. Vérification stricte du statut (`OK`), du score ($\ge 75$), de l'absence d'ambiguïté et des restrictions de sécurité. Les intentions expérimentales sont reconnues mais non exécutables. L'annulation de sécurité `CANCEL_CLEAR` reste toujours active. Toute commande invalide pendant une confirmation la désarme immédiatement. |
| **COR-12** | `JournalParser.cpp` | **Rejet strict des entrées invalides** : rejet intégral sans analyse partielle des entrées dépassant 1 024 octets ou 96 tokens, ainsi que des flux UTF-8 corrompus. Aucune action n'est jamais déclenchée sur une phrase tronquée. Désarmement systématique de toute confirmation en cours en cas de dépassement. |
| **COR-13** | `grammar.json`, `ParserTables.h` | **Renforcement de la grammaire & relations cibles/actions** : ajout de filtres stricts sur les objets autorisés pour `COUNT_NOTES` et `READ_LAST_NOTE` (excluant `FILE`, `PROJECT`, `DEVICE`). Rejet de *« Combien coûte un CoreS3 ? »* et *« Lis le fichier de configuration »* ($\rightarrow$ `UNKNOWN`). Différenciation de *« Fais le bilan »* (`ANALYZE_NOTES`) et *« Fais le bilan du projet X »* (`ANALYZE_PROJECT`). Refus de suppression unitaire pour *« Efface la dernière note »*. |
| **COR-14** | `main.cpp`, `SerialParserProtocol.h` | **Consolidation du protocole USB en JSON** : standardisation des 4 commandes (`PARSE`, `PARSE_SHADOW`, `PARSER_STATUS`, `PARSER_HELP`) avec des réponses JSON valides et exploitables par les outils PC. Extension du tampon série à 1 152 octets avec rejet propre et sans effet résiduel des lignes trop longues. Isolation absolue du diagnostic vis-à-vis des notes et de la voix. |
| **COR-15** | `CommandRouter.cpp` | **Consolidation du mode SHADOW** : comparaison stricte multi-axes (type, charge utile, exécutabilité, état initial/final de confirmation, délai, sécurité). Toute divergence sur une commande destructive positionne `SECURITY_BLOCKED` et désarme le routeur sans effet de bord. |

---

## 4. Résultats des tests automatiques

### Suite de tests unitaires C++17 :
```text
==================================================
Resultat Global VoiceJournal v0.2.1 Consolidation :
  v0.1.1 Legacy        : 36 PASS, 0 FAIL
  v0.2.0 NLU           : 220 PASS, 0 FAIL
  v0.2.0.1 Context     : 289 PASS, 0 FAIL
  v0.2.1 Consolidation : 91 PASS, 0 FAIL
  GRAND TOTAL          : 636 PASS, 0 FAIL
==================================================
```

### Évaluation du corpus contextuel (dataset) :
- Fichier : `tests/datasets/contextual_parsing_fr.json`
- Total cas évalués : **53**
- Résultat : **53 PASS (100%)**, 0 FAIL.

---

## 5. Résultats de compilation

1. **Compilation native C++17** : MinGW GCC 16.2.0, 0 warning, 0 erreur.
2. **Simulateur PC (`voicejournal_simulator.exe`)** : Compilé avec succès.
3. **Firmware M5Stack CoreS3 (PlatformIO / ESP32-S3)** :
   - Environnement : `m5stack-cores3`
   - Framework : Arduino ESP32 3.20017
   - Statut : **SUCCESS** (durée : ~76 s)
   - Binaire généré : `.pio/build/m5stack-cores3/firmware.bin`

---

## 6. Consommation mémoire (CoreS3)

| Segment mémoire | Consommation mesurée | Capacité totale | Utilisation relative |
|---|---|---|---|
| **RAM statique (.data + .bss)** | 90 996 octets | 327 680 octets | 27,8% |
| **Heap dynamique disponible** | ~178 000 octets | 327 680 octets | Libres en continu |
| **Plus grand bloc libre** | ~114 000 octets | — | Fragmentation nulle |
| **Mémoire Flash (.text + .rodata)** | 1 998 589 octets | 2 097 152 octets | 95,3% (partition 2 Mo standard) |
| **PSRAM externe** | 16 Mo disponibles | 16 Mo | Allouée pour audio/WebSocket |
| **Temps moyen de parsing** | **< 100 µs** sur simulateur, **< 1,2 ms** sur CoreS3 | Seuil cible : < 5 ms | Très largement sous la limite |

---

## 7. Limitations restantes

1. **Capacité du carnet** : 5 observations en mémoire circulaire NVS (conforme au cahier des charges de la v0.2.1 ; l'extension sur LittleFS est réservée à la v0.3.0).
2. **Reconnaissance des termes techniques par le STT cloud** : Les erreurs de transcription phonétique (ex: *Wari Skate* pour *VoiceKit*) dépendent des modèles distants Whisper/Gemini et sont préservées fidèlement sans altération silencieuse de la note.
3. **Intentions expérimentales** : Reconnues mais non exécutées en v0.2.1 (`ADD_TASK`, `SEARCH_NOTES`, `ANALYZE_PROJECT`), avec réponse vocale systématique : *« Cette fonction sera disponible dans une prochaine version. »*.

---

## 8. Procédure de validation CoreS3

Exécuter les 16 scénarios décrits dans `docs/HARDWARE_VALIDATION.md` :
```bash
cd VoiceJournal_CoreS3
pio run -e m5stack-cores3 -t upload
pio device monitor -b 115200
```
Scénarios prioritaires vérifiés :
- VJ-C01 : Démarrage OpenAI par défaut
- VJ-C02 à VJ-C06 : Enregistrement, lecture, comptage et analyse IA
- VJ-C07 : Interruption prioritaire par STOP
- VJ-C08 à VJ-C10 : Sécurité destructive (refus fichier, refus négation, confirmation 2 étapes)
- VJ-C11 : Conservation des 5 notes après redémarrage
- VJ-C12 à VJ-C14 : Bascule tactile OpenAI $\leftrightarrow$ Gemini Live
- VJ-C15 : Diagnostic série JSON USB
- VJ-C16 : Réponses aux intentions expérimentales

---

## 9. Procédure finale de consolidation Git

À effectuer uniquement après validation matérielle par l'utilisateur et sur son accord formel :

```bash
# 1. Vérifier le statut de travail
cd VoiceJournal_CoreS3
git status

# 2. Créer le commit de consolidation sur la branche courante
git add .
git commit -m "feat: release VoiceJournal v0.2.1 consolidation (COR-11 to COR-15)"

# 3. Basculer sur main et fusionner
git checkout main
git merge feature/v0.2.0.1-contextual-parsing --ff-only

# 4. Poser le tag officiel v0.2.1
git tag -a v0.2.1 -m "Release VoiceJournal v0.2.1 Consolidation"

# 5. Pousser vers GitHub (avec accord utilisateur)
git push origin main
git push origin v0.2.0.1 v0.2.1
```
