/**
 * chair_node.ino
 * How Muddy? — Chair Node Firmware (RSSI-based table assignment)
 *
 * Hardware:  XIAO ESP32-C3 + FSR + 10kΩ resistor + 1S LiPo
 * Protocol:  ESP-NOW — listens for table beacons, sends to strongest
 *
 * Behavior:
 *   1. Wake up (or boot).
 *   2. Listen briefly for table beacons; pick strongest by RSSI.
 *   3. Power on FSR, take DEBOUNCE_SAMPLES ADC readings.
 *   4. Determine if seat is occupied.
 *   5. Send OccupancyPacket via ESP-NOW to assigned table.
 *   6. Sleep for REPORT_INTERVAL_MS or deep-sleep if off-hours.
 *
 * 6.1820 Mobile and Sensor Computing — Spring 2026
 * Ria Verma · Elaine Wang · Eileen Zu
 */

 #include <Arduino.h>
 #include <esp_now.h>
 #include <WiFi.h>
 #include <esp_sleep.h>
 #include <time.h>
 #include "config.h"
 
 // ─── Packet Definitions ───────────────────────────────────────────────────
 // Beacon sent BY tables, received by chairs.
 struct TableBeacon {
   uint8_t  table_id;
   uint8_t  mac[6];   // table's own MAC, so chair can reply directly
 };
 
 // Report sent BY chairs to their assigned table.
 struct OccupancyPacket {
   char     chair_id[8];     // e.g. "T1C1\0"
   uint8_t  table_id;        // which table chair thinks it belongs to
   bool     occupied;        // true = person seated
   uint16_t adc_raw;         // raw ADC value for debugging
   int8_t   rssi;            // RSSI of the chosen table beacon
   uint32_t uptime_ms;       // ms since last boot
   uint8_t  battery_pct;     // 0–100
 };
 
 // ─── Table Tracking ───────────────────────────────────────────────────────
 #define MAX_TABLES        8
 #define BEACON_LISTEN_MS  500    // listen this long before deciding
 #define STALE_MS          3000
 
 struct TableEntry {
   uint8_t  table_id;
   uint8_t  mac[6];
   int      rssi_avg;
   uint32_t last_heard_ms;
   bool     active;
   bool     peer_added;
 };
 
 static TableEntry g_tables[MAX_TABLES];
 static bool       g_sendComplete = false;
 static bool       g_sendSuccess  = false;
 
 // ─── ESP-NOW callbacks ────────────────────────────────────────────────────
 void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
   g_sendSuccess  = (status == ESP_NOW_SEND_SUCCESS);
   g_sendComplete = true;
 #if DEBUG_ENABLED
   Serial.printf("[ESP-NOW] Send %s\n", g_sendSuccess ? "OK" : "FAIL");
 #endif
 }
 
 void onBeacon(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
   if (len < (int)sizeof(TableBeacon)) return;
   TableBeacon b;
   memcpy(&b, data, sizeof(b));
   int rssi = info->rx_ctrl->rssi;
 
   // Find existing slot for this table_id, else first free slot
   int slot = -1;
   for (int i = 0; i < MAX_TABLES; i++) {
     if (g_tables[i].active && g_tables[i].table_id == b.table_id) { slot = i; break; }
   }
   if (slot == -1) {
     for (int i = 0; i < MAX_TABLES; i++) {
       if (!g_tables[i].active) { slot = i; break; }
     }
   }
   if (slot == -1) return;
 
   if (!g_tables[slot].active) {
     g_tables[slot].rssi_avg = rssi;
     g_tables[slot].active   = true;
     g_tables[slot].table_id = b.table_id;
     memcpy(g_tables[slot].mac, b.mac, 6);
     g_tables[slot].peer_added = false;
 
     // Add as ESP-NOW peer so we can send back
     esp_now_peer_info_t peer{};
     memcpy(peer.peer_addr, b.mac, 6);
     peer.channel = ESPNOW_CHANNEL;
     peer.encrypt = false;
     if (esp_now_add_peer(&peer) == ESP_OK) g_tables[slot].peer_added = true;
   } else {
     // Exponential moving average to smooth RSSI
     g_tables[slot].rssi_avg = (g_tables[slot].rssi_avg * 7 + rssi) / 8;
   }
   g_tables[slot].last_heard_ms = millis();
 }
 
 /** Return index of strongest currently-fresh table, or -1 if none. */
 int pickBestTable() {
   uint32_t now = millis();
   int best = -1;
   int bestRssi = -200;
   for (int i = 0; i < MAX_TABLES; i++) {
     if (!g_tables[i].active) continue;
     if (now - g_tables[i].last_heard_ms > STALE_MS) {
       g_tables[i].active = false;
       continue;
     }
     if (g_tables[i].rssi_avg > bestRssi) {
       bestRssi = g_tables[i].rssi_avg;
       best = i;
     }
   }
   return best;
 }
 
 // ─── FSR / ADC helpers (unchanged) ────────────────────────────────────────
 uint16_t readFSR() {
   digitalWrite(FSR_VCC_PIN, HIGH);
   delay(5);
   uint16_t val = analogRead(FSR_PIN);
   digitalWrite(FSR_VCC_PIN, LOW);
   return val;
 }
 
 bool debounceOccupancy(uint16_t &adc_out) {
   uint32_t sum = 0;
   uint8_t  above = 0;
   for (int i = 0; i < DEBOUNCE_SAMPLES; i++) {
     uint16_t v = readFSR();
     sum += v;
     if (v > OCCUPIED_THRESHOLD_ADC) above++;
     delay(SAMPLE_INTERVAL_MS);
   }
   adc_out = (uint16_t)(sum / DEBOUNCE_SAMPLES);
   return (above == DEBOUNCE_SAMPLES);
 }
 
 uint8_t estimateBatteryPct() {
   const uint16_t ADC_100 = 2606;
   const uint16_t ADC_0   = 1862;
   uint16_t raw = analogRead(A1);
   if (raw >= ADC_100) return 100;
   if (raw <= ADC_0)   return 0;
   return (uint8_t)(((uint32_t)(raw - ADC_0) * 100) / (ADC_100 - ADC_0));
 }
 
 // ─── Time helper (unchanged) ──────────────────────────────────────────────
 bool isDuringOperatingHours() {
   struct tm timeinfo;
   if (!getLocalTime(&timeinfo, 100)) return true;
   int h = timeinfo.tm_hour;
   return (h >= PUB_OPEN_HOUR && h < PUB_CLOSE_HOUR);
 }
 
 // ─── Setup ────────────────────────────────────────────────────────────────
 void setup() {
 #if DEBUG_ENABLED
   Serial.begin(SERIAL_BAUD);
   delay(100);
   Serial.printf("\n[Chair %s] Boot\n", CHAIR_ID);
 #endif
 
   pinMode(FSR_VCC_PIN, OUTPUT);
   digitalWrite(FSR_VCC_PIN, LOW);
   analogReadResolution(12);
 
   WiFi.mode(WIFI_STA);
   WiFi.disconnect();
 
   if (esp_now_init() != ESP_OK) {
 #if DEBUG_ENABLED
     Serial.println("[ESP-NOW] init failed — rebooting");
 #endif
     ESP.restart();
   }
   esp_now_register_send_cb(onDataSent);
   esp_now_register_recv_cb(onBeacon);
 
   for (int i = 0; i < MAX_TABLES; i++) g_tables[i].active = false;
 }
 
 // ─── Main Loop ────────────────────────────────────────────────────────────
 void loop() {
   // 1. Listen briefly for table beacons (callbacks fire in background).
   uint32_t listenStart = millis();
   while (millis() - listenStart < BEACON_LISTEN_MS) delay(10);
 
   // 2. Pick strongest table.
   int idx = pickBestTable();
   if (idx == -1) {
 #if DEBUG_ENABLED
     Serial.println("[Chair] No table beacon heard — skipping report");
 #endif
     delay(REPORT_INTERVAL_MS);
     return;
   }
 
   // 3. Read FSR.
   uint16_t adcVal = 0;
   bool occupied = debounceOccupancy(adcVal);
 
 #if DEBUG_ENABLED
   Serial.printf("[Chair %s] table=%u rssi=%d ADC=%u occ=%s\n",
                 CHAIR_ID,
                 g_tables[idx].table_id,
                 g_tables[idx].rssi_avg,
                 adcVal,
                 occupied ? "YES" : "NO");
 #endif
 
   // 4. Build & send packet to assigned table.
   OccupancyPacket pkt{};
   strncpy(pkt.chair_id, CHAIR_ID, sizeof(pkt.chair_id) - 1);
   pkt.table_id    = g_tables[idx].table_id;
   pkt.occupied    = occupied;
   pkt.adc_raw     = adcVal;
   pkt.rssi        = (int8_t)g_tables[idx].rssi_avg;
   pkt.uptime_ms   = millis();
   pkt.battery_pct = estimateBatteryPct();
 
   g_sendComplete = false;
   esp_now_send(g_tables[idx].mac, (uint8_t*)&pkt, sizeof(pkt));
   uint32_t t0 = millis();
   while (!g_sendComplete && (millis() - t0 < 200)) delay(10);
 
   // 5. Sleep strategy (unchanged).
   if (isDuringOperatingHours()) {
     delay(REPORT_INTERVAL_MS);
   } else {
 #if DEBUG_ENABLED
     Serial.println("[Chair] Off hours — entering deep sleep");
     Serial.flush();
 #endif
     esp_deep_sleep(SLEEP_DURATION_US);
   }
 }