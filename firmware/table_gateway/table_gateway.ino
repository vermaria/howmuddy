/**
 * table_gateway.ino
 * How Muddy? — Table Gateway Firmware
 *
 * Hardware:  XIAO ESP32-C3 (permanently powered via USB or LiPo + charger)
 *
 * Behavior:
 *   1. Receive OccupancyPackets from nearby chair nodes via ESP-NOW.
 *   2. Track RSSI per chair; claim chairs whose smoothed RSSI > threshold.
 *   3. Every UPLOAD_INTERVAL_MS, POST aggregated occupancy JSON to the backend.
 *
 * JSON payload (POST /api/report):
 * {
 *   "table_id": "T1",
 *   "chairs": [
 *     { "chair_id": "T1C1", "occupied": true,  "rssi": -52, "adc_raw": 2100 },
 *     { "chair_id": "T1C2", "occupied": false, "rssi": -49, "adc_raw":  300 }
 *   ],
 *   "timestamp_ms": 1713800000000
 * }
 *
 * 6.1820 Mobile and Sensor Computing — Spring 2026
 * Ria Verma · Elaine Wang · Eileen Zu
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── Packet (must match chair_node) ──────────────────────────────────────
struct OccupancyPacket {
  char     chair_id[8];
  bool     occupied;
  uint16_t adc_raw;
  uint32_t uptime_ms;
  uint8_t  battery_pct;
};

// ─── Per-Chair State ─────────────────────────────────────────────────────
struct ChairState {
  char     chair_id[8];
  bool     occupied;
  uint16_t adc_raw;
  uint8_t  battery_pct;

  // RSSI smoothing ring buffer
  int8_t   rssi_buf[RSSI_SMOOTHING_N];
  uint8_t  rssi_idx;
  bool     rssi_filled;
  int8_t   rssi_smoothed;

  // Reassignment hysteresis
  int8_t   best_rssi_seen;
  uint8_t  challenge_count;

  uint32_t last_seen_ms;
  bool     active;          // true = we've heard from this chair recently
};

// ─── Globals ─────────────────────────────────────────────────────────────
static ChairState g_chairs[MAX_CHAIRS];
static uint8_t    g_chairCount = 0;
static uint32_t   g_lastUpload = 0;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

// ─── Helpers ─────────────────────────────────────────────────────────────

ChairState *findOrCreateChair(const char *chair_id) {
  for (int i = 0; i < g_chairCount; i++) {
    if (strcmp(g_chairs[i].chair_id, chair_id) == 0) return &g_chairs[i];
  }
  if (g_chairCount >= MAX_CHAIRS) return nullptr;
  ChairState &s = g_chairs[g_chairCount++];
  memset(&s, 0, sizeof(s));
  strncpy(s.chair_id, chair_id, 7);
  s.best_rssi_seen = -127;
  s.active = true;
  return &s;
}

void updateRSSI(ChairState &s, int8_t rssi) {
  s.rssi_buf[s.rssi_idx] = rssi;
  s.rssi_idx = (s.rssi_idx + 1) % RSSI_SMOOTHING_N;
  if (s.rssi_idx == 0) s.rssi_filled = true;

  // Compute average
  int sum = 0;
  int n   = s.rssi_filled ? RSSI_SMOOTHING_N : s.rssi_idx;
  if (n == 0) { s.rssi_smoothed = rssi; return; }
  for (int i = 0; i < n; i++) sum += s.rssi_buf[i];
  s.rssi_smoothed = (int8_t)(sum / n);
}

bool claimChair(const ChairState &s) {
  return s.rssi_smoothed >= RSSI_CLAIM_THRESHOLD;
}

// ─── ESP-NOW receive callback ─────────────────────────────────────────────
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(OccupancyPacket)) return;

  OccupancyPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  // RSSI of the packet is available via esp_wifi_sta_get_rssi or from
  // the promiscuous callback; here we use a simplified approach where
  // the gateway's ESP-NOW stack provides it via esp_now_recv_info in IDF 5+.
  // For IDF 4 compatibility, we pass 0 and rely on previously seen RSSI.
  int8_t rssi = wifi_pkt_rx_ctrl_rssi;  // populated by the driver on IDF 5+
  // Fallback: -60 dBm (nearby assumption) if driver doesn't populate it
  if (rssi == 0) rssi = -60;

  portENTER_CRITICAL_ISR(&g_mux);
  ChairState *s = findOrCreateChair(pkt.chair_id);
  if (s) {
    s->occupied     = pkt.occupied;
    s->adc_raw      = pkt.adc_raw;
    s->battery_pct  = pkt.battery_pct;
    s->last_seen_ms = millis();
    s->active       = true;
    updateRSSI(*s, rssi);
  }
  portEXIT_CRITICAL_ISR(&g_mux);

#if DEBUG_ENABLED
  Serial.printf("[GW] Chair %s  occ=%d  adc=%u  rssi=%d\n",
                pkt.chair_id, pkt.occupied, pkt.adc_raw, rssi);
#endif
}

// ─── HTTP Upload ──────────────────────────────────────────────────────────

void uploadOccupancy() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }

  // Build JSON
  StaticJsonDocument<1024> doc;
  doc["table_id"]      = TABLE_ID;
  doc["timestamp_ms"]  = (uint64_t)millis();  // relative; backend uses server time too

  JsonArray chairs = doc.createNestedArray("chairs");

  portENTER_CRITICAL(&g_mux);
  uint32_t now = millis();
  for (int i = 0; i < g_chairCount; i++) {
    ChairState &s = g_chairs[i];
    // Mark stale chairs (no packet in 30 s) as inactive
    if ((now - s.last_seen_ms) > 30000) { s.active = false; continue; }
    if (!claimChair(s)) continue;  // chair belongs to another table

    JsonObject c  = chairs.createNestedObject();
    c["chair_id"] = s.chair_id;
    c["occupied"] = s.occupied;
    c["rssi"]     = s.rssi_smoothed;
    c["adc_raw"]  = s.adc_raw;
    c["battery_pct"] = s.battery_pct;
  }
  portEXIT_CRITICAL(&g_mux);

  String body;
  serializeJson(doc, body);

  String url = String("http://") + API_HOST + ":" + API_PORT + API_PATH;
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);

#if DEBUG_ENABLED
  Serial.printf("[GW] POST %s  → %d\n", url.c_str(), code);
  if (code > 0) Serial.println(http.getString());
#endif

  http.end();
}

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
#if DEBUG_ENABLED
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.printf("\n[Gateway %s] Boot\n", TABLE_ID);
#endif

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait up to 10 s for WiFi (non-blocking in production, blocking here for clarity)
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000) delay(200);
#if DEBUG_ENABLED
  Serial.printf("[GW] WiFi %s  IP: %s\n",
                WiFi.status() == WL_CONNECTED ? "OK" : "FAIL",
                WiFi.localIP().toString().c_str());
#endif

  // ESP-NOW (operates on the same STA interface)
  if (esp_now_init() != ESP_OK) {
#if DEBUG_ENABLED
    Serial.println("[GW] ESP-NOW init failed");
#endif
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);

#if DEBUG_ENABLED
  Serial.printf("[GW] My MAC: %s\n", WiFi.macAddress().c_str());
  Serial.println("[GW] Ready");
#endif
}

// ─── Main Loop ────────────────────────────────────────────────────────────
void loop() {
  if ((millis() - g_lastUpload) >= UPLOAD_INTERVAL_MS) {
    uploadOccupancy();
    g_lastUpload = millis();
  }
  delay(50);
}
