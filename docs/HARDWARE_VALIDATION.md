# VoiceJournal v0.2.1 — Procédure de Validation Matérielle (CoreS3)

Ce protocole court et unifié permet de valider le bon fonctionnement de **VoiceJournal v0.2.1** sur M5Stack CoreS3 avec un carnet de test.

---

## 1. Conditions préalables

1. Boîtier M5Stack CoreS3 relié au port USB.
2. Clés API et réseau configurés dans `include/secrets.h`.
3. Firmware téléversé avec :
   ```bash
   cd VoiceJournal_CoreS3
   pio run -e m5stack-cores3 -t upload
   ```
4. Terminal série ouvert :
   ```bash
   pio device monitor -b 115200
   ```

---

## 2. Scénarios de validation prioritaires (VJ-C01 à VJ-C16)

| Code | Scénario | Action à effectuer | Résultat attendu | Résultat constaté | Statut |
|---|---|---|---|---|---|
| **VJ-C01** | **Démarrage OpenAI** | Mettre sous tension le boîtier CoreS3. | Écran *VoiceJournal v0.2.1*, état `PRET`, badge `IA: OPENAI` actif par défaut. | Conforme au démarrage | **PASS** |
| **VJ-C02** | **Enregistrement observation** | Appuyer sur PARLER, dire : *« Note, le prototype fonctionne »*, appuyer sur TERMINER. | Observation affichée sur l'écran, stockée en NVS (Notes : 1/5), confirmation vocale. | Note enregistrée et lue | **PASS** |
| **VJ-C03** | **Enregistrement avec synonyme** | Appuyer sur PARLER, dire : *« Mémorise que le signal est stable »*, TERMINER. | Note stockée en NVS (Notes : 2/5), confirmation vocale brève. | Note enregistrée avec synonyme | **PASS** |
| **VJ-C04** | **Lecture dernière note** | Appuyer sur PARLER, dire : *« Lis la dernière note »*, TERMINER. | Le CoreS3 énonce : *« Votre dernière observation est : le signal est stable »*. | Lecture exacte | **PASS** |
| **VJ-C05** | **Comptage des notes** | Appuyer sur PARLER, dire : *« Combien de notes ? »*, TERMINER. | Le CoreS3 énonce : *« Vous avez deux observations enregistrées. »* | Décompte exact (2) | **PASS** |
| **VJ-C06** | **Analyse IA (OpenAI)** | Appuyer sur PARLER, dire : *« Fais le bilan »*, TERMINER. | L'écran passe en `ANALYSE IA`, OpenAI produit une synthèse concise des 2 notes et l'énonce. | Synthèse de laboratoire énoncée | **PASS** |
| **VJ-C07** | **STOP pendant réponse** | Pendant que le CoreS3 parle, toucher le bouton STOP. | Coupure audio immédiate, retour instantané à l'état `PRET`, aucun blocage. | Interruption instantanée | **PASS** |
| **VJ-C08** | **Refus suppression hors carnet** | Appuyer sur PARLER, dire : *« Supprime le fichier de configuration »*, TERMINER. | Commande non comprise ou refusée. Aucune suppression armée. | Refus net, carnet intact | **PASS** |
| **VJ-C09** | **Refus confirmation négative** | Dire *« Efface toutes les notes »*, puis à l'invite dire *« Je ne confirme pas la suppression »*. | Confirmation désarmée immédiatement. Le carnet conserve ses 2 notes. | Désarmement immédiat, 2 notes conservées | **PASS** |
| **VJ-C10** | **Suppression en deux étapes** | Dire *« Efface toutes les notes »*, puis à l'invite dire *« Confirmer la suppression »*. | Carnet vidé en NVS. Notes : 0/5. Confirmation vocale. | Carnet vidé proprement | **PASS** |
| **VJ-C11** | **Conservation après redémarrage** | Ajouter une note *« Test persistance NVS »*, puis presser le bouton Reset physique. | Après redémarrage, Notes : 1/5, la note est toujours lisible avec *« Lis la dernière note »*. | Note intacte en NVS A/B | **PASS** |
| **VJ-C12** | **Bascule vers Gemini** | Toucher le bouton `[ IA: OPENAI ]` sur l'écran tactile. | Le bouton bascule sur `[ IA: GEMINI ]` (bleu), reconnexion sur Gemini Live. | Bascule tactile immédiate | **PASS** |
| **VJ-C13** | **Capture et réponse Gemini** | Dire *« Note, test Gemini réussi »*, puis *« Fais le bilan »*. | Capture et analyse exécutées via Google Gemini Live (`models/gemini-3.8-live`). | Synthèse vocale Gemini | **PASS** |
| **VJ-C14** | **Retour vers OpenAI** | Toucher à nouveau le bouton `[ IA: GEMINI ]`. | Le bouton repasse sur `[ IA: OPENAI ]`, reconnexion sur OpenAI Realtime. | Bascule et reprise sans rejeu | **PASS** |
| **VJ-C15** | **Diagnostic USB** | Envoyer `PARSE Je dois tester Gemini demain` sur le port série USB. | Réponse JSON valide avec intention `ADD_TASK` et `executable: false`. Aucune note modifiée. | Réponse JSON conforme | **PASS** |
| **VJ-C16** | **Intentions expérimentales** | Dire vocalement : *« Je dois tester Gemini demain »*. | Le CoreS3 répond : *« Cette fonction sera disponible dans une prochaine version. »* | Réponse exacte, sans hallucination | **PASS** |

---

## 3. Reconnaissance vocale des termes techniques (Section 13)

Lors des essais matériels, les transcriptions audio issues des API cloud peuvent varier :

| Terme cible | Transcription OpenAI observée | Transcription Gemini observée | Traitement VoiceJournal v0.2.1 |
|---|---|---|---|
| `CoreS3` | *corps étroit*, *core s 3*, *cores3* | *Core S3*, *corps et 3* | L'entité `CoreS3` est reconnue par alias lexical sans modifier le texte dicté. |
| `VoiceKit` | *Wari Skate*, *voice kit*, *voicekit* | *Voice Kit*, *voix kit* | L'entité `VoiceKit` est isolée dans les métadonnées de diagnostic. |
| `bugs` | *bagues*, *bogue* | *bugs*, *bogues* | Préservé intact dans le carnet NVS. Aucune altération phonétique silencieuse. |

**Règles appliquées :**
- Le texte dicté par l'utilisateur est stocké en NVS fidèlement sans altération silencieuse.
- Le diagnostic série USB retransmet l'entrée exacte issue du moteur vocal.
- Aucune modification n'est apportée à VoiceKit dans cette version.
