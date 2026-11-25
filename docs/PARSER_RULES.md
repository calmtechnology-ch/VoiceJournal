# Règles de Parsing & Concepts — VoiceJournal v0.2.0

## 1. Déclaratif vs Tables Compilées

VoiceJournal v0.2.0 utilise une approche déclarative pour définir ses règles, concepts, alias et entités.
- **Fichiers sources déclaratifs :**
  - `rules/concepts.json` : Dictionnaire des concepts et de leurs mots-clés lexicaux.
  - `rules/intents.json` : Définition des intentions (règles, prérequis, scores, exclusions).
  - `rules/entities.json` : Définition des entités techniques canoniques et de leurs alias.
  - `rules/aliases.json` : Règles de normalisation de locutions courantes.
- **Générateur de code (`tools/generate_parser_tables.py`) :**
  - Compile ces fichiers JSON en tables C++ constantes (`include/parser/ParserTables.h`).
  - Aucun parseur JSON dynamique n'est embarqué sur l'ESP32-S3, ce qui élimine toute allocation dynamique et garantit une empreinte RAM minimale.

---

## 2. Dictionnaire des Concepts

Chaque mot du lexique est associé à un masque binaire 32 bits (`uint32_t`) représentant ses concepts d'appartenance :

| Concept | Masque binaire | Rôle & Exemples de mots-clés |
|---|---|---|
| `ADD` | `0x0001` | *note, noter, notez, memorise, enregistre, retiens, ajoute, ecris, consigne* |
| `SEARCH` | `0x0002` | *cherche, chercher, retrouve, retrouver, trouve, rappelle, recherche* |
| `READ` | `0x0004` | *lis, lire, relis, ecoute, derniere, dernier, precedente* |
| `COUNT` | `0x0008` | *combien, nombre, total, quantite, compte, compter* |
| `ANALYZE` | `0x0010` | *bilan, synthese, resume, resumer, analyse, recapitulatif, synthetise* |
| `DELETE` | `0x0020` | *efface, effacer, supprime, supprimer, vide, vider, purge, reinitialise, raz* |
| `CONFIRM` | `0x0040` | *confirme, confirmer, validation, oui, absolument, definitivement, certainement* |
| `CANCEL` | `0x0080` | *annule, annuler, annulez, laisse, tomber, abandonne, arrete, non* |
| `HELP` | `0x0100` | *aide, aider, secours, commandes, fonctionnement, options, manuel, guide* |
| `PROBLEM` | `0x0200` | *probleme, bug, erreur, panne, defaut, anomalie, dysfonctionnement, crash, incident* |
| `TASK` | `0x0400` | *tache, todo, action, prevoir, planifier, faire, devoir, doit, dois* |
| `RESOLVED` | `0x0800` | *corrige, resolu, repare, fixe, regle, resolue, reparee, corrigee* |
| `DATE_RELATIVE` | `0x1000` | *aujourd hui, hier, demain, maintenant, ce matin, ce soir, apres demain, avant hier* |
| `PROJECT` | `0x2000` | *projet, systeme, module, composant, cible* |
| `NOTE_TARGET` | `0x4000` | *note, notes, observation, observations, carnet, journal, entree, entrees* |
| `NEGATION` | `0x8000` | *ne, n, pas, plus, jamais, rien, aucun, aucune, point, guere* |

Un mot peut appartenir à plusieurs concepts selon son acception (ex. *"note"* appartient à `ADD` et à `NOTE_TARGET`).

---

## 3. Intentions et Règles d'Activation

### 3.1 Intentions Exécutables (v0.2.0)

| Intention | Score | Règle d'activation | Action métier VoiceJournal |
|---|---|---|---|
| `ADD_NOTE` | 95 | Le premier token possède le concept `ADD` et une charge utile non vide suit. | Enregistrement dans la banque NVS A/B active (max 180 octets). |
| `READ_LAST_NOTE` | 90 | Présence du concept `READ`, absence de `DELETE`, `TASK`, `PROBLEM` et pas de négation. | Lecture vocale TTS et affichage de la dernière note. |
| `COUNT_NOTES` | 90 | Présence du concept `COUNT`, absence de `DELETE` et pas de négation. | Annonce vocale du nombre de notes stockées. |
| `ANALYZE_NOTES` | 90 | Présence du concept `ANALYZE`, absence de `DELETE` et d'entités spécifiques. | Synthèse vocale générée par l'IA configurée (Gemini / OpenAI). |
| `REQUEST_CLEAR` | 95 | Présence du concept `DELETE`, sans négation, non reconnu comme confirmation exacte. | Arme l'attente de confirmation de suppression pendant 15 secondes. |
| `CONFIRM_CLEAR` | 100 | Attente armée ET formulation strictement identique à une phrase autorisée sans négation. | Efface toutes les notes en NVS (carnet vidé à `0 / 5`). |
| `CANCEL_CLEAR` | 95 | Présence du concept `CANCEL` OU présence de `DELETE` avec négation de commande. | Désarme l'attente de confirmation sans modifier le carnet. |
| `HELP` | 90 | Présence du concept `HELP` sans commande concurrente. | Synthèse vocale résumant les commandes vocales disponibles. |

### 3.2 Intentions Expérimentales (Reconnues mais Non Exécutables)

Ces intentions sont reconnues par le moteur NLU pour diagnostic, affichage dans le Parsing Lab et simulation, mais **ne modifient en aucun cas le stockage** :

| Intention | Score | Règle d'activation | Réponse vocale CoreS3 |
|---|---|---|---|
| `SEARCH_NOTES` | 90 | Concept `SEARCH` ou `PROBLEM` (non résolu) sans entité ni date. | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `ADD_TASK` | 90 | Concept `TASK` sans concept `READ` (ex. *« Je dois tester... »*). | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `RESOLVE_PROBLEM` | 90 | Concept `RESOLVED` (ex. *« Le bug est corrigé »*). | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `LIST_TASKS` | 90 | Présence conjointe de `READ` et `TASK`. | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `SEARCH_BY_PROJECT` | 90 | Concept `SEARCH` combiné avec une entité projet ou le concept `PROJECT`. | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `SEARCH_BY_DATE` | 90 | Concept `DATE_RELATIVE` combiné avec `SEARCH` ou `NOTE_TARGET`. | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |
| `ANALYZE_PROJECT` | 95 | Concept `ANALYZE` combiné avec la mention explicite d'une entité projet. | *« La demande est comprise, mais cette fonction n'est pas encore disponible. »* |

---

## 4. Règles de Tolérance aux Erreurs (Fuzzy Matching)

L'algorithme de Damerau-Levenshtein autorise une distance d'édition maximale de **1** sur les mots du lexique, sous réserve des exclusions impératives suivantes :

1. **Longueur minimale :** Le mot doit comporter au moins **4 caractères**.
   - *Refusés :* *"oui"*, *"non"*, *"lis"*, *"ne"*, *"pas"*.
2. **Exclusion des nombres et versions :**
   - Aucune substitution approximative pour les nombres (ex. *"1"*, *"deux"*).
   - Aucune substitution pour les versions logicielles (ex. *"0.1.1"*, *"2.4"*).
3. **Exclusion des identifiants techniques :**
   - Les noms de composants et entités techniques (`ESP32`, `CoreS3`, `VoiceKit`, `OpenAI`, `Gemini`, `RuleBot`) ne font l'objet d'aucun rapprochement approximatif.
4. **Exclusion formelle des confirmations de suppression :**
   - Une confirmation destructive exige une correspondance exacte et stricte. Aucun mot approximatif ne peut valider un `CONFIRM_CLEAR`.
5. **Rejet des ambiguïtés fuzzy :** Si un mot altéré est à distance 1 de deux mots distincts du lexique, il est rejeté.
6. **Pénalité de score :** Lorsqu'un rapprochement fuzzy est appliqué, le score de l'intention candidate est pénalisé de **15 points**.

---

## 5. Règles de Négation et Sécurité Destructive

Le moteur applique une analyse sémantique rigoureuse des négations :

### 5.1 Phrases Pièges de Confirmation
Les formulations suivantes sont formellement interceptées et requalifiées en annulation :
- *« Je ne confirme pas la suppression »* $\rightarrow$ `CANCEL_CLEAR`
- *« Ne confirme pas »* $\rightarrow$ `CANCEL_CLEAR`
- *« Pas de suppression »* $\rightarrow$ `CANCEL_CLEAR`
- *« Ne supprime rien »* $\rightarrow$ `CANCEL_CLEAR`

### 5.2 Règle d'Isolation de la Charge Utile (`ADD_NOTE`)
Lorsque la phrase commence par un verbe d'enregistrement (`ADD`), la négation appartient au contenu dicté et ne doit pas annuler la commande :
- *« Note, ne supprime pas les fichiers de VoiceKit »* $\rightarrow$ `ADD_NOTE`, Payload : *« ne supprime pas les fichiers de VoiceKit »*.
- *« Mémorise que le voyant ne s'allume pas »* $\rightarrow$ `ADD_NOTE`, Payload : *« que le voyant ne s'allume pas »*.
- *« Enregistre : pas d'anomalie détectée »* $\rightarrow$ `ADD_NOTE`, Payload : *« pas d'anomalie détectée »*.
