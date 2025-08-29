/*
 * CONFIGURATION TEMPLATE - ESP32 AI Voice Assistant
 * 
 * INSTRUCTIONS:
 * 1. Copy this file as "config_private.h"
 * 2. Insert your real credentials
 * 3. The config_private.h file is excluded from Git for security
 * 
 * NOTE: Never modify this template with real credentials!
 */

#ifndef CONFIG_TEMPLATE_H
#define CONFIG_TEMPLATE_H

// === WIFI CONFIGURATION ===
#define WIFI_SSID     "YOUR_WIFI_SSID"        // Your WiFi network name
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"    // Your WiFi network password

// === GOOGLE CLOUD CONFIGURATION ===
// Get your API key from: https://console.cloud.google.com/apis/credentials
#define GCP_API_KEY   "YOUR_GOOGLE_CLOUD_API_KEY"

// === GEMINI CONFIGURATION ===
#define GEMINI_MODEL  "gemini-1.5-flash-latest"  // Gemini model to use

// === ADVANCED CONFIGURATIONS (optional) ===

// Connection timeouts (milliseconds)
#define GEMINI_TIMEOUT_MS    30000   // Timeout for Gemini API
#define TTS_TIMEOUT_MS       40000   // Timeout for Google TTS
#define STT_TIMEOUT_MS       25000   // Timeout for Google STT

// Audio configuration
#define RECORDING_TIME_MS    5000    // Audio recording duration (ms)
#define SAMPLE_RATE          16000   // Audio sampling frequency

// ToF sensor configuration
#define TRIGGER_DISTANCE     10      // Trigger distance in cm
#define COOLDOWN_TIME_MS     2000    // Wait time between conversations

// TTS configuration
#define TTS_VOICE_NAME       "en-US-Neural2-D"  // English voice (male)
// Alternative voices:
// "en-US-Neural2-F" - Female voice
// "en-US-Neural2-D" - Male voice

#define TTS_SPEAKING_RATE    1.1     // Speaking speed (0.25 - 4.0)
#define TTS_VOLUME_GAIN      2.0     // Volume gain (-96.0 - 16.0 dB)

// Gemini configuration
#define GEMINI_MAX_TOKENS    128     // Maximum number of output tokens
#define GEMINI_TEMPERATURE   0.7     // Response creativity (0.0 - 1.0)

// Debug level (0=None, 1=Error, 2=Warn, 3=Info, 4=Debug)
#define DEBUG_LEVEL          3

#endif // CONFIG_TEMPLATE_H

/*
 * USAGE EXAMPLE:
 * 
 * 1. Create Google Cloud account: https://console.cloud.google.com/
 * 2. Enable APIs:
 *    - Speech-to-Text API
 *    - Text-to-Speech API  
 *    - Vertex AI API (for Gemini)
 * 3. Create API Key in "APIs & Services" > "Credentials"
 * 4. Copy this file as "config_private.h"
 * 5. Replace placeholders with your real data
 * 6. Compile and upload firmware
 * 
 * SECURITY:
 * - Never share the config_private.h file
 * - Never commit credentials to the repository
 * - Always use this template for public distribution
 */