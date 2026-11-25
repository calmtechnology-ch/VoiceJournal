# Rapport de Non-Régression & Résultats des Tests — VoiceJournal v0.2.0

## 1. Synthèse Globale

La transition de VoiceJournal v0.1.1 vers la v0.2.0 intègre un banc complet de tests unitaires et d'intégration validé sous environnement C++17 natif :

```text
==================================================
Résultats Globaux VoiceJournal v0.2.0 :
  - Tests Historiques v0.1.1 : 36 PASS / 36 (0 FAIL)
  - Tests NLU Parser v0.2.0  : 220 PASS / 220 (0 FAIL)
  - TOTAL GÉNÉRAL            : 256 PASS / 256 (0 FAIL)
  - Taux de Réussite         : 100 %
==================================================
```

---

## 2. Détail des Tests Historiques v0.1.1 (34 cas / 36 assertions)

L'intégralité des fonctionnalités critiques de la version v0.1.1 a été rigoureusement re-vérifiée :

| Composant | Scénario testé | Assertions | Résultat |
|---|---|---|---|
| **JournalStorage** | Carnet initialement vide | 5 | `PASS` |
| **JournalStorage** | 1, 5 puis 6 notes (rotation circulaire A/B) | 5 | `PASS` |
| **JournalStorage** | Rejet vide et dépassement limite 180 octets | 4 | `PASS` |
| **JournalStorage** | Préservation UTF-8 et troncature sûre | 3 | `PASS` |
| **JournalStorage** | Reprise après redémarrage simulé | 4 | `PASS` |
| **JournalStorage** | Échecs ciblés note, head et count | 3 | `PASS` |
| **JournalStorage** | Clear persistant et contrôle de remove() | 3 | `PASS` |
| **JournalStorage** | Coupure simulée à chaque étape de `addNote()` | 4 | `PASS` |
| **JournalStorage** | Coupure simulée à chaque étape de `clear()` | 3 | `PASS` |
| **JournalStorage** | Échec `begin()` et diagnostic de corruption | 3 | `PASS` |
| **JournalStorage** | Migration v0.1.0 -> v0.1.0.1 (5 notes en rotation) | 4 | `PASS` |
| **CommandRouter** | Préfixe NOTE, casse et rejet vide/milieu | 4 | `PASS` |
| **CommandRouter** | Commandes obligatoires, casse et accents | 4 | `PASS` |
| **CommandRouter** | Confirmation CLEAR, annulation et expiration | 4 | `PASS` |
| **JournalUI** | STOP prioritaire 50 ms et 100 ms après PARLER | 3 | `PASS` |
| **JournalUI** | Anti-rebond PARLER rejette un 2e PARLER à 50 ms | 3 | `PASS` |
| **JournalUI** | STOP maintenu émet un seul événement | 3 | `PASS` |
| **VoiceController** | Partiel vs final et exécution unique | 3 | `PASS` |
| **VoiceController** | STOP pendant LISTENING sans stop() redondant | 3 | `PASS` |
| **VoiceController** | STOP pendant TRANSCRIBING + finale tardive refusée | 3 | `PASS` |
| **VoiceController** | STOP pendant ANALYZING + réponse IA tardive refusée | 3 | `PASS` |
| **VoiceController** | STOP pendant SPEAKING | 3 | `PASS` |
| **VoiceController** | STOP puis PARLER + libération asynchrone | 3 | `PASS` |
| **VoiceController** | Deux STOP successifs idempotents | 3 | `PASS` |
| **VoiceController** | STOP puis reconnexion sans rejeu, NVS intact | 3 | `PASS` |
| **VoiceController** | Refus des requêtes concurrentes | 3 | `PASS` |
| **VoiceController** | Analyse IA bornée sans double say() | 3 | `PASS` |
| **VoiceController** | Timeout transcription puis nouvelle capture | 3 | `PASS` |
| **VoiceController** | Watchdogs IA et TTS récupérables | 3 | `PASS` |
| **JournalUI** | Sélecteur fournisseur IA (hit-test, debounce) | 4 | `PASS` |
| **VoiceController** | Gemini par défaut + bascule avec OpenAI + reconnexion | 4 | `PASS` |
| **VoiceController** | Refus bascule si clé manquante ou moteur occupé | 3 | `PASS` |
| **VoiceController** | Gemini initial, profils vocaux et capture après bascule | 3 | `PASS` |
| **VoiceController** | Clé initiale manquante puis reprise par bascule | 3 | `PASS` |
| **VoiceController** | Confirmation suppression majuscules + fenêtre TTS | 3 | `PASS` |
| **VoiceController** | FormatDisplayTranscript convertit majuscules intégrales | 6 | `PASS` |

---

## 3. Détail de la Suite de Tests NLU v0.2.0 (220 cas de test)

La suite NLU couvre 11 catégories distinctes conformément aux spécifications du cahier des charges :

| Catégorie | Description | Nombre de tests | Résultat |
|---|---|---|---|
| **Catégorie 1** | Normalisation UTF-8, accents français, ligatures, élisions | 16 tests | `16 PASS / 0 FAIL` |
| **Catégorie 2** | Tokenisation, calcul d'offsets bijectifs original/normalisé | 12 tests | `12 PASS / 0 FAIL` |
| **Catégorie 3** | Dictionnaire lexical, masques de concepts, bigrammes | 18 tests | `18 PASS / 0 FAIL` |
| **Catégorie 4** | Reconnaissance des intentions (exécutables, expér., libres) | 28 tests | `28 PASS / 0 FAIL` |
| **Catégorie 5** | Extraction des paramètres (entités, projets, types, dates, payload) | 22 tests | `22 PASS / 0 FAIL` |
| **Catégorie 6** | Tolérance aux erreurs (Damerau-Levenshtein, garde-fous) | 16 tests | `16 PASS / 0 FAIL` |
| **Catégorie 7** | Détection des négations, isolation de payload dictée | 22 tests | `22 PASS / 0 FAIL` |
| **Catégorie 8** | Gestion des ambiguïtés (verbes en conflit, priorités) | 16 tests | `16 PASS / 0 FAIL` |
| **Catégorie 9** | Sécurité de suppression (confirmations strictes, refus partiel) | 16 tests | `16 PASS / 0 FAIL` |
| **Catégorie 10** | Comparaison historique en mode SHADOW | 22 tests | `22 PASS / 0 FAIL` |
| **Catégorie 11** | Robustesse des entrées (caractères invalides, injections, vides) | 12 tests | `12 PASS / 0 FAIL` |

---

## 4. Analyse Comparative en Mode Shadow

Le mode Shadow a été soumis à 22 scénarios comparant l'ancien routeur v0.1.1 et le nouveau moteur v0.2.0 :

1. **Cas MATCH (Commandes historiques standard) :**
   - *« Note, observation de test »* : Les deux moteurs identifient `ADD_NOTE`.
   - *« Lis la dernière note »* : Les deux moteurs identifient `READ_LAST_NOTE`.
   - *« Combien de notes ? »* : Les deux moteurs identifient `COUNT_NOTES`.
   - *« Fais le bilan »* : Les deux moteurs identifient `ANALYZE_NOTES`.
   - *« Efface toutes les notes »* : Les deux moteurs identifient `REQUEST_CLEAR`.
   - *« Confirmer la suppression »* (avec confirmation armée) : Les deux moteurs identifient `CONFIRM_CLEAR`.
   - *« Annuler »* : Les deux moteurs identifient `CANCEL_CLEAR`.
   - *« Aide »* : Les deux moteurs identifient `HELP`.

2. **Cas NEW_INTENT (Enrichissement sans régression) :**
   - *« Retrouve les bugs de VoiceKit »* : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `SEARCH_NOTES`. Statut : `NEW_INTENT`.
   - *« Je dois tester Gemini demain »* : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `ADD_TASK`. Statut : `NEW_INTENT`.
   - *« Fais le bilan du projet RuleBot »* : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `ANALYZE_PROJECT`. Statut : `NEW_INTENT`.
   - *« Le bug est corrigé »* : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `RESOLVE_PROBLEM`. Statut : `NEW_INTENT`.
   - *« Mémorise le niveau de batterie »* : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `ADD_NOTE`. Statut : `NEW_INTENT`.
   - *« retruve les observations »* (Fuzzy) : V0.1.1 retourne `UNKNOWN` ; V0.2.0 identifie `SEARCH_NOTES`. Statut : `NEW_INTENT`.

3. **Cas SECURITY_BLOCKED (Divergence sur action destructive) :**
   - *« Je ne confirme pas la suppression »* : V0.1.1 avec l'ancienne fonction aurait pu valider en raison de la présence de la chaîne sous-jacente ; V0.2.0 détecte formellement la négation, bloque toute exécution destructive et désarme le verrou. Statut : `SECURITY_BLOCKED`.
