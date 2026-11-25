# Rapport de Release v0.2.0 — Intelligent Parsing Foundation

**Projet :** VoiceJournal CoreS3  
**Version cible :** v0.2.0  
**Version de référence :** v0.1.1 (commit `e0ca1d6e7b2784d70d6f7ac6d8cdd655a01ff996`)  
**Date :** 25 Novembre 2026  
**Auteur :** Antigravity Agentic Assistant & Calm Technology  

---

## 1. État Final du Dépôt Git

- **Tag de référence v0.1.1 :** Créé sur le commit d'audit initial `e0ca1d6e7b2784d70d6f7ac6d8cdd655a01ff996`.
- **Branche active de développement :** `feature/v0.2.0-intelligent-parsing`.
- **Intégrité de la branche principale (`main`) :** Aucune fusion automatique n'a été effectuée sur `main`. La branche `main` et le tag `v0.1.1` demeurent intacts et immédiatement récupérables.
- **Dépôt tiers VoiceKit :** Strictement inchangé (commit de référence conservé : `16646c73acf01a8d57408391bc6de15a288b8910`, v0.6.7.2).

---

## 2. Résumé Exécutif de l'Évolution Réalisée

La version v0.2.0 fait franchir à VoiceJournal une étape majeure : le passage d'un système de routage textuel rigide par préfixes à une **fondation NLU locale complète, déterministe et explicable**.

### Principaux jalons accomplis :
1. **Compréhension du langage naturel local (C++17) :** Pipeline de traitement complet sans dépendance cloud ni bibliothèque lourde externe (aucun modèle neuronal probabiliste, ni LVGL, ni LittleFS, ni base vectorielle).
2. **Normalisation UTF-8 avancée et préservation de charge utile :** Traitement complet des diacritiques français, apostrophes et élisions, tout en conservant une bijection exacte avec les positions du texte source pour restituer fidèlement accents, majuscules et notations techniques des observations dictées.
3. **Architecture déclarative à tables compilées en Flash :** Dictionnaire de concepts (`rules/concepts.json`), entités (`rules/entities.json`), alias (`rules/aliases.json`) et intentions (`rules/intents.json`) compilés sous forme de structures C++ constantes compactes (`ParserTables.h`), réduisant l'allocation RAM statique à moins de 600 octets.
4. **Sécurité destructive sans compromis :** Correction définitive de la faille historique de `isExactConfirmPhrase()` grâce à un analyseur de négations explicite et un filtrage strict interdisant tout fuzzy matching sur les confirmations.
5. **Mode SHADOW comparatif :** Possibilité d'exécuter silencieusement le moteur v0.2.0 en miroir du moteur v0.1.1 pour valider la non-régression in situ avec verrouillage automatique en cas de divergence sur action destructive.
6. **Parsing Lab embarqué (M5Stack CoreS3) :** Vue diagnostique temps réel affichant transcription, intentions, scores, masques de concepts, entités reconnues et temps d'exécution en microsecondes, avec bouton `STOP` prioritaire permanent.
7. **Simulateur PC autonome :** Binaire exécutable sur poste de développement offrant un mode REPL interactif, une option `--eval`, une option `--shadow` et l'exécution de la suite de tests.

---

## 3. Architecture Logicielle & Composants Créés

### 3.1 Arborescence des fichiers créés et modifiés

```text
VoiceJournal_CoreS3/
├── include/
│   ├── AppConfig.h                  # Version v0.2.0, ParserMode (LEGACY/INTELLIGENT/SHADOW)
│   ├── CommandRouter.h              # Routage triple mode, struct CommandResult enrichie
│   ├── JournalUI.h                  # Vue Parsing Lab, gestion du bouton [LAB] / [RETOUR]
│   ├── VoiceController.h            # Transmission ParsingLabData, traitement EXPERIMENTAL
│   └── parser/
│       ├── ParserTypes.h            # Enums IntentId, ConceptId, EntityId, struct ParseResult
│       ├── ParserTables.h           # Tables constantes C++ générées en mémoire Flash
│       ├── TextNormalizer.h         # Normaliseur UTF-8 & bijection d'offsets
│       ├── Tokenizer.h              # Découpage lexical & assignation des offsets
│       ├── ConceptDictionary.h      # Masques binaires 32 bits & recherche logarithmique
│       ├── FuzzyMatcher.h           # Distance Damerau-Levenshtein bornée avec exclusions
│       ├── NegationDetector.h       # Détecteur sémantique de négations françaises
│       ├── SlotExtractor.h          # Extracteur d'entités, projets, types, dates et payload
│       ├── IntentMatcher.h          # Scoring déterministe, détection d'ambiguïté
│       └── JournalParser.h          # Façade principale du moteur de parsing C++17
├── src/
│   ├── CommandRouter.cpp            # Logique de routage LEGACY, INTELLIGENT et SHADOW
│   ├── JournalUI.cpp                # Rendu écran Parsing Lab (320x240 M5GFX)
│   ├── VoiceController.cpp          # Gestion du toggle Lab et des retours vocaux adaptés
│   ├── main.cpp                     # Routage tactile du bouton LAB vers VoiceController
│   └── parser/
│       ├── TextNormalizer.cpp
│       ├── Tokenizer.cpp
│       ├── ConceptDictionary.cpp
│       ├── FuzzyMatcher.cpp
│       ├── NegationDetector.cpp
│       ├── SlotExtractor.cpp
│       ├── IntentMatcher.cpp
│       └── JournalParser.cpp
├── rules/
│   ├── concepts.json                # Source déclarative des concepts lexicaux
│   ├── intents.json                 # Règles de déclenchement des intentions
│   ├── entities.json                # Entités techniques canoniques et alias
│   └── aliases.json                 # Remplacement de locutions
├── tools/
│   └── generate_parser_tables.py    # Script de génération des tables constantes C++
├── simulator/
│   ├── CMakeLists.txt               # Configuration CMake du simulateur PC
│   ├── main.cpp                     # CLI, REPL et banc de test du simulateur
│   └── voicejournal_simulator.exe   # Binaire compilé autonome
├── tests/
│   ├── test_main.cpp                # Point d'entrée consolidé (Legacy + NLU)
│   ├── parser/
│   │   ├── test_nlu_suite.h         # Déclaration du banc de test NLU
│   │   └── test_nlu_suite.cpp       # 220 tests unitaires NLU (11 catégories)
│   └── build/
│       └── voicejournal_tests.exe   # Binaire consolidé des tests natifs
└── docs/
    ├── baseline/BASELINE_v0.1.1.md  # État des lieux et référence v0.1.1
    ├── PARSER_ARCHITECTURE.md       # Architecture technique détaillée
    ├── PARSER_RULES.md              # Spécification des règles et concepts
    ├── PARSER_USER_GUIDE.md         # Guide utilisateur CoreS3 et simulateur
    ├── REGRESSION_REPORT.md         # Rapport consolidé des 256 tests natifs
    ├── HARDWARE_VALIDATION.md       # Matrice de validation matérielle (44 scénarios)
    └── RELEASE_REPORT_v0.2.0.md     # Présent rapport de release
```

---

## 4. Synthèse des Tests & Validation Non-Régression

Le banc de test automatisé natif C++17 regroupe :
- **34 tests historiques v0.1.1 (36 assertions) :** Validation intégrale du stockage NVS A/B, de la rotation circulaire des 5 notes, de l'anti-rebond tactile, de la priorité absolue de `STOP` sous tous les états et de la gestion multi-fournisseurs (OpenAI / Gemini).
- **220 tests NLU v0.2.0 :** Couverture systématique des 11 catégories de parsing (normalisation, tokenisation, concepts, intentions, slots, fuzzy, négations, ambiguïtés, suppression sécurisée, shadow comparison, robustesse).

**Résultat global : 256 PASS / 256 (0 FAIL) — 100% de réussite.**

---

## 5. Résultats Détaillés de la Validation en Mode Shadow

Le mode Shadow a été validé sur 22 scénarios comparatifs :
- **MATCH (8 scénarios) :** Toutes les commandes vocales canoniques historiques (`ADD_NOTE`, `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR`, `CONFIRM_CLEAR`, `CANCEL_CLEAR`, `HELP`) produisent des résultats identiques entre les deux moteurs.
- **NEW_INTENT (7 scénarios) :** Les formulations enrichies reconnues par le moteur intelligent (ex. synonymes d'ajout comme *"Mémorise..."*, recherche de bugs, ajout de tâches, fuzzy matching) sont qualifiées en `NEW_INTENT` sans bloquer le comportement legacy.
- **SECURITY_BLOCKED (7 scénarios) :** En présence d'une divergence sur une action destructive (ex. *"Je ne confirme pas la suppression"*), le système bloque immédiatement l'effacement et désarme l'attente avec le statut `SECURITY_BLOCKED`.

---

## 6. Analyse Mémoire et Performances

### 6.1 Empreinte Mémoire sur ESP32-S3 (M5Stack CoreS3)
Mesures issues de la compilation PlatformIO en mode Release :
- **RAM utilisée :** `85 540 octets` sur 327 680 octets disponibles (**26,1 %**).
  - *Augmentation par rapport à v0.1.1 :* ~592 octets.
  - *Objectif du cahier des charges :* < 16 Ko de RAM ajoutée $\rightarrow$ **Objectif largement respecté**.
- **Mémoire Flash utilisée :** `1 827 965 octets` sur 2 097 152 octets alloués (**87,2 %**).
  - Les tables lexicales et règles générées occupent environ 14 Ko de mémoire Flash `PROGMEM`.

### 6.2 Temps de Traitement (Latence NLU)
- **Latence mesurée sur PC :** **210 à 240 µs** (0,21 à 0,24 ms).
- **Latence estimée sur ESP32-S3 (240 MHz) :** **0,8 à 1,4 ms**.
- *Plafond autorisé par le cahier des charges :* < 5 ms $\rightarrow$ **Excellente marge opérationnelle**.

---

## 7. Matrice de Validation Matérielle (CoreS3)

Conformément à la règle de rigueur et d'honnêteté technique du projet, les tests n'ayant pas été physiquement déroulés sur le boîtier CoreS3 physique connecté demeurent explicitement renseignés à l'état `NOT RUN` :

- **Scénarios VJ-01 à VJ-28 (Fonctionnalités v0.1.1) :** 28 scénarios `NOT RUN`.
- **Scénarios VJ-29 à VJ-44 (Nouveautés NLU v0.2.0) :** 16 scénarios `NOT RUN`.
- **Bilan matériel global :** `0 PASS` / `0 FAIL` / `44 NOT RUN`.
- La procédure complète et les traces attendues sont documentées dans [`docs/HARDWARE_VALIDATION.md`](file:///c:/Users/phall/Documents/Antigravity-5.1/VoiceJournal/VoiceJournal_CoreS3/docs/HARDWARE_VALIDATION.md).

---

## 8. Procédure de Test du Simulateur PC

Le simulateur autonome ne nécessite aucun matériel ESP32.

```powershell
# Accéder au répertoire du projet
cd c:\Users\phall\Documents\Antigravity-5.1\VoiceJournal\VoiceJournal_CoreS3

# 1. Exécuter le banc de validation automatique du simulateur (17 tests)
.\simulator\voicejournal_simulator.exe --test

# 2. Évaluer une phrase complexe avec préservation textuelle et entités
.\simulator\voicejournal_simulator.exe --eval "Note, Gemini se déconnecte sur le CoreS3."

# 3. Tester la tolérance aux fautes d'inattention (Fuzzy matching)
.\simulator\voicejournal_simulator.exe --eval "retruve les bugs de voicekit"

# 4. Tester l'interception de négation
.\simulator\voicejournal_simulator.exe --eval "Je ne confirme pas la suppression"

# 5. Évaluer en mode comparatif Shadow
.\simulator\voicejournal_simulator.exe --shadow "Fais le bilan du projet RuleBot"

# 6. Lancer le REPL interactif
.\simulator\voicejournal_simulator.exe
```

---

## 9. Procédure de Compilation et Flash sur M5Stack CoreS3

### Prérequis :
- Visual Studio Code avec l'extension PlatformIO IDE installée, ou PlatformIO Core CLI (`pio`).
- Câble USB-C connecté au port USB principal du M5Stack CoreS3.
- Fichier `include/secrets.h` configuré avec vos identifiants Wi-Fi et clés d'API (OpenAI et/ou Gemini).

### Commandes :
```powershell
cd c:\Users\phall\Documents\Antigravity-5.1\VoiceJournal\VoiceJournal_CoreS3

# Compiler le firmware
pio run -e m5stack-cores3

# Téléverser sur le M5Stack CoreS3
pio run -e m5stack-cores3 -t upload

# Ouvrir le moniteur série à 115 200 bauds
pio device monitor -b 115200
```

---

## 10. Guide des Commandes Vocales Supportées

### 10.1 Commandes Vocales Exécutables
1. **Ajout d'une observation (`ADD_NOTE`) :**
   - *« Note, <texte> »*, *« Mémorise <texte> »*, *« Enregistre <texte> »*, *« Retiens <texte> »*.
2. **Lecture de la dernière note (`READ_LAST_NOTE`) :**
   - *« Lis la dernière note »*, *« Relis mon observation »*.
3. **Comptage des notes (`COUNT_NOTES`) :**
   - *« Combien de notes ? »*, *« Compte les observations »*.
4. **Bilan / Synthèse IA (`ANALYZE_NOTES`) :**
   - *« Fais le bilan »*, *« Résume les observations »*, *« Synthèse du carnet »*.
5. **Demande d'effacement (`REQUEST_CLEAR`) :**
   - *« Efface toutes les notes »*, *« Supprime le carnet »*, *« Réinitialise les observations »*.
6. **Confirmation d'effacement (`CONFIRM_CLEAR`) :**
   - *« Confirmer la suppression »*, *« Oui efface »*, *« Supprimer définitivement »* (dans les 15 s suivant la demande).
7. **Annulation d'effacement (`CANCEL_CLEAR`) :**
   - *« Annuler »*, *« Ne supprime pas »*, *« Laisse tomber »*, ou appui sur `[STOP]`.
8. **Aide vocale (`HELP`) :**
   - *« Aide »*, *« Quelles sont les commandes ? »*.

### 10.2 Intentions Expérimentales Reconnues (Non Exécutables)
Lorsque l'utilisateur énonce l'une de ces commandes, le CoreS3 répond :
*« La demande est comprise, mais cette fonction n'est pas encore disponible. »*
- `SEARCH_NOTES` : *« Retrouve les observations sur la batterie »*
- `ADD_TASK` : *« Je dois tester Gemini demain »*
- `RESOLVE_PROBLEM` : *« Le problème WiFi est corrigé »*
- `LIST_TASKS` : *« Liste les tâches à faire »*
- `SEARCH_BY_PROJECT` : *« Cherche dans le projet VoiceKit »*
- `SEARCH_BY_DATE` : *« Observations d'hier »*, *« Notes de ce matin »*
- `ANALYZE_PROJECT` : *« Fais le bilan du projet RuleBot »*

---

## 11. Limitations Connues & Dette Technique

1. **Dates absolues non modélisées :** Les expressions temporelles sont limitées aux dates relatives (`hier`, `aujourd'hui`, `demain`, `ce matin`, `ce soir`). Aucun calendrier absolu (ex. *« le 15 décembre »*) ni synchronisation NTP n'est intégré dans cette version.
2. **Capacité du carnet :** Le stockage reste strictement borné à **5 notes circulaires** de **180 octets UTF-8** maximum chacune en NVS A/B.
3. **Partition Flash :** La Flash est occupée à **87,2 %** avec le framework ESP-IDF/Arduino complet, M5Unified et VoiceKit. Une future partition de 4 Mo ou 8 Mo sera requise pour de nouveaux modèles lexicaux étendus.
4. **Validation matérielle :** L'ensemble des 44 scénarios matériels doit faire l'objet d'une campagne de validation physique sur banc CoreS3.

---

## 12. Perspectives pour la v0.3.0

1. **Activation des fonctionnalités expérimentales :** Implémentation du moteur de recherche dans les observations existantes (`SEARCH_NOTES`) et du filtrage par projet (`SEARCH_BY_PROJECT`).
2. **Gestionnaire de tâches dédié :** Séparation en NVS d'une banque d'observations techniques et d'une banque de tâches (`TODO`) avec statut résolu/non résolu.
3. **Horodatage absolu via NTP :** Intégration d'une synchronisation d'horloge réseau pour associer un timestamp ISO-8601 à chaque observation et permettre la recherche par plage de dates.
4. **Optimisation de la partition Flash :** Migration vers une table de partition personnalisée (type `large_spiffs` ou `custom_app`) pour porter l'espace applicatif à 3 ou 4 Mo.
