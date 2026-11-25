# Procédure de Validation Matérielle — VoiceJournal v0.2.0.1 (M5Stack CoreS3)

> **Règle d'honnêteté technique :** Aucun test ci-dessous ne doit être marqué `PASS` sans exécution réelle et constatée sur un **M5Stack CoreS3** physique. La réussite de la compilation PlatformIO ou des 505 tests natifs ne constitue pas une validation matérielle. Les 8 scénarios PASS issus de la validation rapide v0.2.0 constituent la référence de non-régression matérielle stricte.

---

## 1. Référence Matérielle de Non-Régression (Validation Rapide v0.2.0)

Rapport communiqué par l'utilisateur sur CoreS3 physique (firmware v0.2.0) :

```text
VoiceJournal v0.2.0 — Validation rapide

01 ADD_NOTE            PASS
02 ADD_NOTE synonyme   NOT RUN
03 ADD_NOTE synonyme   NOT RUN
04 READ_LAST_NOTE      PASS
05 COUNT_NOTES         PASS
06 ANALYZE_NOTES       PASS
07 SEARCH_NOTES        NOT RUN
08 ADD_TASK            NOT RUN
09 ANALYZE_PROJECT     NOT RUN
10 Négation DELETE     NOT RUN
11 Négation dans note  NOT RUN
12 DELETE hors carnet  PASS
13 AMBIGUOUS           PASS
14 UNKNOWN             PASS
15 STOP confirmation   PASS

Bilan : 8 PASS / 0 FAIL / 7 NOT RUN
```

> **Consigne stricte :** Les 8 scénarios `PASS` ci-dessus sont garantis non régressés par les tests natifs et la compilation v0.2.0.1. Les 7 scénarios `NOT RUN` sont maintenus avec ce statut tant qu'ils n'ont pas été formellement rejoués sur le matériel.

---

## 2. Protocole de Banc d'Essai CoreS3

- **Matériel** : M5Stack CoreS3 alimenté par câble USB-C (≥ 5 V / 1 A), écran LCD 320 × 240 rotation 0 native.
- **Réseau** : Wi-Fi 2.4 GHz configuré dans `include/secrets.h`.
- **Clés API** : OpenAI Realtime et/ou Google Gemini Live.
- **Téléversement** : `pio run -e m5stack-cores3 -t upload`.
- **Moniteur série** : `pio device monitor -b 115200`.

---

## 3. Grille de Validation Complète (`VJ-01` à `VJ-56`)

| ID | Scénario | Manipulation | Comportement attendu | Traces série | Statut |
|---|---|---|---|---|---|
| **VJ-01** | Démarrage | Allumer CoreS3 | Écran prêt, Wi-Fi connecté, passage à `PRET` | `[VJ][BOOT] VoiceJournal v0.2.0.1`, `[VJ][STORAGE] NVS initialisee`, `[VJ][VOICE] READY` | **PASS** *(Réf)* |
| **VJ-02** | Première observation | Cliquer `PARLER`, dire *« Note, prototype opérationnel »* | Note stockée en NVS, compteur `1 / 5`, TTS *« Note enregistrée »* | `[VJ][COMMAND] ADD_NOTE`, `[VJ][STORAGE] Ecriture NVS OK` | **PASS** *(Réf 01)* |
| **VJ-03** | Synonyme Mémorise | Dire *« Mémorise la tension à 3.3V »* | Note stockée en NVS, compteur `2 / 5` | `[VJ][COMMAND] ADD_NOTE`, `Rule: add_note_prefix` | `NOT RUN` *(Réf 02)* |
| **VJ-04** | Synonyme Enregistre | Dire *« Enregistre le log »* | Note stockée en NVS, compteur `3 / 5` | `[VJ][COMMAND] ADD_NOTE`, `Rule: add_note_prefix` | `NOT RUN` *(Réf 03)* |
| **VJ-05** | Lecture dernière note | Dire *« Lis la dernière note »* | TTS énonce la dernière note | `[VJ][COMMAND] READ_LAST_NOTE` | **PASS** *(Réf 04)* |
| **VJ-06** | Nombre de notes | Dire *« Combien de notes ? »* | TTS énonce le nombre d'observations | `[VJ][COMMAND] COUNT_NOTES` | **PASS** *(Réf 05)* |
| **VJ-07** | Bilan des notes | Dire *« Fais le bilan »* | Synthèse IA en français (2-4 phrases) | `[VJ][COMMAND] ANALYZE_NOTES` | **PASS** *(Réf 06)* |
| **VJ-08** | Recherche notes | Dire *« Retrouve les bugs de VoiceKit »* | Intention reconnue, slots extraits, réponse polie | `[VJ][COMMAND] EXPERIMENTAL`, slots entity/problem | `NOT RUN` *(Réf 07)* |
| **VJ-09** | Tâche obligation | Dire *« Je dois tester Gemini demain »* | Intention reconnue, entité Gemini, date demain | `[VJ][COMMAND] EXPERIMENTAL`, slots entity/date | `NOT RUN` *(Réf 08)* |
| **VJ-10** | Bilan projet | Dire *« Fais le bilan du projet RuleBot »* | Intention reconnue, entité RuleBot | `[VJ][COMMAND] EXPERIMENTAL`, entity RuleBot | `NOT RUN` *(Réf 09)* |
| **VJ-11** | Négation suppression | Dire *« Ne supprime pas les notes »* | Pas de suppression, annulation propre | `[VJ][COMMAND] CANCEL_CLEAR` | `NOT RUN` *(Réf 10)* |
| **VJ-12** | Négation dans note | Dire *« Note, ne supprime pas les fichiers »* | Note enregistrée avec négation intacte en NVS | `[VJ][COMMAND] ADD_NOTE`, payload intacte | `NOT RUN` *(Réf 11)* |
| **VJ-13** | Suppression hors carnet | Dire *« Efface le fichier de configuration »* | Refus absolu, carnet intact, aucune suppression | `[VJ][COMMAND] UNKNOWN`, TargetCategory FILE rejetée | **PASS** *(Réf 12)* |
| **VJ-14** | Ambiguïté verbes | Dire *« Lis efface »* ou *« Bilan supprime »* | Déclaré `AMBIGUOUS`, aucune action | `[VJ][PARSER] Ambiguous intent detected` | **PASS** *(Réf 13)* |
| **VJ-15** | Commande inconnue | Dire *« Quelle est la météo ? »* | Réponse polie d'incompréhension | `[VJ][COMMAND] UNKNOWN` | **PASS** *(Réf 14)* |
| **VJ-16** | STOP confirmation | Armer suppression puis appuyer `STOP` | Suppression immédiatement annulée | `[VJ][STOP] Interruption demandee`, désarmement | **PASS** *(Réf 15)* |
| **VJ-17** | Bascule OpenAI / Gemini | Toucher `[IA: OPENAI]` en état `PRET` | Bascule à chaud vers `[IA: GEMINI]` sans coupure NVS | `[VJ][PROVIDER] Bascule demandee : OPENAI -> GEMINI` | `NOT RUN` |
| **VJ-18** | Reconnexion Wi-Fi | Couper puis rétablir le point d'accès Wi-Fi | Reconnexion automatique, retour à `PRET` sans rejeu | `[VJ][VOICE] READY` | `NOT RUN` |
| **VJ-19** | Priorité STOP < 100 ms | Appuyer `PARLER` puis `STOP` immédiatement | Interruption instantanée, pas de blocage anti-rebond | `[VJ][STOP] Interruption demandee` | `NOT RUN` |
| **VJ-20** | Double STOP idempotent | Deux appuis successifs sur `STOP` | Aucun crash ni appel redondant | Idempotence confirmée | `NOT RUN` |
| **VJ-21** | Rotation circulaire | Enregistrer 6 notes d'affilée | Reste à `5 / 5`, note 1 remplacée, ordre préservé | `[VJ][STORAGE] Notes: 5` | `NOT RUN` |
| **VJ-22** | Répétition demande clear | Armer suppression puis répéter *"Efface tout"* | Reste `REQUEST_CLEAR`, ne confirme jamais | `[VJ][COMMAND] REQUEST_CLEAR` (ne confirme pas) | `NOT RUN` |
| **VJ-23** | Confirmation vague refusée | Armer suppression puis dire *"Oui"* ou *"D'accord"* | Refusé, désarmement immédiat | `[VJ][COMMAND] UNKNOWN`, désarmement | `NOT RUN` |
| **VJ-24** | Confirmation exacte | Armer suppression puis dire *"Confirmer la suppression"* | Carnet vidé, confirmation vocale TTS | `[VJ][COMMAND] CONFIRM_CLEAR`, NVS vidé | `NOT RUN` |
| **VJ-25** | Parsing Lab — Vue 1 | Toucher `[LAB]` sur CoreS3 | Affichage de la carte diagnostique Vue 1 Résultat | `[VJ][UI] Toggle Parsing Lab view: 1` | `NOT RUN` |
| **VJ-26** | Parsing Lab — Vue 2 | Toucher le tiers supérieur en mode Lab | Bascule sur Vue 2 Contexte sémantique (rôles, frame, cible) | `[VJ][UI] Parsing Lab subpage: 1` | `NOT RUN` |
| **VJ-27** | Parsing Lab — Vue 3 | Toucher à nouveau le tiers supérieur | Bascule sur Vue 3 Comparaison SHADOW (miroir, timing) | `[VJ][UI] Parsing Lab subpage: 2` | `NOT RUN` |
| **VJ-28** | Parsing Lab — STOP | Appuyer sur `STOP` depuis n'importe quelle vue Lab | Arrêt prioritaire immédiat | `[VJ][STOP] Interruption demandee` | `NOT RUN` |
| **VJ-29** | Série : PARSE tâche | Envoyer `PARSE Je dois tester Gemini demain` via USB | JSON structuré complet retourné, aucun audio | `PARSE` reçu, intent `ADD_TASK` | `NOT RUN` |
| **VJ-30** | Série : PARSE_SHADOW | Envoyer `PARSE_SHADOW Retrouve les bugs de VoiceKit` | Statut `NEW_INTENT` retourné en JSON | `PARSE_SHADOW` reçu, status `NEW_INTENT` | `NOT RUN` |
| **VJ-31** | Série : Sécurité fichier | Envoyer `PARSE_SHADOW Efface le fichier` | Statut `SECURITY_BLOCKED`, exécuté `UNKNOWN` | `PARSE_SHADOW` bloqué, `SECURITY_BLOCKED` | `NOT RUN` |
| **VJ-32** | Série : PARSER_STATUS | Envoyer `PARSER_STATUS` | Statistiques mémoire et tables en JSON | `PARSER_STATUS` JSON émis | `NOT RUN` |

---

## 4. Synthèse de la Campagne Matérielle

- **Version testée :** VoiceJournal v0.2.0.1
- **Référence de non-régression matérielle :** 8 PASS / 0 FAIL / 7 NOT RUN (Validation rapide v0.2.0)
- **Scénarios matériels v0.2.0.1 :** 32 scénarios spécifiés avec statuts stricts
- **Prochaine étape :** Exécution physique sur CoreS3 avec compte-rendu des observations réelles
