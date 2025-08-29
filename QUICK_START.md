# 🚀 Guida Rapida - ESP32 AI Voice Assistant

## ⚡ Setup in 5 Minuti

### 1. 📋 Prerequisiti
- **Hardware**: ESP32-S3 + Microfono I2S + Speaker I2S
- **Software**: Arduino IDE con supporto ESP32
- **Account**: Google Cloud Platform (gratuito)

### 2. 🔧 Installazione Arduino IDE

1. **Scarica Arduino IDE**: https://www.arduino.cc/en/software
2. **Aggiungi ESP32**:
   - File → Preferences
   - Additional Board Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
3. **Installa Board ESP32**:
   - Tools → Board → Boards Manager
   - Cerca "ESP32" → Installa

### 3. 📚 Librerie Richieste

Installa tramite Library Manager (Tools → Manage Libraries):
```
- ArduinoJson (>= 6.21.0)
- Adafruit VL53L0X
```

### 4. ☁️ Configurazione Google Cloud

#### A. Crea Progetto
1. Vai su: https://console.cloud.google.com/
2. **New Project** → Inserisci nome → Create

#### B. Abilita API
1. **APIs & Services** → **Library**
2. Cerca e abilita:
   - ✅ **Speech-to-Text API**
   - ✅ **Text-to-Speech API**
   - ✅ **Vertex AI API** (per Gemini)

#### C. Genera API Key
1. **APIs & Services** → **Credentials**
2. **Create Credentials** → **API Key**
3. **Copia la chiave** (inizia con `AIza...`)

### 5. ⚙️ Configurazione Progetto

#### A. Scarica Codice
```bash
git clone [URL_REPOSITORY]
cd ESP32-AI-Voice-Assistant
```

#### B. Configura Credenziali
1. **Copia template**:
   ```
   cp config_template.h config_private.h
   ```

2. **Modifica `config_private.h`**:
   ```cpp
   #define WIFI_SSID     "TuoWiFi"           // 📶 Nome WiFi
   #define WIFI_PASSWORD "TuaPassword"       // 🔐 Password WiFi  
   #define GCP_API_KEY   "AIza..."           // 🔑 Tua API Key Google
   ```

### 6. 🔌 Connessioni Hardware

#### Microfono INMP441 → ESP32-S3
```
VCC  → 3.3V
GND  → GND
SCK  → GPIO 4   (Clock)
WS   → GPIO 5   (Word Select)
SD   → GPIO 6   (Data)
```

#### Speaker MAX98357A → ESP32-S3
```
VIN  → 5V
GND  → GND
BCLK → GPIO 16  (Bit Clock)
LRC  → GPIO 17  (Left/Right Clock)
DIN  → GPIO 18  (Data Input)
```

#### Sensore VL53L0X → ESP32-S3 (Opzionale)
```
VIN  → 3.3V
GND  → GND
SDA  → GPIO 20
SCL  → GPIO 21
```

### 7. 📤 Caricamento Firmware

1. **Apri Arduino IDE**
2. **Apri file**: `assistente_ai_optimized/assistente_ai_optimized.ino`
3. **Seleziona Board**:
   - Tools → Board → ESP32 Arduino → **ESP32S3 Dev Module**
4. **Configura Board**:
   - CPU Frequency: **240MHz**
   - Flash Size: **16MB**
   - PSRAM: **OPI PSRAM**
   - Partition Scheme: **16M Flash (3MB APP/9.9MB FATFS)**
5. **Seleziona Porta**: Tools → Port → [Tua porta COM]
6. **Carica**: Ctrl+U o pulsante Upload ➡️

### 8. 🧪 Test Funzionamento

1. **Apri Serial Monitor**: Tools → Serial Monitor (115200 baud)
2. **Attendi connessione WiFi**:
   ```
   ✅ WiFi connesso: 192.168.1.100
   ✅ Pre-connessioni TLS stabilite
   🎯 Sistema pronto!
   ```
3. **Testa conversazione**:
   - **Automatico**: Avvicina mano al sensore (< 10cm)
   - **Manuale**: Premi INVIO nel Serial Monitor
4. **Parla quando vedi**: `🎙️ Registrazione...`
5. **Attendi risposta vocale**

## 🎯 Comandi di Test

### Esempi Vocali
- "Ciao, come stai?"
- "Che ore sono?"
- "Raccontami una barzelletta"
- "Spiegami l'intelligenza artificiale"

### Comandi Serial Monitor
- `INVIO` → Avvia conversazione
- `test` + INVIO → Test rapido sistema

## 🔧 Risoluzione Problemi

### ❌ "WiFi connection failed"
**Soluzione**: Verifica SSID e password in `config_private.h`

### ❌ "HTTPS connect failed"
**Soluzione**: 
- Verifica API Key Google Cloud
- Controlla che le API siano abilitate
- Verifica connessione internet

### ❌ "STT failed"
**Soluzione**:
- Controlla connessioni microfono
- Parla più chiaramente
- Verifica alimentazione

### ❌ "No audio output"
**Soluzione**:
- Controlla connessioni speaker
- Verifica alimentazione (usa alimentatore esterno 5V 2A)
- Controlla volume

### 🔄 Reset Completo
1. Disconnetti alimentazione
2. Tieni premuto BOOT
3. Riconnetti alimentazione
4. Rilascia BOOT
5. Ricarica firmware

## 📊 Monitoraggio Prestazioni

Il sistema mostra metriche in tempo reale:
```
📊 Tempi: STT=15s, Gemini=12s, TTS=4s
⚡ Round parallelo: 20s (63% più veloce!)
💾 Heap: 183KB liberi
💾 PSRAM: 8MB liberi
🌐 WiFi: -45dBm
```

## 🎛️ Personalizzazioni Rapide

### Cambia Voce
```cpp
// In config_private.h
#define TTS_VOICE_NAME "it-IT-Neural2-A"  // Femminile
#define TTS_VOICE_NAME "it-IT-Neural2-C"  // Maschile
```

### Regola Velocità
```cpp
#define TTS_SPEAKING_RATE 1.3  // Più veloce
#define TTS_SPEAKING_RATE 0.8  // Più lento
```

### Modifica Sensibilità Sensore
```cpp
#define TRIGGER_DISTANCE 15  // Distanza in cm
```

---

## 🆘 Supporto

- 🐛 **Bug**: Apri issue su GitHub
- 💬 **Domande**: Discussioni GitHub
- 📖 **Documentazione completa**: `README.md`

**🎉 Buon divertimento con il tuo assistente AI!**