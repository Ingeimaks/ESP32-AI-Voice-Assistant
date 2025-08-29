# 🤖 ESP32 AI Voice Assistant

**Un assistente vocale AI completo basato su ESP32-S3 con Google Cloud Services**

[![ESP32](https://img.shields.io/badge/ESP32-S3-red.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Google Cloud](https://img.shields.io/badge/Google%20Cloud-AI-blue.svg)](https://cloud.google.com/)
[![Arduino](https://img.shields.io/badge/Arduino-IDE-green.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 🎯 Caratteristiche Principali

- **🎙️ Speech-to-Text**: Riconoscimento vocale in tempo reale
- **🤖 AI Gemini**: Risposte intelligenti e contestuali
- **🔊 Text-to-Speech**: Sintesi vocale naturale italiana
- **⚡ Esecuzione Parallela**: Multi-core ottimizzato (63% più veloce)
- **🌐 Pre-connessioni TLS**: Latenza ridotta
- **🛡️ Sistema Anti-crash**: Watchdog ottimizzato e retry automatici
- **📏 Gestione Testi Lunghi**: Divisione intelligente per TTS
- **🎯 Sensore ToF**: Attivazione automatica per presenza

## 🚀 Prestazioni

| Componente | Tempo Originale | Tempo Ottimizzato | Miglioramento |
|------------|----------------|-------------------|---------------|
| **STT** | ~26s | ~15s | **42% più veloce** |
| **Gemini** | ~20s | ~12s | **40% più veloce** |
| **TTS** | ~8s | ~4s | **50% più veloce** |
| **Totale** | ~54s | **~20s** | **🚀 63% più veloce** |

## 🛠️ Hardware Richiesto

### Componenti Principali
- **ESP32-S3** (con PSRAM)
- **Microfono I2S**: INMP441
- **Amplificatore I2S**: MAX98357A
- **Speaker**: 4-8Ω, 3W
- **Sensore ToF**: VL53L0X (opzionale)

### Schema Connessioni

#### Microfono INMP441 (I2S_NUM_1)
```
ESP32-S3    INMP441
--------    -------
GPIO 42  -> SCK (Clock)
GPIO 2   -> WS (Word Select)
GPIO 41  -> SD (Serial Data)
3.3V     -> VDD
GND      -> GND
```

#### Amplificatore MAX98357A (I2S_NUM_0)
```
ESP32-S3    MAX98357A
--------    ---------
GPIO 12  -> BCLK (Bit Clock)
GPIO 13  -> LRC (Left/Right Clock)
GPIO 14  -> DIN (Data Input)
5V       -> VIN
GND      -> GND
```

#### Sensore VL53L0X (I2C)
```
ESP32-S3    VL53L0X
--------    -------
GPIO 21  -> SDA
GPIO 22  -> SCL
3.3V     -> VIN
GND      -> GND
```

## ⚙️ Configurazione Software

### 1. Installazione Arduino IDE
1. Scarica [Arduino IDE](https://www.arduino.cc/en/software)
2. Aggiungi ESP32 board manager:
   - File → Preferences
   - Additional Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Tools → Board → Boards Manager → Cerca "ESP32" → Installa

### 2. Librerie Richieste
Installa tramite Library Manager:
```
- ArduinoJson (>= 6.21.0)
- WiFiClientSecure
- VL53L0X (Pololu)
```

### 3. Configurazione Google Cloud

#### Crea Progetto Google Cloud
1. Vai su [Google Cloud Console](https://console.cloud.google.com/)
2. Crea nuovo progetto
3. Abilita le API:
   - Speech-to-Text API
   - Text-to-Speech API
   - Gemini API (Vertex AI)

#### Genera API Key
1. APIs & Services → Credentials
2. Create Credentials → API Key
3. Copia la chiave generata

### 4. Configurazione Codice

Modifica `assistente_ai_optimized.ino`:

```cpp
// === CONFIGURAZIONE WIFI ===
#define WIFI_SSID     "TUO_WIFI_SSID"
#define WIFI_PASSWORD "TUA_WIFI_PASSWORD"

// === CONFIGURAZIONE GOOGLE CLOUD ===
#define GCP_API_KEY   "TUA_API_KEY_GOOGLE_CLOUD"
#define GEMINI_MODEL  "gemini-1.5-flash-latest"
```

## 📁 Struttura Progetto

```
ESP32-AI-Voice-Assistant/
├── README.md                         # Documentazione principale
├── QUICK_START.md                    # Guida rapida setup
├── LICENSE                           # Licenza MIT
├── config_template.h                 # Template configurazione
├── .gitignore                        # File da escludere da Git
├── assistente_ai_optimized/          # Codice principale
│   └── assistente_ai_optimized.ino   # Firmware ottimizzato
└── hardware/                         # Risorse hardware
    └── hw.png                        # Schema connessioni
```

## 🚀 Quick Start

### 1. Carica il Firmware
1. Copia `config_template.h` in `config_private.h`
2. Configura WiFi e API keys in `config_private.h`
3. Apri `assistente_ai_optimized/assistente_ai_optimized.ino`
4. Seleziona board: "ESP32S3 Dev Module"
5. Configura:
   - CPU Frequency: 240MHz
   - Flash Size: 16MB
   - PSRAM: OPI PSRAM
   - Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
6. Carica il codice

### 2. Test Funzionamento
1. Apri Serial Monitor (115200 baud)
2. Attendi connessione WiFi
3. Avvicina la mano al sensore ToF (< 10cm) o premi INVIO
4. Parla per 5 secondi quando vedi "🎙️ Registrazione..."
5. Attendi la risposta vocale

## 🎛️ Modalità d'Uso

### Trigger Automatico
- **Sensore ToF**: Avvicina la mano a < 10cm
- **Cooldown**: 2 secondi tra conversazioni

### Trigger Manuale
- **Serial Monitor**: Premi INVIO
- **Comando test**: Digita "test" + INVIO

### Comandi Vocali Esempio
- "Ciao, come stai?"
- "Che tempo fa oggi?"
- "Raccontami una barzelletta"
- "Spiegami la fisica quantistica"

## 🔧 Personalizzazione

### Cambia Voce TTS
```cpp
d["voice"]["name"] = "it-IT-Neural2-A";  // Voce femminile
d["voice"]["name"] = "it-IT-Neural2-C";  // Voce maschile (default)
```

### Regola Velocità Parlato
```cpp
d["audioConfig"]["speakingRate"] = 1.3;  // Più veloce
d["audioConfig"]["speakingRate"] = 0.8;  // Più lento
```

### Modifica Sensibilità ToF
```cpp
const int TRIGGER_DISTANCE = 15;  // Distanza in cm
```

## 📊 Monitoraggio

Il sistema fornisce metriche dettagliate:

```
📊 Tempi: STT=15234ms, Gemini=12456ms, TTS=4123ms
⚡ Round parallelo completato: 20567ms
💾 Heap libero: 183524 bytes
💾 PSRAM libero: 8322780 bytes
🌐 WiFi: -45dBm
```

## 🛠️ Troubleshooting

### Problemi Comuni

#### "❌ HTTPS connect failed"
- Verifica connessione WiFi
- Controlla API key Google Cloud
- Verifica che le API siano abilitate

#### "❌ STT fallita"
- Controlla connessioni microfono
- Verifica che il microfono funzioni
- Parla più chiaramente e vicino al microfono

#### "❌ TTS fallita"
- Controlla connessioni speaker
- Verifica alimentazione (potrebbe servire alimentatore esterno)
- Controlla volume sistema

#### Riavvii Continui
- Usa alimentatore esterno 5V 2A
- Verifica connessioni I2S
- Controlla memoria PSRAM

### Debug Avanzato

Abilita debug dettagliato:
```cpp
#define DEBUG_LEVEL 3  // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug
```

## 🤝 Contribuire

1. Fork del progetto
2. Crea feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit delle modifiche (`git commit -m 'Add AmazingFeature'`)
4. Push al branch (`git push origin feature/AmazingFeature`)
5. Apri Pull Request

## 📄 Licenza

Distribuito sotto licenza MIT. Vedi `LICENSE` per maggiori informazioni.

## 🙏 Ringraziamenti

- [Espressif](https://www.espressif.com/) per ESP32-S3
- [Google Cloud](https://cloud.google.com/) per i servizi AI
- [Arduino](https://www.arduino.cc/) per l'IDE
- Community ESP32 per supporto e librerie

## 👨‍💻 Autore

**Ingeimaks** - Creatore e sviluppatore principale

✍️ **Segui il canale Ingeimaks per nuovi progetti ESP32 e altri contenuti di elettronica, Arduino e stampa 3D!**

🎥 **YouTube**: https://www.youtube.com/@Ingeimaks

## 📞 Supporto

- 🐛 **Bug Reports**: Apri un issue su GitHub
- 💡 **Feature Requests**: Discussioni GitHub
- 🎥 **Tutorial e Guide**: Canale YouTube Ingeimaks
- 📧 **Contatto**: Tramite YouTube o GitHub

---

**⭐ Se questo progetto ti è utile, lascia una stella su GitHub!**

**🔗 Condividi con la community maker italiana!**

**🎥 Iscriviti al canale Ingeimaks per altri progetti fantastici!**