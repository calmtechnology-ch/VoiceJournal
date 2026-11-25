# Spécification du format de stockage — VoiceJournal v0.3.0

## 1. Vue d'ensemble

VoiceJournal v0.3.0 introduit une mémoire locale persistante sur **LittleFS**, remplaçant l'ancien tampon circulaire NVS limité à 5 chaînes de 180 octets.

Principes directeurs :
- **Modèle Append-Only** : chaque ajout ou modification est enregistré séquentiellement à la fin d'un journal binaire (`/journal.log`), évitant les réécritures complètes de blocs Flash à chaque opération.
- **Indexation dynamique en RAM** : un index compact (~32 octets par entrée, soit 32 Ko pour 1 000 entrées) est reconstruit en mémoire vive au démarrage en scannant le fichier journal, permettant des filtres et recherches sans latence d'E/S Flash.
- **Résilience aux coupures d'alimentation** : chaque enregistrement est protégé par des balises magiques, une validation CRC32 et un délimiteur de fin de ligne. Si une coupure survient en cours d'écriture, l'enregistrement tronqué est détecté et le fichier est automatiquement ramené au dernier état sain vérifié.
- **Compactage contrôlé et récupérable** : lorsque le journal accumule des suppressions ou des modifications, une procédure de compactage écrit les enregistrements actifs dans `/journal.tmp`, valide l'intégrité globale, puis remplace de manière atomique `/journal.log`.

---

## 2. Structure physique du journal (`/journal.log`)

Le fichier commence par un en-tête fixe de 8 octets, suivi d'une suite séquentielle d'enregistrements binaires.

### En-tête de fichier (8 octets)
```text
+---+---+---+---+---+---+---+---+
| V | J | 0 | 3 | L | O | G | 1 |
+---+---+---+---+---+---+---+---+
 0   1   2   3   4   5   6   7
```
- Constante : `"VJ03LOG1"`
- Tout fichier ne débutant pas par cet en-tête est considéré comme invalide ou étranger et refusé.

---

### Structure d'un enregistrement

Chaque enregistrement se compose d'un préfixe binaire, d'une charge utile JSON UTF-8, d'un contrôle de redondance cyclique (CRC32) et d'un délimiteur de fin de ligne :

```text
+-------+-------+------+--------------------+--------------------+----------+----+
| MAGIC | MAGIC | TYPE |     LONGUEUR       |     PAYLOAD        |  CRC32   | \n |
|  0xAA |  0x55 | uint8| uint32_t (4 bytes) |  (longueur octets) | uint32_t |0x0A|
+-------+-------+------+--------------------+--------------------+----------+----+
    1       1      1             4                 N             4        1
```

| Champ | Type / Taille | Description |
|---|---|---|
| `MAGIC0` | `uint8_t` (0xAA) | Octet de synchronisation 1 |
| `MAGIC1` | `uint8_t` (0x55) | Octet de synchronisation 2 |
| `TYPE` | `uint8_t` | Type d'enregistrement (1 = ENTRY, 2 = DELETE, 10 = PROJECT) |
| `LENGTH` | `uint32_t` (Little-Endian) | Longueur en octets du payload JSON |
| `PAYLOAD` | Chaîne UTF-8 (`LENGTH` octets) | Représentation JSON compacte de l'objet |
| `CRC32` | `uint32_t` (Little-Endian) | CRC-32 (polynôme standard IEEE 802.3 `0xEDB88320`) calculé sur `PAYLOAD` |
| `DELIMITER` | `uint8_t` (0x0A / `\n`) | Délimiteur de fin de ligne facilitant le diagnostic |

---

## 3. Types d'enregistrements et schémas JSON

### Type 1 : `TYPE_ENTRY_DATA` (Ajout ou mise à jour d'entrée)

Exemple de payload :
```json
{
  "id": 42,
  "type": 1,
  "proj": 3,
  "cat": 1795617000,
  "uat": 1795620600,
  "st": 1,
  "dat": 1795703400,
  "text": "Gemini se déconnecte après quelques secondes.",
  "ent": ["Gemini", "CoreS3"],
  "tags": ["connexion"]
}
```

Dictionnaire des clés compactes :
- `id` (`uint32_t`) : Identifiant unique persistant (croissant, 1..N).
- `type` (`int`) :
  - `0` : `NOTE`
  - `1` : `PROBLEM`
  - `2` : `TASK`
  - `3` : `IDEA`
  - `4` : `DECISION`
  - `5` : `RESULT`
- `proj` (`uint32_t`) : ID du projet associé (`0` si aucun).
- `cat` (`int64_t`) : Horodatage de création UTC en secondes epoch (`0` si inconnu).
- `uat` (`int64_t`) : Horodatage de modification UTC (`0` si aucune).
- `st` (`int`) : Statut courant :
  - `0` : `NONE`
  - `1` : `OPEN`
  - `2` : `IN_PROGRESS`
  - `3` : `COMPLETED`
  - `4` : `RESOLVED`
  - `5` : `CANCELLED`
- `dat` (`int64_t`) : Date d'échéance UTC (`0` si aucune).
- `text` (`string`) : Texte original de la transcription (max 1 024 octets UTF-8).
- `ent` (`array<string>`) : Entités nommées reconnues (max 4).
- `tags` (`array<string>`) : Mots-clés (max 4).

### Type 2 : `TYPE_ENTRY_DELETE` (Suppression logique d'une entrée)

Exemple de payload :
```json
{"id":42}
```
Indique à l'index en RAM que l'entrée `id` est marquée supprimée (`isDeleted = true`).

### Type 10 : `TYPE_PROJECT_DATA` (Définition d'un projet)

Exemple de payload :
```json
{
  "id": 3,
  "name": "VoiceKit",
  "cat": 1795600000,
  "st": 0,
  "alias": ["vk", "voice-kit"]
}
```
- `id` (`uint32_t`) : ID unique du projet.
- `name` (`string`) : Nom officiel du projet.
- `cat` (`int64_t`) : Date de création UTC.
- `st` (`int`) : `0` = `ACTIVE`, `1` = `ARCHIVED`.
- `alias` (`array<string>`) : Alias phonétiques ou raccourcis reconnus par le parseur.

---

## 4. Indexation en RAM (`IndexEntry`)

Lors de `JournalRepository::begin()`, le fichier est parcouru séquentiellement. Chaque enregistrement valide peuple l'index mémoire :

```cpp
struct IndexEntry {
    uint32_t id = 0;             // 4 octets
    uint32_t fileOffset = 0;     // 4 octets (position dans journal.log)
    uint32_t payloadLength = 0;  // 4 octets
    EntryType type;              // 1 octet
    EntryStatus status;          // 1 octet
    uint32_t projectId = 0;      // 4 octets
    int64_t createdAt = 0;       // 8 octets
    int64_t dueAt = 0;           // 8 octets
    bool isDeleted = false;      // 1 octet
};
```
Taille totale avec alignement : **32 octets** par entrée.
Pour 1 000 entrées, l'index complet n'occupe que **32 Ko de RAM**, garantissant un fonctionnement fluide sans impacter les buffers audio de VoiceKit.

---

## 5. Algorithme de récupération après coupure d'alimentation

1. Ouverture du fichier `/journal.log` en mode `r+b`.
2. Vérification des 8 octets d'en-tête `FILE_HEADER`. Si absent ou corrompu, le fichier est protégé et un diagnostic est levé (jamais de reformatage silencieux).
3. Lecture séquentielle de chaque bloc :
   - Vérification des octets magiques `0xAA`, `0x55`.
   - Lecture de la longueur `L`.
   - Vérification que `position_courante + L + 4 (crc) + 1 (\n) <= taille_du_fichier`. Si non, coupure d'alimentation détectée en cours d'écriture.
   - Lecture du payload et calcul du CRC32.
   - Si le CRC32 calculé correspond au CRC32 stocké et que le délimiteur `\n` est présent, la position `lastValidPos` est avancée.
4. Si la boucle se termine alors que `lastValidPos < taille_du_fichier`, le fichier est tronqué exactement à `lastValidPos` via `truncate(lastValidPos)`. L'intégrité du journal est ainsi parfaitement restaurée sans perte des données antérieures.
