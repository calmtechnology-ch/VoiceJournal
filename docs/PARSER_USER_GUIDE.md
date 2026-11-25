# Guide Utilisateur du Moteur Contextuel — VoiceJournal v0.2.0.1

Ce guide détaille les formulations orales supportées en français par VoiceJournal v0.2.0.1, le fonctionnement du Parsing Lab sur écran M5Stack CoreS3 et les garanties de sécurité appliquées.

---

## 1. Formulations Exécutables (Actions Réelles sur le Carnet)

Les commandes suivantes modifient ou consultent l'état persistant du carnet en mémoire NVS :

### 1.1 Ajout d'une observation (`ADD_NOTE`)
La commande d'ajout accepte plusieurs verbes d'introduction. La charge utile dictée commence immédiatement après le verbe introductif et est conservée telle quelle (casse, accents, signes de ponctuation, acronymes techniques).

| Formulation | Intention | Comportement |
|---|---|---|
| *"Note, tension mesurée à 3.3 volts"* | `ADD_NOTE` | Ajoute l'observation en NVS |
| *"Mémorise que Gemini se déconnecte"* | `ADD_NOTE` | Ajoute l'observation en NVS |
| *"Enregistre : le capteur répond sur bus I2C"* | `ADD_NOTE` | Ajoute l'observation en NVS |
| *"Retiens la configuration du prototype"* | `ADD_NOTE` | Ajoute l'observation en NVS |
| *"Note, ne supprime pas les fichiers"* | `ADD_NOTE` | **Sécurisé** : la négation est dans la note, l'ajout est effectué |
| *"Mémorise que le voyant ne s'allume pas"* | `ADD_NOTE` | **Sécurisé** : contenu préservé intégralement |

### 1.2 Consultation vocale (`READ_LAST_NOTE`, `COUNT_NOTES`)
| Formulation | Intention | Comportement |
|---|---|---|
| *"Lis la dernière note"* | `READ_LAST_NOTE` | Lit la dernière note via TTS |
| *"Relis mon observation"* | `READ_LAST_NOTE` | Lit la dernière note via TTS |
| *"Écoute la dernière note"* | `READ_LAST_NOTE` | Lit la dernière note via TTS |
| *"Combien de notes ?"* | `COUNT_NOTES` | Énonce le nombre d'observations |
| *"Quel est le nombre d'observations ?"* | `COUNT_NOTES` | Énonce le nombre d'observations |
| *"Compte les notes"* | `COUNT_NOTES` | Énonce le nombre d'observations |

### 1.3 Synthèse globale par IA (`ANALYZE_NOTES`)
| Formulation | Intention | Comportement |
|---|---|---|
| *"Fais le bilan"* | `ANALYZE_NOTES` | Demande une synthèse des notes à l'IA |
| *"Résume les observations"* | `ANALYZE_NOTES` | Demande une synthèse des notes à l'IA |
| *"Donne-moi une synthèse"* | `ANALYZE_NOTES` | Demande une synthèse des notes à l'IA |
| *"Qu'avons-nous observé ?"* | `ANALYZE_NOTES` | Demande une synthèse des notes à l'IA |

### 1.4 Effacement sécurisé du carnet (`REQUEST_CLEAR`, `CONFIRM_CLEAR`, `CANCEL_CLEAR`)
VoiceJournal applique un protocole d'effacement destructif en 2 étapes strictement borné :

1. **Demande d'effacement (`REQUEST_CLEAR`)** :
   - Formulations valides : *"Efface toutes les notes"*, *"Supprime toutes les observations"*, *"Vide le carnet"*, *"Supprime les notes"*.
   - Déclenche un compte à rebours de 30 secondes en attente d'une confirmation explicite.
2. **Confirmation obligatoire (`CONFIRM_CLEAR`)** :
   - Formulations acceptées uniquement pendant l'attente : *"Confirmer la suppression"*, *"Oui efface"*, *"Supprimer définitivement"*, *"Valider la suppression"*.
   - **Garde-fous stricts** : aucun rapprochement approximatif (fuzzy) n'est toléré sur la confirmation. Une formulation vague comme *"Oui"* ou *"D'accord"* est rejetée et désarme l'attente.
   - **Répétition** : répéter *"Efface toutes les notes"* ne confirme JAMAIS la suppression.
3. **Annulation immédiate (`CANCEL_CLEAR`)** :
   - Formulations : *"Annuler"*, *"Ne supprime pas les notes"*, *"Je refuse de confirmer"*, *"Laisse tomber"*, ou simple appui sur le bouton physique/tactile `STOP`.

---

## 2. Formulations Expérimentales Reconnues (Sans Effet de Bord NVS)

Ces intentions sont comprises par le moteur contextuel avec extraction de leurs paramètres (entités, dates, statuts), mais ne modifient pas la mémoire NVS (prévues pour les versions futures) :

| Formulation | Intention | Slots extraits |
|---|---|---|
| *"Je dois tester Gemini demain"* | `ADD_TASK` | `entity="Gemini"`, `date_relative="demain"` |
| *"Il faut que je teste VoiceKit ce matin"* | `ADD_TASK` | `entity="VoiceKit"`, `date_relative="ce matin"` |
| *"N'oublie pas de compiler le firmware"* | `ADD_TASK` | `action="compiler"`, `object="firmware"` |
| *"Retrouve les bugs de VoiceKit"* | `SEARCH_NOTES` | `entity="VoiceKit"`, `type="PROBLEM"` |
| *"Cherche les anomalies sur CoreS3"* | `SEARCH_NOTES` | `entity="CoreS3"`, `type="PROBLEM"` |
| *"Cherche les notes de hier"* | `SEARCH_BY_DATE` | `date_relative="hier"` |
| *"Trouve les observations de ce matin"* | `SEARCH_BY_DATE` | `date_relative="ce matin"` |
| *"Fais le bilan du projet RuleBot"* | `ANALYZE_PROJECT` | `entity="RuleBot"`, `project="RuleBot"` |
| *"Le problème de WiFi est corrigé"* | `RESOLVE_PROBLEM` | `type="RESOLVED"` |
| *"Liste les tâches"* | `LIST_TASKS` | `type="TASK"` |

---

## 3. Formulations Rejetées (Protection et Garde-fous)

Pour empêcher tout comportement inattendu, les phrases suivantes sont formellement reconnues comme **invalides ou non destructives** :

| Formulation | Résultat | Raison |
|---|---|---|
| *"Efface le fichier de configuration"* | `UNKNOWN` | Cible `FILE` interdite d'effacement carnet |
| *"Supprime le projet VoiceKit"* | `UNKNOWN` | Cible `PROJECT` interdite d'effacement carnet |
| *"Efface la dernière note"* | `UNKNOWN` | Suppression unitaire non supportée (pas de global scope) |
| *"Supprime le mot Gemini"* | `UNKNOWN` | Cible lexicale non destructive |
| *"Effacer des fichiers est dangereux"* | `UNKNOWN` | Assertion générale, aucune commande |
| *"Comment supprimer une note ?"* | `UNKNOWN` | Question informative (`QUESTION`), pas une action |
| *"Je ne dois pas tester Gemini"* | `UNKNOWN` | Obligation niée (`obligation`), tâche annulée |
| *"Lis efface"* | `AMBIGUOUS` | Conflit direct entre 2 verbes d'action contradictoires |
| *"Compte les notes et efface tout"* | `AMBIGUOUS` | Conflit entre comptage et effacement |

---

## 4. Parsing Lab sur M5Stack CoreS3 (Section 19)

VoiceJournal intègre sur son écran LCD 320 × 240 un laboratoire de diagnostic en direct accessible par le bouton tactile `[LAB]`.

Le Parsing Lab propose **3 sous-pages** permutables par simple pression tactile sur le tiers supérieur de l'écran :

1. **Vue 1 — Résultat du parsing** :
   - Transcription STT originale affichée.
   - Intention retenue (`IntentId`), caractère exécutable (`EXEC: OUI / NON`).
   - Score de confiance (`Raw` / `Ajusté` / `Pénalité fuzzy`).
   - Règle de grammaire sélectionnée.
2. **Vue 2 — Contexte sémantique (COR-01 à COR-06)** :
   - Locution multi-mots reconnue (`Phrases`).
   - Rôles sémantiques attribués (`Sujet`, `Action`, `Objet`, `Entité`, `Temps`).
   - Portée des négations (`Portée: commande / payload / obligation / confirmation`).
   - Catégorie d'objet cible (`Target: JOURNAL / NOTE / FILE / PROJECT...`).
3. **Vue 3 — Comparaison SHADOW (COR-09)** :
   - Statut comparatif miroir (`MATCH`, `NEW_INTENT`, `SECURITY_BLOCKED`, etc.).
   - Décision du moteur historique v0.1.1 vs moteur intelligent v0.2.0.1.
   - Temps d'analyse en microsecondes.

> **Priorité absolue de `STOP`** : Le bouton `STOP` rouge physique et tactile reste actif et prioritaire dans tous les écrans du Parsing Lab.
