# Guide Utilisateur du Moteur NLU — VoiceJournal v0.2.0

Ce guide explique comment utiliser, configurer, tester et inspecter le moteur de compréhension du langage naturel de **VoiceJournal v0.2.0**.

---

## 1. Utilisation Vocale sur M5Stack CoreS3

### 1.1 Dictée d'une observation (`ADD_NOTE`)
Appuyez brièvement sur le bouton physique/tactile `[PARLER]`, parlez, puis appuyez sur `[TERMINER]`.
- *Exemples reconnus :*
  - *« Note, le CoreS3 consomme 120 mA en veille »*
  - *« Mémorise que le WiFi 2.4 GHz fonctionne parfaitement »*
  - *« Enregistre : tension mesurée à 3,3 V »*
  - *« Retiens d'ajuster le gain micro »*
- **Préservation intégrale :** Les accents, majuscules et signes de ponctuation sont conservés en mémoire NVS.

### 1.2 Consultation et Synthèse
- **Relire la dernière note :** *« Lis la dernière note »*, *« Relis mon observation »*.
- **Compter les notes :** *« Combien de notes ? »*, *« Compte les observations »*, *« Quel est le nombre de notes ? »*.
- **Synthèse IA globale :** *« Fais le bilan »*, *« Résume les observations »*, *« Donne-moi la synthèse »*.
- **Demande d'aide :** *« Aide »*, *« Quelles sont les commandes ? »*.

### 1.3 Effacement Sécurisé en Deux Étapes
1. **Étape 1 : Demande d'effacement**
   - Prononcez : *« Efface toutes les notes »*, *« Supprime le carnet »*, ou *« Réinitialise les observations »*.
   - VoiceJournal confirme vocalement la mise en attente et active une fenêtre de 15 secondes.
2. **Étape 2 : Confirmation explicite ou Annulation**
   - **Pour confirmer :** Prononcez une formule explicite telle que *« Confirmer la suppression »*, *« Oui efface »*, ou *« Supprimer définitivement »*.
   - **Pour annuler :** Prononcez *« Annuler »*, *« Ne supprime pas »*, *« Laisse tomber »*, appuyez sur `[STOP]`, ou laissez simplement expirer le délai de 15 secondes.
   - *Sécurité :* Dire simplement *« oui »* ou formuler une négation (*« Je ne confirme pas »*) annule immédiatement l'effacement.

---

## 2. Le Parsing Lab sur M5Stack CoreS3

VoiceJournal v0.2.0 embarque un écran de diagnostic temps réel accessible directement sur l'écran tactile du CoreS3.

### 2.1 Accéder au Parsing Lab
1. Sur l'écran principal, repérez le bouton tactile **`[LAB]`** situé en haut à droite.
2. Appuyez sur **`[LAB]`** : l'écran bascule instantanément vers la vue diagnostique.
3. Pour revenir à la vue principale, appuyez sur le bouton **`[RETOUR]`** situé au même emplacement.
4. **Priorité absolue de STOP :** Le bouton `STOP` rouge situé en bas de l'écran reste constamment actif, y compris en mode Parsing Lab.

### 2.2 Informations affichées dans le Parsing Lab
- **Transcription reçue :** Le texte exact retourné par le service STT (OpenAI ou Gemini).
- **Intention & Confiance :** L'intention identifiée (ex. `ADD_NOTE`, `SEARCH_NOTES`) et son score (sur 100).
- **Concepts & Entités :** Liste des masques conceptuels et entités techniques reconnues (ex. `Gemini`, `CoreS3`).
- **Négation & Fuzzy :** Indicateurs confirmant si une négation ou une correction approximative a été sollicitée.
- **Temps d'exécution NLU :** Durée exacte du parsing en microsecondes (typiquement 200 à 400 µs).
- **Statut Comparatif (Mode Shadow) :** Indique la corrélation avec le moteur legacy (`MATCH`, `NEW_INTENT`, `SECURITY_BLOCKED`).

---

## 3. Configuration des Modes de Parsing

Le mode de parsing se configure centralement dans le fichier [`include/AppConfig.h`](file:///c:/Users/phall/Documents/Antigravity-5.1/VoiceJournal/VoiceJournal_CoreS3/include/AppConfig.h) :

```cpp
// Modes disponibles : ParserMode::LEGACY, ParserMode::INTELLIGENT, ParserMode::SHADOW
inline constexpr ParserMode DEFAULT_PARSER_MODE = ParserMode::INTELLIGENT;
```

- **`ParserMode::INTELLIGENT` (Défaut) :** Exploite pleinement le moteur NLU C++17 avec gestion des concepts, des entités, du fuzzy matching et des négations.
- **`ParserMode::LEGACY` :** Restaure le comportement strict de la version v0.1.1 (comparaison de sous-chaînes rigide).
- **`ParserMode::SHADOW` :** Exécute l'action selon le moteur v0.1.1, tout en évaluant silencieusement le moteur v0.2.0 en parallèle et en enregistrant les divergences pour comparaison.

---

## 4. Utilisation du Simulateur PC

Le simulateur autonome permet de tester, débugger et évaluer des transcriptions directement sur PC sans matériel.

### 4.1 Compilation du Simulateur
Dans le dossier `VoiceJournal_CoreS3` :
```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Itests/mocks `
    src/parser/TextNormalizer.cpp src/parser/Tokenizer.cpp src/parser/FuzzyMatcher.cpp `
    src/parser/ConceptDictionary.cpp src/parser/NegationDetector.cpp src/parser/SlotExtractor.cpp `
    src/parser/IntentMatcher.cpp src/parser/JournalParser.cpp src/CommandRouter.cpp `
    simulator/main.cpp -o simulator/voicejournal_simulator.exe
```

### 4.2 Lancement et Options CLI
```powershell
# 1. Lancement de la validation de base (17 tests)
.\simulator\voicejournal_simulator.exe --test

# 2. Évaluation détaillée d'une phrase en mode INTELLIGENT
.\simulator\voicejournal_simulator.exe --eval "Note, Gemini se déconnecte sur le CoreS3."

# 3. Évaluation comparative en mode SHADOW
.\simulator\voicejournal_simulator.exe --shadow "Retrouve les bugs de VoiceKit"

# 4. Mode interactif REPL
.\simulator\voicejournal_simulator.exe
```

### 4.3 Commandes du Mode Interactif REPL
- `mode legacy` : Bascule le simulateur en mode historique.
- `mode intelligent` : Bascule en mode intelligent v0.2.0.
- `mode shadow` : Bascule en mode comparatif.
- `arm_clear` : Simule l'armement d'une confirmation de suppression.
- `cancel_clear` : Annule l'armement.
- `test` : Exécute le banc de tests rapide.
- `exit` ou `quit` : Quitte le simulateur.
- *Toute autre saisie* : Évalue la phrase et affiche son arbre syntaxique complet.
