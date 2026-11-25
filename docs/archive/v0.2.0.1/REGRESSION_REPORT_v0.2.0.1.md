# Rapport de Non-Régression — VoiceJournal v0.2.0.1

**Date d'évaluation :** 25 novembre 2026  
**Branche Git :** `feature/v0.2.0.1-contextual-parsing`  
**Tags de référence préservés :** `v0.1.1` (`e0ca1d6e...`), `v0.2.0` (`d56c551d...`)  
**Compilateur Natif :** GCC C++17 (`g++ 16.2.0` x86_64)  
**Compilateur Embarqué :** PlatformIO `espressif32@6.9.0` (ESP32-S3 Xtensa GCC 8.4.0)

---

## 1. Synthèse Globale des Tests Unitaires

La suite de tests unitaires exécutée avec `-Wall -Wextra -Werror` regroupe l'intégralité des tests historiques des versions v0.1.1 et v0.2.0, complétée par la nouvelle suite de tests contextuels v0.2.0.1 :

```text
==================================================
Resultat Global VoiceJournal v0.2.0.1 :
  v0.1.1 Legacy    : 36 PASS, 0 FAIL (100%)
  v0.2.0 NLU       : 220 PASS, 0 FAIL (100%)
  v0.2.0.1 Context : 249 PASS, 0 FAIL (100%)
  GRAND TOTAL      : 505 PASS, 0 FAIL (100%)
==================================================
```

---

## 2. Détail par Suite de Tests

### 2.1 Suite Historique v0.1.1 (36 assertions / 34 scénarios) — 100% PASS
Validation des modules d'infrastructure bas niveau :
- `JournalStorage` : persistance NVS A/B, rotation circulaire à 5 notes, tronquage UTF-8 sûr, injection de pannes à chaque étape d'écriture, migration ascendante non destructive.
- `CommandRouter` : commandes impératives, respect de la casse, confirmation CLEAR stricte et expiration temporelle.
- `JournalUI` : priorité absolue de `STOP` dans tous les états, anti-rebond `PARLER` à 180 ms, maintien du contact tactile.
- `VoiceController` : bascule à chaud OpenAI Realtime ↔ Google Gemini Live, gestion des profils vocaux, corrélation des tours STT par `turnId`, filtrage des transcriptions tardives après `STOP`.

### 2.2 Suite NLU v0.2.0 (220 tests distincts) — 100% PASS
Validation du moteur NLU probabiliste léger v0.2.0 :
- Catégorie 1 : Normalisation UTF-8 et élisions françaises (16 tests) : `16 PASS, 0 FAIL`
- Catégorie 2 : Tokenisation et suivi des coordonnées (12 tests) : `12 PASS, 0 FAIL`
- Catégorie 3 : Dictionnaire de concepts compact (18 tests) : `18 PASS, 0 FAIL`
- Catégorie 4 : Reconnaissance des intentions déclarées (28 tests) : `28 PASS, 0 FAIL`
- Catégorie 5 : Extraction des paramètres et entités (22 tests) : `22 PASS, 0 FAIL`
- Catégorie 6 : Tolérance aux fautes (Fuzzy Levenshtein) (16 tests) : `16 PASS, 0 FAIL`
- Catégorie 7 : Négations et sécurité destructive (22 tests) : `22 PASS, 0 FAIL`
- Catégorie 8 : Gestion des ambiguïtés sémantiques (16 tests) : `16 PASS, 0 FAIL`
- Catégorie 9 : Suppression sécurisée et armement temporisé (16 tests) : `16 PASS, 0 FAIL`
- Catégorie 10 : Comparaison miroir SHADOW (22 tests) : `22 PASS, 0 FAIL`
- Catégorie 11 : Robustesse des entrées et chaînes longues (12 tests) : `12 PASS, 0 FAIL`

### 2.3 Suite Contextuelle v0.2.0.1 (249 tests distincts) — 100% PASS
Validation des nouvelles exigences COR-01 à COR-10 :
- Catégorie 12 : Locutions multi-mots (*longest-match*, COR-02) (25 tests) : `25 PASS, 0 FAIL`
- Catégorie 13 : Rôles sémantiques et cadre sémantique (COR-01) (25 tests) : `25 PASS, 0 FAIL`
- Catégorie 14 : Grammaire contextuelle combinatoire (COR-03, COR-04) (40 tests) : `40 PASS, 0 FAIL`
- Catégorie 15 : Actions et objets cibles (`TargetCategory`, COR-05) (25 tests) : `25 PASS, 0 FAIL`
- Catégorie 16 : Portée contextuelle des négations (COR-06) (30 tests) : `30 PASS, 0 FAIL`
- Catégorie 17 : Arbitrage d'ambiguïté indépendant du fuzzy (COR-07) (21 tests) : `21 PASS, 0 FAIL`
- Catégorie 18 : Sécurité destructive et confirmation (COR-08) (30 tests) : `30 PASS, 0 FAIL`
- Catégorie 19 : Comparateur SHADOW complet à 7 états (COR-09) (21 tests) : `21 PASS, 0 FAIL`
- Catégorie 20 : Robustesse mémoire et limites bornées (COR-10) (16 tests) : `16 PASS, 0 FAIL`
- Catégorie 21 : Protocole série USB de diagnostic (Section 17) (16 tests) : `16 PASS, 0 FAIL`

---

## 3. Évaluation sur Jeu de Données (`tests/datasets/contextual_parsing_fr.json`)

Évaluation automatique via le simulateur console :
```text
Total cas évalués : 53
PASS              : 53 (100%)
FAIL              : 0
Taux de succès    : 100.0%
```

---

## 4. Mesures d'Empreinte Mémoire sur M5Stack CoreS3 (ESP32-S3)

| Métrique | v0.2.0 Baseline | v0.2.0.1 Contextuel | Évolution | Seuil toléré (Section 25) |
|---|---|---|---|---|
| **RAM (Heap / Data)** | 85 916 octets (26,2%) | 86 276 octets (26,3%) | **+360 octets** | **+16 Ko max (+16 384 o)** |
| **Flash (App binary)** | 1 829 440 octets (87,2%) | 1 844 049 octets (87,9%) | **+14 609 octets** | Partition 2 Mo (2 097 152 o) |
| **Temps parsing moyen** | ~195 µs | ~220 µs | **+25 µs** | 50 000 µs (50 ms max) |

L'augmentation de RAM est limitée à **360 octets**, ce qui représente **moins de 2,2% de la marge allouée de 16 Ko**. Toutes les tables déclaratives résident en mémoire Flash statique.

---

## 5. Bilan de Non-Régression Matérielle

Rappel des 8 scénarios matériels validés avec succès sur M5Stack CoreS3 physique (Section 3 du brief) :
1. `ADD_NOTE` : **PASS** (Non régressé)
2. `READ_LAST_NOTE` : **PASS** (Non régressé)
3. `COUNT_NOTES` : **PASS** (Non régressé)
4. `ANALYZE_NOTES` : **PASS** (Non régressé)
5. `DELETE hors carnet` : **PASS** (Non régressé, renforcé par COR-05)
6. `AMBIGUOUS` : **PASS** (Non régressé, renforcé par COR-07)
7. `UNKNOWN` : **PASS** (Non régressé)
8. `STOP confirmation` : **PASS** (Non régressé)

Les 7 scénarios historiques `NOT RUN` demeurent classés `NOT RUN` conformément aux consignes.
