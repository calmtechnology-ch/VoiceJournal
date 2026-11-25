# VoiceJournal v0.3.0 — Mémoire structurée & interface LVGL 9

Application embarquée en C++17 pour **M5Stack CoreS3** (ESP32-S3), propulsée par la façade publique [`<VoiceKit.h>`](https://github.com/calmtechnology-ch/VoiceKit) (révision `16646c73acf01a8d57408391bc6de15a288b8910`). 

VoiceJournal intègre un moteur de compréhension du langage naturel (NLU) déterministe hors-ligne, un double fournisseur d'IA vocale (**OpenAI Realtime** par défaut, basculable vers **Google Gemini Live**), une **mémoire structurée LittleFS** (jusqu'à 1 000 entrées avec indexation RAM), et une **interface graphique moderne sous LVGL 9**.

La version **v0.3.0** apporte la persistance structurée, la gestion des projets, des tâches avec échéance, des problèmes, un moteur de recherche multicritère local et une interface tactile fluide respectant la philosophie CalmTechnology.

> **Statut de qualification v0.3.0 :**
> - **646 tests unitaires natifs C++17 réussis à 100%** (36 legacy, 220 NLU, 289 contextuels, 91 consolidation, 10 mémoire structurée).
> - **Jeu de données contextuel 21/21 cas benchmark réussis à 100%** sur le simulateur PC.
> - **Empreinte mémoire ESP32-S3 maîtrisée** : RAM 47,0% (154 092 / 327 680 B), Flash 39,1% (2 462 037 / 6 291 456 B dans la partition app de 6 Mo).
> - **Partitionnement Flash 16 Mo** : 6 Mo application, 9 Mo LittleFS, 20 Ko NVS préservée sans perte de données.
> - **Protocole série JSON étendu** pour l'interfaçage avec des applications externes et l'exportation.

---

## 1. Qu'est-ce que VoiceJournal ?

**VoiceJournal** transforme un boîtier autonome M5Stack CoreS3 en carnet de laboratoire vocal et assistant de mémoire pour ingénieurs, techniciens et makers :
1. **Dictée d'observations techniques structurées** : capturez vos notes, problèmes, tâches, idées, décisions et résultats sur le terrain ou à l'établi.
2. **Mémoire structurée LittleFS (jusqu'à 1 000 entrées)** : journal append-only avec intégrité CRC32, réparation automatique sur coupure d'alimentation et index en RAM ultra-rapide.
3. **Moteur NLU contextuel local (< 1 ms)** : analyse sémantique déterministe en C++17 embarqué sur l'ESP32-S3, distinguant précisément les intentions de création, de recherche et d'organisation.
4. **Gestion des projets et des tâches** : associez des observations au projet courant (« Ouvre le projet VoiceKit ») et suivez vos échéances (« Je dois tester Gemini demain »).
5. **Recherche multicritère locale** : retrouvez instantanément vos observations sans base vectorielle ni modèle cloud.
6. **Interface LVGL 9 épurée** : 4 vues principales (Accueil, Journal, Projets, Tâches), écran de détail avec relecture vocale TTS, et bouton STOP prioritaire universel.

---

## 2. Matériel nécessaire

- **M5Stack CoreS3** (ESP32-S3 dual-core, 16 Mo Flash, 8 Mo PSRAM, écran tactile 320 × 240, double microphone ES7210, haut-parleur AW88298 et PMIC AXP2101).
- **Câble USB-C** haute qualité assurant l'alimentation (≥ 5 V / 1 A) et la liaison de données.
- **Réseau Wi-Fi 2,4 GHz** (802.11 b/g/n — le 5 GHz n'est pas supporté par l'ESP32-S3).
- **Clé API d'un fournisseur IA** :
  - [OpenAI Realtime API](https://platform.openai.com/api-keys) (fournisseur initial par défaut).
  - [Google Gemini Live API](https://aistudio.google.com/) (Google AI Studio).
- Aucun capteur ou module Grove/externe n'est requis.

---

## 3. Installation rapide

### Prérequis
- [VS Code](https://code.visualstudio.com/) avec l'extension [PlatformIO IDE](https://platformio.org/) ou le CLI PlatformIO (`pio`).
- Un compilateur C++17 local (GCC / Clang / MSVC) pour exécuter la suite de tests natifs sur PC.

### Téléchargement et compilation
```bash
# Cloner le dépôt et ouvrir le dossier VoiceJournal_CoreS3
git clone https://github.com/calmtechnology-ch/VoiceJournal.git
cd VoiceJournal/VoiceJournal_CoreS3

# Copier le fichier d'exemple des secrets
cp include/secrets.example.h include/secrets.h

# Éditer include/secrets.h avec vos identifiants Wi-Fi et clés API
# Puis compiler et téléverser sur le CoreS3 connecté en USB :
pio run -e m5stack-cores3 -t upload

# Ouvrir le moniteur série USB à 115 200 bauds :
pio device monitor -b 115200
```

### Exécution des tests natifs et du simulateur PC
```bash
# Compiler et exécuter les 646 tests unitaires sur PC :
cd tests
mkdir build && cd build
cmake ..
cmake --build .
./voicejournal_tests

# Lancer l'évaluation du jeu de données contextuel :
./voicejournal_simulator --dataset ../datasets/contextual_parsing_fr.json
```

---

## 4. Configuration

Toutes les options de l'application sont centralisées dans deux fichiers d'en-tête :

### Clés et accès réseau : `include/secrets.h`
Créez ce fichier à partir de [`include/secrets.example.h`](include/secrets.example.h) :
```cpp
#pragma once

// Identifiants Wi-Fi (2.4 GHz uniquement)
#define WIFI_SSID     "MonReseauWifi"
#define WIFI_PASSWORD "MonMotDePasse"

// Clés API des fournisseurs IA
#define OPENAI_KEY    "sk-proj-..."
#define GEMINI_KEY    "AIzaSy..."
```

### Paramètres de l'application : `include/AppConfig.h`
Modifiez les paramètres globaux dans [`include/AppConfig.h`](include/AppConfig.h) si nécessaire :
- `DEFAULT_AI_PROVIDER` : `AiProvider::OPENAI` (par défaut) ou `AiProvider::GEMINI`.
- `DEFAULT_PARSER_MODE` : `ParserMode::INTELLIGENT` (défaut), `SHADOW` ou `LEGACY`.
- `MAX_OBSERVATIONS` : 5 observations conservées en NVS.
- `MAX_OBSERVATION_LENGTH` : 180 octets par observation.
- `DISPLAY_ORIENTATION` : orientation paysage native 320 × 240 (M5GFX rotation 1).

---

## 5. Commandes vocales disponibles (v0.2.1)

VoiceJournal v0.2.1 se concentre sur un ensemble officiel de fonctionnalités robustes et hautement fiables :

| Fonction | Exemples de phrases vocales | Action effectuée |
|---|---|---|
| **Ajouter une note** | *"Note, le prototype fonctionne"*<br>*"Mémorise que la tension est de 3.3V"*<br>*"Enregistre la mesure du capteur"* | Enregistre l'observation en NVS (max 5) et met à jour l'écran. |
| **Relire la dernière note** | *"Lis la dernière note"*<br>*"Relis la note"* | L'IA vocale dicte le contenu exact de la dernière observation. |
| **Compter les notes** | *"Combien de notes ?"*<br>*"Quel est le nombre d'observations ?"* | Affiche et énonce le nombre de notes enregistrées. |
| **Analyser les observations** | *"Fais le bilan"*<br>*"Résume les observations"* | L'IA synthétise l'ensemble des notes du carnet. |
| **Demander la suppression** | *"Efface toutes les notes"*<br>*"Supprime le carnet"* | Arme la confirmation de suppression (sécurité 10 s). |
| **Confirmer la suppression** | *"Confirmer la suppression"* | Efface l'ensemble de la partition NVS (si armé). |
| **Annuler** | *"Annuler"* / *"Non"* / *"Stop"* | Désarme la suppression ou interrompt l'action en cours. |
| **Aide** | *"Quelles sont les commandes ?"*<br>*"Aide"* | Liste les commandes vocales supportées. |

> [!NOTE]
> Les intentions de recherche par date (`SEARCH_BY_DATE`), recherche thématique (`SEARCH_NOTES`), gestion de tâches (`ADD_TASK`, `LIST_TASKS`) et analyse de projet (`ANALYZE_PROJECT`) sont reconnues par le moteur NLU mais sont volontairement désactivées dans l'application v0.2.1 (*"Cette fonction sera disponible dans une prochaine version"*). Elles constituent le cœur de la version **v0.3.0** détaillée dans le document [`docs/ROADMAP.md`](docs/ROADMAP.md).

---

## 6. Utilisation du Parsing Lab

Le **Parsing Lab** est un outil de diagnostic tactile embarqué accessible directement sur le CoreS3 :

1. Sur l'écran principal, appuyez sur le bouton **`[LAB]`** situé discrètement en haut à droite.
2. Chaque appui successif fait défiler les 3 vues disponibles :
   - **Vue 1 — Résultat** : affiche l'intention reconnue, le score de confiance (%), le statut d'autorisation (`AUTORISEE`, `FONCTION INDISPONIBLE`, `COMMANDE INCONNUE`, `COMMANDE AMBIGUE`, `REFUS DE SECURITE`), la cible sémantique, la portée et le temps d'analyse en microsecondes.
   - **Vue 2 — Contexte sémantique** : affiche la transcription brute reçue, les rôles grammaticaux associés à chaque token et les entités extraites.
   - **Vue 3 — Comparaison Shadow** : compare en temps réel la décision du moteur contextuel intelligent avec celle de l'analyseur historique (Legacy) pour vérifier la non-régression.
3. Le bouton rouge **`STOP`** reste visible et prioritaire en bas à droite de l'écran Lab.
4. Appuyez sur **`[RETOUR]`** pour revenir à l'écran standard.

---

## 7. Utilisation du mode diagnostic série (USB)

Le protocole série USB permet de tester et qualifier le moteur de parsing sans micro, sans connexion Wi-Fi et sans consommer de jetons d'API cloud. Les commandes sont strictement isolées de la machine d'état audio et du carnet NVS.

Connectez le CoreS3 en USB à 115 200 bauds et envoyez les commandes suivantes :

| Commande série | Description | Format de réponse (JSON) |
|---|---|---|
| `PARSE <texte>` | Analyse une phrase avec le moteur actif | `{"command":"PARSE","input":"...","intent":"...","status":"OK","executable":true}` |
| `PARSE_SHADOW <texte>` | Analyse comparative Intelligent vs Legacy | `{"command":"PARSE_SHADOW","shadowStatus":"MATCH","divergent":false}` |
| `PARSER_STATUS` | Affiche l'état courant et la mémoire | `{"command":"PARSER_STATUS","version":"0.2.1","mode":"INTELLIGENT",...}` |
| `PARSER_HELP` | Liste des commandes série acceptées | `{"command":"PARSER_HELP","supportedCommands":[...]}` |

Pour la spécification complète de la structure des réponses JSON et des exemples de trames, consultez [`docs/SERIAL_PARSER_PROTOCOL.md`](docs/SERIAL_PARSER_PROTOCOL.md).

---

## 8. Résolution des problèmes courants

| Symptôme | Cause probable | Solution |
|---|---|---|
| **Écran figé sur "Connexion Wi-Fi..."** | Réseau 5 GHz ou identifiants incorrects dans `include/secrets.h`. | Vérifier que le SSID est sur la bande 2,4 GHz et que le mot de passe est exact. |
| **Erreur "Clé API invalide ou absente"** | Clé OpenAI ou Gemini manquante ou laissée avec la valeur modèle. | Vérifier la clé dans `include/secrets.h` sans espaces superflus. |
| **Le PTT ne répond pas au clic** | Conflit tactile ou clic trop bref. | Effectuer un appui franc au centre du bouton bleu `PARLER`. |
| **La suppression du carnet ne fonctionne pas** | Absence de portée globale ou non-confirmation dans les 10 secondes. | Dire explicitement *"Efface toutes les notes"*, puis *"Confirmer la suppression"* sous 10 s. |
| **Le CoreS3 ne répond plus pendant une synthèse** | Réponse IA trop longue ou flux réseau interrompu. | Toucher immédiatement le bouton rouge **`STOP`** pour forcer l'interruption. |
| **Erreur de compilation PlatformIO sur `secrets.h`** | Fichier `secrets.h` non créé. | Dupliquer `include/secrets.example.h` vers `include/secrets.h`. |

---

## 9. Documentation détaillée

Pour approfondir l'architecture et les spécifications techniques de VoiceJournal v0.2.1 :
- **Guide Utilisateur complet** : [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md)
- **Architecture logicielle et flux de données** : [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Guide du moteur de parsing contextuel** : [`docs/PARSER_GUIDE.md`](docs/PARSER_GUIDE.md)
- **Protocole série JSON unifié** : [`docs/SERIAL_PARSER_PROTOCOL.md`](docs/SERIAL_PARSER_PROTOCOL.md)
- **Procédure de validation matérielle sur CoreS3** : [`docs/HARDWARE_VALIDATION.md`](docs/HARDWARE_VALIDATION.md)
- **Rapport de consolidation v0.2.1** : [`docs/CONSOLIDATION_REPORT_v0.2.1.md`](docs/CONSOLIDATION_REPORT_v0.2.1.md)
- **Feuille de route v0.3.0+** : [`docs/ROADMAP.md`](docs/ROADMAP.md)
- **Archives des versions antérieures** : [`docs/archive/`](docs/archive/)

---

## 10. Licence

Ce projet est sous licence **MIT**. Voir le fichier [`LICENSE`](LICENSE) pour plus de détails.  
La bibliothèque externe **VoiceKit** est également sous licence **MIT** ([calmtechnology-ch/VoiceKit](https://github.com/calmtechnology-ch/VoiceKit)).
