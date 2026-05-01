#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

struct TableBeacon { 
  uint8_t table_id; 
  uint8_t mac[6]; 
};

struct OccupancyPacket {
  char chair_id[8]; 
  uint8_t table_id; 
  bool occupied;
  uint16_t adc_raw; 
  int8_t rssi; 
  uint32_t uptime_ms; 
  uint8_t battery_pct;
};

// State tracking for backend upload 
struct ChairState {
  char chair_id[8];
  bool occupied;
  uint16_t adc_raw;
  int8_t rssi_smoothed;
  uint32_t last_seen_ms;
  bool active;
};

// Global variables
static uint8_t g_selfMac[6];
static const uint8_t BCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static ChairState g_chairs[MAX_CHAIRS];
static uint8_t g_chairCount = 0;
static uint32_t g_lastUpload = 0;

// Helper to find or create a chair entry in our local list 
ChairState *findOrCreateChair(const char *chair_id) {
  for (int i = 0; i < g_chairCount; i++) {
    if (strcmp(g_chairs[i].chair_id, chair_id) == 0) return &g_chairs[i];
  }
  if (g_chairCount >= MAX_CHAIRS) return nullptr;
  ChairState &s = g_chairs[g_chairCount++];
  strncpy(s.chair_id, chair_id, 7);
  s.active = true;
  return &s;
}

// Receive callback: Store incoming chair data for the next upload cycle 
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(OccupancyPacket)) return;
  
  OccupancyPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  
  // Only accept if aimed at this table 
  if (pkt.table_id != TABLE_ID_NUM) return;

  ChairState *s = findOrCreateChair(pkt.chair_id);
  if (s) {
    s->occupied = pkt.occupied;
    s->adc_raw = pkt.adc_raw;
    s->rssi_smoothed = info->rx_ctrl->rssi; // Use direct HW RSSI 
    s->last_seen_ms = millis();
    s->active = true;
  }
}

// HTTP Upload to Flask Backend 
void uploadToBackend() {
  if (WiFi.status() != WL_CONNECTED) return;

  StaticJsonDocument<1024> doc;
  doc["table_id"] = TABLE_ID;
  doc["timestamp_ms"] = (uint64_t)millis();
  JsonArray chairsArr = doc.createNestedArray("chairs");

  uint32_t now = millis();
  for (int i = 0; i < g_chairCount; i++) {
    if (!g_chairs[i].active) continue;
    // Mark inactive if not seen for 30 seconds 
    if (now - g_chairs[i].last_seen_ms > 30000) { g_chairs[i].active = false; continue; }

    JsonObject c = chairsArr.createNestedObject();
    c["chair_id"] = g_chairs[i].chair_id;
    c["occupied"] = g_chairs[i].occupied;
    c["rssi"] = g_chairs[i].rssi_smoothed;
    c["adc_raw"] = g_chairs[i].adc_raw;
  }

  String body;
  serializeJson(doc, body);
  String url = String("http://") + API_HOST + ":" + API_PORT + API_PATH; 

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  
  #if DEBUG_ENABLED
    Serial.printf("[GW] Uploaded to %s -> Status: %d\n", url.c_str(), code);
  #endif
  http.end();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin("MIT", "p$QfHpUP1d"); 
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  int homeChannel = WiFi.channel();
  WiFi.macAddress(g_selfMac);

  if (esp_now_init() != ESP_OK) ESP.restart();
  esp_now_register_recv_cb(onDataRecv);

  // Match Peer Channel to MIT WiFi Home Channel 
  esp_now_peer_info_t bcast{};
  memcpy(bcast.peer_addr, BCAST_ADDR, 6);
  bcast.channel = homeChannel; 
  bcast.encrypt = false;
  esp_now_add_peer(&bcast);

  Serial.printf("[GW] System Live. MIT Channel: %u | IP: %s\n", homeChannel, WiFi.localIP().toString().c_str()); // use whatever channel is reported here to connect chair
}

void loop() {
  uint32_t now = millis();

  // 1. Broadcast Beacon for Chairs
  static uint32_t lastBeacon = 0;
  if (now - lastBeacon > 200) {
    TableBeacon b = {TABLE_ID_NUM, {0}}; 
    memcpy(b.mac, g_selfMac, 6);
    esp_now_send(BCAST_ADDR, (uint8_t*)&b, sizeof(b));
    lastBeacon = now;
  }

  // 2. Periodic Backend Upload
  if (now - g_lastUpload > UPLOAD_INTERVAL_MS) {
    uploadToBackend();
    g_lastUpload = now;
  }
}
