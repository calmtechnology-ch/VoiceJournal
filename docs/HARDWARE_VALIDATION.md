# Procédure de validation matérielle — VoiceJournal v0.1.0 (M5Stack CoreS3)

> **Règle d'honnêteté technique :** Aucun test ci-dessous ne doit être marqué `PASS` sans exécution réelle sur un **M5Stack CoreS3** physique connecté au Wi-Fi 2.4 GHz et à Google Gemini Live.

## 1. Prérequis de banc d'essai

- M5Stack CoreS3 alimenté par câble USB-C données (≥ 5 V / 1 A).
- `include/secrets.h` configuré avec un réseau Wi-Fi 2.4 GHz et une clé API Gemini valide.
- Firmware compilé et téléversé (`pio run -t upload`).
- Moniteur série ouvert à 115 200 bauds (`pio device monitor -b 115200`).

## 2. Grille de validation manuelle

Statuts autorisés : `PASS` | `FAIL` | `NOT RUN`

| ID | Scénario | Procédure de test | Résultat attendu | Statut | Observations |
|---|---|---|---|---|---|
| **VJ-01** | Démarrage | Alimenter le CoreS3 et observer l'écran et le moniteur série | Affichage immédiat de l'interface, connexion Wi-Fi/Gemini puis passage à `PRET` (`[VJ][VOICE] READY`) | `NOT RUN` | |
| **VJ-02** | Première observation | Cliquer `PARLER`, dire *« Note, le prototype redémarre »*, cliquer `TERMINER` | Note enregistrée en NVS (`[VJ][STORAGE] Ecriture NVS OK`), compteur à `1 / 5`, confirmation TTS *« Note enregistrée. »* | `NOT RUN` | |
| **VJ-03** | Deuxième observation | Dicter une seconde note *« Note, température stable à 38 degrés »* | Compteur passe à `2 / 5`, écriture NVS confirmée | `NOT RUN` | |
| **VJ-04** | Lecture dernière note | Dire *« Lis la dernière note »* | Lecture vocale et affichage du texte exact de la deuxième observation | `NOT RUN` | |
| **VJ-05** | Nombre de notes | Dire *« Combien de notes ? »* | Restitution vocale *« Le carnet contient 2 notes. »* | `NOT RUN` | |
| **VJ-06** | Demande de bilan | Dire *« Fais le bilan »* (puis tester *« Résume les observations »*) | Passage en `ANALYSE IA`, synthèse Gemini en français (2 à 4 phrases), sans invention ni double `say()` | `NOT RUN` | |
| **VJ-07** | Redémarrage | Couper l'alimentation USB-C puis redémarrer le CoreS3 | Compteur `2 / 5` immédiatement visible au boot avant même la connexion Wi-Fi ; notes intactes | `NOT RUN` | |
| **VJ-08** | Six observations | Enregistrer 6 notes successives | Rotation circulaire : compteur reste `5 / 5`, la 1re note est remplacée, ordre chronologique 2..6 préservé | `NOT RUN` | |
| **VJ-09** | STOP pendant TTS | Lancer *« Lis la dernière note »* puis cliquer `STOP` en cours de lecture | Audio coupé immédiatement, retour à `PRET`, notes conservées, commande suivante fonctionnelle | `NOT RUN` | |
| **VJ-10** | STOP pendant STT | Cliquer `PARLER`, parler puis cliquer `STOP` sans cliquer `TERMINER` | Micro fermé, aucune note parasite enregistrée, retour à `PRET` | `NOT RUN` | |
| **VJ-11** | Capture silencieuse | Cliquer `PARLER`, rester silencieux 2 s, cliquer `TERMINER` | Aucune note enregistrée, retour propre à `PRET` | `NOT RUN` | |
| **VJ-12** | Déconnexion Wi-Fi | Couper le point d'accès Wi-Fi | Passage à `DECONNECTE`, bouton `PARLER` désactivé, compteur de notes toujours affiché, notes NVS intactes | `NOT RUN` | |
| **VJ-13** | Reconnexion | Rétablir le point d'accès Wi-Fi | Retour automatique à `PRET` sans rejouer la dernière commande | `NOT RUN` | |
| **VJ-14** | Clics rapides | Appuyer rapidement plusieurs fois sur `PARLER` | Anti-rebond actif, aucune double capture ni erreur audio | `NOT RUN` | |
| **VJ-15** | Suppression sans confirmation | Dire *« Efface toutes les notes »*, puis dire *« Oui »* ou attendre 15 s | Aucune suppression effectuée, toutes les notes conservées | `NOT RUN` | |
| **VJ-16** | Suppression confirmée | Dire *« Efface toutes les notes »*, puis *« Confirmer la suppression »* dans les 15 s | Carnet vidé (`0 / 5`), confirmation TTS, reste vide après redémarrage | `NOT RUN` | |
| **VJ-17** | Transcription tardive | Cliquer `TERMINER` puis cliquer `STOP` immédiatement avant réception finale | Résultat obsolète ignoré (`[VJ][STT] Transcription obsolete ignoree`), aucune commande exécutée | `NOT RUN` | |
| **VJ-18** | Timeout | Couper le réseau juste après `TERMINER` (pendant `TRANSCRIPTION`) | Déclenchement du watchdog (8 s), pas de blocage permanent, retour à état cohérent | `NOT RUN` | |

## 3. Synthèse de campagne

- **Date d'exécution :** _À compléter_
- **Opérateur :** _À compléter_
- **Bilan :** `0 PASS` / `0 FAIL` / `18 NOT RUN`
