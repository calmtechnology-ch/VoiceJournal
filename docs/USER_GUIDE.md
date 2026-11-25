# VoiceJournal v0.2.1 — Guide Utilisateur

Carnet de laboratoire vocal embarqué intelligent pour **M5Stack CoreS3**.

---

## 1. À quoi sert VoiceJournal ?

VoiceJournal permet aux ingénieurs et techniciens de dicter, consulter et analyser vocalement leurs observations de laboratoire directement sur un boîtier autonome M5Stack CoreS3, sans dépendre du cloud pour la compréhension des commandes locales.

### Points forts de la version v0.2.1 (Consolidation) :
- **Autonomie locale** : Reconnaissance locale instantanée (< 1 ms sur CoreS3) des commandes en français sans latence cloud.
- **Protection stricte (COR-11 & COR-12)** : Rejet strict des entrées invalides (> 1024 octets, > 96 tokens, UTF-8 invalide). Aucune suppression accidentelle grâce à une validation centralisée et une confirmation en deux étapes strictement bornée.
- **Grammaire renforcée (COR-13)** : Rejet des concepts isolés. Les commandes de lecture et de décompte ciblent obligatoirement le carnet ou les notes, refusant formellement les fichiers ou équipements.
- **Bi-fournisseur IA** : Choix tactile direct entre OpenAI Realtime (par défaut au démarrage) et Google Gemini Live pour la dictée vocale et les synthèses de laboratoire.
- **Conservation des notes** : Stockage non volatil NVS circulaire A/B (5 observations de 180 octets) préservé après coupure d'alimentation.
- **Bouton STOP d'urgence** : Arrêt prioritaire immédiat en tout état (écoute, analyse, lecture audio).
- **Diagnostic USB unifié (COR-14)** : Commandes série JSON sans effet de bord pour automatiser les tests ou connecter une interface PC.
- **Mode SHADOW consolidé (COR-15)** : Comparaison miroir non destructive entre moteur contextuel et moteur historique.

---

## 2. Interface tactile et boutons

L'écran LCD tactile 320 × 240 propose une interface sobre et claire :

```text
+-------------------------------------------------------------+
| VoiceJournal v0.2.1         [LAB]         [PRET / VERT]     | <- En-tête
+-------------------------------------------------------------+
|                                                             |
|  Notes : 3 / 5                [ IA: OPENAI ]  (o)           | <- Carte centrale
|  ---------------------------------------------------------  |
|  Derniere transcription :                                   |
|  "Note, le filtre passe-bas fonctionne a 10 kHz."           |
|                                                             |
|  ---------------------------------------------------------  |
|  Observation enregistree (3/5)                              | <- Message d'état
+-------------------------------------------------------------+
|      [   PARLER   ]             |          [ STOP ]         | <- Boutons d'action
+-------------------------------------------------------------+
```

### Éléments de l'écran principal :
1. **En-tête** :
   - Titre applicatif : `VoiceJournal v0.2.1`.
   - Bouton discret `[LAB]` : accès direct au Parsing Lab de diagnostic.
   - Badge d'état : `PRET`, `ECOUTE`, `TRANSCRIPTION`, `ANALYSE IA`, `REPONSE AUDIO`, `ARRET`, `ERREUR`.
2. **Carte centrale** :
   - Compteur de notes actives : `Notes : X / 5`.
   - Bouton `[ IA: OPENAI ]` ou `[ IA: GEMINI ]` : appuyer dessus bascule instantanément de fournisseur IA (anti-rebond de 350 ms).
   - Pastille de connectivité (verte = connecté, orange = déconnecté).
   - Affichage de la dernière phrase dictée ou réponse TTS.
   - Message d'état explicite en bas de carte.
3. **Zone d'action inférieure** :
   - Bouton `[ PARLER ]` (bleu) / `[ TERMINER ]` (rouge) : PTT tactile à deux clics.
   - Bouton `[ STOP ]` (rouge) : Annulation et interruption prioritaire immédiate.

---

## 3. Commandes vocales disponibles (v0.2.1)

VoiceJournal v0.2.1 propose un ensemble officiel de commandes fiables :

| Fonction | Exemples d'expressions | Action réalisée |
|---|---|---|
| **Ajouter une observation** | *« Note, le prototype fonctionne »*<br>*« Mémorise que Gemini se déconnecte »*<br>*« Enregistre la tension mesurée à 3,3 volts »* | Enregistre l'observation dans le carnet circulaire NVS et confirme vocalement. |
| **Relire la dernière note** | *« Lis la dernière note »*<br>*« Relis mon observation »*<br>*« Lis ma note »* | Restitue vocalement le texte exact de la dernière observation enregistrée. |
| **Compter les notes** | *« Combien de notes ? »*<br>*« Compte les observations »*<br>*« Nombre de notes »* | Énonce le nombre exact d'observations actuellement conservées. |
| **Analyser le carnet** | *« Fais le bilan »*<br>*« Résume les observations »*<br>*« Analyse les notes »* | Envoie les observations au LLM actif (OpenAI/Gemini) et prononce la synthèse de laboratoire. |
| **Effacer le carnet** | *« Efface toutes les notes »*<br>*« Supprime toutes les observations »*<br>*« Vide le carnet »* | Arme la demande d'effacement et invite vocalement à confirmer sous 30 secondes. |
| **Confirmer la suppression** | *« Confirmer la suppression »*<br>*« Oui, je confirme la suppression »* | Efface définitivement toutes les notes du carnet NVS. |
| **Annuler la suppression** | *« Annuler »*<br>*« Ne supprime pas les notes »*<br>*« Laisse tomber »* | Désarme immédiatement l'attente sans rien effacer. |
| **Aide vocale** | *« Quelles sont les commandes ? »*<br>*« Aide »* | Présente vocalement la liste des commandes opérationnelles disponibles. |

> [!IMPORTANT]
> Les intentions expérimentales (*« Je dois tester Gemini demain »*, *« Retrouve les bugs de VoiceKit »*, *« Fais le bilan du projet X »*) sont reconnues mais répondent :  
> *« Cette fonction sera disponible dans une prochaine version. »*

---

## 4. Sécurité des suppressions (2 étapes)

Pour garantir qu'aucune observation de laboratoire ne soit perdue par erreur :
1. Une commande d'effacement doit explicitement cibler **le carnet** ou **l'ensemble des notes** (*« Efface toutes les notes »*).
2. Une commande ciblant un fichier (*« Supprime le fichier de configuration »*) ou une note unitaire (*« Efface la dernière note »*) est formellement **refusée**.
3. Lors de la demande, le CoreS3 demande : *« Confirmez-vous la suppression des notes ? Dites confirmer la suppression. »*
4. Si vous dites *« Je ne confirme pas la suppression »*, *« Non »*, *« Annuler »*, ou appuyez sur **STOP**, la confirmation est désarmée et le carnet reste intact.
5. Si aucune confirmation exacte n'est reçue dans un délai de **30 secondes**, la demande expire automatiquement.

---

## 5. Accès au Parsing Lab (Diagnostic embarqué)

En touchant le bouton discret `[LAB]` en haut à droite :
- **Vue 1/3 (Résultat)** : Affiche la transcription brute, l'intention NLU, le statut clair (`AUTORISEE`, `FONCTION INDISPONIBLE`, `COMMANDE INCONNUE`, `COMMANDE AMBIGUE`, `REFUS DE SECURITE`), le score d'acceptation et le temps de parsing (en µs).
- **Vue 2/3 (Contexte)** : Détaille l'action, l'objet, l'entité extraite, la date relative et la portée des négations.
- **Vue 3/3 (Comparaison)** : Mode de comparaison miroir entre le moteur historique et le moteur contextuel.
- Toucher `[RETOUR]` pour revenir à l'écran principal. Le bouton **STOP** reste opérationnel en permanence.

---

## 6. Diagnostic série USB (PC)

Connectez le CoreS3 à votre PC en USB (115 200 bauds) pour envoyer des commandes de test sans toucher aux notes réelles :
```text
PARSE Je dois tester Gemini demain
PARSE_SHADOW Lis la derniere note
PARSER_STATUS
PARSER_HELP
```
Les réponses sont émises sous forme de JSON structuré immédiatement exploitable par un script ou une application PC. Voir [`SERIAL_PARSER_PROTOCOL.md`](./SERIAL_PARSER_PROTOCOL.md) pour la spécification complète.

---

## 7. Dépannage matériel

- **L'écran reste noir ou bloqué au démarrage** :
  Vérifiez que le câble USB-C est bien connecté à un port fournissant au moins 1 A. Vérifiez dans `include/secrets.h` que le SSID Wi-Fi et les clés API sont correctement renseignés.
- **Message « Erreur Wi-Fi »** :
  L'ESP32-S3 ne supporte que la bande Wi-Fi 2,4 GHz. Assurez-vous que votre point d'accès n'est pas configuré uniquement en 5 GHz.
- **La dictée vocale ne répond pas** :
  Assurez-vous d'avoir cliqué une première fois sur `[ PARLER ]` (le bouton devient rouge `[ TERMINER ]`), dictez votre phrase distinctement à 15-20 cm des microphones du CoreS3, puis cliquez sur `[ TERMINER ]`.
- **Interruption d'urgence** :
  À tout moment, appuyez sur le bouton rouge `[ STOP ]` à l'écran : la lecture audio, l'enregistrement ou l'attente de confirmation s'interrompt instantanément.

---

## 8. Documentation de référence

- Architecture logicielle : [`ARCHITECTURE.md`](./ARCHITECTURE.md)
- Guide du moteur contextuel C++17 : [`PARSER_GUIDE.md`](./PARSER_GUIDE.md)
- Protocole série USB JSON : [`SERIAL_PARSER_PROTOCOL.md`](./SERIAL_PARSER_PROTOCOL.md)
- Cahier de validation matérielle : [`HARDWARE_VALIDATION.md`](./HARDWARE_VALIDATION.md)
- Vision v0.3.0 : [`ROADMAP.md`](./ROADMAP.md)
- Rapport de consolidation v0.2.1 : [`CONSOLIDATION_REPORT_v0.2.1.md`](./CONSOLIDATION_REPORT_v0.2.1.md)
