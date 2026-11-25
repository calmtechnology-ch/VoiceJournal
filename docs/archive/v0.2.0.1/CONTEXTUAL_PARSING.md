# Architecture du Moteur de Parsing Contextuel — VoiceJournal v0.2.0.1

## 1. Vision et Objectifs

VoiceJournal v0.2.0.1 fait évoluer le moteur d'analyse syntaxique vers un **moteur contextuel déterministe** capable de comprendre les relations sémantiques entre les mots, les concepts, les actions et leurs objets.

L'objectif n'est plus simplement d'identifier des mots-clés indépendants, mais de construire une représentation sémantique structurée (`SemanticFrame`) de l'intention utilisateur.

Le moteur respecte les contraintes impératives suivantes :
- **100% Local et Déterministe** : aucun appel cloud, aucun LLM, aucun réseau de neurones, aucun embedding ni base vectorielle.
- **Explicable** : chaque décision est traçable via les locutions reconnues, les rôles sémantiques, la règle de grammaire appliquée, les scores bruts/ajustés et le détail des pénalités.
- **Ultra-léger pour ESP32-S3** : tables constantes compilées en ROM/Flash (`PROGMEM`), zéro allocation dynamique sur le tas pendant l'évaluation des règles, empreinte RAM additionnelle inférieure à 400 octets (bien en deçà du plafond de 16 Ko).
- **Temps d'exécution garanti** : analyse complète d'une phrase en moins de 350 microsecondes (plafond admissible : 50 millisecondes).

---

## 2. Pipeline de Traitement Contextuel en 10 Étapes

```text
Texte brut (Transcription STT ou Commande Série)
       |
[1] Vérification des limites de robustesse mémoire (COR-10)
       |
[2] Validation & Normalisation UTF-8 (coordonnées bijectives)
       |
[3] Tokenisation bornée (max 96 tokens)
       |
[4] Analyse lexicale & Dictionnaire de concepts (masques binaires 32 bits)
       |
[5] Reconnaissance des locutions multi-mots (COR-02, longest-match)
       |
[6] Analyse et portée des négations (COR-06)
       |
[7] Analyse des rôles sémantiques (COR-01, 12 rôles)
       |
[8] Construction du cadre sémantique (SemanticFrame, COR-05)
       |
[9] Évaluation de la grammaire contextuelle & Filtrage de sécurité (COR-03, COR-04, COR-05, COR-08)
       |
[10] Extraction des paramètres & Production du ParseResult
       |
CommandRouter (INTELLIGENT, LEGACY, ou SHADOW avec 7 statuts)
```

### Détail des étapes :

1. **Vérification des limites de robustesse (COR-10)** : Rejet immédiat de toute entrée dépassant 1024 octets (`INPUT_TOO_LONG`), avec préservation de la qualification de commande (ex: `ADD_NOTE`).
2. **Normalisation UTF-8** : Conversion en minuscules, suppression des accents pour l'appariement lexical, standardisation des apostrophes et des espaces insécables, tout en conservant une table de correspondance bijective vers les indices du texte brut original.
3. **Tokenisation bornée** : Découpage en mots avec limitation stricte à 96 tokens sans dépassement de tampon mémoire.
4. **Analyse lexicale** : Recherche binaire dans le dictionnaire de concepts compilé (231 entrées constantes) avec masques de bits `ConceptId`.
5. **Reconnaissance des locutions (COR-02)** : Algorithme déterministe privilégiant la locution valide la plus longue (*longest-match*) sur une table de 51 expressions prédéfinies.
6. **Portée des négations (COR-06)** : Distinction formelle entre négation de la commande principale (`command`), négation contenue dans l'observation dictée (`payload`), négation d'une obligation (`obligation`) et refus explicite de confirmation (`confirmation`).
7. **Rôles sémantiques (COR-01)** : Attribution de 12 rôles (`SUBJECT`, `ACTION`, `OBJECT`, `ENTITY`, `TIME`, `NEGATION`, `OBLIGATION`, `QUANTIFIER`, `QUESTION`, `CONNECTOR`, etc.).
8. **Construction du cadre sémantique (`SemanticFrame`)** : Synthèse de l'action, de l'objet, de la catégorie de cible (`TargetCategory`), de l'entité technique et de la date relative.
9. **Grammaire contextuelle (COR-03, COR-04, COR-05, COR-08)** : Évaluation des 19 règles déclaratives constantes compilées en tables Flash. Rejet strict des cibles non autorisées (ex: `FILE`, `PROJECT`, suppressions unitaires) pour `REQUEST_CLEAR`. Détection des ambiguïtés indépendamment des pénalités de score approximatif.
10. **Extraction des paramètres** : Extraction canonique des entités techniques, types de problème/statut et dates relatives dans les slots du résultat.

---

## 3. Modularité du Code

L'architecture du moteur contextuel est divisée en composants découplés situés dans `src/parser/` et `include/parser/` :

| Composant | Fichiers | Responsabilité |
|---|---|---|
| `JournalParser` | `JournalParser.h`, `.cpp` | Façade principale orchestrant le pipeline, validation UTF-8 et bornage mémoire |
| `TextNormalizer` | `TextNormalizer.h`, `.cpp` | Normalisation du texte, suivi d'indices bijectifs et extraction propre de la charge utile |
| `Tokenizer` | `Tokenizer.h`, `.cpp` | Découpage en tokens avec coordonnées d'origine |
| `ConceptDictionary` | `ConceptDictionary.h`, `.cpp` | Consultation du dictionnaire lexical constant et masquage binaire 32 bits |
| `PhraseRecognizer` | `PhraseRecognizer.h`, `.cpp` | Détection longest-match des locutions multi-mots (COR-02) |
| `SemanticRoleAnalyzer` | `SemanticRoleAnalyzer.h`, `.cpp` | Annotation des rôles sémantiques et construction du `SemanticFrame` (COR-01, COR-05) |
| `GrammarEvaluator` | `GrammarEvaluator.h`, `.cpp` | Évaluation générique des règles déclaratives combinatoires (COR-03, COR-04) |
| `NegationDetector` | `NegationDetector.h`, `.cpp` | Analyse de la polarité et calcul de la portée des négations (COR-06) |
| `FuzzyMatcher` | `FuzzyMatcher.h`, `.cpp` | Distance Damerau-Levenshtein bornée avec exclusion stricte des chiffres, confirmations et mots courts (COR-07) |
| `SlotExtractor` | `SlotExtractor.h`, `.cpp` | Extraction des entités, projets, statuts et dates |
| `IntentMatcher` | `IntentMatcher.h`, `.cpp` | Arbitrage des intentions, gestion d'ambiguïté et garde-fous de sécurité |
| `CommandRouter` | `CommandRouter.h`, `.cpp` | Routage applicatif, validation centralisée des actions (COR-08) et comparateur SHADOW à 7 états (COR-09) |
| `ParserTables` | `ParserTables.h` | Tables constantes C++ générées automatiquement par `tools/generate_parser_tables.py` |

---

## 4. Gestion de la Mémoire et Performance Embarquée

- **Zéro allocation dynamique sur le tas pendant l'évaluation** : Les structures `Token`, `RecognizedPhrase`, `IntentCandidate` et `SlotValue` sont instanciées avec des capacités bornées et préallouées sur la pile.
- **Tables Flash** : Toutes les règles issues de `rules/*.json` sont converties en structures `constexpr` C++ par `tools/generate_parser_tables.py`. Aucun parseur JSON dynamique n'est embarqué sur l'ESP32.
- **Empreinte mémoire mesurée sur CoreS3** :
  - RAM utilisée : 86 276 octets (26,3% de 320 Ko).
  - Évolution par rapport à la v0.2.0 (+360 octets).
  - Flash utilisée : 1 844 049 octets (87,9% de la partition 2 Mo).
- **Temps d'exécution moyen** :
  - Commande simple (`ADD_NOTE`) : ~180 µs.
  - Commande contextuelle complète (`Je dois tester Gemini demain`) : ~216 µs.
  - Commande avec comparaison SHADOW : ~347 µs.
