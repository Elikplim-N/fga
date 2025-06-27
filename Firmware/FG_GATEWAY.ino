#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>

// ───── CONFIG ──────────────────────────────────────────────────────────────
// Wi-Fi credentials
const char* WIFI_SSID = "E";
const char* WIFI_PASS = "qqqqqqqq";

// Supabase REST
const char* SUPA_BASE   = "https://xhxzpucbjqtxjvggeigw.supabase.co";
const char* DEVICES_URL = "/rest/v1/devices";
const char* ALERTS_URL  = "/rest/v1/alerts";
const char* SUPA_KEY    = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InhoeHpwdWNianF0eGp2Z2dlaWd3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDkyMjkwNDIsImV4cCI6MjA2NDgwNTA0Mn0.2HnZGjoZilWVr2S2N-ROqOSNp1XgRBzp6aExPS4EZeQ";



// LoRa (VSPI) pins & params
const int   LORA_SS   = 5;
const int   LORA_RST  = 14;
const int   LORA_DIO0 = 26;
const long  LORA_FREQ = 433E6;
const int   LORA_SF   = 12;
const long  LORA_BW   = 125E3;
const int   LORA_CR   = 5;
const int   LORA_TXP  = 20;

// SIM800L (for SMS alerts) UART pins
#define SIM_RX_PIN 16
#define SIM_TX_PIN 17

// The phone number to receive SMS alerts (E.164 format)
const char* SMS_RECIPIENT = "+233204618397";

// HardwareSerial instance for SIM800L on UART2
HardwareSerial sim800(2);

// ───── HELPERS ──────────────────────────────────────────────────────────────
// Connect to Wi-Fi (blocking)
void wifiConnect() {
  Serial.print("🔌 Wi-Fi “"); Serial.print(WIFI_SSID); Serial.print("”… ");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(" ✓");
}

// Prepare HTTPClient with Supabase headers
void httpBegin(HTTPClient &http, const String &path) {
  http.begin(String(SUPA_BASE) + path);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey",       SUPA_KEY);
  http.addHeader("Authorization","Bearer " + String(SUPA_KEY));
}

// Initialize SIM800L for SMS: echo off & text mode
void simInitSMS() {
  sim800.begin(9600, SERIAL_8N1, SIM_TX_PIN, SIM_RX_PIN);
  delay(1000);
  sim800.println("AT");       // basic
  delay(200); sim800.println("ATE0");  // echo off
  delay(200); sim800.println("AT+CMGF=1"); // SMS text mode
  delay(200); // no need to wait for OK in production
}

// Send an SMS via SIM800L; returns true on “OK”
bool sendSMS(const char* number, const String &text) {
  sim800.print("AT+CMGS=\""); sim800.print(number); sim800.println("\"");
  delay(200);
  // wait for '>' prompt
  unsigned long t = millis();
  while (millis() - t < 2000 && sim800.available()) {
    if (sim800.read() == '>') break;
  }
  sim800.print(text);
  sim800.write(26); // ASCII 26 = Ctrl+Z
  // wait for “+CMGS:” or “OK”
  t = millis();
  while (millis() - t < 10000) {
    if (sim800.available()) {
      String line = sim800.readStringUntil('\n');
      if (line.indexOf("+CMGS:") != -1) return true;
    }
  }
  return false;
}

// Upsert device in Supabase, return its UUID
String ensureDevice(const String &devId, const String &devName) {
  HTTPClient http;
  httpBegin(http, DEVICES_URL);
  http.addHeader("Prefer","resolution=merge-duplicates,return=representation");

  StaticJsonDocument<128> doc;
  JsonArray arr = doc.to<JsonArray>();
  JsonObject o = arr.createNestedObject();
  o["device_id"] = devId;
  o["name"]      = devName;
  String body; serializeJson(doc, body);

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  Serial.printf("POST %s → %d\n%s\n", DEVICES_URL, code, resp.c_str());
  if (code >= 200 && code < 300) {
    StaticJsonDocument<128> rd;
    if (!deserializeJson(rd, resp) && rd.is<JsonArray>() && rd.size()>0) {
      return rd[0]["id"].as<String>();
    }
  }

  // on conflict or error, try GET
  HTTPClient http2;
  String url = String(DEVICES_URL)
             + "?select=id&device_id=eq." + devId;
  httpBegin(http2, url);
  int c2 = http2.GET();
  String r2 = http2.getString();
  http2.end();
  Serial.printf("GET %s → %d\n%s\n", url.c_str(), c2, r2.c_str());
  if (c2 == 200) {
    StaticJsonDocument<128> rd2;
    if (!deserializeJson(rd2, r2) && rd2.is<JsonArray>() && rd2.size()>0) {
      return rd2[0]["id"].as<String>();
    }
  }

  return "";
}

// Post alert to Supabase; returns true on 2xx
bool postAlert(const String &uuid,
               const String &atype,
               int           confidence,
               double        latitude,
               double        longitude,
               int           battPct,
               bool          vibration)
{
  HTTPClient http;
  httpBegin(http, ALERTS_URL);

  StaticJsonDocument<256> doc;
  JsonArray arr = doc.to<JsonArray>();
  JsonObject o = arr.createNestedObject();
  o["device_id"   ] = uuid;
  o["alert_type"  ] = atype;
  o["confidence"  ] = confidence;
  o["latitude"    ] = latitude;
  o["longitude"   ] = longitude;
  o["battery_level"] = battPct;
  o["vibration"   ] = vibration;
  String body; serializeJson(doc, body);

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  Serial.printf("POST %s → %d\n", ALERTS_URL, code);
  return (code >= 200 && code < 300);
}

// ───── SETUP ───────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);

  wifiConnect();
  simInitSMS();

  SPI.begin(18, 19, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("🛑 LoRa init failed"); while (1);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TXP);
  LoRa.receive();
  Serial.println("✅ Gateway ready (Wi-Fi + SMS alerts)");
}

// ───── LOOP ────────────────────────────────────────────────────────────────
void loop() {
  int sz = LoRa.parsePacket();
  if (sz == 0) {
    delay(10);
    return;
  }

  // 1) Read and parse JSON
  String raw;
  while (LoRa.available()) raw += char(LoRa.read());
  raw.trim();
  Serial.printf("📡 RX (%d): %s\n", sz, raw.c_str());

  StaticJsonDocument<256> jdoc;
  if (deserializeJson(jdoc, raw)) {
    Serial.println("⚠️ JSON parse error");
    return;
  }
  String devId   = jdoc["device_id"].as<String>();
  String devName = jdoc["name"].as<String>();
  String atype   = jdoc["alert_type"].as<String>();
  int    conf    = jdoc["confidence"].as<int>();
  double lat     = jdoc["latitude"].as<double>();
  double lon     = jdoc["longitude"].as<double>();
  int    batt    = jdoc["battery_level"].as<int>();
  bool   vib     = jdoc["vibration"].as<bool>();

  // 2) ACK immediately
  LoRa.idle();
  LoRa.beginPacket();
    LoRa.print("ACK:"); LoRa.print(devId);
  LoRa.endPacket();
  LoRa.receive();
  Serial.println("↩️ ACK sent: " + devId);

  // 3) Upsert device
  String uuid = ensureDevice(devId, devName);
  if (uuid.isEmpty()) {
    Serial.println("❌ ensureDevice failed");
    return;
  }

  // 4) Post alert
  if (postAlert(uuid, atype, conf, lat, lon, batt, vib)) {
    Serial.println("✅ Alert saved");
    // 5) Send SMS notification
    String sms = devName + " alerted: " + atype
               + " (" + String(conf) + "%)";
    if ( sendSMS(SMS_RECIPIENT, sms) ) {
      Serial.println("📩 SMS sent");
    } else {
      Serial.println("❌ SMS failed");
    }
  } else {
    Serial.println("❌ postAlert failed");
  }
}