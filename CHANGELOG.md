# Changelog — VoiceJournal

Toutes les évolutions notables de **VoiceJournal** sont consignées dans ce fichier.

## [0.3.0] - 2026-11-25 — Structured Memory Release

Version majeure transformant VoiceJournal en un véritable assistant de mémoire personnelle et technique structurée avec stockage LittleFS et interface graphique sous LVGL 9.

### Ajouté
- **Mémoire structurée LittleFS (`JournalRepository`)** :
  - Journal append-only avec format binaire délimité, CRC32, récupération sur coupure de courant et compactage contrôlé.
  - Index en mémoire RAM (~32 Ko pour 1 000 entrées) permettant des requêtes immédiates sans lecture disque.
  - Modèle de données enrichi (`MemoryModel.h`) : types d'entrées (`NOTE`, `PROBLEM`, `TASK`, `IDEA`, `DECISION`, `RESULT`), statuts de cycle de vie (`NONE`, `OPEN`, `IN_PROGRESS`, `COMPLETED`, `RESOLVED`, `CANCELLED`), association à des projets, étiquettes et entités.
  - Abstraction du système de fichiers (`IFileSystem`, `NativeFileSystem` sur PC, `LittleFSFileSystem` sur ESP32-S3).
- **Interface graphique moderne LVGL 9 (`JournalUI`)** :
  - Intégration de LVGL 9.6.0 avec pilote d'affichage M5GFX (RGB565 swapped, rotation 0 paysage natif 320 × 240).
  - Navigation par onglets : Accueil, Journal (3 cartes réutilisables paginées), Projets, Tâches.
  - Écran de détail avec lecture vocale de note (`READ_ENTRY`) et bascule de statut tactile.
  - Bannière vocale permanente avec bouton STOP prioritaire instantané et statut IA/fournisseur.
  - Intégration des polices Segoe UI avec accents français complets (`font_french_12`, `font_french_14`, `font_french_bold_16`).
- **Services métier découplés** :
  - `ProjectService` : création, alias, bascule de projet courant (« Ouvre le projet VoiceKit »), archivage.
  - `TaskService` : création de tâches (« Je dois tester Gemini demain »), clôture (« La tâche tester Gemini est terminée »), réouverture, annulation.
  - `SearchService` : moteur de recherche multicritère local sans recours à une base vectorielle (recherche par mots-clés normalisés, entités, projets, types, statuts, dates relatives).
  - `TimeService` : horodatage interne en UTC, synchronisation RTC + NTP non bloquante, fuseau horaire `Europe/Zurich` avec gestion automatique heure d'été / hiver, analyseur de dates relatives en français.
  - `AnalysisService` : constructeur de contexte d'analyse IA borné (10 observations max, 1 500 octets max, balisage `<OBSERVATIONS_UTILISATEUR>`).
- **Migration automatique NVS vers LittleFS (`StorageMigration`)** :
  - Migration non destructive et idempotente des notes v0.2.1 vers la mémoire structurée au premier démarrage.
  - Conservation intégrale des clés et données NVS d'origine.
- **Table de partitionnement 16 Mo (`partitions_16MB.csv`)** :
  - Partition applicative portée à 6 Mo (occupation 39,1 %).
  - Partition LittleFS dédiée de 9 Mo.
  - Partition NVS préservée à l'adresse exacte d'origine `0x9000` (20 Ko).
- **Protocole série JSON étendu** :
  - Nouvelles commandes série : `JOURNAL_STATUS`, `GET_ENTRY`, `LIST_PROJECTS`, `LIST_TASKS`, `EXPORT_JOURNAL`.
- **Suite de tests Structured Memory v0.3.0 (`test_v030_structured_memory_suite`)** :
  - 10 nouveaux scénarios complets couvrant le repository, la tolérance aux coupures d'alimentation, le compactage, la migration NVS, les services et le routeur de commandes.
  - Total global porté à **646 tests PASS, 0 FAIL (100% succès)**.

---

## [0.2.1] - 2026-11-25 — Consolidation Release

Version de consolidation générale de VoiceJournal, établissant un socle logiciel unifié, robuste et sécurisé pour l'ESP32-S3 / M5Stack CoreS3.

### Ajouté
- **COR-11 — Validation centralisée des actions (`CommandRouter`)** :
  - Découplage strict entre intention reconnue et action autorisée.
  - Vérification systématique du statut de parsing, du score minimal, du caractère exécutable (`isExecutable`), de l'absence d'ambiguïté et des restrictions métier.
  - Typage des intentions expérimentales (`ADD_TASK`, `ANALYZE_PROJECT`) en `CommandType::EXPERIMENTAL` avec `isExecutable = false` et réponse vocale dédiée : *« Cette fonction sera disponible dans une prochaine version. »*.
  - Préservation inconditionnelle des annulations d'urgence (`CANCEL_CLEAR`, `STOP`).
- **COR-12 — Rejet strict des entrées invalides (`JournalParser`)** :
  - Rejet intégral sans tronquage des entrées dépassant 1 024 octets (`INPUT_TOO_LONG`) ou 96 tokens (`TOO_MANY_TOKENS`).
  - Validation UTF-8 préalable immédiate (`INVALID_UTF8`) avant toute allocation dynamique.
  - Désarmement immédiat d'une confirmation de suppression en attente en cas d'entrée invalide.
- **COR-13 — Renforcement des règles de grammaire (`rules/grammar.json`)** :
  - Ajout des contraintes `allowedTargets: ["JOURNAL", "JOURNAL_NOTE"]` et `forbiddenTargets: ["FILE", "PROJECT", "DEVICE"]` sur les requêtes de comptage et de lecture.
  - Suppression du fuzzy matching sur toute confirmation destructive.
  - Rejet de toute suppression visant un objet autre que le carnet complet (`supprime le fichier de configuration` $\rightarrow$ `UNKNOWN`).
- **COR-14 — Consolidation du protocole série USB JSON (`SerialParserProtocol.h`)** :
  - Réponses JSON normalisées et typées pour `PARSE`, `PARSE_SHADOW`, `PARSER_STATUS` et `PARSER_HELP`.
  - Capacité du tampon série portée à 1 152 octets avec purge automatique de ligne en cas de débordement.
  - Isolation totale du flux de diagnostic par rapport aux actions métier et au stockage NVS.
- **COR-15 — Consolidation du mode SHADOW de non-régression** :
  - Comparaison isolée des résultats d'intention, de payload, d'exécutabilité et de sécurité.
  - Détection des divergences critiques (`SECURITY_BLOCKED`) et de structure d'exécution.
  - Correction de l'armement prématuré de confirmation dans le parseur intelligent.
- **Suite de tests de consolidation v0.2.1 (`test_consolidation_v021_suite`)** :
  - 91 nouveaux tests unitaires et d'intégration validant les correctifs COR-11 à COR-15 et les suppressions sur NVS réelle.
  - Total de la suite de tests natifs porté à **636 tests réussis à 100%**.
- **Refonte de la documentation** :
  - `README.md` synthétique répondant aux 8 questions clés.
  - `USER_GUIDE.md`, `ARCHITECTURE.md`, `HARDWARE_VALIDATION.md`, `SERIAL_PARSER_PROTOCOL.md`, `ROADMAP.md` et `CONSOLIDATION_REPORT_v0.2.1.md`.
  - Archivage des versions antérieures dans `docs/archive/v0.1.1/`, `docs/archive/v0.2.0/` et `docs/archive/v0.2.0.1/`.

### Modifié
- Fournisseur IA initial par défaut réaligné sur **OpenAI Realtime** (`AiProvider::OPENAI`) dans `AppConfig.h`.
- Écran principal CoreS3 simplifié : affichage des seules informations utiles, suppression du jargon technique en dehors du mode LAB.
- Indicateurs Lab enrichis des 5 statuts explicites : `AUTORISEE`, `FONCTION INDISPONIBLE`, `COMMANDE INCONNUE`, `COMMANDE AMBIGUE`, `REFUS DE SECURITE`.

---

## [0.2.0.1] - 2026-11-25 — Contextual Parsing Corrective

Version corrective majeure du moteur de compréhension du langage naturel (NLU) de VoiceJournal, faisant évoluer le modèle vers un **moteur contextuel local, déterministe et explicable** pour M5Stack CoreS3 (ESP32-S3).

### Ajouté
- **COR-01 — Analyse des rôles sémantiques (`SemanticRoleAnalyzer`)** :
  - Énumération `SemanticRole` à 12 rôles formels : `UNKNOWN`, `SUBJECT`, `ACTION`, `OBJECT`, `ENTITY`, `QUANTIFIER`, `QUALIFIER`, `TIME`, `NEGATION`, `OBLIGATION`, `QUESTION`, `CONNECTOR`, `PAYLOAD`.
  - Structure `SemanticFrame` associant action principale, objet, portée, entités cibles, marqueurs temporels et modaux.
- **COR-02 — Reconnaissance des locutions (`PhraseRecognizer`)** :
  - Analyseur déterministe privilégiant la locution valide la plus longue (*longest match*).
  - Déclaration de 51 locutions multi-mots dans `rules/phrases.json` (obligations, négations, fenêtres temporelles, portées globales, expressions idiomatiques).
  - Préservation des positions et pronoms d'origine.
- **COR-03 — Grammaire sémantique légère (`GrammarEvaluator`)** :
  - Moteur de règles combinatoires ordonnées évaluant rôles obligatoires, facultatifs et interdits.
  - 19 règles formelles déclarées dans `rules/grammar.json`, compilées en tables constantes C++ (`ParserTables.h`).
- **COR-04 — Règles déclaratives exécutées et audit de synchronisation** :
  - Synchronisation stricte entre `rules/concepts.json`, `rules/phrases.json`, `rules/grammar.json`, `rules/entities.json`, `rules/aliases.json` et le générateur Python `tools/generate_parser_tables.py`.
  - Évaluation générique des contraintes sans interprétation dynamique de JSON sur l'ESP32.
- **COR-05 — Typage sémantique des actions et de leurs objets** :
  - Catégorisation des objets cibles (`TargetCategory`) : `JOURNAL`, `JOURNAL_NOTE`, `TASK`, `PROJECT`, `PROBLEM`, `FILE`, `DEVICE`, `UNKNOWN_OBJECT`.
  - Rejet systématique en `UNKNOWN` de toute suppression ciblant des objets non autorisés (`FILE`, `PROJECT`, `DEVICE`, etc.) ou une note singulière sans portée globale.
- **COR-06 — Portée contextuelle des négations (`NegationDetector`)** :
  - Détection sémantique de la portée : négation d'action, d'obligation, d'annulation ou contenue dans le texte dicté.
  - Préservation du statut `ADD_NOTE` pour les notes dictées avec négations (*"Note, ne supprime pas les fichiers"*).
  - Interprétation positive des locutions d'obligation négative (*"ne pas oublier de"*, *"n'oublie pas de"* $\rightarrow$ `ADD_TASK`).
- **COR-07 — Découplage fuzzy matching et détection d'ambiguïtés** :
  - Évaluation de l'ambiguïté basée sur le score brut avant pénalité de distance Damerau-Levenshtein.
  - Détection immédiate d'ambiguïté en cas de collision entre verbes d'action concurrents sans subordination (*"Lis efface"* $\rightarrow$ `AMBIGUOUS`).
  - Interdiction du fuzzy sur nombres, versions, identifiants techniques et confirmations destructives.
- **COR-08 — Validation centralisée et sécurité destructive (`CommandRouter`)** :
  - Exigence stricte d'une portée globale (`hasGlobalScope == true`) et d'une cible `JOURNAL` / `JOURNAL_NOTE` pour `REQUEST_CLEAR`.
  - Blocage de sécurité `SECURITY_BLOCKED` en cas de divergence destructive en mode `SHADOW`.
- **COR-09 — Comparateur SHADOW complet** :
  - Implémentation des 7 statuts formels : `MATCH`, `SEMANTIC_EQUIVALENT`, `SMART_UPGRADE`, `TARGET_MISMATCH`, `DIVERGENT`, `SECURITY_BLOCKED`, `SYNTAX_ERROR`.
- **COR-10 — Borne mémoire, résilience et mode diagnostic série USB** :
  - Limite stricte `MAX_INPUT_BYTES = 1024` et `MAX_TOKENS = 96`.
  - Validation UTF-8 conforme RFC 3629 (rejet des surlongueurs et des demi-codets UTF-16 surrogates).
  - Protocole série USB de diagnostic : commandes `PARSE <texte>`, `PARSE_SHADOW <texte>`, `PARSER_STATUS`, `PARSER_HELP`, retour JSON standardisé, isolation absolue sans effets de bord audio/stockage.
  - Interface tactile Parsing Lab CoreS3 enrichie à 3 vues (`Vue 1 : Résultat`, `Vue 2 : Contexte sémantique`, `Vue 3 : Comparaison Shadow`).
- **Simulateur autonome étendu** :
  - Support de `--dataset <path>` avec rapport de validation automatisé (`tests/datasets/contextual_parsing_fr.json`, 53 cas, 100% PASS).
- **Suite de tests natifs C++17** :
  - Ajout de 249 tests contextuels (Catégories 12 à 21).
  - Grand total de **505 tests PASS à 100%** (36 legacy v0.1.1 + 220 NLU v0.2.0 + 249 contextuels v0.2.0.1).
  - Empreinte RAM ESP32-S3 préservée : +360 octets statiques (86 276 octets / 26,3% RAM, 1 844 049 octets / 87,9% Flash).

---

## [0.2.0] - 2026-11-25 — Intelligent Parsing Foundation

### Ajouté
- **Moteur NLU local C++17 embarqué (`JournalParser`)** :
  - Découpage modulaire : `TextNormalizer`, `Tokenizer`, `ConceptDictionary`, `FuzzyMatcher`, `NegationDetector`, `SlotExtractor`, `IntentMatcher`, `JournalParser`.
  - Normalisation UTF-8 avancée avec gestion exhaustive des accents français, ligatures, élisions et ponctuation.
  - Préservation intégrale et bijective de la charge utile originale (accents, casse, ponctuation et notations techniques préservés en NVS).
  - Dictionnaire de concepts compact en Flash (masques binaires 32 bits, recherche logarithmique en $O(\log N)$).
  - Tolérance aux fautes d'inattention via distance Damerau-Levenshtein bornée ($d \le 1$), avec garde-fous stricts interdisant le fuzzy sur mots courts (< 4 caractères), nombres, versions logicielles, identifiants techniques et confirmations.
  - Détection sémantique des négations avec isolation de la charge utile (ex. *"Note, ne supprime pas..."* est traité comme `ADD_NOTE`).
  - Détection des ambiguïtés lorsque deux intentions concurrentes ont un score proche ($\Delta \le 5$).
- **Nouvelles intentions reconnues** :
  - Exécutables : `ADD_NOTE` (avec synonymes *"Mémorise"*, *"Enregistre"*, *"Retiens"*), `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR`, `CONFIRM_CLEAR`, `CANCEL_CLEAR`, `HELP`.
  - Expérimentales (reconnues sans altération de stockage) : `SEARCH_NOTES`, `ADD_TASK`, `RESOLVE_PROBLEM`, `LIST_TASKS`, `SEARCH_BY_PROJECT`, `SEARCH_BY_DATE`, `ANALYZE_PROJECT`.
- **Modes de parsing configurables (`CommandRouter`)** :
  - `ParserMode::INTELLIGENT` (défaut) : moteur NLU complet.
  - `ParserMode::LEGACY` : comportement historique strict v0.1.1.
  - `ParserMode::SHADOW` : exécution legacy avec analyse miroir intelligente et blocage de sécurité en cas de divergence destructive (`SECURITY_BLOCKED`).
- **Parsing Lab sur M5Stack CoreS3 (`JournalUI`)** :
  - Bouton tactile `[LAB]` / `[RETOUR]` permettant de basculer sur un écran de diagnostic en direct.
  - Bouton `STOP` rouge prioritaire maintenu en permanence.
- **Simulateur autonome PC (`simulator/`)** :
  - Exécutable autonome compilable sur PC avec options `--test`, `--eval`, `--shadow` et mode REPL interactif.
- **Banc de tests natifs étendu** :
  - Ajout de 220 tests NLU couvrant 11 catégories (`test_nlu_suite.cpp`).
  - Total général de **256 tests PASS** (36 assertions v0.1.1 + 220 NLU).

### Corrigé
- **Sécurité des confirmations destructives** : correction de la fonction historique `isExactConfirmPhrase()` pour refuser formellement toute négation (*"Je ne confirme pas..."*) ou confirmation approximative (*"oui"*, *"d'accord"*).

---

## [0.1.1] - 2026-11-23 — Multi-fournisseurs OpenAI & Gemini

### Ajouté
- **Support multi-fournisseurs OpenAI Realtime (par défaut) et Google Gemini Live** :
  - `AppConfig` : ajout de l'énumération `AiProvider` (`OPENAI` par défaut, voix `alloy` ; `GEMINI` en option, voix `Aoede`), constante unique `AppConfig::DEFAULT_AI_PROVIDER` et `defaultVoiceForProvider()` `constexpr`.
  - `include/secrets.example.h` et `src/main.cpp` : prise en charge de `OPENAI_KEY` / `OPENAI_API_KEY` et `GEMINI_KEY` / `GEMINI_API_KEY`, transmises simultanément à `VoiceKitConfig` (`openAiApiKey` / `geminiApiKey`) avec filtrage des valeurs modèles (`VOTRE_CLE_API_*`, `MA_CLE_OPENAI`, `MA_CLE_GEMINI`).
  - `JournalUI` : ajout d'un sélecteur tactile `[IA: OPENAI]` / `[IA: GEMINI]` dans l'en-tête de carte (paysage 320 × 240, rotation 0), actif hors activité vocale (`READY`, `DISCONNECTED`, `ERROR_STATE`) avec anti-rebond dédié (`350 ms`) sans altérer la priorité de `STOP`.
  - `VoiceController` & `VoiceKitEngineAdapter` : bascule dynamique OpenAI ↔ Gemini (`onSwitchProviderRequested()` / `selectProvider()`), y compris en reprise depuis `DISCONNECTED` ou `ERROR_STATE`, sans jamais toucher aux notes NVS, avec vérification préalable de la présence de la clé API cible.
  - **Tests natifs C++17** : extension à **34 tests** couvrant OpenAI par défaut, Gemini comme fournisseur initial, la sélection du profil vocal, la gestion des clés API, la bascule tactile/contrôleur, la reprise après déconnexion/erreur et le refus si clé manquante ou moteur occupé.

### Corrigé
- **Source unique de configuration fournisseur** : suppression de la surcharge `VOICEJOURNAL_DEFAULT_PROVIDER` dans `src/main.cpp` afin que `AppConfig::DEFAULT_AI_PROVIDER` dans `include/AppConfig.h` soit l'unique point de configuration du fournisseur au démarrage.
- **Validation explicite de la clé API au démarrage** : `src/main.cpp` signale immédiatement une erreur explicite si le fournisseur initial n'a pas de clé valide.
- **Cohérence du fournisseur affiché au démarrage et à la transition `READY`** : synchronisation exacte de l'état affiché avec le moteur réel.

---

## [0.1.0.1] - 2026-11-21 — Corrective

### Corrigé
- **`JournalUI` — Priorité réelle du bouton `STOP`** :
  - Suppression de l'anti-rebond global partagé qui bloquait `STOP` pendant 180 ms après un clic sur `PARLER`.
  - Évaluation de `STOP` en premier, indépendante de l'anti-rebond dédié à `PARLER / TERMINER` (`180 ms`).
  - Détection de nouveau contact physique (`processTouchSample`) empêchant les déclenchements multiples.
- **`JournalStorage` — Fiabilisation de la persistance NVS** :
  - Double instantané borné A/B avec marqueur FNV-1a écrit en dernier.
  - Protection contre les coupures d'alimentation à n'importe quelle étape.
  - Migration automatique et non destructive des carnets existants au format v0.1.0.
- **`VoiceController` — Interruption `STOP`, reconnexion et transcriptions tardives** :
  - Suppression de l'appel redondant `engine_.stop()` après `cancelListening()`.
  - Idempotence complète de deux `STOP` successifs.
  - Filtrage des réponses IA tardives après `STOP` ou nouvelle capture.

---

## [0.1.0] - 2026-11-21

### Ajouté
- Projet PlatformIO autonome `VoiceJournal_CoreS3` ciblant **M5Stack CoreS3 (ESP32-S3)** en C++17.
- Intégration de la façade publique `<VoiceKit.h>` (**VoiceKit v0.6.7.2**) avec **Google Gemini Live** en mode non conversationnel (`enableConversation = false`).
- `JournalStorage` : tampon circulaire persistant en NVS (5 observations max, 180 octets UTF-8 max).
- `CommandRouter` : routeur de commandes vocales déterministe.
- `VoiceController` : machine à états applicative explicite (`JournalState`).
- `JournalUI` : interface tactile M5GFX paysage 320 × 240 avec boutons `PARLER / TERMINER` + `STOP`.
- Suite de tests unitaires natifs C++17.
