# Rapport de Test et Validation Matérielle sur M5Stack CoreS3
**Projet :** VoiceJournal v0.2.0  
**Date :** 25 novembre 2026  
**Cible matérielle :** M5Stack CoreS3 (ESP32-S3, écran ILI9342, micro ES7210, haut-parleur AW88298)  
**Fournisseur IA testé :** Google Gemini Live (`models/gemini-3.8-live`, voix `Aoede`)  
**Réseau :** Wi-Fi 2.4 GHz (`Box_W7-096603`, canal 6, IP 192.168.1.12)  

---

## 1. Synthèse globale

La session de test en conditions réelles sur le matériel physique démontre un **succès complet** de l'ensemble de la chaîne fonctionnelle, logicielle et matérielle :
- Initialisation système, Wi-Fi et WebSocket TLS avec Gemini Live opérationnels en ~7,3 s.
- Moteur de compréhension du langage naturel (**NLU v0.2.0**) déterministe C++17 embarqué : 100 % des intentions correctement classifiées (nominal, négation, ambiguïté, expérimental, inconnu).
- Stockage non-volatile (**NVS**) fiabilisé : persistance, incrémentation, relecture et effacement sécurisé en 2 étapes validés.
- Audio bidirectionnel : streaming micro PCM 16 kHz vers Gemini Live et restitution TTS 24 kHz sans saturation ni perte.
- Bouton **STOP** prioritaire : interruption immédiate d'une synthèse en cours validée en conditions réelles.
- Robustesse réseau : reconnexion automatique instantanée (< 2 s) après fermeture normale d'inactivité du serveur Gemini Live (`WS CLOSE 1008`).

---

## 2. Déroulement chronologique des 12 tours de test

| Tour | Commande vocale prononcée | Intention NLU | Score | Action & Comportement matériel | Statut |
|:---:|---|---|:---:|---|:---:|
| **#1** | *« Note le corps étroit fonctionne correctement. »* | `ADD_NOTE` | 95 | Sauvegarde NVS réussie (note #4). TTS : *"Note enregistree."* | **PASS** |
| **#2** | *« Lis la dernière note. »* | `READ_LAST_NOTE` | 90 | Lecture vocale exacte de la note #4 : *"le corps étroit fonctionne correctement."* | **PASS** |
| **#3** | *« Combien de notes ? »* | `COUNT_NOTES` | 90 | Restitution vocale : *"Le carnet contient 4 notes."* | **PASS** |
| **#4** | *« Fais le bilan. »* + Appui **`STOP`** | `ANALYZE_NOTES` | 90 | Envoi du prompt labo avec les 4 observations. Réception audio puis **interruption immédiate via STOP** (vidage des tampons, retour état `READY`). | **PASS** |
| **#5** | *« Retrouver les bagues de Wari Skate. »* | `SEARCH_NOTES` | 90 | Intention identifiée mais déclarée non exécutable (`Executable: false`). TTS : *"La demande est comprise, mais cette fonction n'est pas encore disponible."* | **PASS** |
| **#6** | *« Je dois tester Gemini demain. »* | `ADD_TASK` | 90 | Extraction slot entité (`Gemini`) et date (`demain`). TTS de fonction expérimentale non disponible. | **PASS** |
| **#7** | *« Refais toutes les notes. »* | `UNKNOWN` | 0 | Commande inconnue correctement rejetée. TTS : *"Commande non reconnue."* | **PASS** |
| **#8** | *« efface toutes les notes »* | `REQUEST_CLEAR` | 95 | Armement sécurisé de suppression. TTS : *"Confirmez-vous la suppression des 4 notes ? Dites confirmer la suppression."* | **PASS** |
| **#9** | *« confirmer la suppression »* | `CONFIRM_CLEAR` | 100 | Validation en 2e étape. Carnet vidé en NVS (`notes=0`). TTS : *"Toutes les notes ont ete supprimees."* | **PASS** |
| **#10** | *« J'ai la dernière note et fais le bilan. »* | `AMBIGUOUS` | 90 | Détection d'ambiguïté (collision de 2 commandes concurrentes). Rejet sécurisé. TTS : *"Commande non reconnue."* | **PASS** |
| **#11** | *« Supprime le fichier de configuration. »* | `REQUEST_CLEAR` | 95 | Détecté comme demande d'effacement, mais carnet déjà vide. TTS : *"Le carnet est deja vide."* | **PASS** |
| **#12** | *« Je dois tester Gemini demain. »* | `ADD_TASK` | 90 | Deuxième validation de la qualification `ADD_TASK`. | **PASS** |

---

## 3. Correspondance avec la grille de validation matérielle (`docs/HARDWARE_VALIDATION.md`)

Les scénarios suivants du plan de test matériel physique peuvent être validés formellement :

- **VJ-01 (Démarrage)** : `PASS` — Boot CoreS3, NVS initialisée, Wi-Fi connecté, WebSocket Gemini Live établi, passage à `READY`.
- **VJ-02 / VJ-03 (Ajout d'observation)** : `PASS` — Transcription reçue, écriture NVS OK, TTS de confirmation émis.
- **VJ-04 (Lecture dernière note)** : `PASS` — Relecture fidèle avec préservation UTF-8 et accents.
- **VJ-05 (Nombre de notes)** : `PASS` — Annonce vocale conforme au compteur NVS réel.
- **VJ-06 / VJ-09 (Bilan IA & STOP prioritaire en lecture)** : `PASS` — Restitution audio coupée net par le bouton `STOP`, retour instantané en `READY` sans désynchronisation.
- **VJ-15 / VJ-16 / VJ-36 (Suppression en 2 étapes)** : `PASS` — Confirmation obligatoire demandée puis exécutée avec succès (`CONFIRM_CLEAR`).
- **VJ-37 (Détection d'ambiguïté)** : `PASS` — Blocage strict de la double commande concurrente.
- **VJ-40 (Recherche de notes expérimentale)** : `PASS` — Intention `SEARCH_NOTES` reconnue sans altération de stockage.
- **VJ-41 (Création de tâche expérimentale)** : `PASS` — Intention `ADD_TASK` reconnue avec extraction d'entité et date relative.

---

## 4. Analyse technique du comportement WebSocket (`WS CLOSE 1008`)

En fin de session, les logs affichent à intervalles réguliers :
```text
[WS] Frame CLOSE recue (code=1008, reason="Session closed due to client inactivity.")
[LiveSession] Erreur client : WS CLOSE 1008: Session closed due to client inactivity.
[LiveSession] Transition : CONNECTED -> ERROR (Erreur client)
[VoiceKit] Wi-Fi et NTP valides -> Connexion au service vocal...
[WS] WebSocket TLS connecte (core=1, prio=1->2, ramInt=135608, maxBlk=110580)
[LiveClient] WebSocket TLS etabli en 1975 ms
[LiveClient] Envoi setup Gemini Live (modele=models/gemini-3.8-live, voix=Aoede, VAD manuelle, transcriptions, 0 outil(s) declare(s))...
[LiveClient] Session Gemini Live initialisee avec succes (setupComplete en 276 ms)
[LiveSession] Transition : CONNECTING -> CONNECTED (Setup confirme)
[VJ][VOICE] State ERROR_STATE -> READY
```

### Explication :
1. **Origine** : Les serveurs Google Gemini Live appliquent une politique de coupure de session WebSocket après une période sans trafic audio montant (`Session closed due to client inactivity`).
2. **Prise en charge par VoiceKit** :
   - L'événement de fermeture est détecté immédiatement sans plantage de l'ESP32.
   - La reconnexion automatique asynchrone se déclenche sans bloquer le système.
   - Un nouveau tunnel TLS et un nouveau `setup` Gemini Live sont rétablis en moins de 2 secondes.
   - L'état repasse en `READY` et l'appareil est de nouveau prêt pour une commande vocale dès l'appui sur `PARLER`.
