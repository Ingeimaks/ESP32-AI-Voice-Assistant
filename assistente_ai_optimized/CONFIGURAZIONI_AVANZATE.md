# Configurazioni Avanzate ESP32-S3 Assistente AI

## 🎛️ Parametri di Tuning Audio

### Qualità vs Prestazioni

```cpp
// CONFIGURAZIONE ALTA QUALITÀ (più lenta)
#define SR             22050    // 22kHz per qualità premium
#define RECORD_SEC     7        // Registrazioni più lunghe
#define AUDIO_BUFFER_SIZE 8192  // Buffer grandi
#define I2S_READ_LARGE    8192  // Lettura I2S grande

// CONFIGURAZIONE BILANCIATA (consigliata)
#define SR             16000    // 16kHz qualità/prestazioni
#define RECORD_SEC     5        // Standard
#define AUDIO_BUFFER_SIZE 4096  // Buffer medi
#define I2S_READ_LARGE    4096  // Lettura I2S media

// CONFIGURAZIONE VELOCE (prestazioni massime)
#define SR             8000     // 8kHz veloce
#define RECORD_SEC     3        // Registrazioni brevi
#define AUDIO_BUFFER_SIZE 2048  // Buffer piccoli
#define I2S_READ_LARGE    2048  // Lettura I2S piccola
```

### Regolazione Guadagno Microfono

```cpp
// Per microfoni sensibili (ambiente silenzioso)
int SHIFT_BITS = 12;  // Riduce il guadagno

// Per microfoni normali (consigliato)
int SHIFT_BITS = 10;  // Guadagno standard

// Per microfoni poco sensibili (ambiente rumoroso)
int SHIFT_BITS = 8;   // Aumenta il guadagno

// Funzione dinamica di auto-gain
void adjustGain(int16_t peak) {
  if (peak < 1000) SHIFT_BITS = max(8, SHIFT_BITS - 1);
  else if (peak > 8000) SHIFT_BITS = min(12, SHIFT_BITS + 1);
}
```

## 🧠 Ottimizzazioni AI

### Configurazioni Gemini per Diversi Usi

```cpp
// CONVERSAZIONE CASUAL
gen["maxOutputTokens"] = 64;
gen["temperature"] = 0.7;
gen["topP"] = 0.9;
gen["topK"] = 40;

// ASSISTENTE TECNICO
gen["maxOutputTokens"] = 128;
gen["temperature"] = 0.2;
gen["topP"] = 0.8;
gen["topK"] = 20;

// CREATIVITÀ MASSIMA
gen["maxOutputTokens"] = 256;
gen["temperature"] = 0.9;
gen["topP"] = 0.95;
gen["topK"] = 60;

// RISPOSTE BREVI E PRECISE
gen["maxOutputTokens"] = 32;
gen["temperature"] = 0.1;
gen["topP"] = 0.7;
gen["topK"] = 10;
```

### Prompt System Personalizzati

```cpp
String systemPrompt = "Sei un assistente AI italiano. Rispondi sempre in modo:"
                     "- Conciso (max 2 frasi)"
                     "- Amichevole e informale"
                     "- Usando emoji quando appropriato";

// Aggiungi al JSON Gemini
JsonObject system = contents.createNestedObject();
system["role"] = "system";
JsonArray sysParts = system.createNestedArray("parts");
sysParts.createNestedObject()["text"] = systemPrompt;
```

## 🔊 Configurazioni TTS Avanzate

### Voci Disponibili per Italiano

```cpp
// VOCI NEURALI (migliore qualità)
"it-IT-Neural2-A"  // Maschio, naturale
"it-IT-Neural2-C"  // Femmina, naturale (consigliata)

// VOCI WAVENET (buona qualità)
"it-IT-Wavenet-A"  // Maschio
"it-IT-Wavenet-B"  // Femmina
"it-IT-Wavenet-C"  // Femmina
"it-IT-Wavenet-D"  // Maschio

// VOCI STANDARD (veloce)
"it-IT-Standard-A" // Femmina
"it-IT-Standard-B" // Femmina
"it-IT-Standard-C" // Maschio
"it-IT-Standard-D" // Maschio
```

### Parametri Audio TTS

```cpp
// CONFIGURAZIONE NATURALE
d["audioConfig"]["speakingRate"] = 1.0;    // Velocità normale
d["audioConfig"]["pitch"] = 0.0;           // Pitch normale
d["audioConfig"]["volumeGainDb"] = 0.0;    // Volume normale

// CONFIGURAZIONE VELOCE
d["audioConfig"]["speakingRate"] = 1.3;    // 30% più veloce
d["audioConfig"]["pitch"] = 2.0;           // Pitch più alto
d["audioConfig"]["volumeGainDb"] = 3.0;    // Volume più alto

// CONFIGURAZIONE LENTA E CHIARA
d["audioConfig"]["speakingRate"] = 0.8;    // 20% più lenta
d["audioConfig"]["pitch"] = -2.0;          // Pitch più basso
d["audioConfig"]["volumeGainDb"] = 2.0;    // Volume leggermente alto
```

## 📡 Ottimizzazioni Rete

### Configurazioni WiFi per Diversi Scenari

```cpp
// CASA/UFFICIO (prestazioni massime)
WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Potenza massima
WiFi.setSleep(false);                  // No sleep
cli.setBufferSizes(8192, 8192);        // Buffer grandi

// BATTERIA (risparmio energetico)
WiFi.setTxPower(WIFI_POWER_11dBm);     // Potenza media
WiFi.setSleep(WIFI_PS_MIN_MODEM);      // Sleep minimo
cli.setBufferSizes(2048, 2048);        // Buffer piccoli

// RETE LENTA (ottimizzato per latenza)
WiFi.setTxPower(WIFI_POWER_15dBm);     // Potenza alta
WiFi.setSleep(false);                  // No sleep
cli.setTimeout(45000);                 // Timeout lungo
```

### Gestione Errori di Rete Avanzata

```cpp
bool connectWithRetry(const char* host, int port, int maxRetries = 3) {
  for (int i = 0; i < maxRetries; i++) {
    if (cli.connect(host, port)) return true;
    
    Serial.printf("Tentativo %d/%d fallito, retry in %ds\n", i+1, maxRetries, (i+1)*2);
    delay((i+1) * 2000);  // Backoff esponenziale
    
    // Reset WiFi se necessario
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      delay(5000);
    }
  }
  return false;
}
```

## 🎯 Sensore ToF Avanzato

### Configurazioni per Diversi Ambienti

```cpp
// AMBIENTE INTERNO (precisione massima)
lox.setMeasurementTimingBudget(200000);  // 200ms per misura
lox.setSignalRateLimit(0.1);             // Limite segnale basso
lox.setVcselPulsePeriod(VL53L0X_VCSEL_PERIOD_PRE_RANGE, 18);
lox.setVcselPulsePeriod(VL53L0X_VCSEL_PERIOD_FINAL_RANGE, 14);

// AMBIENTE ESTERNO (velocità massima)
lox.setMeasurementTimingBudget(20000);   // 20ms per misura
lox.setSignalRateLimit(0.25);            // Limite segnale alto

// AMBIENTE CON INTERFERENZE
lox.setMeasurementTimingBudget(100000);  // 100ms bilanciato
lox.setSignalRateLimit(0.15);            // Limite medio
```

### Filtro Avanzato per Misure ToF

```cpp
class ToFFilter {
private:
  uint16_t readings[5];
  int index = 0;
  bool filled = false;
  
public:
  uint16_t addReading(uint16_t newReading) {
    readings[index] = newReading;
    index = (index + 1) % 5;
    if (index == 0) filled = true;
    
    if (!filled) return newReading;
    
    // Mediana di 5 valori
    uint16_t sorted[5];
    memcpy(sorted, readings, sizeof(readings));
    
    // Bubble sort semplice
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4-i; j++) {
        if (sorted[j] > sorted[j+1]) {
          uint16_t temp = sorted[j];
          sorted[j] = sorted[j+1];
          sorted[j+1] = temp;
        }
      }
    }
    
    return sorted[2];  // Valore mediano
  }
};

ToFFilter tofFilter;
```

## 🔧 Gestione Memoria Avanzata

### Monitoraggio Memoria Real-time

```cpp
void printMemoryStats() {
  Serial.printf("=== MEMORIA ===\n");
  Serial.printf("Heap libero: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap minimo: %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("PSRAM libera: %u bytes\n", ESP.getFreePsram());
  Serial.printf("PSRAM minima: %u bytes\n", ESP.getMinFreePsram());
  Serial.printf("Stack libero: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
}

// Chiama ogni 30 secondi
static uint32_t lastMemCheck = 0;
if (millis() - lastMemCheck > 30000) {
  lastMemCheck = millis();
  printMemoryStats();
}
```

### Garbage Collection Manuale

```cpp
void forceGarbageCollection() {
  // Forza la pulizia della memoria
  String dummy;
  dummy.reserve(1024);
  dummy = "";
  
  // Compatta heap se possibile
  heap_caps_check_integrity_all(true);
  
  Serial.println("🧹 Garbage collection eseguita");
}

// Esegui dopo ogni conversazione
void do_round_optimized() {
  // ... codice conversazione ...
  
  forceGarbageCollection();
}
```

## ⚡ Profiling e Debug

### Misurazione Prestazioni Dettagliata

```cpp
class PerformanceProfiler {
private:
  struct Timing {
    String name;
    uint32_t start;
    uint32_t duration;
  };
  
  Timing timings[10];
  int count = 0;
  
public:
  void start(const String& name) {
    if (count < 10) {
      timings[count].name = name;
      timings[count].start = millis();
    }
  }
  
  void end() {
    if (count < 10) {
      timings[count].duration = millis() - timings[count].start;
      count++;
    }
  }
  
  void printReport() {
    Serial.println("=== PERFORMANCE REPORT ===");
    uint32_t total = 0;
    for (int i = 0; i < count; i++) {
      Serial.printf("%s: %ums\n", timings[i].name.c_str(), timings[i].duration);
      total += timings[i].duration;
    }
    Serial.printf("TOTALE: %ums\n", total);
    count = 0;
  }
};

PerformanceProfiler profiler;
```

### Debug Audio Avanzato

```cpp
void analyzeAudioQuality(int16_t* samples, size_t count) {
  int16_t min_val = 32767, max_val = -32768;
  uint64_t sum = 0, sumSq = 0;
  int clipped = 0;
  
  for (size_t i = 0; i < count; i++) {
    int16_t s = samples[i];
    if (s < min_val) min_val = s;
    if (s > max_val) max_val = s;
    if (s >= 32760 || s <= -32760) clipped++;
    
    sum += abs(s);
    sumSq += (int32_t)s * s;
  }
  
  float mean = (float)sum / count;
  float rms = sqrt((float)sumSq / count);
  float dynamic_range = 20.0 * log10((float)(max_val - min_val) / 65536.0);
  
  Serial.printf("🎵 Audio: RMS=%.1f, Range=%d-%d, DR=%.1fdB, Clip=%d\n", 
                rms, min_val, max_val, dynamic_range, clipped);
}
```

## 🚀 Ottimizzazioni Sperimentali

### Cache Locale per Risposte Frequenti

```cpp
struct CacheEntry {
  String question;
  String answer;
  uint32_t timestamp;
};

CacheEntry responseCache[5];
int cacheIndex = 0;

bool checkCache(const String& question, String& answer) {
  for (int i = 0; i < 5; i++) {
    if (responseCache[i].question.equalsIgnoreCase(question) && 
        millis() - responseCache[i].timestamp < 300000) { // 5 min cache
      answer = responseCache[i].answer;
      Serial.println("💾 Risposta da cache");
      return true;
    }
  }
  return false;
}

void addToCache(const String& question, const String& answer) {
  responseCache[cacheIndex].question = question;
  responseCache[cacheIndex].answer = answer;
  responseCache[cacheIndex].timestamp = millis();
  cacheIndex = (cacheIndex + 1) % 5;
}
```

### Compressione Audio Real-time

```cpp
// Implementazione ADPCM semplificata per ridurre banda
class SimpleADPCM {
private:
  int16_t last_sample = 0;
  int step_index = 0;
  
public:
  uint8_t encode(int16_t sample) {
    int diff = sample - last_sample;
    // Implementazione semplificata ADPCM
    // ... codice di compressione ...
    return compressed_byte;
  }
  
  int16_t decode(uint8_t compressed) {
    // Implementazione decompressione
    // ... codice di decompressione ...
    return decompressed_sample;
  }
};
```

---

**💡 Suggerimento**: Testa sempre le configurazioni in ambiente controllato prima di usarle in produzione. Ogni ambiente può richiedere parametri diversi per prestazioni ottimali.