# VoiceJournal v0.3.0 — Procédure de Validation Matérielle (CoreS3)

Ce protocole de test permet de valider le fonctionnement de **VoiceJournal v0.3.0 (Structured Memory)** sur M5Stack CoreS3 physique.

---

## 1. Conditions préalables

1. Boîtier M5Stack CoreS3 relié en USB-C à un PC.
2. Clés API et réseau configurés dans `include/secrets.h`.
3. Firmware téléversé avec :
   ```bash
   cd VoiceJournal_CoreS3
   pio run -e m5stack-cores3 -t upload
   ```
4. Terminal série ouvert à 115 200 bauds :
   ```bash
   pio device monitor -b 115200
   ```

---

## 2. Grille de validation matérielle v0.3.0 (VJ3-01 à VJ3-20)

| Code | Scénario | Action à effectuer | Résultat attendu | Statut |
|---|---|---|---|---|
| **VJ3-01** | **Démarrage & Écran LVGL 9** | Mettre sous tension le boîtier CoreS3. | Écran orienté paysage 320 × 240, netteté des polices Segoe UI françaises, onglet Accueil actif, bannière vocale avec bouton STOP. | **À tester** |
| **VJ3-02** | **Migration NVS A/B** | Démarrer sur un boîtier contenant des notes v0.2.1. | Les notes NVS sont importées automatiquement dans LittleFS au premier boot. Drapeau `migrated_v030` posé, aucun doublon au redémarrage. | **À tester** |
| **VJ3-03** | **Création de projet vocal** | Dire : *« Crée le projet VoiceKit »*. | Projet créé avec ID 1. Confirmation vocale. Visible dans l'onglet Projets. | **À tester** |
| **VJ3-04** | **Sélection de projet courant** | Dire : *« Ouvre le projet VoiceKit »*. | Projet courant mis à jour sur l'écran d'accueil ("Projet : VoiceKit"). | **À tester** |
| **VJ3-05** | **Association au projet courant** | Dire : *« Note, Gemini se déconnecte après quelques secondes »*. | Observation ajoutée sous le type `NOTE` et associée au projet VoiceKit. | **À tester** |
| **VJ3-06** | **Enregistrement d'un problème** | Dire : *« Enregistre un problème : coupure WiFi sur le CoreS3 »*. | Entrée créée sous le type `PROBLEM` avec statut `OPEN`. Compteur de problèmes incrémenté. | **À tester** |
| **VJ3-07** | **Ajout d'une tâche vocale** | Dire : *« Je dois tester Gemini demain »*. | Tâche créée sous le type `TASK` avec statut `OPEN` et date relative demain. Visible dans l'onglet Tâches. | **À tester** |
| **VJ3-08** | **Consultation des tâches** | Dire : *« Quelles sont mes tâches ? »*. | L'écran bascule sur l'onglet Tâches et liste les tâches ouvertes. Énonciation vocale du nombre de tâches. | **À tester** |
| **VJ3-09** | **Clôture tactile d'une tâche** | Dans l'onglet Tâches, toucher le bouton "Terminer". | La tâche passe au statut `COMPLETED`. Retirée de la liste des tâches ouvertes. | **À tester** |
| **VJ3-10** | **Clôture vocale d'une tâche** | Dire : *« La tâche tester Gemini est terminée »*. | Tâche correspondante retrouvée et marquée `COMPLETED`. Confirmation vocale. | **À tester** |
| **VJ3-11** | **Consultation du Journal** | Toucher l'onglet "Journal". | Cartes d'entrées affichées par groupes de 3 avec pagination (`Précédent` / `Suivant`). | **À tester** |
| **VJ3-12** | **Vue Détail d'une entrée** | Toucher une carte du Journal. | Affichage plein écran : texte intégral, date Europe/Zurich, type, projet, boutons d'action. | **À tester** |
| **VJ3-13** | **Lecture audio d'une note** | Dans la vue Détail, toucher le bouton "Lire". | Synthèse vocale de l'entrée via OpenAI / Gemini sans blocage de l'interface. | **À tester** |
| **VJ3-14** | **Résolution tactile d'un problème** | Dans la vue Détail d'un problème, toucher "Marquer résolu". | Statut mis à jour à `RESOLVED`. Compteur de problèmes décrémenté. | **À tester** |
| **VJ3-15** | **Recherche par mot-clé** | Dire : *« Retrouve les observations sur Gemini »*. | Recherche multicritère exécutée localement. Présentation des résultats correspondants. | **À tester** |
| **VJ3-16** | **Analyse IA contextuelle bornée** | Dire : *« Fais le bilan »*. | Prompt ciblé injectant les 10 dernières entrées (< 1 500 car.). Synthèse structurée énoncée par l'IA. | **À tester** |
| **VJ3-17** | **Bouton STOP prioritaire** | Toucher STOP rouge pendant la parole de l'IA. | Arrêt immédiat de l'audio et retour à l'état PRET (< 50 ms). | **À tester** |
| **VJ3-18** | **Bascule OpenAI <-> Gemini** | Toucher le bouton IA sur l'écran d'accueil. | Bascule immédiate de profil vocal et reconnexion transparente. | **À tester** |
| **VJ3-19** | **Persistance LittleFS après coupure** | Ajouter une entrée, presser le bouton Reset physique. | Toutes les entrées, projets et statuts sont fidèlement reconstruits au redémarrage. | **À tester** |
| **VJ3-20** | **Protocole série JSON étendu** | Envoyer `JOURNAL_STATUS` et `EXPORT_JOURNAL` via le moniteur série. | Réponse JSON complète retournant métriques et entrées. | **À tester** |
