# VoiceJournal v0.3.0 — Guide Utilisateur (Structured Memory)

Bienvenue dans **VoiceJournal v0.3.0**, votre assistant vocal de laboratoire et de mémoire personnelle et technique pour **M5Stack CoreS3**.

---

## 1. Nouveautés de la version v0.3.0

- **Mémoire structurée LittleFS (jusqu'à 1 000 entrées)** : dépassement définitif de la limite des 5 notes NVS. Vos observations sont conservées de manière sécurisée et organisée.
- **Nouvelle interface graphique sous LVGL 9** : 4 pages épurées (**Accueil**, **Journal**, **Projets**, **Tâches**), vue de détail avec synthèse vocale TTS et bascule de statut.
- **Typage des observations** : reconnaissance automatique des notes (`NOTE`), problèmes (`PROBLEM`), tâches (`TASK`), idées (`IDEA`), décisions (`DECISION`) et résultats (`RESULT`).
- **Gestion des projets** : associez vos observations à un projet en cours (*« Ouvre le projet VoiceKit »*).
- **Gestion des tâches avec échéances** : créez vos tâches par la voix (*« Je dois tester Gemini demain »*) et clôturez-les tactilement ou vocalement (*« La tâche tester Gemini est terminée »*).
- **Moteur de recherche multicritère local** : retrouvez instantanément vos observations par mot-clé (*« Retrouve les observations sur la batterie »*) ou par date relative (*« Qu'ai-je noté hier ? »*).
- **Bouton STOP d'urgence permanent** : interruption immédiate à tout instant d'un toucher sur le bouton STOP rouge en haut à droite.

---

## 2. Interface tactile LVGL 9

L'écran LCD 320 × 240 propose 4 onglets en bas d'écran :

```text
+-------------------------------------------------------------+
| [PRET - OPENAI]   VoiceJournal v0.3.0              [ STOP ] | <- Bannière supérieure (h=36)
+-------------------------------------------------------------+
|                                                             |
|                   ZONE D'AFFICHAGE ACTIVE                   | <- Contenu dynamique (h=168)
|                                                             |
+-------------------------------------------------------------+
|  [ Accueil ]   |   [ Journal ]   |  [ Projets ]  | [ Tâches ] | <- Barre d'onglets (h=36)
+-------------------------------------------------------------+
```

### 1. Onglet Accueil
- Indique l'état du système, le réseau et le fournisseur d'IA actif (**OpenAI Realtime** ou **Google Gemini Live**).
- Affiche le **projet courant actif**.
- Résume les compteurs : total des entrées, tâches ouvertes, problèmes non résolus.
- Bouton tactile `[ PARLER ]` pour démarrer ou stopper la capture vocale (Push-To-Talk).
- Bouton sélecteur tactile `[ IA: OPENAI / GEMINI ]`.

### 2. Onglet Journal
- Affiche les observations sous forme de cartes structurées (3 par page).
- Chaque carte indique le numéro `#ID`, le type (avec code couleur : bleu pour NOTE, rouge pour PROBLÈME, vert pour TÂCHE, jaune pour IDÉE, violet pour DÉCISION, cyan pour RÉSULTAT), l'heure locale et le texte.
- Boutons `◀ Précédent` et `Suivant ▶` pour feuilleter les pages.
- Toucher une carte ouvre la **vue Détail** : texte complet, boutons d'action **Lire** (synthèse vocale) et **Marquer résolu / terminé**.

### 3. Onglet Projets
- Liste l'ensemble des projets enregistrés.
- Toucher un projet le définit immédiatement comme le **projet actif courant**.

### 4. Onglet Tâches
- Liste toutes les tâches actuellement ouvertes (`OPEN`).
- Chaque tâche dispose d'un bouton direct **"Terminer"** permettant de la clôturer en un seul clic tactile.

---

## 3. Guide des commandes vocales

| Objectif | Exemples d'expressions | Résultat |
|---|---|---|
| **Noter une observation** | *« Note que le prototype fonctionne »*<br>*« Mémorise la mesure de 120 mA »* | Enregistrée comme `NOTE` sous le projet courant. |
| **Signaler un problème** | *« Enregistre un problème : Gemini se déconnecte »*<br>*« Problème : coupure WiFi sur le CoreS3 »* | Enregistrée comme `PROBLEM` (`OPEN`), visible dans les alertes. |
| **Créer une tâche** | *« Je dois tester Gemini demain »*<br>*« Ajoute une tâche pour mesurer la mémoire »* | Enregistrée comme `TASK` (`OPEN`), visible dans l'onglet Tâches. |
| **Clôturer une tâche** | *« La tâche tester Gemini est terminée »* | Tâche retrouvée et marquée `COMPLETED`. |
| **Consulter les tâches** | *« Quelles sont mes tâches ? »*<br>*« Affiche les tâches »* | Bascule sur l'onglet Tâches et énonce le décompte vocal. |
| **Créer un projet** | *« Crée le projet VoiceKit »* | Nouveau projet ajouté dans LittleFS. |
| **Changer de projet courant** | *« Ouvre le projet VoiceKit »*<br>*« Travaille sur le projet RuleBot »* | Définit le projet actif. Les prochaines notes y seront rattachées. |
| **Fermer le projet courant** | *« Ferme le projet courant »* | Réinitialise le contexte sans projet associé. |
| **Lister les projets** | *« Quels sont mes projets ? »*<br>*« Liste les projets »* | Bascule sur l'onglet Projets. |
| **Rechercher des notes** | *« Retrouve les observations sur Gemini »*<br>*« Cherche le problème d'alimentation »* | Recherche locale multicritère sans appel au cloud. |
| **Bilan de laboratoire** | *« Fais le bilan »*<br>*« Fais le bilan du projet VoiceKit »* | L'IA (OpenAI/Gemini) produit une synthèse structurée des 10 dernières observations pertinentes. |
| **Arrêt d'urgence** | Toucher le bouton rouge **STOP** | Interrompt immédiatement l'audio et la capture. |
