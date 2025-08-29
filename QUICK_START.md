# 🚀 Quick Start Guide - ESP32 AI Voice Assistant

## ⚡ Setup in 5 Minutes

### 1. 📋 Prerequisites
- **Hardware**: ESP32-S3 + I2S Microphone + I2S Speaker
- **Software**: Arduino IDE with ESP32 support
- **Account**: Google Cloud Platform (free tier available)

### 2. 🔧 Arduino IDE Installation

1. **Download Arduino IDE**: https://www.arduino.cc/en/software
2. **Add ESP32 Support**:
   - File → Preferences
   - Additional Board Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
3. **Install ESP32 Board**:
   - Tools → Board → Boards Manager
   - Search "ESP32" → Install

### 3. 📚 Required Libraries

Install via Library Manager (Tools → Manage Libraries):
```
- ArduinoJson (>= 6.21.0)
- Adafruit VL53L0X
```

### 4. ☁️ Google Cloud Configuration

#### A. Create Project
1. Go to: https://console.cloud.google.com/
2. **New Project** → Enter name → Create

#### B. Enable APIs
1. **APIs & Services** → **Library**
2. Search and enable:
   - ✅ **Speech-to-Text API**
   - ✅ **Text-to-Speech API**
   - ✅ **Vertex AI API** (for Gemini)

#### C. Generate API Key
1. **APIs & Services** → **Credentials**
2. **Create Credentials** → **API Key**
3. **Copy the key** (starts with `AIza...`)

### 5. ⚙️ Project Configuration

#### A. Download Project
```bash
git clone https://github.com/Ingeimaks/ESP32-AI-Voice-Assistant.git
cd ESP32-AI-Voice-Assistant
```

#### B. Configure Credentials
1. **Copy template**: `config_template.h` → `config_private.h`
2. **Edit** `config_private.h`:

```cpp
// WiFi Configuration
#define WIFI_SSID     "YourWiFi"           // 📶 WiFi Name
#define WIFI_PASSWORD "YourPassword"       // 🔐 WiFi Password

// Google Cloud Configuration
#define GCP_API_KEY   "AIza..."            // 🔑 Your API Key
#define GEMINI_MODEL  "gemini-1.5-flash"   // 🤖 AI Model
```

### 6. 🔌 Hardware Wiring

#### INMP441 Microphone (I2S_NUM_1)
```
ESP32-S3    INMP441
--------    -------
GPIO 42  -> SCK
GPIO 2   -> WS
GPIO 41  -> SD
3.3V     -> VDD
GND      -> GND
```

#### MAX98357A Amplifier (I2S_NUM_0)
```
ESP32-S3    MAX98357A
--------    ---------
GPIO 12  -> BCLK
GPIO 13  -> LRC
GPIO 14  -> DIN
5V       -> VIN
GND      -> GND
```

#### VL53L0X ToF Sensor (Optional)
```
ESP32-S3    VL53L0X
--------    -------
GPIO 21  -> SDA
GPIO 22  -> SCL
3.3V     -> VIN
GND      -> GND
```

### 7. 📤 Upload Code

1. **Open**: `assistente_ai_optimized/assistente_ai_optimized.ino`
2. **Select Board**: Tools → Board → ESP32S3 Dev Module
3. **Configure**:
   - **CPU Frequency**: 240MHz
   - **Flash Size**: 16MB
   - **PSRAM**: OPI PSRAM
   - **Partition**: Default 4MB with spiffs
4. **Upload** 🚀

### 8. 🎯 First Test

1. **Open Serial Monitor** (115200 baud)
2. **Wait for WiFi connection** ✅
3. **Trigger conversation**:
   - Move hand near ToF sensor (< 10cm)
   - OR press ENTER in Serial Monitor
4. **Speak for 5 seconds** when you see "🎙️ Recording..."
5. **Listen to AI response** 🔊

## 🎛️ Usage Modes

### Automatic Trigger
- **ToF Sensor**: Move hand within 10cm
- **Cooldown**: 2 seconds between conversations

### Manual Trigger
- **Serial Monitor**: Press ENTER
- **Test Command**: Type "test" + ENTER

### Voice Command Examples
- "Hello, how are you?"
- "What's the weather like today?"
- "Tell me a joke"
- "Explain quantum physics"

## 🔧 Customization

### Change TTS Voice
```cpp
d["voice"]["name"] = "en-US-Neural2-F";  // Female voice
d["voice"]["name"] = "en-US-Neural2-D";  // Male voice (default)
```

### Adjust Speaking Speed
```cpp
d["audioConfig"]["speakingRate"] = 1.3;  // Faster
d["audioConfig"]["speakingRate"] = 0.8;  // Slower
```

### Modify ToF Sensitivity
```cpp
const int TRIGGER_DISTANCE = 15;  // Distance in cm
```

## 📊 Performance Monitoring

The system provides detailed metrics:

```
📊 Times: STT=15234ms, Gemini=12456ms, TTS=4123ms
⚡ Parallel round completed: 20567ms
💾 Free heap: 183524 bytes
💾 Free PSRAM: 8322780 bytes
🌐 WiFi: -45dBm
```

## 🛠️ Troubleshooting

### Common Issues

#### "❌ HTTPS connect failed"
- ✅ Check WiFi connection
- ✅ Verify Google Cloud API key
- ✅ Ensure APIs are enabled

#### "❌ STT failed"
- ✅ Check microphone connections
- ✅ Verify microphone is working
- ✅ Speak clearly and close to microphone

#### "❌ TTS failed"
- ✅ Check speaker connections
- ✅ Verify power supply (may need external 5V 2A)
- ✅ Check system volume

#### Continuous Reboots
- ✅ Use external 5V 2A power supply
- ✅ Verify I2S connections
- ✅ Check PSRAM memory

### Advanced Debug

Enable detailed debugging:
```cpp
#define DEBUG_LEVEL 3  // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug
```

## 🎯 Next Steps

1. **📖 Read full documentation**: [README.md](README.md)
2. **🔧 Customize settings** for your needs
3. **🎥 Watch tutorial videos** on Ingeimaks YouTube
4. **🤝 Join community** on Telegram: https://t.me/Ingeimaks

## 📞 Support

- 🐛 **Bug Reports**: Open GitHub issue
- 💡 **Feature Requests**: GitHub Discussions
- 🎥 **Tutorials**: Ingeimaks YouTube Channel
- 📧 **Contact**: Via YouTube or GitHub

---

**🎉 Congratulations! Your ESP32 AI Voice Assistant is ready!**

**⭐ If this helped you, please star the project on GitHub!**

**🎥 Subscribe to Ingeimaks for more amazing ESP32 projects!**