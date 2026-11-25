# Rapport Correctif Complet — VoiceJournal v0.2.0.1 (Contextual Parsing)

**Auteur :** Antigravity Agent  
**Date :** 25 novembre 2026  
**Version :** `0.2.0.1`  
**Branche Git :** `feature/v0.2.0.1-contextual-parsing`  
**Tags de référence :** `v0.1.1` (`e0ca1d6e...`), `v0.2.0` (`d56c551d...`)

---

## 1. Contexte et Objectifs de la Corrective

La version v0.2.0 de VoiceJournal a posé les bases du parsing NLU embarqué sur M5Stack CoreS3. Toutefois, l'évaluation et les essais matériels ont mis en lumière que l'approche par mots-clés et détection lexicale isolée présentait des limites fondamentales :
- Manque de compréhension des relations entre actions et objets (ex: *"Efface le fichier"* risquait de déclencher une suppression globale de carnet).
- Interprétation erronée des locutions idiomatiques (*"ne pas oublier de..."* pris comme interdiction au lieu d'une obligation).
- Effet de bord du fuzzy matching pouvant masquer des ambiguïtés réelles en abaissant artificiellement les scores de confiance.
- Absence d'un protocole de diagnostic série autonome pour tester le moteur sans solliciter la capture vocale et le réseau.

L'objectif de la **v0.2.0.1** est d'instaurer un **moteur de parsing contextuel** complet, déterministe, explicable, tout en conservant scrupuleusement la compatibilité ESP32-S3 et l'intégralité du socle VoiceKit v0.6.7.2.

---

## 2. Analyse Détaillée des Évolutions par Exigence (COR-01 à COR-10)

### COR-01 — Analyse des rôles sémantiques
- **Implémentation** : `SemanticRoleAnalyzer.h` / `.cpp`.
- **Mécanisme** : Attribution de 12 rôles grammaticaux (`SUBJECT`, `ACTION`, `OBJECT`, `ENTITY`, `QUANTIFIER`, `QUALIFIER`, `TIME`, `NEGATION`, `OBLIGATION`, `QUESTION`, `CONNECTOR`, `PAYLOAD`).
- **Cadre sémantique** : Production d'un `SemanticFrame` typé capturant la structure syntaxique globale de la phrase.
- **Résultat** : *"Je dois tester Gemini demain"* $\rightarrow$ `je:SUBJECT`, `dois:OBLIGATION`, `tester:ACTION`, `Gemini:ENTITY`, `demain:TIME`.

### COR-02 — Reconnaissance des locutions multi-mots
- **Implémentation** : `PhraseRecognizer.h` / `.cpp`.
- **Mécanisme** : Détection *longest-match* déterministe sur une table Flash compilée de 51 expressions définies dans `rules/phrases.json`.
- **Résultat** : Reconnaissance immédiate de *"il faut que"*, *"ne pas oublier de"*, *"toutes les notes"*, *"la dernière observation"*, *"ce matin"*, *"la semaine dernière"*, etc. Les coordonnées de chaque token dans la chaîne d'origine restent préservées de manière bijective.

### COR-03 & COR-04 — Grammaire contextuelle & Règles déclaratives exécutées
- **Implémentation** : `GrammarEvaluator.h` / `.cpp`, `rules/grammar.json`, `tools/generate_parser_tables.py`.
- **Mécanisme** : Remplacement du code procédural codé en dur dans `IntentMatcher.cpp` par 19 règles déclaratives combinatoires ordonnées par priorité et baseScore.
- **Résultat** : Tables C++ `constexpr` générées automatiquement. L'ajout d'une règle de grammaire ou d'un synonyme ne requiert aucune retouche du code de bas niveau.

### COR-05 — Analyse des actions et de leurs objets
- **Implémentation** : `SemanticRoleAnalyzer.cpp`, `IntentMatcher.cpp`.
- **Mécanisme** : Classification des cibles en 8 catégories (`TargetCategory`).
- **Garde-fous** : La règle `request_clear_all` exige obligatoirement une cible de type `JOURNAL` ou `JOURNAL_NOTE` combinée à une portée globale (`hasGlobalScope == true`).
- **Résultat** : Des phrases comme *"Efface le fichier"*, *"Supprime le projet VoiceKit"*, *"Efface la dernière note"*, *"Supprime le mot Gemini"* ou *"Comment supprimer une note ?"* sont formellement refusées et ne déclenchent **JAMAIS** d'effacement du carnet.

### COR-06 — Portée des négations
- **Implémentation** : `NegationDetector.h` / `.cpp`.
- **Mécanisme** : Calcul explicite de la portée `negationScope` (`none`, `command`, `payload`, `obligation`, `confirmation`).
- **Résultat** :
  - *"Note, ne supprime pas les fichiers"* $\rightarrow$ négation classée `payload`, observation enregistrée avec succès.
  - *"Je ne dois pas tester Gemini"* $\rightarrow$ négation classée `obligation`, aucune tâche créée.
  - *"N'oublie pas de tester Gemini"* $\rightarrow$ locution positive neutralisée, tâche créée.
  - *"Je ne confirme pas la suppression"* $\rightarrow$ annulation formelle `CANCEL_CLEAR`.

### COR-07 — Tolérance aux erreurs (Fuzzy) & Arbitrage des ambiguïtés
- **Implémentation** : `FuzzyMatcher.cpp`, `IntentMatcher.cpp`.
- **Correction** : Séparation stricte du calcul de score brut (`rawScore`) et de la pénalité fuzzy (`fuzzyPenalty = 15`). L'ambiguïté est évaluée sur le score brut avant application de la pénalité.
- **Résultat** : Un rapprochement approximatif ne peut plus masquer un conflit d'intention réel. Aucun fuzzy n'est appliqué aux chiffres, versions logicielles, mots de moins de 4 lettres, ni aux confirmations destructives.

### COR-08 — Validation centralisée des actions et sécurité destructive
- **Implémentation** : `CommandRouter.cpp` (`routeIntelligent`).
- **Garde-fous** :
  - Rejet absolu si le parseur détecte une ambiguïté.
  - La répétition d'une demande d'effacement pendant l'attente ne confirme jamais la suppression (reste `REQUEST_CLEAR`).
  - Toute phrase hors sujet reçue pendant l'attente de confirmation désarme et annule immédiatement l'attente.
  - L'expiration temporelle (30 s) ou l'appui sur le bouton `STOP` désarme instantanément la confirmation.

### COR-09 — Comparateur SHADOW complet à 7 états
- **Implémentation** : `CommandRouter.cpp` (`routeShadow`).
- **États opérationnels** : `MATCH`, `NEW_INTENT`, `ACTION_DIVERGENCE`, `STATE_DIVERGENCE`, `PAYLOAD_DIVERGENCE`, `SECURITY_BLOCKED`, `UNKNOWN`.
- **Priorité de sécurité** : En cas de divergence sur une intention destructive (`REQUEST_CLEAR` ou `CONFIRM_CLEAR`), l'action est immédiatement convertie en `UNKNOWN` et le statut est verrouillé à `SECURITY_BLOCKED`.

### COR-10 — Robustesse mémoire et bornage strict des limites
- **Implémentation** : `JournalParser.cpp`, `ParserTypes.h`.
- **Bornes** : Longueur maximale d'entrée 1024 octets (`MAX_INPUT_BYTES`), nombre maximal de tokens 96 (`MAX_TOKENS`), validation stricte UTF-8 avec rejet des surrogates UTF-16 et séquences surlongues.
- **Comportement** : Tronquage et bornage propres sans dépassement de tampon ni crash mémoire.

---

## 3. Évolutions de l'Interface Utilisateur (Parsing Lab)

L'écran du Parsing Lab sur CoreS3 (320 × 240, rotation 0 native) a été enrichi d'un système à **3 sous-pages** permutables par simple contact tactile sur le tiers supérieur de la zone d'affichage :
- **Sous-page 1 : Résultat** (Transcription, Intention, Statut, Règle, Scores, Temps µs).
- **Sous-page 2 : Contexte sémantique** (Locutions, Rôles Sujet/Action/Objet/Entité/Date, Portée de négation, TargetCategory).
- **Sous-page 3 : Comparaison SHADOW** (Statut miroir, Type historique vs Type intelligent, Blocage de sécurité).

Le bouton rouge physique et tactile `STOP` reste prioritaire et actif en permanence.

---

## 4. Mode de Test Série USB Dédié (Section 17)

Ajout dans `src/main.cpp` d'un écouteur non bloquant sur le port USB Serial (115 200 bauds) prenant en charge :
- `PARSE <texte>`
- `PARSE_SHADOW <texte>`
- `PARSER_STATUS`
- `PARSER_HELP`

Ce mode garantit un fonctionnement 100% autonome, sans déclenchement du micro, sans appel cloud et sans modification de la mémoire NVS.

---

## 5. Synthèse des Résultats et Validation

- **Tests unitaires natifs** : **505 PASS, 0 FAIL (100%)**
  - v0.1.1 Legacy : 36 assertions PASS
  - v0.2.0 NLU : 220 assertions PASS
  - v0.2.0.1 Contextual : 249 assertions PASS
- **Jeu de données de validation** : **53/53 PASS (100%)** sur `tests/datasets/contextual_parsing_fr.json`.
- **Compilateur PlatformIO CoreS3** : Succès (`RAM: 26.3% - 86 276 octets, Flash: 87.9% - 1 844 049 octets`).
- **Empreinte mémoire additionnelle** : **+360 octets RAM** (bien en deçà de la limite de +16 Ko).
- **Temps de parsing** : **~220 µs** (largement sous le plafond de 50 ms).
