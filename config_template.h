/*
 * TEMPLATE DI CONFIGURAZIONE - ESP32 AI Voice Assistant
 * 
 * ISTRUZIONI:
 * 1. Copia questo file come "config_private.h"
 * 2. Inserisci le tue credenziali reali
 * 3. Il file config_private.h è escluso da Git per sicurezza
 * 
 * NOTA: Non modificare mai questo template con credenziali reali!
 */

#ifndef CONFIG_TEMPLATE_H
#define CONFIG_TEMPLATE_H

// === CONFIGURAZIONE WIFI ===
#define WIFI_SSID     "IL_TUO_WIFI_SSID"        // Nome della tua rete WiFi
#define WIFI_PASSWORD "LA_TUA_WIFI_PASSWORD"    // Password della tua rete WiFi

// === CONFIGURAZIONE GOOGLE CLOUD ===
// Ottieni la tua API key da: https://console.cloud.google.com/apis/credentials
#define GCP_API_KEY   "LA_TUA_API_KEY_GOOGLE_CLOUD"

// === CONFIGURAZIONE GEMINI ===
#define GEMINI_MODEL  "gemini-1.5-flash-latest"  // Modello Gemini da utilizzare

// === CONFIGURAZIONI AVANZATE (opzionali) ===

// Timeout connessioni (millisecondi)
#define GEMINI_TIMEOUT_MS    30000   // Timeout per Gemini API
#define TTS_TIMEOUT_MS       40000   // Timeout per Google TTS
#define STT_TIMEOUT_MS       25000   // Timeout per Google STT

// Configurazione audio
#define RECORDING_TIME_MS    5000    // Durata registrazione audio (ms)
#define SAMPLE_RATE          16000   // Frequenza campionamento audio

// Configurazione sensore ToF
#define TRIGGER_DISTANCE     10      // Distanza trigger in cm
#define COOLDOWN_TIME_MS     2000    // Tempo attesa tra conversazioni

// Configurazione TTS
#define TTS_VOICE_NAME       "it-IT-Neural2-C"  // Voce italiana (maschile)
// Alternative voci:
// "it-IT-Neural2-A" - Voce femminile
// "it-IT-Neural2-C" - Voce maschile

#define TTS_SPEAKING_RATE    1.1     // Velocità parlato (0.25 - 4.0)
#define TTS_VOLUME_GAIN      2.0     // Guadagno volume (-96.0 - 16.0 dB)

// Configurazione Gemini
#define GEMINI_MAX_TOKENS    128     // Massimo numero di token output
#define GEMINI_TEMPERATURE   0.7     // Creatività risposte (0.0 - 1.0)

// Debug level (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug)
#define DEBUG_LEVEL          3

#endif // CONFIG_TEMPLATE_H

/*
 * ESEMPIO DI UTILIZZO:
 * 
 * 1. Crea account Google Cloud: https://console.cloud.google.com/
 * 2. Abilita le API:
 *    - Speech-to-Text API
 *    - Text-to-Speech API  
 *    - Vertex AI API (per Gemini)
 * 3. Crea API Key in "APIs & Services" > "Credentials"
 * 4. Copia questo file come "config_private.h"
 * 5. Sostituisci i placeholder con i tuoi dati reali
 * 6. Compila e carica il firmware
 * 
 * SICUREZZA:
 * - Non condividere mai il file config_private.h
 * - Non committare credenziali nel repository
 * - Usa sempre questo template per la distribuzione pubblica
 */