# Référence de base — VoiceJournal v0.2.0

Ce document consigne l'état complet, vérifié et reproductible de **VoiceJournal v0.2.0** avant les évolutions correctives et contextuelles de la version **v0.2.0.1**.

---

## 1. Référence Git et traçabilité

- **Dépôt Git :** `VoiceJournal_CoreS3`
- **Branche de départ :** `main`
- **Tag Git v0.1.1 :** `9e62342bb61cc07ea92574dffd90bb74c3dd4d05` (commit pointé : `e0ca1d6e7b2784d70d6f7ac6d8cdd655a01ff996`)
- **Tag Git v0.2.0 :** `75382b7d2e74deeb67eca081a8650eb484490368` (commit pointé : `d56c551d39327db41a708fb1c13fab7a2ae48548`)
- **Commit HEAD de référence baseline :** `9631b00` (*docs: add hardware test and validation report for M5Stack CoreS3*)
- **Branche de travail v0.2.0.1 :** `feature/v0.2.0.1-contextual-parsing` (créée à partir de ce commit)
- **Dépendance VoiceKit :** Commit `16646c73acf01a8d57408391bc6de15a288b8910` (préparation `v0.6.7.2`), strictement inchangée.

---

## 2. Empreinte mémoire de référence (v0.2.0)

Mesurée lors de la compilation PlatformIO en mode Release (`platformio.ini`, cible `m5stack-cores3`, ESP32-S3 @ 240 MHz) :

- **RAM statique utilisée :** `85 540 octets` sur 327 680 octets disponibles (**26,1 %**).
- **Mémoire Flash utilisée :** `1 827 965 octets` sur 2 097 152 octets alloués (**87,2 %**).
- **Partition utilisée :** `no_ota.csv` (2 Mo alloués pour l'application).
- **RAM libre restante :** 242 140 octets.
- **Flash libre restante :** 269 187 octets (12,8 %).

---

## 3. Résultats des tests existants (Banc natif v0.2.0)

Exécution du binaire de test compilé en C++17 natif :
```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Itests -Itests/mocks src/JournalStorage.cpp src/CommandRouter.cpp src/VoiceController.cpp src/JournalUI.cpp src/parser/TextNormalizer.cpp src/parser/Tokenizer.cpp src/parser/FuzzyMatcher.cpp src/parser/ConceptDictionary.cpp src/parser/NegationDetector.cpp src/parser/SlotExtractor.cpp src/parser/IntentMatcher.cpp src/parser/JournalParser.cpp tests/parser/test_nlu_suite.cpp tests/test_main.cpp -o tests/build/voicejournal_tests.exe
.\tests\build\voicejournal_tests.exe
```

Résultats :
```text
=== VoiceJournal v0.1.1 — Tests unitaires natifs C++17 ===
36 PASS, 0 FAIL

=== VoiceJournal v0.2.0 — Suite de tests NLU (220 cas de test) ===
[PASS] Catégorie 1 : Normalisation UTF-8 (16 tests)
[PASS] Catégorie 2 : Tokenisation (12 tests)
[PASS] Catégorie 3 : Dictionnaire de concepts (18 tests)
[PASS] Catégorie 4 : Reconnaissance des intentions (28 tests)
[PASS] Catégorie 5 : Extraction des paramètres (22 tests)
[PASS] Catégorie 6 : Tolérance aux erreurs (Fuzzy) (16 tests)
[PASS] Catégorie 7 : Négations et sécurité (22 tests)
[PASS] Catégorie 8 : Gestion des ambiguïtés (16 tests)
[PASS] Catégorie 9 : Suppression sécurisée (16 tests)
[PASS] Catégorie 10 : Comparaison historique SHADOW (22 tests)
[PASS] Catégorie 11 : Robustesse des entrées (12 tests)
Total NLU Tests : 220 PASS, 0 FAIL

Total Global : 256 PASS, 0 FAIL
```

Simulateur CLI (`simulator/voicejournal_simulator.exe --test`) :
- 17 tests exécutés : 17 PASS, 0 FAIL.

---

## 4. Architecture du moteur v0.2.0

Le pipeline NLU introduit dans la v0.2.0 se décompose ainsi :

```text
Texte d'entrée
      |
TextNormalizer (dépliage accents français, apostrophes, ponctuation, bijection offsets)
      |
Tokenizer (découpage en mots avec conservation des coordonnées)
      |
ConceptDictionary (lexique statique flash, masques 32 bits, lookup binaire)
      |
NegationDetector (détection de "ne...pas", "ne...plus", "sans", annulations)
      |
SlotExtractor (recherche d'entités techniques 'Gemini', 'CoreS3', projets, dates)
      |
IntentMatcher (scoring déterministe basé sur masques de concepts et règles)
      |
JournalParser (façade consolidant ParseResult avec fuzzy penalty)
      |
CommandRouter (modes LEGACY, INTELLIGENT, SHADOW)
```

---

## 5. Comportements connus et limitations identifiées

### 5.1 Comportements observés
- Le dictionnaire de concepts associe chaque mot à un masque binaire de concepts.
- Le scoring additionne des poids déterministes sur présence de concepts requis et bonus.
- Les intentions non destructives expérimentales (`SEARCH_NOTES`, `ADD_TASK`, `ANALYZE_PROJECT`) sont correctement qualifiées `executable = false`.
- La confirmation de suppression exige une formule explicite et rejette le fuzzy matching.

### 5.2 Défauts et limitations de la v0.2.0
1. **Absence d'analyse des objets de l'action (COR-05) :** Le moteur ne sait pas sur quel objet s'applique une action. Exemple critique constaté en test matériel : *« Supprime le fichier de configuration »* déclenche `REQUEST_CLEAR` (demande d'effacement du carnet), car le verbe « supprime » active le concept de suppression sans vérifier que l'objet visé est un fichier hors carnet.
2. **Absence de reconnaissance de locutions (COR-02) :** Les groupes de mots tels que *« je dois »*, *« il faut que »*, *« ne pas oublier de »*, *« la semaine dernière »* ne sont pas analysés comme une unité sémantique.
3. **Absence de rôles sémantiques (COR-01) :** Les mots sont réduits à des concepts sans représentation de leur rôle grammatical (sujet, obligation, action, objet, temps, entité).
4. **Portée des négations non contextualisée (COR-06) :** La présence de la négation est évaluée au niveau global de la phrase, ce qui peut fausser le parsing lorsque la négation fait partie du texte dicté de la note (ex: *« Note, ne supprime pas les fichiers »*) ou lorsqu'elle exprime une obligation positive (ex: *« N'oublie pas de tester Gemini »*).
5. **Défaut d'ambiguïté avec fuzzy matching (COR-07) :** L'application d'une pénalité fuzzy peut faire passer les scores sous le seuil d'acceptation ou sous le seuil de détection d'ambiguïté, masquant ainsi une collision réelle.
6. **Règles déclaratives JSON non entièrement découplées (COR-04) :** Bien que `intents.json` définisse des règles, certaines contraintes et logiques de détection sont codées en dur dans `IntentMatcher.cpp`.
7. **Comparateur SHADOW limité (COR-09) :** La comparaison SHADOW n'inspecte que le type de commande généré et l'intention, sans comparer les paramètres extraits, le caractère exécutable, ni l'état de confirmation avant/après analyse.
8. **Absence de limites d'entrées explicites et mesurées (COR-10) :** Utilisation intensive de chaînes dynamiques sans plafonnement strict prévenant la fragmentation ou le dépassement sur entrées aberrantes.
9. **Absence d'interface de test série sans STT sur CoreS3 (Section 17) :** Impossible d'injecter une commande textuelle par port série pour diagnostiquer le parser indépendamment du micro et du cloud STT.
10. **Parsing Lab mono-vue (Section 18) :** L'interface tactile de diagnostic ne propose qu'une seule vue d'information agrégée.

---

## 6. Résultats matériels de référence communiqués

Validation rapide issue des essais sur M5Stack CoreS3 physique (25 novembre 2026) :

```text
VoiceJournal v0.2.0 — Validation rapide

01 ADD_NOTE            PASS
02 ADD_NOTE synonyme   NOT RUN
03 ADD_NOTE synonyme   NOT RUN
04 READ_LAST_NOTE      PASS
05 COUNT_NOTES         PASS
06 ANALYZE_NOTES       PASS
07 SEARCH_NOTES        NOT RUN
08 ADD_TASK            NOT RUN
09 ANALYZE_PROJECT     NOT RUN
10 Négation DELETE     NOT RUN
11 Négation dans note  NOT RUN
12 DELETE hors carnet  PASS
13 AMBIGUOUS           PASS
14 UNKNOWN             PASS
15 STOP confirmation   PASS

Bilan : 8 PASS / 0 FAIL / 7 NOT RUN
```

Observations complémentaires du rapport matériel physique (`docs/RAPPORT_TEST_CORES3.md`) :
- **Tour #11 :** *« Supprime le fichier de configuration. »* $\rightarrow$ Classé par erreur comme `REQUEST_CLEAR` (score 95). La protection a fonctionné uniquement parce que le carnet était vide à ce moment précis, mais le routage était sémantiquement erroné.
- Les 8 scénarios PASS constituent la référence stricte de non-régression matérielle.
- Les 7 scénarios NOT RUN restent à valider formellement en conditions réelles ou par simulation textuelle.
