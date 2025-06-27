#include <Arduino.h>
#include <PDM.h>
#include <fga-beta_inferencing.h>  // Edge Impulse SDK + your model
#include <SPI.h>
#include <LoRa.h>

// ————— Configuration —————
#define EIDSP_QUANTIZE_FILTERBANK            0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 4

// ——— LoRa pins & settings ———
const int   LORA_CS       = 2;       // D2
const int   LORA_RST      = 4;       // D4
const int   LORA_DIO0     = 3;       // D3
const long  LORA_FREQ     = 433E6;
const int   LORA_TX_POWER = 20;
const int   LORA_SPREAD   = 12;
const long  LORA_BW       = 125E3;
const int   LORA_CR       = 5;

// ——— Identification & metadata ———
const char DEVICE_ID[]   = "FG-002";
const char DEVICE_NAME[] = "ForestNode-02";
const double LATITUDE    = 12.345678;
const double LONGITUDE   = -98.765432;

// ——— Sensors & buzzer ———
const int PIN_VIBRATION = 5;  // D5
const int PIN_BUZZER    = 6;  // D6
const int PIN_BATTERY   = A0; // A0

// ——— Send interval (modifiable) ———
unsigned long sendIntervalMs = 60UL * 1000UL;
unsigned long lastSendMs     = 0;

// ——— Downlink buffer ———
static const int DL_BUF_SIZE = 64;
static char      dlBuf[DL_BUF_SIZE];

// ——— TinyML buffers ———
typedef struct {
  int16_t *buffers[2];
  uint8_t  buf_select, buf_ready;
  uint32_t buf_count,   n_samples;
} inference_t;
static inference_t inference;
static bool        record_ready = false;
static int16_t    *sampleBuffer;

// ——— Latest inference ———
const char *currentLabel      = "none";
float       currentConfidence = 0.0f;

// ——— Battery full scale ———
const float BATTERY_MAX_VOLTAGE = 4.2f;

// ——— Function prototypes ———
static void  pdm_data_ready_callback();
static bool  microphone_inference_start(uint32_t n_samples);
static bool  microphone_inference_record();
static int   microphone_audio_signal_get_data(
                  size_t offset, size_t length, float *out_ptr);
static bool  sendWithRetry(const char *payload);
static void  beepLocate();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // — Audio inference setup —
  if (!microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE)) {
    Serial.println("🛑 Audio buffer alloc failed");
    while (1);
  }
  Serial.println("▶️ Audio inference ready");

  // — Sensors / buzzer —
  pinMode(PIN_VIBRATION, INPUT);
  pinMode(PIN_BUZZER,    OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // — LoRa init —
  SPI.begin();  
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("🛑 LoRa init failed");
    while (1);
  }
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSpreadingFactor(LORA_SPREAD);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  // start in RX continuous so we can catch ACKs/commands:
  LoRa.receive();
  Serial.println("✅ LoRa ready, RX mode");
}

void loop() {
  // — 1) Record a slice —
  if (!microphone_inference_record()) {
    Serial.println("❌ Audio record error");
    return;
  }

  // — 2) Run inference —
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
  signal.get_data     = microphone_audio_signal_get_data;
  ei_impulse_result_t result;
  if (run_classifier_continuous(&signal, &result, false)
      != EI_IMPULSE_OK) {
    Serial.println("❌ Classifier error");
    return;
  }

  // — 3) Pick top label —
  float  bestVal = 0.0f;
  size_t bestIx  = 0;
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > bestVal) {
      bestVal = result.classification[i].value;
      bestIx  = i;
    }
  }
  currentLabel      = result.classification[bestIx].label;
  currentConfidence = bestVal;

  // — 4) Read sensors —
  bool vibDetected = digitalRead(PIN_VIBRATION) == HIGH;
  int rawBatt      = analogRead(PIN_BATTERY);
  float battV      = (rawBatt / 1023.0f) * BATTERY_MAX_VOLTAGE;
  int   battPct    = int((battV / BATTERY_MAX_VOLTAGE) * 100.0f);

  // — 5) Periodic send —
  unsigned long now = millis();
  if (now - lastSendMs >= sendIntervalMs) {
    lastSendMs = now;

    // build JSON
    char payload[384];
    snprintf(payload, sizeof(payload),
      "{"
        "\"device_id\":\"%s\","
        "\"name\":\"%s\","
        "\"alert_type\":\"%s\","
        "\"confidence\":%d,"
        "\"latitude\":%.6f,"
        "\"longitude\":%.6f,"
        "\"battery_level\":%d,"
        "\"vibration\":%d"
      "}",
      DEVICE_ID,
      DEVICE_NAME,
      currentLabel,
      int(currentConfidence * 100),
      LATITUDE,
      LONGITUDE,
      battPct,
      vibDetected ? 1 : 0
    );

    bool ok = sendWithRetry(payload);
    if (ok) Serial.println("✅ Sent & ACK received");
    else    Serial.println("❌ Send failed");

    // radio is already in RX mode from sendWithRetry
  }

  // — 6) Handle downlink commands —
  //    Since we’re in continuous RX, just check parsePacket
  int pkt = LoRa.parsePacket();
  if (pkt) {
    String cmd;
    while (LoRa.available()) cmd += char(LoRa.read());
    cmd.trim();
    Serial.print("🌐 DL: "); Serial.println(cmd);

    // 6a) Locate command?
    if (!strncmp(cmd.c_str(), "CMD:BEEP:", 9) &&
        cmd.substring(9) == DEVICE_ID)
    {
      beepLocate();
    }
    // 6b) Interval change: "CMD:SET_INTERVAL:<ms>"
    else if (!strncmp(cmd.c_str(), "CMD:SET_INTERVAL:", 17))
    {
      String arg = cmd.substring(17);
      unsigned long v = arg.toInt();
      if (v >= 1000) {
        sendIntervalMs = v;
        Serial.print("⏱ Interval updated to ");
        Serial.print(sendIntervalMs);
        Serial.println(" ms");
        // optional ACK for interval change:
        LoRa.beginPacket();
          LoRa.print("ACK:INTERVAL:");
          LoRa.print(sendIntervalMs);
        LoRa.endPacket();
        LoRa.receive();
      }
    }
  }
}

// — sendWithRetry: TX → switch to RX → wait for ACK:<DEVICE_ID>
static bool sendWithRetry(const char *payload) {
  // bump this up so the gateway has time to do the HTTP + POST + TX ACK
  const unsigned long ACK_TIMEOUT = 5000;  
  const int MAX_RETRIES = 3;
  unsigned long backoff = ACK_TIMEOUT;

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    // 1) Stop any RX, then TX
    LoRa.idle();
    LoRa.beginPacket();
      LoRa.print(payload);
    LoRa.endPacket();

    // 2) Go right back into continuous RX
    LoRa.receive();
    Serial.print("✉️  Try ");
    Serial.print(attempt);
    Serial.print(": ");
    Serial.println(payload);

    // 3) Wait up to ACK_TIMEOUT for an “ACK:<DEVICE_ID>”
    unsigned long deadline = millis() + ACK_TIMEOUT;
    while (millis() < deadline) {
      int sz = LoRa.parsePacket();
      if (sz) {
        String resp;
        while (LoRa.available()) resp += char(LoRa.read());
        resp.trim();
        Serial.print("📥 Got DL: "); Serial.println(resp);

        if (resp == String("ACK:") + DEVICE_ID) {
          return true;
        }
        // also handle locator or interval‐set commands here…
      }
    }

    Serial.println("⚠️  No ACK, backing off…");
    delay(backoff);
    backoff = min(backoff * 2, 20000UL);
  }

  // leave the radio in RX so you still catch commands
  LoRa.receive();
  return false;
}

// — beepLocate: tada! —
static void beepLocate() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(150);
  }
}

// — PDM + EI audio plumbing ——
static void pdm_data_ready_callback() {
  int bytes = PDM.available();
  bytes = PDM.read((char*)sampleBuffer, bytes);
  if (!record_ready) return;
  int16_t *dst = inference.buffers[inference.buf_select];
  int      n   = bytes >> 1;
  for (int i = 0; i < n; i++) {
    dst[inference.buf_count++] = sampleBuffer[i];
    if (inference.buf_count >= inference.n_samples) {
      inference.buf_select ^= 1;
      inference.buf_count   = 0;
      inference.buf_ready   = 1;
    }
  }
}

static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffers[0] = (int16_t*)malloc(n_samples * sizeof(int16_t));
  inference.buffers[1] = (int16_t*)malloc(n_samples * sizeof(int16_t));
  sampleBuffer         = (int16_t*)malloc((n_samples>>1) * sizeof(int16_t));
  if (!inference.buffers[0] || !inference.buffers[1] || !sampleBuffer)
    return false;
  inference.buf_select = 0;
  inference.buf_count  = 0;
  inference.n_samples  = n_samples;
  inference.buf_ready  = 0;
  PDM.onReceive(pdm_data_ready_callback);
  PDM.setBufferSize((n_samples>>1) * sizeof(int16_t));
  if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
    ei_printf("🛑 PDM begin failed\n");
    return false;
  }
  record_ready = true;
  return true;
}

static bool microphone_inference_record() {
  while (!inference.buf_ready) delay(1);
  inference.buf_ready = 0;
  return true;
}

static int microphone_audio_signal_get_data(
    size_t offset, size_t length, float *out_ptr) {
  int16_t *src = inference.buffers[inference.buf_select ^ 1] + offset;
  numpy::int16_to_float(src, out_ptr, length);
  return 0;
}
