# Guide du Moteur Contextuel C++17 — VoiceJournal v0.2.1

Ce guide détaille les principes de fonctionnement, la grammaire déclarative, l'analyse sémantique et les garanties de sécurité du moteur NLU contextuel C++17 de **VoiceJournal v0.2.1**.

---

## 1. Principes et Pipeline d'Analyse

Le moteur contextuel embarqué s'exécute intégralement en local sur le processeur ESP32-S3 (temps d'analyse typique : 200 à 450 microsecondes) selon un pipeline déterministe en 11 étapes sans allocation dynamique bloquante :

```text
Texte original
       │
1. Contrôle des limites d'entrée (max 1024 octets)
       │
2. Validation UTF-8 conforme RFC 3629
       │
3. Normalisation (accents français, ligatures, ponctuation, casse)
       │
4. Tokenisation bornée (max 96 tokens)
       │
5. Analyse lexicale & reconnaissance des locutions au plus long préfixe (51 locutions)
       │
6. Attribution des rôles sémantiques (12 rôles) & construction du cadre sémantique (SemanticFrame)
       │
7. Typage des actions et de leurs objets cibles (TargetCategory)
       │
8. Évaluation de la grammaire contextuelle déclarative (19 règles déclaratives ordonnées)
       │
9. Analyse fine de la portée des négations (action, obligation, annulation, contenu dicté)
       │
10. Détection des ambiguïtés & scoring avec pénalité fuzzy bornée (Damerau-Levenshtein d <= 1)
       │
11. Validation de sécurité centralisée (CommandRouter) ──► ParseResult
```

---

## 2. Rôles Sémantiques et Catégories de Cibles

### 2.1 Les 12 Rôles Sémantiques (`SemanticRole`)
- `UNKNOWN` : Token non reconnu ou sans portée sémantique.
- `SUBJECT` : Pronom ou sujet grammatical (*je*, *nous*, *tu*).
- `ACTION` : Verbe ou indicateur d'action (*noter*, *lire*, *effacer*, *compter*, *analyser*).
- `OBJECT` : Objet direct de l'action (*note*, *carnet*, *fichier*, *bilan*).
- `ENTITY` : Entité nommée ou composant technique identifié (*Gemini*, *VoiceKit*, *CoreS3*).
- `QUANTIFIER` : Quantificateur ou portée (*tout*, *tous*, *dernier*, *nombre*).
- `QUALIFIER` : Qualificatif ou statut (*technique*, *corrigé*, *urgent*).
- `TIME` : Indicateur temporel ou date relative (*demain*, *ce matin*, *hier*).
- `NEGATION` : Mot de négation (*ne*, *pas*, *non*, *jamais*).
- `OBLIGATION` : Marqueur d'obligation ou d'intention (*dois*, *faut*, *penser à*).
- `QUESTION` : Mot interrogatif (*combien*, *quel*, *comment*).
- `CONNECTOR` : Conjonction ou liaison (*et*, *de*, *que*).
- `PAYLOAD` : Texte dicté destiné au stockage.

### 2.2 Catégories d'Objets Cibles (`TargetCategory`)
Le moteur classe chaque objet identifié dans l'une des catégories suivantes :
- `JOURNAL` : Le carnet de notes dans sa globalité.
- `JOURNAL_NOTE` : Une observation ou note du carnet.
- `TASK` : Une tâche à accomplir.
- `PROJECT` : Un projet logiciel ou matériel.
- `PROBLEM` : Une anomalie, un bug ou un dysfonctionnement.
- `FILE` : Un fichier système ou fichier de configuration.
- `DEVICE` : Un appareil, microcontrôleur ou capteur.
- `UNKNOWN_OBJECT` : Tout objet non répertorié.

---

## 3. Grammaire Contextuelle Déclarative (`rules/grammar.json`)

Le moteur compile 19 règles ordonnées générées sous forme de tables statiques C++ dans `ParserTables.h`.

### Exemples clés et contraintes renforcées (COR-13) :
1. **Comptage des notes (`count_notes_query`)** :
   - Cibles autorisées : `JOURNAL`, `JOURNAL_NOTE`.
   - Cibles expressément interdites : `FILE`, `PROJECT`, `DEVICE`.
   - *« Combien de notes ? »* $\rightarrow$ `COUNT_NOTES` (Exécutable).
   - *« Combien coûte un CoreS3 ? »* $\rightarrow$ `UNKNOWN` (Non exécutable).
2. **Lecture de la dernière note (`read_last_note_cmd`)** :
   - Cibles autorisées : `JOURNAL`, `JOURNAL_NOTE`.
   - Cibles expressément interdites : `FILE`, `PROJECT`, `DEVICE`.
   - *« Lis la dernière note »* $\rightarrow$ `READ_LAST_NOTE` (Exécutable).
   - *« Lis le fichier de configuration »* $\rightarrow$ `UNKNOWN` (Non exécutable).
3. **Analyse globale et analyse de projet** :
   - *« Fais le bilan »* $\rightarrow$ `ANALYZE_NOTES` (Exécutable).
   - *« Fais le bilan du projet VoiceKit »* $\rightarrow$ `ANALYZE_PROJECT` (Expérimental, non exécutable).
4. **Suppression et effacement sécurisé (`clear_request_cmd`)** :
   - Exige impérativement une cible `JOURNAL` ou `JOURNAL_NOTE` ET un quantificateur global (`ALL`, `GLOBAL`).
   - *« Efface toutes les notes »* $\rightarrow$ `REQUEST_CLEAR` (Arme la confirmation).
   - *« Efface la dernière note »* $\rightarrow$ Refus (pas de portée globale, suppression unitaire non supportée).
   - *« Supprime le fichier de configuration »* $\rightarrow$ `UNKNOWN` / `NO_ACTION` (Cible interdite).
5. **Gestion des tâches et des obligations** :
   - *« Je dois tester Gemini demain »* $\rightarrow$ `ADD_TASK` (Expérimental, non exécutable).
   - *« Je ne dois pas tester Gemini »* $\rightarrow$ `NO_ACTION` / `UNKNOWN` (Obligation niée).
   - *« N'oublie pas de tester Gemini »* $\rightarrow$ `ADD_TASK` (Double négation sémantiquement affirmative).

---

## 4. Garanties de Sécurité (COR-11 à COR-15)

- **COR-11 — Validation centralisée** : Aucune intention reconnue n'est exécutée si son score est insuffisant (< 60), si le statut est ambigu, ou si des restrictions métier s'appliquent. Les commandes invalides reçues pendant une attente d'effacement désarment la confirmation.
- **COR-12 — Rejet strict des dépassements** : Entrées > 1024 octets ou > 96 tokens rejetées intégralement sans analyse de début de phrase. Rejet strict des UTF-8 invalides.
- **COR-13 — Relations actions-objets strictes** : Interdiction du fuzzy matching sur les commandes destructives, interdiction des cibles concurrentes, interdiction des suppressions sur des fichiers ou projets.
- **COR-14 — Protocoles de diagnostic isolés** : Les requêtes USB retournent du JSON structuré sans effet de bord sur le carnet ni sur la session audio.
- **COR-15 — Mode SHADOW non destructif** : Comparaison miroir vérifiant le type, la charge utile, les paramètres, l'état de confirmation et les divergences de sécurité sans double exécution.

---

## 5. Bilan des Tests et Non-Régression

Le moteur contextuel v0.2.1 est validé par **636 tests unitaires et d'intégration natifs C++17 réussis à 100%** :
- 36 tests historiques v0.1.1 (Persistance NVS, stockage A/B, bascule OpenAI/Gemini).
- 220 tests sémantiques NLU v0.2.0 (Normalisation, fuzzy matching, extraction).
- 289 tests contextuels v0.2.0.1 (Rôles sémantiques, locutions, grammaire, négations).
- 91 tests de consolidation v0.2.1 (Validation COR-11 à COR-15, protocoles série, robustesse).
- Évaluation automatisée sur dataset (`contextual_parsing_fr.json`) : **53/53 cas réussis (100%)**.
