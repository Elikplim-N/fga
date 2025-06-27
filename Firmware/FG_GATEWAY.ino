#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>

// ───── CONFIG ──────────────────────────────────────────────────────────────
// Wi-Fi credentials
const char* WIFI_SSID = "E";
const char* WIFI_PASS = "qqqqqqqq";

// Supabase REST endpoints & key
const char* SUPA_BASE   = "https://xhxzpucbjqtxjvggeigw.supabase.co";
const char* DEVICES_URL = "/rest/v1/devices";
const char* ALERTS_URL  = "/rest/v1/alerts";
const char* SUPA_KEY    = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
                         "2HnZGjoZilWVr2S2N-ROqOSNp1XgRBzp6aExPS4EZeQ";

// LoRa (VSPI)
const int LORA_SS   = 5;
const int LORA_RST  = 14;
const int LORA_DIO0 = 26;
const long LORA_FREQ = 433E6;
const int LORA_SF   = 12;
const long LORA_BW  = 125E3;
const int LORA_CR   = 5;
const int LORA_TXP  = 20;

// SIM800L (UART2)
#define SIM_RX_PIN 16
#define SIM_TX_PIN 17
const char* SIM_APN  = "your.apn.here";
const char* SIM_USER = "";
const char* SIM_PASS = "";

// Hardware serial for SIM800L
HardwareSerial sim800(2);

// ────── UTILS ───────────────────────────────────────────────────────────────
// Wi-Fi connect (blocking)
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
  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
}

// Wait for a single "OK" from SIM800L, or timeout
bool simWaitOK(unsigned long timeout = 2000) {
  unsigned long t = millis();
  while (millis() - t < timeout) {
    if (sim800.available()) {
      String line = sim800.readStringUntil('\n');
      line.trim();
      if (line == "OK")   return true;
      if (line == "ERROR") return false;
    }
  }
  return false;
}

// Wait for '>' prompt after AT+HTTPDATA
bool simWaitPrompt(unsigned long timeout = 2000) {
  unsigned long t = millis();
  while (millis() - t < timeout) {
    if (sim800.available() && (char)sim800.read() == '>') return true;
  }
  return false;
}

// Read all available chars into a String (with timeout)
String simReadAll(unsigned long timeout = 3000) {
  String resp;
  unsigned long t = millis();
  while (millis() - t < timeout) {
    while (sim800.available()) resp += char(sim800.read());
    delay(50);
  }
  return resp;
}

// Extract HTTP response code from "+HTTPACTION: 1,<code>,"
int simParseHTTPCode(const String &resp) {
  int idx = resp.indexOf("1,");
  if (idx >= 0) {
    int comma = resp.indexOf(',', idx+2);
    if (comma >= 0) {
      return resp.substring(idx+2, comma).toInt();
    }
  }
  return -1;
}

// Initialize SIM800L GPRS bearer and HTTP service
void simInit() {
  sim800.begin(9600, SERIAL_8N1, SIM_TX_PIN, SIM_RX_PIN);
  delay(1000);
  sim800.println("AT");         simWaitOK();
  sim800.println("ATE0");       simWaitOK();  // echo off
  sim800.println("AT+CFUN=1");  simWaitOK();  // full functionality
  sim800.println("AT+CPIN?");   simWaitOK();
  sim800.println("AT+CREG?");   simWaitOK();
  sim800.println("AT+CGATT=1"); simWaitOK();
  sim800.print  ("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r"); simWaitOK();
  sim800.print  ("AT+SAPBR=3,1,\"APN\",\""); sim800.print(SIM_APN); sim800.println("\""); simWaitOK();
  if (*SIM_USER) {
    sim800.print("AT+SAPBR=3,1,\"USER\",\""); sim800.print(SIM_USER); sim800.println("\""); simWaitOK();
    sim800.print("AT+SAPBR=3,1,\"PWD\",\"");  sim800.print(SIM_PASS); sim800.println("\""); simWaitOK();
  }
  sim800.println("AT+SAPBR=1,1");   simWaitOK(10000);
  sim800.println("AT+HTTPINIT");    simWaitOK();
}

// Perform an HTTP POST via SIM800L, return HTTP status code or -1
int simHTTPPostCode(const String &url, const String &payload) {
  sim800.println("AT+HTTPTERM"); simWaitOK();      // teardown if needed
  sim800.println("AT+HTTPINIT");  simWaitOK();
  sim800.println("AT+HTTPPARA=\"CID\",1");        simWaitOK();
  sim800.print  ("AT+HTTPPARA=\"URL\",\""); sim800.print(url); sim800.println("\""); simWaitOK();
  sim800.println("AT+HTTPPARA=\"CONTENT\",\"application/json\""); simWaitOK();
  sim800.print  ("AT+HTTPDATA="); sim800.print(payload.length()); sim800.println(",20000");
  if (!simWaitPrompt(5000)) return -1;
  sim800.print(payload);
  if (!simWaitOK(20000)) return -1;
  sim800.println("AT+HTTPACTION=1");  // POST
  delay(5000);
  String resp = simReadAll(5000);
  sim800.println("AT+HTTPTERM");     simWaitOK();
  return simParseHTTPCode(resp);
}

// Unified POST: Wi-Fi first, else SIM800L
bool doPost(const String &path, const String &body, int &outCode) {
  // 1) Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    httpBegin(http, path);
    outCode = http.POST(body);
    http.end();
    if (outCode >= 200 && outCode < 300) return true;
    // else fall through to SIM
  }
  // 2) SIM fallback
  String fullUrl = String(SUPA_BASE) + path;
  outCode = simHTTPPostCode(fullUrl, body);
  return (outCode >= 200 && outCode < 300);
}

// ───── UPSELL DEVICE ───────────────────────────────────────────────────────
String ensureDevice(const String &devId, const String &devName) {
  // JSON for upsert
  StaticJsonDocument<128> doc;
  JsonArray arr = doc.to<JsonArray>();
  JsonObject o = arr.createNestedObject();
  o["device_id"] = devId;
  o["name"]      = devName;
  String body; serializeJson(doc, body);

  // POST /devices?on_conflict=device_id
  String path = String(DEVICES_URL) + "?on_conflict=device_id";
  int code;
  if (doPost(path, body, code)) {
    // parse returned JSON
    HTTPClient dummy; String dummyResp; // we need a temp buffer
    // For SIM fallback we don't capture resp body, so only handle Wi-Fi case
    // Easiest: always re-GET via Wi-Fi if possible
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      httpBegin(http, path + "&select=id");
      int c2 = http.GET();
      String r2 = http.getString();
      http.end();
      if (c2 == 200) {
        StaticJsonDocument<128> rd2;
        if (!deserializeJson(rd2, r2) && rd2.is<JsonArray>() && rd2.size()>0) {
          return rd2[0]["id"].as<String>();
        }
      }
    }
    // fallback, return empty
    return String();
  }
  // on failure
  Serial.printf("❌ Device upsert failed (%d)\n", code);
  return String();
}

// ───── POST ALERT ─────────────────────────────────────────────────────────
bool postAlert(const String &uuid,
               const String &atype,
               int           confidence,
               double        latitude,
               double        longitude,
               int           battPct,
               bool          vibration)
{
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

  int code;
  bool ok = doPost(ALERTS_URL, body, code);
  Serial.printf("POST %s → %d\n", ALERTS_URL, code);
  return ok;
}

// ───── SETUP ───────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);
  wifiConnect();
  simInit();

  // LoRa setup
  SPI.begin(18, 19, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("🛑 LoRa init failed");
    while (1);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TXP);

  // Continuous RX
  LoRa.receive();
  Serial.println("✅ Gateway ready (Wi-Fi + GSM fallback)");
}

// ───── LOOP ────────────────────────────────────────────────────────────────
void loop() {
  int sz = LoRa.parsePacket();
  if (sz == 0) {
    delay(10);
    return;
  }

  // Read JSON
  String raw;
  while (LoRa.available()) raw += char(LoRa.read());
  raw.trim();
  Serial.printf("📡 RX (%d): %s\n", sz, raw.c_str());

  StaticJsonDocument<256> jdoc;
  if (deserializeJson(jdoc, raw)) {
    Serial.println("⚠️ JSON parse error");
    return;
  }

  // Extract fields
  String devId   = jdoc["device_id"].as<String>();
  String devName = jdoc["name"].as<String>();
  String atype   = jdoc["alert_type"].as<String>();
  int    conf    = jdoc["confidence"].as<int>();
  double lat     = jdoc["latitude"].as<double>();
  double lon     = jdoc["longitude"].as<double>();
  int    batt    = jdoc["battery_level"].as<int>();
  bool   vib     = jdoc["vibration"].as<bool>();

  // ACK immediately
  LoRa.idle();
  LoRa.beginPacket();
    LoRa.print("ACK:"); LoRa.print(devId);
  LoRa.endPacket();
  Serial.println("↩️ ACK sent: " + devId);
  LoRa.receive();

  // Upsert the device
  String uuid = ensureDevice(devId, devName);
  if (uuid.isEmpty()) {
    Serial.println("❌ ensureDevice failed");
    return;
  }

  // Post the alert
  if (!postAlert(uuid, atype, conf, lat, lon, batt, vib)) {
    Serial.println("❌ postAlert failed");
  } else {
    Serial.println("✅ Alert saved");
  }
}

