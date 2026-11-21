# Changelog — VoiceJournal

Toutes les évolutions notables de **VoiceJournal** sont consignées dans ce fichier.

## [0.1.0] - 2026-11-21

### Ajouté
- Projet PlatformIO autonome `VoiceJournal_CoreS3` ciblant **M5Stack CoreS3 (ESP32-S3)** en C++17.
- Intégration de la façade publique `<VoiceKit.h>` (**VoiceKit v0.6.7.2**) avec **Google Gemini Live** en mode non conversationnel (`enableConversation = false`).
- `JournalStorage` : tampon circulaire persistant en NVS (`Preferences`, namespace `"vjournal"`, 5 observations max, 180 octets UTF-8 max, écriture charge utile avant métadonnées, protection UTF-8).
- `CommandRouter` : routeur de commandes vocales déterministe (`NOTE`, `READ_LAST_NOTE`, `COUNT_NOTES`, `ANALYZE_NOTES`, `REQUEST_CLEAR` avec confirmation explicite temporisée et annulation).
- `VoiceController` : machine à états applicative explicite (`JournalState`), corrélation stricte des tours STT par `turnId`, file TTS courte, analyse Gemini bornée avec séparation données/instructions, `STOP` prioritaire non destructif et watchdogs non bloquants.
- `JournalUI` : interface tactile M5GFX paysage 320 × 240 (sans LVGL 9) avec anti-rebond tactile, affichage hors ligne du compteur de notes et boutons `PARLER / TERMINER` + `STOP`.
- Diagnostics série structurés à 115 200 bauds (`[VJ][...]`) et diagnostic mémoire périodique configurable.
- Suite de tests unitaires natifs C++17 (`tests/test_main.cpp`, `tests/TEST_PLAN.md`) couvrant `JournalStorage`, `CommandRouter` et `VoiceController`.
- Documentation complète : `README.md`, `docs/ARCHITECTURE.md`, `docs/HARDWARE_VALIDATION.md` (grille `VJ-01` à `VJ-18`).
