#pragma once

// =============================================================================
// VoiceJournal v0.1.1 — Fichier modele d'identifiants
// =============================================================================
// Copiez ce fichier vers include/secrets.h et renseignez vos vrais identifiants.
// OpenAI est le fournisseur par defaut ; renseignez les deux cles pour basculer
// dynamiquement entre OpenAI et Gemini depuis l'ecran tactile.
// Ne publiez JAMAIS include/secrets.h dans Git.
// =============================================================================

// Reseau Wi-Fi 2.4 GHz uniquement (l'ESP32-S3 ne supporte pas le 5 GHz)
#define WIFI_SSID     "VOTRE_RESEAU_WIFI_2_4GHZ"
#define WIFI_PASSWORD "VOTRE_MOT_DE_PASSE_WIFI"

// Cle API OpenAI Realtime — fournisseur par defaut (https://platform.openai.com/api-keys)
#define OPENAI_KEY    "VOTRE_CLE_API_OPENAI"

// Cle API Google Gemini Live — fournisseur alternatif (https://aistudio.google.com/)
#define GEMINI_KEY    "VOTRE_CLE_API_GEMINI"
