/*
 * ESP32 AI Voice Assistant - Versione Ottimizzata
 * 
 * ISTRUZIONI CONFIGURAZIONE:
 * 1. Copia il file "config_template.h" come "config_private.h"
 * 2. Modifica "config_private.h" con le tue credenziali WiFi e Google Cloud
 * 3. Compila e carica il firmware
 * 
 * NOTA: Il file config_private.h è escluso da Git per sicurezza
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "driver/i2s.h"
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Includi configurazione privata (crea questo file da config_template.h)
#ifdef __has_include
  #if __has_include("../config_private.h")
    #include "../config_private.h"
    #define CONFIG_LOADED
  #endif
#endif

#ifndef CONFIG_LOADED
  #error "ERRORE: File config_private.h non trovato! Copia config_template.h come config_private.h e inserisci le tue credenziali."
#endif

// ==== CONFIG WiFi & Google ====
const char* wifi_ssid = WIFI_SSID;
const char* wifi_pass = WIFI_PASSWORD;
#define LANG_CODE      "it-IT"

const char* STT_HOST  = "speech.googleapis.com";
const char* GEM_HOST  = "generativelanguage.googleapis.com";
const char* TTS_HOST  = "texttospeech.googleapis.com";
bool TOF_OK = false;

// ==== AUDIO OTTIMIZZATO ====
#define SR             16000    // 16 kHz per migliore qualità
#define RECORD_SEC     5        // 5 secondi
#define AUDIO_BUFFER_SIZE 4096  // Buffer più grande per PSRAM

// ==== I2S RX (INMP441) — ESP32-S3 OTTIMIZZATO ====
#define I2S_RX_PORT    I2S_NUM_0
#define RX_PIN_BCLK    4
#define RX_PIN_LRCLK   5
#define RX_PIN_DIN     6

// ==== I2S TX (MAX98357A) — ESP32-S3 OTTIMIZZATO ====
#define I2S_TX_PORT    I2S_NUM_1
#define TX_PIN_BCLK    16
#define TX_PIN_LRCLK   17
#define TX_PIN_DOUT    18

// ==== I2C (VL53L0X) ====
#define SDA_PIN        20
#define SCL_PIN        21
#define TRIGGER_MM     100      // 10 cm

Adafruit_VL53L0X lox;

// ==== MIC GAIN OTTIMIZZATO ====
int SHIFT_BITS = 10; // Ottimizzato per 16kHz
inline int16_t i2s32to16(int32_t s32){ return (int16_t)(s32 >> SHIFT_BITS); }

// ==== BUFFER GLOBALI IN PSRAM ====
#define I2S_READ_LARGE      4096  // Buffer più grande
static int32_t* g_i2sBuf = nullptr;
static uint8_t* g_ulawBuf = nullptr;
static int16_t* g_pcmBuf = nullptr;

// Buffer Base64 ottimizzati
#define ENC_CHUNK_BYTES     8192  // Chunk più grandi
static unsigned char* g_b64Out = nullptr;
static uint8_t* g_encIn = nullptr;
static inline size_t b64_len(size_t raw){ return ((raw + 2) / 3) * 4; }
static uint8_t g_b64_tail[2]; 
static size_t g_b64_tailLen=0, g_b64_sent=0;

// Queue per audio processing
QueueHandle_t audioQueue;
struct AudioChunk {
  uint8_t* data;
  size_t size;
};

// Pre-connessioni TLS per ottimizzazione
WiFiClientSecure* sttClient = nullptr;
WiFiClientSecure* geminiClient = nullptr;
WiFiClientSecure* ttsClient = nullptr;
bool preconnectionsReady = false;

// Task handles per esecuzione parallela
TaskHandle_t sttTaskHandle = nullptr;
TaskHandle_t geminiTaskHandle = nullptr;
TaskHandle_t ttsTaskHandle = nullptr;

// Strutture per comunicazione tra task
struct STTResult {
  String transcript;
  bool success;
  uint32_t processingTime;
};

struct GeminiResult {
  String reply;
  bool success;
  uint32_t processingTime;
};

struct TTSResult {
  bool success;
  uint32_t processingTime;
};

STTResult sttResult;
GeminiResult geminiResult;
TTSResult ttsResult;
SemaphoreHandle_t sttSemaphore;
SemaphoreHandle_t geminiSemaphore;
SemaphoreHandle_t ttsSemaphore;

// Sistema anti-ripetizione
String lastUserInput = "";
String lastGeminiReply = "";
uint32_t lastConversationTime = 0;

// ==== MEMORY MANAGEMENT ====
void* psram_malloc(size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!ptr) {
    Serial.printf("❌ PSRAM allocation failed for %u bytes\n", size);
    ptr = malloc(size); // Fallback to internal RAM
  }
  return ptr;
}

// ==== PRE-CONNESSIONI TLS ====
void initPreconnections() {
  Serial.println("🔗 Inizializzazione pre-connessioni TLS...");
  
  sttClient = new WiFiClientSecure();
  geminiClient = new WiFiClientSecure();
  ttsClient = new WiFiClientSecure();
  
  sttClient->setInsecure();
  geminiClient->setInsecure();
  ttsClient->setInsecure();
  
  sttClient->setNoDelay(true);
  geminiClient->setNoDelay(true);
  ttsClient->setNoDelay(true);
  
  // Pre-connetti in background
  uint32_t startTime = millis();
  
  bool sttOk = sttClient->connect(STT_HOST, 443);
  bool geminiOk = geminiClient->connect(GEM_HOST, 443);
  bool ttsOk = ttsClient->connect(TTS_HOST, 443);
  
  uint32_t connectTime = millis() - startTime;
  
  if (sttOk && geminiOk && ttsOk) {
    preconnectionsReady = true;
    Serial.printf("✅ Pre-connessioni TLS pronte in %ums\n", connectTime);
  } else {
    Serial.printf("⚠️ Pre-connessioni parziali: STT=%s Gemini=%s TTS=%s\n", 
                  sttOk ? "OK" : "FAIL", geminiOk ? "OK" : "FAIL", ttsOk ? "OK" : "FAIL");
  }
}

void cleanupPreconnections() {
  if (sttClient) { sttClient->stop(); delete sttClient; sttClient = nullptr; }
  if (geminiClient) { geminiClient->stop(); delete geminiClient; geminiClient = nullptr; }
  if (ttsClient) { ttsClient->stop(); delete ttsClient; ttsClient = nullptr; }
  preconnectionsReady = false;
}

// Verifica e ripristina pre-connessioni se necessario
void checkAndRestorePreconnections() {
  bool needRestore = false;
  
  if (!sttClient || !sttClient->connected()) {
    Serial.println("⚠️ Pre-connessione STT persa, ripristino...");
    needRestore = true;
  }
  if (!geminiClient || !geminiClient->connected()) {
    Serial.println("⚠️ Pre-connessione Gemini persa, ripristino...");
    needRestore = true;
  }
  if (!ttsClient || !ttsClient->connected()) {
    Serial.println("⚠️ Pre-connessione TTS persa, ripristino...");
    needRestore = true;
  }
  
  if (needRestore) {
    cleanupPreconnections();
    delay(1000); // Pausa prima di riconnettersi
    initPreconnections();
  }
}

// ==== TASK PARALLELI ====
void sttTask(void* parameter) {
  String* inputText = (String*)parameter;
  uint32_t startTime = millis();
  
  // Retry automatico per STT
  int retryCount = 0;
  const int maxRetries = 2;
  
  do {
    if (retryCount > 0) {
      Serial.printf("🔄 STT retry %d/%d\n", retryCount, maxRetries);
      delay(1000); // Pausa tra retry
    }
    
    sttResult.success = stt_stream_optimized(sttResult.transcript);
    retryCount++;
    
  } while (!sttResult.success && retryCount <= maxRetries);
  
  sttResult.processingTime = millis() - startTime;
  
  if (!sttResult.success) {
    Serial.printf("❌ STT fallita dopo %d tentativi\n", maxRetries);
  }
  
  xSemaphoreGive(sttSemaphore);
  vTaskDelete(NULL);
}

void geminiTask(void* parameter) {
  String* inputText = (String*)parameter;
  uint32_t startTime = millis();
  
  // Aspetta che STT sia completato con timeout più lungo
  if (xSemaphoreTake(sttSemaphore, pdMS_TO_TICKS(45000)) == pdTRUE) {
    if (sttResult.success) {
      // Retry automatico per Gemini
      int retryCount = 0;
      const int maxRetries = 2;
      
      do {
        if (retryCount > 0) {
          Serial.printf("🔄 Gemini retry %d/%d\n", retryCount, maxRetries);
          delay(2000); // Pausa più lunga per Gemini
        }
        
        geminiResult.success = gemini_generate_reply_optimized(sttResult.transcript, geminiResult.reply);
        retryCount++;
        
      } while (!geminiResult.success && retryCount <= maxRetries);
      
      if (!geminiResult.success) {
        Serial.printf("❌ Gemini fallita dopo %d tentativi\n", maxRetries);
      }
    } else {
      Serial.println("⚠️ STT fallita, Gemini non eseguito");
      geminiResult.success = false;
    }
  } else {
    Serial.println("❌ Timeout STT (45s), Gemini non eseguito");
    geminiResult.success = false;
  }
  
  geminiResult.processingTime = millis() - startTime;
  xSemaphoreGive(geminiSemaphore);
  vTaskDelete(NULL);
}

void ttsTask(void* parameter) {
  String* inputText = (String*)parameter;
  uint32_t startTime = millis();
  
  // Aspetta che Gemini sia completato con timeout più lungo
  if (xSemaphoreTake(geminiSemaphore, pdMS_TO_TICKS(50000)) == pdTRUE) {
    if (geminiResult.success) {
      // Retry automatico per TTS
      int retryCount = 0;
      const int maxRetries = 2;
      
      do {
        if (retryCount > 0) {
          Serial.printf("🔄 TTS retry %d/%d\n", retryCount, maxRetries);
          delay(1500); // Pausa media per TTS
        }
        
        ttsResult.success = googleTTS_say_mulaw_optimized(geminiResult.reply);
        retryCount++;
        
      } while (!ttsResult.success && retryCount <= maxRetries);
      
      if (!ttsResult.success) {
        Serial.printf("❌ TTS fallita dopo %d tentativi\n", maxRetries);
      }
    } else {
      Serial.println("⚠️ Gemini fallita, TTS non eseguito");
      ttsResult.success = false;
    }
  } else {
    Serial.println("❌ Timeout Gemini (50s), TTS non eseguito");
    ttsResult.success = false;
  }
  
  ttsResult.processingTime = millis() - startTime;
  xSemaphoreGive(ttsSemaphore);
  vTaskDelete(NULL);
}

void initParallelTasks() {
  // Crea semafori
  sttSemaphore = xSemaphoreCreateBinary();
  geminiSemaphore = xSemaphoreCreateBinary();
  ttsSemaphore = xSemaphoreCreateBinary();
  
  if (!sttSemaphore || !geminiSemaphore || !ttsSemaphore) {
    Serial.println("❌ Errore creazione semafori");
    return;
  }
  
  Serial.println("✅ Task paralleli inizializzati");
}

void initBuffers() {
  // Alloca buffer in PSRAM per prestazioni migliori
  g_i2sBuf = (int32_t*)psram_malloc(I2S_READ_LARGE * sizeof(int32_t));
  g_ulawBuf = (uint8_t*)psram_malloc(I2S_READ_LARGE);
  g_pcmBuf = (int16_t*)psram_malloc(I2S_READ_LARGE * sizeof(int16_t));
  g_b64Out = (unsigned char*)psram_malloc(((ENC_CHUNK_BYTES+2)/3)*4 + 64);
  g_encIn = (uint8_t*)psram_malloc(ENC_CHUNK_BYTES + 8);
  
  // Crea queue per audio processing
  audioQueue = xQueueCreate(10, sizeof(AudioChunk));
  
  if (!g_i2sBuf || !g_ulawBuf || !g_pcmBuf || !g_b64Out || !g_encIn || !audioQueue) {
    Serial.println("❌ Buffer allocation failed!");
    ESP.restart();
  }
  
  Serial.printf("✅ Buffers allocati in PSRAM: %u KB\n", 
    (I2S_READ_LARGE*7 + ENC_CHUNK_BYTES*2 + 128) / 1024);
}

// ==== UTILS OTTIMIZZATI ====
void wifiConnect(){
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Massima potenza
  WiFi.setSleep(false); // Disabilita sleep per prestazioni
  WiFi.begin(wifi_ssid, wifi_pass);
  
  Serial.print("WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
    Serial.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi connection failed!");
    ESP.restart();
  }
  
  Serial.printf("\n✅ WiFi connesso - IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("📶 RSSI: %d dBm\n", WiFi.RSSI());
}

void i2sInitRX(){
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,    // Più buffer DMA
    .dma_buf_len = 1024,    // Buffer DMA più grandi
    .use_apll = true,       // Usa APLL per clock più preciso
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_32BIT
  };
  
  i2s_pin_config_t pins = {
    .bck_io_num = RX_PIN_BCLK,
    .ws_io_num = RX_PIN_LRCLK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = RX_PIN_DIN
  };
  
  ESP_ERROR_CHECK(i2s_driver_install(I2S_RX_PORT, &cfg, 0, NULL));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_RX_PORT, &pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_RX_PORT));
  
  Serial.println("✅ I2S RX inizializzato (16kHz, APLL)");
}

void i2sInitTX(){
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,    // Più buffer DMA
    .dma_buf_len = 512,     // Buffer DMA ottimizzati
    .use_apll = true,       // Usa APLL
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_16BIT
  };
  
  i2s_pin_config_t pins = {
    .bck_io_num = TX_PIN_BCLK,
    .ws_io_num = TX_PIN_LRCLK,
    .data_out_num = TX_PIN_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  ESP_ERROR_CHECK(i2s_driver_install(I2S_TX_PORT, &cfg, 0, NULL));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_TX_PORT, &pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_TX_PORT));
  
  Serial.println("✅ I2S TX inizializzato (16kHz, APLL)");
}

// Dechunk ottimizzato con pre-allocazione
String dechunk(const String& in){
  String out;
  out.reserve(in.length()); // Pre-alloca memoria
  int idx = 0;
  
  while(true){
    int crlf = in.indexOf("\r\n", idx);
    if (crlf < 0) break;
    
    unsigned long n = strtoul(in.substring(idx, crlf).c_str(), nullptr, 16);
    idx = crlf + 2;
    if (n == 0) break;
    if (idx + (int)n > in.length()) break;
    
    out += in.substring(idx, idx + n);
    idx += n;
    if (idx + 2 <= (int)in.length() && in[idx]=='\r' && in[idx+1]=='\n') idx += 2;
  }
  
  return out.length() ? out : in;
}

// μ-law helpers ottimizzati
inline uint8_t pcm16_to_mulaw(int16_t s){
  const uint16_t BIAS = 132;
  uint16_t mag = (s < 0) ? (uint16_t)(-s) : (uint16_t)s;
  mag += BIAS;
  if (mag > 0x1FFF) mag = 0x1FFF;
  
  uint8_t seg = 0;
  uint16_t tmp = mag >> 6;
  while(tmp){ seg++; tmp >>= 1; }
  
  uint8_t uval = (seg << 4) | ((mag >> (seg + 3)) & 0x0F);
  uval = ~uval;
  if (s >= 0) uval |= 0x80;
  else uval &= 0x7F;
  return uval;
}

static inline int16_t mulaw_to_pcm16(uint8_t u){
  u = ~u;
  int t = ((u & 0x0F) << 3) + 0x84;
  t <<= ((unsigned)u & 0x70) >> 4;
  return (u & 0x80) ? (0x84 - t) : (t - 0x84);
}

// Base64 streaming encoder ottimizzato
bool b64_stream_write(WiFiClientSecure& cli, const uint8_t* data, size_t rawBytes){
  size_t total = g_b64_tailLen + rawBytes;
  size_t encLen = (total / 3) * 3;
  size_t outLen = 0;
  
  if (encLen > 0){
    size_t needFromNew = encLen - g_b64_tailLen;
    if (g_b64_tailLen) memcpy(g_encIn, g_b64_tail, g_b64_tailLen);
    memcpy(g_encIn + g_b64_tailLen, data, needFromNew);
    
    if (mbedtls_base64_encode(g_b64Out, ENC_CHUNK_BYTES*2, &outLen, g_encIn, encLen) != 0) 
      return false;
    
    size_t written = cli.write(g_b64Out, outLen);
    if (written != outLen) {
      Serial.printf("❌ Write mismatch: %u/%u\n", written, outLen);
      return false;
    }
    
    g_b64_sent += outLen;
    size_t remain = total - encLen;
    if (remain) memcpy(g_b64_tail, data + needFromNew, remain);
    g_b64_tailLen = remain;
  } else {
    memcpy(g_b64_tail + g_b64_tailLen, data, rawBytes);
    g_b64_tailLen += rawBytes;
  }
  return true;
}

bool b64_stream_flush(WiFiClientSecure& cli){
  if (!g_b64_tailLen) return true;
  size_t outLen = 0;
  if (mbedtls_base64_encode(g_b64Out, ENC_CHUNK_BYTES*2, &outLen, g_b64_tail, g_b64_tailLen) != 0) 
    return false;
  
  size_t written = cli.write(g_b64Out, outLen);
  g_b64_sent += outLen;
  g_b64_tailLen = 0;
  return written == outLen;
}

// ==== STT OTTIMIZZATO (16kHz, μ-law streaming) ====
bool stt_stream_optimized(String& outText){
  const size_t samplesTarget = (size_t)SR * RECORD_SEC;
  const size_t ulawBytes = samplesTarget;
  const size_t expectedB64 = b64_len(ulawBytes);

  // Pre-costruisci JSON request
  String pre = F("{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":");
  pre += String(SR);
  pre += F(",\"languageCode\":\"");
  pre += String(LANG_CODE);
  pre += F("\",\"enableAutomaticPunctuation\":true,\"maxAlternatives\":1,\"model\":\"latest_long\"},\"audio\":{\"content\":\"");
  String post = F("\"}}");
  const size_t contentLen = pre.length() + expectedB64 + post.length();

  // Debug pre-connessioni
  Serial.printf("🔍 Debug STT - preconnectionsReady: %s\n", preconnectionsReady ? "SI" : "NO");
  Serial.printf("🔍 Debug STT - sttClient: %s\n", sttClient ? "VALIDO" : "NULL");
  if (sttClient) {
    Serial.printf("🔍 Debug STT - sttClient connected: %s\n", sttClient->connected() ? "SI" : "NO");
  }

  WiFiClientSecure* cli = nullptr;
  bool usePreconnection = false;
  
  // Usa pre-connessione se disponibile e valida
  if (preconnectionsReady && sttClient && sttClient->connected()) {
    cli = sttClient;
    usePreconnection = true;
    Serial.println("✅ Uso pre-connessione STT");
  } else {
    // Fallback: crea nuova connessione
    cli = new WiFiClientSecure();
    cli->setTimeout(35000); // Timeout più lungo per STT
    cli->setInsecure();
    cli->setNoDelay(true);
    
    uint32_t t0 = millis();
    if (!cli->connect(STT_HOST, 443)){
      Serial.println("❌ HTTPS connect STT fallback");
      delete cli;
      return false;
    }
    uint32_t tTLS = millis();
    Serial.printf("⏱️ TLS STT fallback: %ums\n", tTLS - t0);
  }

  String pathReq = "/v1/speech:recognize?key=" + String(GCP_API_KEY);
  cli->print(String("POST ") + pathReq + " HTTP/1.1\r\n");
  cli->print(String("Host: ") + STT_HOST + "\r\n");
  cli->print("Content-Type: application/json\r\n");
  cli->print("Content-Length: " + String(contentLen) + "\r\n");
  cli->print("Connection: close\r\n\r\n");
  cli->print(pre);

  g_b64_tailLen = 0;
  g_b64_sent = 0;

  // Acquisizione ottimizzata con buffer più grandi
  uint64_t sumSq = 0;
  int16_t peak = 0;
  size_t samplesSent = 0;
  uint32_t tUp0 = millis();
  
  Serial.println("🎙️ Registrazione 5s (16kHz)... parla!");
  
  while (samplesSent < samplesTarget){
    size_t br = 0;
    esp_err_t err = i2s_read(I2S_RX_PORT, (void*)g_i2sBuf, I2S_READ_LARGE * sizeof(int32_t), &br, pdMS_TO_TICKS(100));
    
    if (err != ESP_OK) {
      Serial.printf("❌ I2S read error: %d\n", err);
      continue;
    }
    
    size_t got32 = br / sizeof(int32_t);
    if (!got32) continue;

    size_t n = got32;
    if (samplesSent + n > samplesTarget) n = samplesTarget - samplesSent;

    // Conversione ottimizzata
      for (size_t i = 0; i < n; ++i){
        int16_t s16 = i2s32to16(g_i2sBuf[i]);
        int16_t a = (s16 < 0) ? -s16 : s16;
        if (a > peak) peak = a;
        sumSq += (int32_t)s16 * (int32_t)s16;
        g_ulawBuf[i] = pcm16_to_mulaw(s16);
      }

      if (!b64_stream_write(*cli, g_ulawBuf, n)) {
        Serial.println("❌ base64 encode");
        if (!usePreconnection) delete cli;
        return false;
      }
      samplesSent += n;
      
      // Yield e watchdog reset più frequenti per evitare timeout
      if (samplesSent % 4000 == 0) {
        taskYIELD();
        esp_task_wdt_reset();
        
        // Controllo memoria durante registrazione
        if (ESP.getFreeHeap() < 30000) {
          Serial.println("⚠️ Memoria bassa durante registrazione");
        }
      }
  }
  
  if (!b64_stream_flush(*cli)) {
    Serial.println("❌ base64 tail");
    if (!usePreconnection) delete cli;
    return false;
  }
  cli->print(post);

  uint32_t tUp1 = millis();
  float rms = sqrtf((float)sumSq / (float)samplesTarget);
  Serial.printf("⏱️ Upload audio: %ums | RMS≈%.0f Peak=%d\n", tUp1 - tUp0, rms, peak);
  Serial.printf("✅ Base64 attesa=%u, inviata=%u\n", (unsigned)expectedB64, (unsigned)g_b64_sent);

  // Lettura risposta ottimizzata
  String resp;
  resp.reserve(16384); // Pre-alloca
  
  while (cli->connected() || cli->available()) {
    if (cli->available()) {
      String chunk = cli->readString();
      resp += chunk;
    } else {
      delay(1);
    }
  }
  
  uint32_t tResp = millis();
  Serial.printf("⏱️ STT risposta: %ums\n", tResp - tUp1);

  int p = resp.indexOf("\r\n\r\n");
  String body = (p >= 0) ? resp.substring(p + 4) : resp;
  body = dechunk(body);

  // JSON parsing ottimizzato
  DynamicJsonDocument doc(16384); // Buffer più grande
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.print("JSON parse err: ");
    Serial.println(err.c_str());
    Serial.println(body.substring(0, 500));
    if (!usePreconnection) delete cli;
    return false;
  }
  
  outText = doc["results"][0]["alternatives"][0]["transcript"] | "";
  bool success = outText.length() > 0;
  
  // Pulizia memoria se non è pre-connessione
  if (!usePreconnection) {
    delete cli;
  }
  
  return success;
}

// ==== Gemini OTTIMIZZATO ====
bool gemini_generate_reply_optimized(const String& userText, String& outReply){
  // Debug pre-connessioni
  Serial.printf("🔍 Debug Gemini - preconnectionsReady: %s\n", preconnectionsReady ? "SI" : "NO");
  Serial.printf("🔍 Debug Gemini - geminiClient: %s\n", geminiClient ? "VALIDO" : "NULL");
  if (geminiClient) {
    Serial.printf("🔍 Debug Gemini - geminiClient connected: %s\n", geminiClient->connected() ? "SI" : "NO");
  }

  WiFiClientSecure* cli = nullptr;
  bool usePreconnection = false;
  uint32_t gTLS = millis(); // Timing per debug
  
  // Usa pre-connessione se disponibile e valida
  if (preconnectionsReady && geminiClient && geminiClient->connected()) {
    cli = geminiClient;
    usePreconnection = true;
    Serial.println("✅ Uso pre-connessione Gemini");
  } else {
    // Fallback: crea nuova connessione
    cli = new WiFiClientSecure();
    cli->setTimeout(30000); // Timeout più lungo per Gemini
    cli->setInsecure();
    cli->setNoDelay(true);
    
    uint32_t g0 = millis();
    if(!cli->connect(GEM_HOST, 443)){
      Serial.println("❌ HTTPS connect Gemini fallback");
      delete cli;
      return false;
    }
    gTLS = millis();
    Serial.printf("⏱️ TLS Gemini fallback: %ums\n", gTLS - g0);
  }

  // Sistema anti-ripetizione
  uint32_t currentTime = millis();
  bool isRepetitive = false;
  
  // Controlla se è una domanda ripetitiva
  if (userText == lastUserInput && (currentTime - lastConversationTime) < 300000) { // 5 minuti
    Serial.println("⚠️ Domanda ripetitiva rilevata, aggiungo contesto");
    isRepetitive = true;
  }
  
  // JSON ottimizzato
  DynamicJsonDocument d(2048);
  JsonArray contents = d.createNestedArray("contents");
  
  // Aggiungi contesto se necessario
  if (isRepetitive && lastGeminiReply.length() > 0) {
    JsonObject assistant = contents.createNestedObject();
    assistant["role"] = "model";
    JsonArray assistantParts = assistant.createNestedArray("parts");
    assistantParts.createNestedObject()["text"] = lastGeminiReply;
  }
  
  JsonObject u = contents.createNestedObject();
  u["role"] = "user";
  JsonArray parts = u.createNestedArray("parts");
  
  String enhancedText = userText;
  if (isRepetitive) {
    enhancedText = "Hai già risposto a questa domanda. Puoi fornire una risposta diversa o più dettagliata? " + userText;
  }
  
  parts.createNestedObject()["text"] = enhancedText;
  
  JsonObject gen = d.createNestedObject("generationConfig");
  gen["maxOutputTokens"] = 128; // Più token per risposte migliori
  gen["temperature"] = 0.3;
  gen["candidateCount"] = 1;
  gen["topP"] = 0.8;
  gen["topK"] = 40;

  String body;
  serializeJson(d, body);
  
  String path = "/v1beta/models/" + String(GEMINI_MODEL) + ":generateContent?key=" + String(GCP_API_KEY);
  cli->print(String("POST ") + path + " HTTP/1.1\r\n");
  cli->print(String("Host: ") + GEM_HOST + "\r\n");
  cli->print("Content-Type: application/json\r\n");
  cli->print("Content-Length: " + String(body.length()) + "\r\n");
  cli->print("Connection: close\r\n\r\n");
  cli->print(body);

  String resp;
  resp.reserve(8192);
  
  while(cli->connected() || cli->available()){
    if(cli->available()) {
      resp += cli->readString();
    } else {
      delay(1);
    }
  }
  
  uint32_t gDone = millis();
  Serial.printf("⏱️ Gemini risposta: %ums\n", gDone - gTLS);

  int p = resp.indexOf("\r\n\r\n");
  String respBody = (p >= 0) ? resp.substring(p + 4) : resp;
  respBody = dechunk(respBody);
  
  DynamicJsonDocument out(12288);
  if(deserializeJson(out, respBody)){
    Serial.println("Gemini JSON parse err");
    Serial.println(respBody.substring(0, 500));
    if (!usePreconnection) delete cli;
    return false;
  }
  
  outReply = out["candidates"][0]["content"]["parts"][0]["text"] | "";
  bool success = outReply.length() > 0;
  
  // Aggiorna cronologia conversazione
  if (success) {
    lastUserInput = userText;
    lastGeminiReply = outReply;
    lastConversationTime = millis();
  }
  
  // Pulizia memoria se non è pre-connessione
  if (!usePreconnection) {
    delete cli;
  }
  
  return success;
}

// Base64 decoder ottimizzato
static inline int b64val(char c){
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  if (c == '=') return -2;
  return -1;
}

// ==== TTS OTTIMIZZATO ====
bool googleTTS_say_mulaw_optimized(const String& text){
  // Debug pre-connessioni
  Serial.printf("🔍 Debug TTS - preconnectionsReady: %s\n", preconnectionsReady ? "SI" : "NO");
  Serial.printf("🔍 Debug TTS - ttsClient: %s\n", ttsClient ? "VALIDO" : "NULL");
  if (ttsClient) {
    Serial.printf("🔍 Debug TTS - ttsClient connected: %s\n", ttsClient->connected() ? "SI" : "NO");
  }

  WiFiClientSecure* cli = nullptr;
  bool usePreconnection = false;
  
  // Usa pre-connessione se disponibile e valida
  if (preconnectionsReady && ttsClient && ttsClient->connected()) {
    cli = ttsClient;
    usePreconnection = true;
    Serial.println("✅ Uso pre-connessione TTS");
  } else {
    // Fallback: crea nuova connessione
    cli = new WiFiClientSecure();
    cli->setTimeout(40000); // Timeout più lungo per TTS
    cli->setInsecure();
    cli->setNoDelay(true);
    
    if(!cli->connect(TTS_HOST, 443)){
      Serial.println("❌ HTTPS connect TTS fallback");
      delete cli;
      return false;
    }
    Serial.println("✅ TTS fallback connesso");
  }

  // JSON ottimizzato per TTS
  DynamicJsonDocument d(1536);
  d["input"]["text"] = text;
  d["voice"]["languageCode"] = LANG_CODE;
  d["voice"]["name"] = "it-IT-Neural2-C"; // Voce neurale migliore
  d["voice"]["ssmlGender"] = "MALE"; // Corretto: Neural2-C è voce maschile
  d["audioConfig"]["audioEncoding"] = "MULAW";
  d["audioConfig"]["sampleRateHertz"] = SR;
  d["audioConfig"]["speakingRate"] = 1.1; // Leggermente più veloce
  d["audioConfig"]["pitch"] = 0.0;
  d["audioConfig"]["volumeGainDb"] = 2.0; // Volume più alto

  String req;
  serializeJson(d, req);
  
  String path = "/v1/text:synthesize?key=" + String(GCP_API_KEY);
  cli->print(String("POST ") + path + " HTTP/1.1\r\n");
  cli->print(String("Host: ") + TTS_HOST + "\r\n");
  cli->print("Content-Type: application/json\r\n");
  cli->print("Content-Length: " + String(req.length()) + "\r\n");
  cli->print("Connection: close\r\n\r\n");
  cli->print(req);

  // Lettura risposta ottimizzata
  String resp;
  resp.reserve(32768); // Buffer più grande per audio
  
  while (cli->connected() || cli->available()){
    if (cli->available()) {
      resp += cli->readString();
    } else {
      delay(1);
    }
  }
  
  int p = resp.indexOf("\r\n\r\n");
  String body = (p >= 0) ? resp.substring(p + 4) : resp;
  body = dechunk(body);

  // Debug: stampa parte della risposta per analisi
  Serial.printf("📊 TTS Response size: %d bytes\n", body.length());
  if (body.length() > 0) {
    String preview = body.substring(0, min(300, (int)body.length()));
    preview.replace("\n", "\\n");
    preview.replace("\r", "\\r");
    Serial.printf("📋 TTS Response preview: %s\n", preview.c_str());
  }
  
  // Trova audioContent con debug migliorato
  int k = body.indexOf("\"audioContent\"");
  if (k < 0) {
    Serial.println("❌ TTS: audioContent non trovato");
    
    // Cerca possibili varianti
    if (body.indexOf("error") >= 0) {
      Serial.println("🔍 Possibile errore nell'API TTS");
    }
    if (body.indexOf("audioContent") >= 0) {
      Serial.println("🔍 audioContent trovato senza quotes");
    }
    if (body.indexOf("audio_content") >= 0) {
      Serial.println("🔍 Trovato audio_content (snake_case)");
    }
    
    if (!usePreconnection) delete cli;
    return false;
  }
  
  k = body.indexOf(':', k);
  if (k < 0) {
    if (!usePreconnection) delete cli;
    return false;
  }
  
  int q0 = body.indexOf('"', k);
  if (q0 < 0) {
    if (!usePreconnection) delete cli;
    return false;
  }
  q0++;

  // Configura I2S per playback
  ESP_ERROR_CHECK(i2s_set_clk(I2S_TX_PORT, SR, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO));

  // Decode e playback ottimizzato con buffer più grandi
  const size_t PCM_BUFFER_SIZE = 1024;
  size_t pcmFill = 0;
  int quartet[4];
  int qi = 0;
  bool done = false;

  for (int i = q0; i < body.length() && !done; ++i){
    char c = body[i];
    
    // Reset watchdog basato su tempo per evitare riavvii con risposte lunghe
    static uint32_t lastTimeCheck = 0;
    uint32_t currentTime = millis();
    if (currentTime - lastTimeCheck >= 200) { // Ogni 200ms
      esp_task_wdt_reset();
      taskYIELD();
      lastTimeCheck = currentTime;
    }
    
    if (c == '"') {
      done = true;
      while (qi && qi < 4) quartet[qi++] = -2;
    } else {
      int v = b64val(c);
      if (v < 0) continue;
      quartet[qi++] = v;
      
      if (qi == 4){
        int v0 = quartet[0], v1 = quartet[1], v2 = quartet[2], v3 = quartet[3];
        uint8_t out[3];
        out[0] = (uint8_t)((v0 << 2) | ((v1 & 0x30) >> 4));
        out[1] = (uint8_t)(((v1 & 0x0F) << 4) | ((v2 >= 0 ? v2 : 0) >> 2));
        out[2] = (uint8_t)(((v2 >= 0 ? v2 : 0) << 6) | (v3 >= 0 ? v3 : 0));
        
        int outN = 3;
        if (v3 == -2) outN = 2;
        if (v2 == -2) outN = 1;

        // Controllo watchdog basato su tempo per evitare riavvii
        static uint32_t lastWatchdogReset = 0;
        uint32_t currentTime = millis();
        
        for (int j = 0; j < outN; ++j){
          g_pcmBuf[pcmFill++] = mulaw_to_pcm16(out[j]);
          
          // Reset watchdog basato su tempo invece che su campioni
          if (currentTime - lastWatchdogReset >= 500) { // Ogni 500ms
            esp_task_wdt_reset();
            taskYIELD();
            lastWatchdogReset = currentTime;
            
            // Controllo memoria durante riproduzione
            if (ESP.getFreeHeap() < 25000) {
              Serial.println("⚠️ Memoria critica durante TTS");
            }
          }
          
          if (pcmFill == PCM_BUFFER_SIZE){
            size_t bw = 0;
            esp_err_t e = i2s_write(I2S_TX_PORT, (const void*)g_pcmBuf, 
                                   PCM_BUFFER_SIZE * sizeof(int16_t), &bw, pdMS_TO_TICKS(1000));
            if (e != ESP_OK){
              Serial.printf("❌ i2s_write err=%d bw=%u\n", (int)e, (unsigned)bw);
              if (!usePreconnection) delete cli;
              return false;
            }
            pcmFill = 0;
            
            // Reset watchdog dopo ogni buffer scritto
            esp_task_wdt_reset();
            taskYIELD();
            
            // Controlla se il task è stato interrotto
            TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
            if (currentTask != NULL && eTaskGetState(currentTask) == eDeleted) {
              return false;
            }
          }
        }
        qi = 0;
      }
    }
  }

  // Flush buffer finale
  if (pcmFill){
    size_t bw = 0;
    esp_err_t e = i2s_write(I2S_TX_PORT, (const void*)g_pcmBuf, 
                           pcmFill * sizeof(int16_t), &bw, pdMS_TO_TICKS(500));
    if (e != ESP_OK){
      Serial.printf("❌ i2s_write final err=%d bw=%u\n", (int)e, (unsigned)bw);
      if (!usePreconnection) delete cli;
      return false;
    }
  }

  Serial.println("✅ TTS playback completato (16kHz ottimizzato)");
  
  // Pulizia memoria se non è pre-connessione
  if (!usePreconnection) {
    delete cli;
  }
  
  return true;
}

// ==== ROUND COMPLETO OTTIMIZZATO ====
void do_round_optimized(){
  uint32_t roundStart = millis();
  
  // 1) STT ottimizzato
  String transcript;
  Serial.println("🎙️ Avvio STT ottimizzato...");
  if(!stt_stream_optimized(transcript)){
    Serial.println("❌ STT fallita");
    return;
  }
  Serial.print("📝 Trascrizione: ");
  Serial.println(transcript);

  // 2) Gemini ottimizzato
  String reply;
  Serial.println("🤖 Elaborazione Gemini...");
  if(!gemini_generate_reply_optimized(transcript, reply)){
    Serial.println("❌ Gemini fallita");
    return;
  }
  Serial.print("💬 Risposta: ");
  Serial.println(reply);

  // 3) TTS ottimizzato
  Serial.println("🔊 Sintesi vocale...");
  if(!googleTTS_say_mulaw_optimized(reply)) {
    Serial.println("❌ TTS fallita");
  }
  
  uint32_t roundEnd = millis();
  Serial.printf("⏱️ Round completo: %ums\n", roundEnd - roundStart);
  Serial.printf("💾 Heap libero: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("💾 PSRAM libero: %u bytes\n", ESP.getFreePsram());
}

// ==== ROUND PARALLELO ULTRA-OTTIMIZZATO ====
void do_round_parallel(){
  uint32_t roundStart = millis();
  
  Serial.println("🚀 Avvio round parallelo ultra-ottimizzato...");
  
  // Verifica e ripristina pre-connessioni se necessario
  checkAndRestorePreconnections();
  
  // Controllo memoria prima di iniziare
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t freePsram = ESP.getFreePsram();
  
  Serial.printf("💾 Memoria disponibile: Heap=%u, PSRAM=%u\n", freeHeap, freePsram);
  
  if (freeHeap < 50000) {
    Serial.println("⚠️ Memoria heap bassa, forzando garbage collection");
    // Forza pulizia memoria
    String dummy;
    dummy.reserve(1000);
    dummy = "";
  }
  
  if (freePsram < 1000000) {
    Serial.println("❌ PSRAM insufficiente per operazione");
    return;
  }
  
  // Reset risultati
  sttResult.success = false;
  geminiResult.success = false;
  ttsResult.success = false;
  
  // Avvia tutti i task in parallelo
  String dummyParam = "";
  
  // Task STT (inizia subito)
  xTaskCreatePinnedToCore(
    sttTask,
    "STT_Task",
    8192,
    &dummyParam,
    2,
    &sttTaskHandle,
    0  // Core 0
  );
  
  // Task Gemini (aspetterà STT)
  xTaskCreatePinnedToCore(
    geminiTask,
    "Gemini_Task",
    8192,
    &dummyParam,
    2,
    &geminiTaskHandle,
    1  // Core 1
  );
  
  // Task TTS (aspetterà Gemini)
  xTaskCreatePinnedToCore(
    ttsTask,
    "TTS_Task",
    8192,
    &dummyParam,
    2,
    &ttsTaskHandle,
    0  // Core 0
  );
  
  // Aspetta completamento di tutti i task con timeout esteso
  if (xSemaphoreTake(ttsSemaphore, pdMS_TO_TICKS(90000)) == pdTRUE) {
    uint32_t roundEnd = millis();
    
    // Mostra risultati
    if (sttResult.success) {
      Serial.printf("📝 Trascrizione (%ums): %s\n", sttResult.processingTime, sttResult.transcript.c_str());
    } else {
      Serial.println("❌ STT fallita");
    }
    
    if (geminiResult.success) {
      Serial.printf("💬 Risposta (%ums): %s\n", geminiResult.processingTime, geminiResult.reply.c_str());
    } else {
      Serial.println("❌ Gemini fallita");
    }
    
    if (ttsResult.success) {
      Serial.printf("🔊 TTS completato (%ums)\n", ttsResult.processingTime);
    } else {
      Serial.println("❌ TTS fallita");
    }
    
    Serial.printf("⚡ Round parallelo completato: %ums\n", roundEnd - roundStart);
    Serial.printf("📊 Tempi: STT=%ums, Gemini=%ums, TTS=%ums\n", 
                  sttResult.processingTime, geminiResult.processingTime, ttsResult.processingTime);
    
  } else {
    Serial.println("❌ Timeout round parallelo (90s)");
    
    // Cleanup forzato dei task se ancora in esecuzione
    if (sttTaskHandle) {
      vTaskDelete(sttTaskHandle);
      sttTaskHandle = nullptr;
    }
    if (geminiTaskHandle) {
      vTaskDelete(geminiTaskHandle);
      geminiTaskHandle = nullptr;
    }
    if (ttsTaskHandle) {
      vTaskDelete(ttsTaskHandle);
      ttsTaskHandle = nullptr;
    }
    
    // Reset pre-connessioni dopo timeout
    Serial.println("🔄 Reset pre-connessioni dopo timeout");
    cleanupPreconnections();
    delay(2000);
    initPreconnections();
  }
  
  // Pulizia task handle per evitare riferimenti non validi
  sttTaskHandle = nullptr;
  geminiTaskHandle = nullptr;
  ttsTaskHandle = nullptr;
  
  // Statistiche memoria
  Serial.printf("💾 Heap libero: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("💾 PSRAM libero: %u bytes\n", ESP.getFreePsram());
}

void setup(){
  Serial.begin(115200);
  delay(500);
  
  // Configurazione CPU per prestazioni massime
  setCpuFrequencyMhz(240); // Frequenza massima
  
  // Configurazione watchdog senza monitoraggio core IDLE per evitare crash
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,  // 30 secondi timeout
    .idle_core_mask = 0,  // NON monitorare i core IDLE
    .trigger_panic = false // Non causare panic, solo reset
  };
  esp_task_wdt_init(&wdt_config);
  // Non aggiungere task al watchdog per evitare interferenze con audio
  
  Serial.println("🚀 ESP32-S3 Assistente AI Ottimizzato");
  Serial.printf("💾 PSRAM: %u bytes\n", ESP.getPsramSize());
  Serial.printf("💾 Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("⚡ CPU: %u MHz\n", getCpuFrequencyMhz());
  
  // Inizializza buffer in PSRAM
  initBuffers();
  
  // WiFi ottimizzato
  wifiConnect();

  // I2C ToF
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000); // I2C veloce
  
  if(!lox.begin()){
    Serial.println("❌ VL53L0X non trovato");
    TOF_OK = false;
  } else {
    Serial.println("✅ VL53L0X inizializzato");
    lox.setMeasurementTimingBudgetMicroSeconds(20000); // Misure più veloci
    TOF_OK = true;
  }

  // I2S ottimizzato
  i2sInitRX();
  i2sInitTX();

  // Inizializza task paralleli e pre-connessioni
  initParallelTasks();
  initPreconnections();

  Serial.println("\n✅ Sistema ultra-ottimizzato pronto! Avvicina la mano (<10cm) o premi INVIO");
  Serial.println("📊 Configurazione: 16kHz, APLL, Buffer PSRAM, CPU 240MHz");
  Serial.println("💡 Modalità parallela attiva per prestazioni massime!");
}

unsigned long cooldown_until = 0;
int underCount = 0;

void loop(){
  // Trigger ToF ottimizzato
  if (TOF_OK) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    bool valid = (measure.RangeStatus == 0);
    uint16_t mm = measure.RangeMilliMeter;

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 500){ // Meno frequente per prestazioni
      lastPrint = millis();
      Serial.printf("📏 Distanza: %s%u mm\n", valid ? "" : "(inv) ", mm);
    }

    if (millis() >= cooldown_until) {
      if (valid && mm <= TRIGGER_MM) {
        if (++underCount >= 2) { // Trigger più veloce
          Serial.println("🚀 Trigger ToF → Avvio conversazione parallela");
          do_round_parallel();
          cooldown_until = millis() + 2000; // Cooldown ridotto
          underCount = 0;
        }
      } else {
        underCount = 0;
      }
    }
  }

  // Trigger Serial
  if (Serial.available()){
    while(Serial.available()) Serial.read();
    Serial.println("🚀 Trigger manuale → Avvio conversazione parallela");
    do_round_parallel();
  }

  // Pulizia periodica I2S per evitare rumore
  static uint32_t lastClean = 0;
  if (millis() - lastClean > 100){
    lastClean = millis();
    int16_t zero[64] = {0};
    size_t bw = 0;
    i2s_write(I2S_TX_PORT, zero, sizeof(zero), &bw, 0);
  }
  
  // Controllo periodico pre-connessioni (ogni 30 secondi)
  static uint32_t lastPreconnCheck = 0;
  if (millis() - lastPreconnCheck > 30000) {
    lastPreconnCheck = millis();
    checkAndRestorePreconnections();
  }
  
  // Yield per FreeRTOS
  delay(10);
}