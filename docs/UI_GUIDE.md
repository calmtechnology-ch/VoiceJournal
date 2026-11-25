# Guide de l'interface utilisateur LVGL 9 — VoiceJournal v0.3.0

## 1. Philosophie visuelle et ergonomie

L'interface de VoiceJournal v0.3.0 a été entièrement conçue sous **LVGL 9** en respectant la philosophie **CalmTechnology** :
- **Discrétion et lisibilité** : palette sombre et contrastée (fond ardoise profonde `#12161A`, conteneurs `#1E252D`, bordures subtiles `#2C3642`).
- **Typographie soignée** : polices françaises anti-crénelées (Segoe UI 12, 14 et 16 gras) avec support complet des diacritiques de la langue française (`é`, `è`, `ê`, `à`, `ç`, `ô`, `î`, etc.).
- **Orientation native CoreS3** : affichage paysage natif 320 × 240 (`M5.Display.setRotation(0)` selon les spécifications matérielles de la dalle ILI9342).
- **Priorité absolue à la voix** : une bannière supérieure persistante de 36 pixels de hauteur affiche en temps réel l'état vocal, la transcription ou le message système, ainsi qu'un bouton **STOP** rouge prioritaire immédiatement accessible d'un geste tactile.

---

## 2. Architecture de l'écran

```text
+--------------------------------------------------------------+
| [ETAT VOCAL / TRANSCRIPTION]                       [ STOP ]  |  <- Bannière Voix (h=36px)
+--------------------------------------------------------------+
|                                                              |
|                     ZONE DE CONTENU                          |  <- Pages dynamiques
|              (Accueil / Journal / Projets / Tâches)           |     (h=168px)
|                                                              |
+--------------------------------------------------------------+
|  [ Accueil ]   |   [ Journal ]   |  [ Projets ]  | [ Tâches ] |  <- Barre de Navigation (h=36px)
+--------------------------------------------------------------+
```

---

## 3. Les 4 pages principales

### 1. Page Accueil
- **Statut opérationnel** : état de la connexion Wi-Fi, fournisseur d'IA actif (**OpenAI** ou **Gemini**).
- **Projet courant** : nom du projet actif sous lequel les prochaines notes seront classées par défaut.
- **Compteurs en temps réel** : nombre total d'entrées mémorisées, nombre de tâches en cours, nombre de problèmes ouverts.
- **Bouton Push-To-Talk (PTT)** : commande tactile tactile réactive pour démarrer ou arrêter la dictée vocale sans passer par la voix.
- **Sélecteur d'IA** : bouton d'alternance rapide entre OpenAI Realtime et Google Gemini Live.

### 2. Page Journal
- Présentation sous forme de **cartes individuelles réutilisables** (3 cartes affichées par écran pour une navigation ultra fluide).
- Chaque carte résume :
  - L'identifiant (`#42`) et le badge de type (`NOTE`, `PROBLÈME`, `TÂCHE`, `IDÉE`, `DÉCISION`, `RÉSULTAT`).
  - L'horodatage en heure locale (Europe/Zurich).
  - Un extrait du texte nettoyé et tronqué proprement sans coupure de mot ni de caractère UTF-8.
- **Boutons de pagination** (`◀ Précédent` / `Suivant ▶`) pour parcourir l'ensemble des entrées.
- Toucher une carte ouvre immédiatement la **vue Détail**.

### 3. Page Projets
- Liste des projets créés et actifs.
- Mise en valeur visuelle du **projet courant** (surlignage couleur accent cyan CalmTechnology).
- Nombre de tâches associées à chaque projet.
- Toucher un projet le définit instantanément comme projet de travail courant.

### 4. Page Tâches
- Liste dédiée aux éléments d'action (`EntryType::TASK`) avec statut `OPEN` ou `IN_PROGRESS`.
- Indication claire de l'échéance si définie.
- **Bouton tactile direct "Terminer"** en face de chaque tâche permettant de la clôturer en un seul clic sans commande vocale.

---

## 4. Vue Détail d'une entrée

Accessible par un toucher sur n'importe quelle entrée du journal :
- **En-tête complet** : type, identifiant, statut, projet associé, date de création et date de dernière modification.
- **Texte intégral** de la transcription (jusqu'à 1 024 octets avec défilement fluide).
- **Entités et mots-clés** détectés affichés sous forme de badges (*chips*).
- **Actions contextuelles** :
  - Bouton **"Marquer résolu / terminé"** : change immédiatement le statut de l'entrée dans LittleFS.
  - Bouton **"Lire"** : déclenche la synthèse vocale TTS de l'observation via le fournisseur d'IA actif.
  - Bouton **"Retour"** : réaffiche la liste du journal à la position exacte précédemment consultée.

---

## 5. Interactions vocales synchronisées avec l'écran

L'écran reflète instantanément toute action vocale :
- Dire *« Ouvre le projet VoiceKit »* bascule l'indicateur de projet courant sur l'accueil et sélectionne la page Projets.
- Dire *« Je dois tester Gemini demain »* ajoute la tâche et met à jour dynamiquement la liste des tâches et les compteurs.
- Dire *« Affiche mes tâches »* ou *« Liste mes projets »* commute automatiquement la page active sur l'onglet correspondant.
