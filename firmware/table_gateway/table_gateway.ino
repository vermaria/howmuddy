/**
 * table_gateway.ino
 * How Muddy? — Table Gateway Firmware
 *
 * Hardware:  XIAO ESP32-C3 (permanently powered via USB or LiPo + charger)
 *
 * Behavior:
 *   1. Broadcast a TableBeacon every BEACON_INTERVAL_MS so chairs can
 *      discover us and measure RSSI.
 *   2. Receive OccupancyPackets from chairs that have chosen this table
 *      (chair-side RSSI assignment).
 *   3. Track per-chair state with smoothed RSSI for telemetry.
 *   4. Every UPLOAD_INTERVAL_MS, POST aggregated occupancy JSON to backend.
 *
 * JSON payload (POST /api/report):
 * {
 *   "table_id": "T1",
 *   "chairs": [
 *     { "chair_id": "T1C1", "occupied": true,  "rssi": -52, "adc_raw": 2100, "battery_pct": 87 }
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
 #include <esp_wifi.h>
 #include <HTTPClient.h>
 #include <ArduinoJson.h>
 #include "config.h"
 
 #ifndef BEACON_INTERVAL_MS
 #define BEACON_INTERVAL_MS 200
 #endif
 
 #ifndef TABLE_ID_NUM
 #define TABLE_ID_NUM 1   // numeric form for the beacon (TABLE_ID stays as string for JSON)
 #endif
 
 // ─── Packet Definitions (must match chair_node) ──────────────────────────
 struct TableBeacon {
   uint8_t  table_id;
   uint8_t  mac[6];
 };
 
 struct OccupancyPacket {
   char     chair_id[8];
   uint8_t  table_id;        // which table the chair claims to belong to
   bool     occupied;
   uint16_t adc_raw;
   int8_t   rssi;            // RSSI the chair measured from us
   uint32_t uptime_ms;
   uint8_t  battery_pct;
 };
 
 // ─── Per-Chair State ─────────────────────────────────────────────────────
 struct ChairState {
   char     chair_id[8];
   bool     occupied;
   uint16_t adc_raw;
   uint8_t  battery_pct;
 
   // RSSI smoothing ring buffer (RSSI as seen at the gateway side)
   int8_t   rssi_buf[RSSI_SMOOTHING_N];
   uint8_t  rssi_idx;
   bool     rssi_filled;
   int8_t   rssi_smoothed;
 
   uint32_t last_seen_ms;
   bool     active;
 };
 
 // ─── Globals ─────────────────────────────────────────────────────────────
 static ChairState     g_chairs[MAX_CHAIRS];
 static uint8_t        g_chairCount = 0;
 static uint32_t       g_lastUpload = 0;
 static uint32_t       g_lastBeacon = 0;
 static uint8_t        g_selfMac[6];
 static portMUX_TYPE   g_mux = portMUX_INITIALIZER_UNLOCKED;
 
 static const uint8_t  BCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
 
 // ─── Helpers ─────────────────────────────────────────────────────────────
 ChairState *findOrCreateChair(const char *chair_id) {
   for (int i = 0; i < g_chairCount; i++) {
     if (strcmp(g_chairs[i].chair_id, chair_id) == 0) return &g_chairs[i];
   }
   if (g_chairCount >= MAX_CHAIRS) return nullptr;
   ChairState &s = g_chairs[g_chairCount++];
   memset(&s, 0, sizeof(s));
   strncpy(s.chair_id, chair_id, 7);
   s.active = true;
   return &s;
 }
 
 void updateRSSI(ChairState &s, int8_t rssi) {
   s.rssi_buf[s.rssi_idx] = rssi;
   s.rssi_idx = (s.rssi_idx + 1) % RSSI_SMOOTHING_N;
   if (s.rssi_idx == 0) s.rssi_filled = true;
 
   int sum = 0;
   int n   = s.rssi_filled ? RSSI_SMOOTHING_N : s.rssi_idx;
   if (n == 0) { s.rssi_smoothed = rssi; return; }
   for (int i = 0; i < n; i++) sum += s.rssi_buf[i];
   s.rssi_smoothed = (int8_t)(sum / n);
 }
 
 // ─── ESP-NOW receive callback ─────────────────────────────────────────────
 void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
   if (len != (int)sizeof(OccupancyPacket)) return;
 
   OccupancyPacket pkt;
   memcpy(&pkt, data, sizeof(pkt));
 
   // Trust the chair's RSSI-based assignment — only accept packets aimed at us.
   if (pkt.table_id != TABLE_ID_NUM) return;
 
   int8_t rssi = info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : -60;
 
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
   Serial.printf("[GW] Chair %s  occ=%d  adc=%u  rssi(gw)=%d  rssi(chair)=%d\n",
                 pkt.chair_id, pkt.occupied, pkt.adc_raw, rssi, pkt.rssi);
 #endif
 }
 
 // ─── Beacon ───────────────────────────────────────────────────────────────
 void sendBeacon() {
   TableBeacon b{};
   b.table_id = TABLE_ID_NUM;
   memcpy(b.mac, g_selfMac, 6);
   esp_now_send(BCAST_ADDR, (uint8_t*)&b, sizeof(b));
 }
 
 // ─── HTTP Upload ──────────────────────────────────────────────────────────
 void uploadOccupancy() {
   if (WiFi.status() != WL_CONNECTED) {
     WiFi.reconnect();
     return;
   }
 
   StaticJsonDocument<1024> doc;
   doc["table_id"]     = TABLE_ID;
   doc["timestamp_ms"] = (uint64_t)millis();
 
   JsonArray chairs = doc.createNestedArray("chairs");
 
   portENTER_CRITICAL(&g_mux);
   uint32_t now = millis();
   for (int i = 0; i < g_chairCount; i++) {
     ChairState &s = g_chairs[i];
     if ((now - s.last_seen_ms) > 30000) { s.active = false; continue; }
     if (!s.active) continue;
 
     JsonObject c     = chairs.createNestedObject();
     c["chair_id"]    = s.chair_id;
     c["occupied"]    = s.occupied;
     c["rssi"]        = s.rssi_smoothed;
     c["adc_raw"]     = s.adc_raw;
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
 
   // Connect to WiFi first so ESP-NOW pins itself to the AP's channel —
   // this guarantees chairs can hear our beacon if they ever join WiFi.
   WiFi.mode(WIFI_STA);
   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   uint32_t t0 = millis();
   while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000) delay(200);
 
   uint8_t channel = WiFi.channel();
 #if DEBUG_ENABLED
   Serial.printf("[GW] WiFi %s  IP: %s  channel: %u\n",
                 WiFi.status() == WL_CONNECTED ? "OK" : "FAIL",
                 WiFi.localIP().toString().c_str(),
                 channel);
 #endif
 
   WiFi.macAddress(g_selfMac);
 
   if (esp_now_init() != ESP_OK) {
 #if DEBUG_ENABLED
     Serial.println("[GW] ESP-NOW init failed");
 #endif
     ESP.restart();
   }
   esp_now_register_recv_cb(onDataRecv);
 
   // Add broadcast peer for beacons
   esp_now_peer_info_t bcast{};
   memcpy(bcast.peer_addr, BCAST_ADDR, 6);
   bcast.channel = channel;
   bcast.encrypt = false;
   esp_now_add_peer(&bcast);
 
 #if DEBUG_ENABLED
   Serial.printf("[GW] My MAC: %s\n", WiFi.macAddress().c_str());
   Serial.println("[GW] Ready, beaconing...");
 #endif
 }
 
 // ─── Main Loop ────────────────────────────────────────────────────────────
 void loop() {
   uint32_t now = millis();
 
   if (now - g_lastBeacon >= BEACON_INTERVAL_MS) {
     sendBeacon();
     g_lastBeacon = now;
   }
 
   if (now - g_lastUpload >= UPLOAD_INTERVAL_MS) {
     uploadOccupancy();
     g_lastUpload = now;
   }
 
   delay(20);
 }