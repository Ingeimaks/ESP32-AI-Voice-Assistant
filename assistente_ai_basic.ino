#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "driver/i2s.h"
#include "mbedtls/base64.h"

// ==== CONFIG WiFi & Google ====
const char* WIFI_SSID = "Vodafone-Secondario";
const char* WIFI_PASS = "jkzdfc356wvsb4i";
#define GCP_API_KEY   "AIzaSyBE8LN5t84X0Mpez3-QVNaQbuMzgJgjQZk"
#define LANG_CODE      "it-IT"
#define GEMINI_MODEL   "gemini-1.5-flash-8b"

const char* STT_HOST  = "speech.googleapis.com";
const char* GEM_HOST  = "generativelanguage.googleapis.com";
const char* TTS_HOST  = "texttospeech.googleapis.com";
bool TOF_OK = false;

// ==== AUDIO ====
#define SR             8000     // 8 kHz (μ-law)
#define RECORD_SEC     5        // 5 secondi

// ==== I2S RX (INMP441) — ESP32-S3 ====
#define I2S_RX_PORT    I2S_NUM_0
#define RX_PIN_BCLK    4
#define RX_PIN_LRCLK   5
#define RX_PIN_DIN     6

// ==== I2S TX (MAX98357A) — ESP32-S3 ====
#define I2S_TX_PORT    I2S_NUM_1
#define TX_PIN_BCLK    16
#define TX_PIN_LRCLK   17
#define TX_PIN_DOUT    18

// ==== I2C (VL53L0X) ====
#define SDA_PIN        20
#define SCL_PIN        21
#define TRIGGER_MM     100      // 10 cm

Adafruit_VL53L0X lox;

// ==== MIC GAIN ====
int SHIFT_BITS = 11; // regola 10..12 se serve
inline int16_t i2s32to16(int32_t s32){ return (int16_t)(s32 >> SHIFT_BITS); }

// ==== BUFFER GLOBALI ====
#define I2S_READ            1024
static int32_t g_i2sBuf[I2S_READ];
static uint8_t g_ulawBuf[I2S_READ];

#define ENC_CHUNK_BYTES     (I2S_READ)
static unsigned char g_b64Out[((ENC_CHUNK_BYTES+2)/3)*4 + 32];
static uint8_t       g_encIn[ENC_CHUNK_BYTES + 2];
static inline size_t b64_len(size_t raw){ return ((raw + 2) / 3) * 4; }
static uint8_t g_b64_tail[2]; static size_t g_b64_tailLen=0, g_b64_sent=0;

// ==== UTILS ====
void wifiConnect(){
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status()!=WL_CONNECTED){ delay(300); Serial.print("."); }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
}

void i2sInitRX(){
  i2s_config_t cfg = {
    .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX),
    .sample_rate=SR,
    .bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT, // INMP441: 24b in 32b
    .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,  // L/R a GND = LEFT
    .communication_format=(i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags=ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count=8,
    .dma_buf_len=512,
    .use_apll=false, .tx_desc_auto_clear=false, .fixed_mclk=0
  };
  i2s_pin_config_t pins = {
    .bck_io_num=RX_PIN_BCLK, .ws_io_num=RX_PIN_LRCLK,
    .data_out_num=I2S_PIN_NO_CHANGE, .data_in_num=RX_PIN_DIN
  };
  i2s_driver_install(I2S_RX_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_RX_PORT, &pins);
  i2s_zero_dma_buffer(I2S_RX_PORT);
}

void i2sInitTX(){
  i2s_config_t cfg = {
    .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX),
    .sample_rate=SR,
    .bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format=(i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags=ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count=8,
    .dma_buf_len=256,
    .use_apll=false, .tx_desc_auto_clear=true, .fixed_mclk=0
  };
  i2s_pin_config_t pins = {
    .bck_io_num=TX_PIN_BCLK, .ws_io_num=TX_PIN_LRCLK,
    .data_out_num=TX_PIN_DOUT, .data_in_num=I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_TX_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_TX_PORT, &pins);
  i2s_zero_dma_buffer(I2S_TX_PORT);
}

// dechunk risposte HTTP "chunked"
String dechunk(const String& in){
  String out; int idx=0;
  while(true){
    int crlf = in.indexOf("\r\n", idx); if (crlf < 0) break;
    unsigned long n = strtoul(in.substring(idx, crlf).c_str(), nullptr, 16);
    idx = crlf + 2; if (n == 0) break;
    if (idx + (int)n > in.length()) break;
    out += in.substring(idx, idx + n);
    idx += n;
    if (idx + 2 <= (int)in.length() && in[idx]=='\r' && in[idx+1]=='\n') idx += 2;
  }
  return out.length()?out:in;
}

// μ-law helpers
inline uint8_t pcm16_to_mulaw(int16_t s){
  const uint16_t BIAS = 132;
  uint16_t mag = (s < 0) ? (uint16_t)(-s) : (uint16_t)s;
  mag += BIAS; if (mag > 0x1FFF) mag = 0x1FFF;
  uint8_t seg = 0; uint16_t tmp = mag >> 6; while(tmp){ seg++; tmp >>= 1; }
  uint8_t uval = (seg << 4) | ((mag >> (seg + 3)) & 0x0F);
  uval = ~uval; if (s >= 0) uval |= 0x80; else uval &= 0x7F; return uval;
}
static inline int16_t mulaw_to_pcm16(uint8_t u){
  u = ~u; int t = ((u & 0x0F) << 3) + 0x84;
  t <<= ((unsigned)u & 0x70) >> 4;
  return (u & 0x80) ? (0x84 - t) : (t - 0x84);
}

// Base64 streaming encoder (per STT)
bool b64_stream_write(WiFiClientSecure& cli, const uint8_t* data, size_t rawBytes){
  size_t total = g_b64_tailLen + rawBytes;
  size_t encLen = (total / 3) * 3;
  size_t outLen = 0;
  if (encLen > 0){
    size_t needFromNew = encLen - g_b64_tailLen;
    if (g_b64_tailLen) memcpy(g_encIn, g_b64_tail, g_b64_tailLen);
    memcpy(g_encIn + g_b64_tailLen, data, needFromNew);
    if (mbedtls_base64_encode(g_b64Out, sizeof(g_b64Out), &outLen, g_encIn, encLen) != 0) return false;
    cli.write(g_b64Out, outLen);
    g_b64_sent += outLen;
    size_t remain = total - encLen; // 0..2
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
  if (mbedtls_base64_encode(g_b64Out, sizeof(g_b64Out), &outLen, g_b64_tail, g_b64_tailLen) != 0) return false;
  cli.write(g_b64Out, outLen); g_b64_sent += outLen; g_b64_tailLen = 0;
  return true;
}

// ==== STT (5s, μ-law streaming) ====
bool stt_stream_5s(String& outText){
  const size_t samplesTarget = (size_t)SR * RECORD_SEC;
  const size_t ulawBytes     = samplesTarget;
  const size_t expectedB64   = b64_len(ulawBytes);

  String pre = F("{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":");
  pre += String(SR);
  pre += F(",\"languageCode\":\"");
  pre += String(LANG_CODE);
  pre += F("\",\"enableAutomaticPunctuation\":false,\"maxAlternatives\":1},\"audio\":{\"content\":\"");
  String post = F("\"}}");
  const size_t contentLen = pre.length() + expectedB64 + post.length();

  WiFiClientSecure cli; cli.setTimeout(20000); cli.setInsecure(); cli.setNoDelay(true);
  uint32_t t0 = millis();
  if (!cli.connect(STT_HOST, 443)){ Serial.println("❌ HTTPS connect STT"); return false; }
  uint32_t tTLS = millis(); Serial.printf("⏱️ TLS STT: %ums\n", tTLS - t0);

  String pathReq = "/v1/speech:recognize?key=" + String(GCP_API_KEY);
  cli.print(String("POST ") + pathReq + " HTTP/1.1\r\n");
  cli.print(String("Host: ") + STT_HOST + "\r\n");
  cli.print("Content-Type: application/json\r\n");
  cli.print("Content-Length: " + String(contentLen) + "\r\n");
  cli.print("Connection: close\r\n\r\n");
  cli.print(pre);

  g_b64_tailLen = 0; g_b64_sent = 0;

  // acquisizione + upload
  uint64_t sumSq = 0; int16_t peak = 0;
  size_t samplesSent = 0;
  uint32_t tUp0 = millis();
  Serial.println("🎙️ Registrazione 5s… parla!");
  while (samplesSent < samplesTarget){
    size_t br = 0;
    i2s_read(I2S_RX_PORT, (void*)g_i2sBuf, sizeof(g_i2sBuf), &br, portMAX_DELAY);
    size_t got32 = br / sizeof(int32_t);
    if (!got32) continue;

    size_t n = got32;
    if (samplesSent + n > samplesTarget) n = samplesTarget - samplesSent;

    for (size_t i = 0; i < n; ++i){
      int16_t s16 = i2s32to16(g_i2sBuf[i]);
      int16_t a = (s16<0)?-s16:s16;
      if (a > peak) peak = a;
      sumSq += (int32_t)s16*(int32_t)s16;
      g_ulawBuf[i] = pcm16_to_mulaw(s16);
    }

    if (!b64_stream_write(cli, g_ulawBuf, n)) { Serial.println("❌ base64 encode"); return false; }
    samplesSent += n;
  }
  if (!b64_stream_flush(cli)) { Serial.println("❌ base64 tail"); return false; }
  cli.print(post);

  uint32_t tUp1 = millis();
  float rms = sqrtf((float)sumSq / (float)samplesTarget);
  Serial.printf("⏱️ Upload audio: %ums | RMS≈%.0f  Peak=%d\n", tUp1 - tUp0, rms, peak);
  Serial.printf("✅ Base64 attesa=%u, inviata=%u\n", (unsigned)expectedB64, (unsigned)g_b64_sent);

  String resp; while (cli.connected() || cli.available()) { if (cli.available()) resp += cli.readString(); else delay(2); }
  uint32_t tResp = millis(); Serial.printf("⏱️ STT risposta: %ums\n", tResp - tUp1);

  int p = resp.indexOf("\r\n\r\n");
  String body = (p >= 0) ? resp.substring(p + 4) : resp;
  body = dechunk(body);

  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, body);
  if (err) { Serial.print("JSON parse err: "); Serial.println(err.c_str()); Serial.println(body); return false; }
  outText = doc["results"][0]["alternatives"][0]["transcript"] | "";
  return outText.length() > 0;
}

// ==== Gemini ====
bool gemini_generate_reply(const String& userText, String& outReply){
  WiFiClientSecure cli; cli.setTimeout(15000); cli.setInsecure(); cli.setNoDelay(true);
  uint32_t g0=millis(); if(!cli.connect(GEM_HOST,443)){ Serial.println("❌ HTTPS connect Gemini"); return false; }
  uint32_t gTLS=millis(); Serial.printf("⏱️ TLS Gemini: %ums\n", gTLS-g0);

  DynamicJsonDocument d(1536);
  JsonArray contents=d.createNestedArray("contents");
  JsonObject u=contents.createNestedObject(); u["role"]="user";
  JsonArray parts=u.createNestedArray("parts"); parts.createNestedObject()["text"]=userText;
  JsonObject gen=d.createNestedObject("generationConfig");
  gen["maxOutputTokens"]=64; gen["temperature"]=0.2; gen["candidateCount"]=1;

  String body; serializeJson(d,body);
  String path="/v1beta/models/"+String(GEMINI_MODEL)+":generateContent?key="+String(GCP_API_KEY);
  cli.print(String("POST ")+path+" HTTP/1.1\r\n");
  cli.print(String("Host: ")+GEM_HOST+"\r\n");
  cli.print("Content-Type: application/json\r\n");
  cli.print("Content-Length: "+String(body.length())+"\r\n");
  cli.print("Connection: close\r\n\r\n");
  cli.print(body);

  String resp; while(cli.connected()||cli.available()){ if(cli.available()) resp+=cli.readString(); else delay(2); }
  uint32_t gDone=millis(); Serial.printf("⏱️ Gemini risposta: %ums\n", gDone-gTLS);

  int p=resp.indexOf("\r\n\r\n"); String respBody=(p>=0)?resp.substring(p+4):resp; respBody=dechunk(respBody);
  DynamicJsonDocument out(12288);
  if(deserializeJson(out,respBody)){ Serial.println("Gemini JSON parse err"); Serial.println(respBody); return false; }
  outReply = out["candidates"][0]["content"]["parts"][0]["text"] | "";
  return outReply.length()>0;
}

// ==== Base64 decoder helpers (per TTS) ====
static inline int b64val(char c){
  if (c>='A' && c<='Z') return c - 'A';
  if (c>='a' && c<='z') return c - 'a' + 26;
  if (c>='0' && c<='9') return c - '0' + 52;
  if (c=='+') return 62;
  if (c=='/') return 63;
  if (c=='=') return -2; // padding
  return -1;            // ignora (spazi, \n, ecc.)
}

// ==== TTS: dechunk + Base64→PCM16 streaming ====
bool googleTTS_say_mulaw(const String& text){
  WiFiClientSecure cli; cli.setTimeout(25000); cli.setInsecure(); cli.setNoDelay(true);
  if(!cli.connect(TTS_HOST,443)){ Serial.println("❌ HTTPS connect TTS"); return false; }

  DynamicJsonDocument d(1024);
  d["input"]["text"]=text;
  d["voice"]["languageCode"]=LANG_CODE; 
  d["voice"]["name"]="it-IT-Wavenet-C";
  d["audioConfig"]["audioEncoding"]="MULAW";
  d["audioConfig"]["sampleRateHertz"]=SR;

  String req; serializeJson(d, req);
  String path="/v1/text:synthesize?key="+String(GCP_API_KEY);
  cli.print(String("POST ")+path+" HTTP/1.1\r\n");
  cli.print(String("Host: ")+TTS_HOST+"\r\n");
  cli.print("Content-Type: application/json\r\n");
  cli.print("Content-Length: "+String(req.length())+"\r\n");
  cli.print("Connection: close\r\n\r\n");
  cli.print(req);

  // leggi risposta completa
  String resp;
  while (cli.connected() || cli.available()){
    if (cli.available()) resp += cli.readString();
    else delay(2);
  }
  int p = resp.indexOf("\r\n\r\n");
  String body = (p>=0) ? resp.substring(p+4) : resp;
  body = dechunk(body);

  // trova "audioContent":"...
  int k = body.indexOf("\"audioContent\"");
  if (k < 0) {
    Serial.println("❌ TTS: audioContent non trovato");
    String head = body.substring(0, min(200,(int)body.length()));
    head.replace("\n","\\n");
    Serial.printf("Body head: %s\n", head.c_str());
    return false;
  }
  k = body.indexOf(':', k);
  if (k < 0) { Serial.println("❌ TTS: ':' non trovato"); return false; }
  int q0 = body.indexOf('"', k);
  if (q0 < 0) { Serial.println("❌ TTS: quote inizio non trovato"); return false; }
  q0++; // inizio base64

  // I2S TX clock
  i2s_set_clk(I2S_TX_PORT, SR, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  // decode base64 a flusso -> μ-law -> PCM16 -> I2S
  const size_t N = 256;
  int16_t pcm16[N]; size_t pcmFill = 0;

  int quartet[4]; int qi = 0; bool done = false;

  for (int i = q0; i < body.length() && !done; ++i){
    char c = body[i];
    if (c == '"') {
      done = true;
      while (qi && qi < 4) quartet[qi++] = -2; // padding finale
    } else {
      int v = b64val(c);
      if (v < 0) continue; // ignora whitespace/newline
      quartet[qi++] = v;
      if (qi == 4){
        int v0=quartet[0], v1=quartet[1], v2=quartet[2], v3=quartet[3];
        uint8_t out[3];
        out[0] = (uint8_t)((v0<<2) | ((v1 & 0x30)>>4));
        out[1] = (uint8_t)(((v1 & 0x0F)<<4) | ((v2>=0 ? v2 : 0)>>2));
        out[2] = (uint8_t)(((v2>=0 ? v2 : 0)<<6) | (v3>=0 ? v3 : 0));
        int outN = 3; if (v3==-2) outN=2; if (v2==-2) outN=1;

        for (int j=0;j<outN;++j){
          pcm16[pcmFill++] = mulaw_to_pcm16(out[j]);
          if (pcmFill == N){
            size_t bw=0; 
            esp_err_t e=i2s_write(I2S_TX_PORT,(const void*)pcm16,N*sizeof(int16_t),&bw,pdMS_TO_TICKS(300));
            if (e!=ESP_OK){ Serial.printf("❌ i2s_write err=%d bw=%u\n",(int)e,(unsigned)bw); return false; }
            pcmFill=0; yield();
          }
        }
        qi=0;
      }
    }
  }

  if (pcmFill){
    size_t bw=0; 
    esp_err_t e=i2s_write(I2S_TX_PORT,(const void*)pcm16,pcmFill*sizeof(int16_t),&bw,pdMS_TO_TICKS(300));
    if (e!=ESP_OK){ Serial.printf("❌ i2s_write err=%d bw=%u\n",(int)e,(unsigned)bw); return false; }
  }

  Serial.println("✅ TTS playback done (streaming)");
  return true;
}

// ==== Round completo ====
void do_round(){
  // 1) STT
  String transcript;
  if(!stt_stream_5s(transcript)){ Serial.println("❌ STT fallita"); return; }
  Serial.print("📝 Hai detto: "); Serial.println(transcript);

  // 2) Gemini
  String reply;
  Serial.println("🤖 Gemini in corso...");
  if(!gemini_generate_reply(transcript, reply)){ Serial.println("❌ Gemini fallita"); return; }
  Serial.print("💬 Risposta: "); Serial.println(reply);

  // 3) TTS
  Serial.println("🔊 Parlo nello speaker...");
  if(!googleTTS_say_mulaw(reply)) Serial.println("❌ TTS fallita");
}

void setup(){
  Serial.begin(115200); delay(200);
  wifiConnect();

  // I2C ToF
  // I2C ToF
Wire.begin(SDA_PIN, SCL_PIN);
if(!lox.begin()){
  Serial.println("❌ VL53L0X non trovato. Controlla collegamenti/I2C.");
  TOF_OK = false;
} else {
  Serial.println("✅ VL53L0X OK.");
  TOF_OK = true;
}


  i2sInitRX();
  i2sInitTX();

  Serial.println("Assistente pronto! Avvicina la mano a <10 cm per parlare 5s (oppure premi INVIO).");
}

unsigned long cooldown_until = 0;
int underCount = 0;

void loop(){
  // Trigger da ToF < 10 cm (con piccolo debounce)
  VL53L0X_RangingMeasurementData_t measure;
  if (TOF_OK) {
    lox.rangingTest(&measure, false);
    bool valid = (measure.RangeStatus == 0);
    uint16_t mm = measure.RangeMilliMeter;

    static uint32_t lastPrint=0;
    if (millis()-lastPrint > 250){
      lastPrint = millis();
      Serial.printf("Distanza: %s%u mm\n", valid?"":"(inv) ", mm);
    }

    if (millis() >= cooldown_until) {
      if (valid && mm <= TRIGGER_MM) {
        if (++underCount >= 3) { // 3 letture consecutive sotto soglia
          Serial.println("🚀 Trigger ToF < 10 cm → Avvio round");
          do_round();
          cooldown_until = millis() + 3000; // 3s di cooldown
          underCount = 0;
        }
      } else {
        underCount = 0;
      }
    }
  }

  // Trigger alternativo da Serial (premi INVIO)
  if (Serial.available()){
    while(Serial.available()) Serial.read();
    do_round();
  }

  // zeri periodici per ridurre ronzii su alcuni ampli in idle
  static uint32_t last=0;
  if (millis()-last > 50){
    last = millis();
    int16_t zero[128]={0}; size_t bw=0;
    i2s_write(I2S_TX_PORT, zero, sizeof(zero), &bw, 0);
  }
}
