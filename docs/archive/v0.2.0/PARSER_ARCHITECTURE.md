# Architecture du Moteur NLU — VoiceJournal v0.2.0

## 1. Vue d'ensemble

La version v0.2.0 de VoiceJournal introduit un moteur de compréhension du langage naturel (**NLU — Natural Language Understanding**) local, déterministe, explicable, extensible et strictement optimisé pour les ressources du microcontrôleur **M5Stack CoreS3 (ESP32-S3)**.

### Principes directeurs de conception
1. **Indépendance totale :** `JournalParser` ne dépend d'aucun framework matériel (aucun appel direct à Arduino, M5GFX, VoiceKit ou NVS). Il est écrit en pur C++17 standard, compilable aussi bien sous PC (Windows/Linux/macOS) que sur ESP32-S3.
2. **Déterminisme et explicabilité :** Aucune boîte noire, aucun réseau de neurones, aucun modèle probabiliste opaque. Chaque décision de parsing s'appuie sur une règle nommée, une table de concepts compacte et un score explicable consultable en temps réel.
3. **Consommation mémoire bornée :** Les tables lexicales, règles et alias d'entités sont compilés sous forme de structures constantes stockées en mémoire Flash (`constexpr` / `PROGMEM`). L'empreinte RAM dynamique ajoutée au CoreS3 est strictement inférieure à **16 Ko** (~592 octets de données statiques mesurées).
4. **Temps de traitement temps réel :** Le temps de traitement complet d'une transcription audio est inférieur à **5 ms** (durée typique mesurée : **0,23 ms** / **230 µs** sur PC et < 1,5 ms sur ESP32-S3 à 240 MHz).
5. **Préservation scrupuleuse de la charge utile :** Le texte original UTF-8 n'est jamais altéré. Les accents, majuscules, acronymes techniques et ponctuations des notes dictées sont intégralement préservés.

---

## 2. Diagramme de flux de traitement

```text
               +----------------------------------+
               |     VoiceKit STT (OpenAI/Gemini) |
               +----------------------------------+
                                |
                                v
               +----------------------------------+
               |        Texte original UTF-8      |
               +----------------------------------+
                                |
                                v
             ========================================
             |            JournalParser             |
             |                                      |
             | 1. TextNormalizer                    |
             |    - Nettoyage UTF-8                 |
             |    - Suppression accents français    |
             |    - Détection élisions & quotes     |
             |    - Mapping bijectif norm<->orig    |
             |                                      |
             | 2. Tokenizer                         |
             |    - Découpage en tokens             |
             |    - Calcul des offsets orig/norm    |
             |                                      |
             | 3. ConceptDictionary & FuzzyMatcher  |
             |    - Recherche binaire dans Flash    |
             |    - Damerau-Levenshtein borné (d<=1)|
             |    - Masques binaires 32 bits        |
             |                                      |
             | 4. SlotExtractor                     |
             |    - Détection entités & projets     |
             |    - Détection types (BUG, TASK, ...) |
             |    - Détection dates relatives       |
             |    - Découpage sûr de la payload     |
             |                                      |
             | 5. NegationDetector                  |
             |    - Négations globales & verbes     |
             |    - Isolation payload dictée        |
             |    - Négation de confirmation        |
             |                                      |
             | 6. IntentMatcher                     |
             |    - Filtrage des règles candidates  |
             |    - Calcul déterministe des scores  |
             |    - Détection des ambiguïtés        |
             |    - Séparation Executable / Expér.  |
             ========================================
                                |
                                v
               +----------------------------------+
               |            ParseResult           |
               +----------------------------------+
                                |
                                v
               +----------------------------------+
               |           CommandRouter          |
               | (Modes: LEGACY, INTELLIGENT,     |
               |         SHADOW avec sécurité)    |
               +----------------------------------+
                                |
                                v
               +----------------------------------+
               |          VoiceController         |
               |   - Exécution JournalStorage     |
               |   - Réponse vocale VoiceKit      |
               |   - Diagnostics Parsing Lab      |
               +----------------------------------+
```

---

## 3. Composants détaillés

### 3.1 TextNormalizer
- **Rôle :** Convertit une chaîne UTF-8 brute en texte normalisé minuscule ASCII sans ponctuation tout en établissant une table de correspondance de positions bijectives (`normToOrigStart` et `normToOrigEnd`).
- **Traitement des accents :** Prise en charge exhaustive des caractères accentués français multi-octets UTF-8 (`é`, `è`, `ê`, `ë`, `à`, `â`, `î`, `ï`, `ô`, `ù`, `û`, `ü`, `ç`, `æ`, `œ`).
- **Espaces insécables et ponctuation :** Remplacement des apostrophes courbes (`’`), espaces insécables (`\u00A0`, `\u202F`), tirets cadratins et ponctuations par des espaces standard.
- **Préservation de la charge utile :** Grâce aux offsets d'origine, l'extraction de la note dictée conserve fidèlement le texte original avec sa casse et ses accents (ex. *"Note, Gemini se déconnecte sur le CoreS3."* extrait fidèlement *"Gemini se déconnecte sur le CoreS3."*).

### 3.2 Tokenizer
- **Rôle :** Isole les mots délimités par des espaces dans la chaîne normalisée.
- **Structure de Token :**
  ```cpp
  struct Token {
      std::string text;
      size_t normStart;
      size_t normEnd;
      size_t origStart;
      size_t origEnd;
      uint32_t conceptMask;
      bool isFuzzy;
      std::string matchedLexicon;
  };
  ```

### 3.3 ConceptDictionary
- **Rôle :** Associe chaque token à un ou plusieurs concepts abstraits via un masque binaire 32 bits (`uint32_t conceptMask`).
- **Stockage Flash :** Table triée par ordre alphabétique (`LEXICON_TABLE`) contenant 179 entrées lexicales générées automatiquement par `tools/generate_parser_tables.py` à partir de `rules/concepts.json`.
- **Recherche logarithmique :** Utilisation de `std::lower_bound` pour une complexité de recherche en $O(\log N)$.
- **Concepts multi-mots (Bigrammes) :** Prise en charge native des expressions composées comme *"aujourd hui"*, *"ce matin"*, *"apres demain"*, *"avant hier"*.

### 3.4 FuzzyMatcher
- **Rôle :** Applique une tolérance aux fautes d'inattention ou de transcription phonétique.
- **Algorithme :** Distance d'édition Damerau-Levenshtein bornée à une distance de **1** (insertion, suppression, substitution ou transposition adjacente).
- **Garde-fous de sécurité stricts :**
  - Mots courts (< 4 caractères) formellement exclus (ex. *"oui"*, *"non"*, *"lis"*, *"ne"*).
  - Chiffres et nombres formellement exclus.
  - Versions logicielles (ex. `v0.1.1`, `2.4`) formellement exclues.
  - Identifiants techniques et noms de projets formellement exclus.
  - Formulations de confirmation de suppression formellement exclues.
  - Rejet systématique en présence de plusieurs candidats équivalents.

### 3.5 NegationDetector
- **Rôle :** Détecte la présence de négations en langue française (`ne`, `n'`, `pas`, `plus`, `jamais`, `rien`, `aucun`, `aucune`) et qualifie leur portée.
- **Règle fondamentale d'isolation :** Lorsqu'un préfixe d'enregistrement (`ADD`) est détecté en tête de phrase (ex. *"Note, ne supprime pas les fichiers de VoiceKit."*), la négation appartient au contenu dicté et **ne doit en aucun cas** annuler l'intention d'ajout ni déclencher une annulation de suppression.
- **Sécurité des confirmations :** Détecte immédiatement des formulations pièges comme *"Je ne confirme pas la suppression"* et les transforme en `CANCEL_CLEAR`.

### 3.6 SlotExtractor
- **Rôle :** Extrait les paramètres associés aux intentions :
  - `entity` : Identifiant canonique (`VoiceKit`, `VoiceJournal`, `CoreS3`, `ESP32`, `OpenAI`, `Gemini`, `RuleBot`, `VoiceFlow`).
  - `project` : Projet technique ciblé.
  - `type` : Qualification (`PROBLEM`, `TASK`, `RESOLVED`).
  - `date_relative` : Expression temporelle (`hier`, `aujourd hui`, `demain`, `ce matin`, `ce soir`, `maintenant`).
  - `payload` : Charge utile brute nettoyée de la commande.

### 3.7 IntentMatcher
- **Rôle :** Rapproche les concepts, entités, slots et négations des règles d'intention déclaratives.
- **Classification :**
  - **Intentions exécutables v0.2.0 :** `ADD_NOTE`, `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR`, `CONFIRM_CLEAR`, `CANCEL_CLEAR`, `HELP`.
  - **Intentions expérimentales v0.2.0 :** `SEARCH_NOTES`, `ADD_TASK`, `RESOLVE_PROBLEM`, `LIST_TASKS`, `SEARCH_BY_PROJECT`, `SEARCH_BY_DATE`, `ANALYZE_PROJECT`.
  - **Statuts d'erreur / sécurité :** `UNKNOWN`, `AMBIGUOUS`, `NO_ACTION`.
- **Détection d'ambiguïté :** Si deux intentions différentes obtiennent un score proche ($\Delta \le 5$ points avec un score de base $\ge 80$), le moteur refuse de trancher silencieusement et émet le statut `AMBIGUOUS`.

---

## 4. Intégration système & Modes de fonctionnement

### 4.1 CommandRouter
Le `CommandRouter` fait le pont entre la chaîne de traitement audio et les composants métier de VoiceJournal. Il expose trois modes configurables dans `include/AppConfig.h` :
- **`ParserMode::LEGACY` :** Routage historique strict de la v0.1.1 (comparaison par préfixes et chaînes exactes).
- **`ParserMode::INTELLIGENT` :** Nouveau moteur NLU actif par défaut, exploitant l'ensemble des règles et concepts.
- **`ParserMode::SHADOW` :** Mode de validation comparative. Exécute le moteur legacy pour l'action effective, tout en exécutant silencieusement le moteur intelligent en parallèle. Calcule le statut de comparaison (`MATCH`, `NEW_INTENT`, `DIVERGENCE`, `SECURITY_BLOCKED`).

### 4.2 Règle absolue de sécurité destructive
En mode `SHADOW` comme en mode `INTELLIGENT`, toute divergence détectée sur une commande destructive (`REQUEST_CLEAR` ou `CONFIRM_CLEAR`) bloque immédiatement l'action destructive, désarme la confirmation en attente et assigne le statut `SECURITY_BLOCKED`.

### 4.3 Parsing Lab UI (CoreS3)
Sur l'écran tactile du M5Stack CoreS3, un bouton `[LAB]` permet de basculer sur un écran de diagnostic en direct. Cet écran affiche :
- La transcription brute reçue.
- L'intention retenue et son score de confiance.
- La liste des concepts et entités identifiés.
- Les indicateurs de négation et d'usage de fuzzy matching.
- Le temps d'exécution en microsecondes.
- Le comparatif en mode Shadow.
- Le bouton `STOP` conserve une priorité absolue et reste actif pour interrompre l'appareil à tout instant.
