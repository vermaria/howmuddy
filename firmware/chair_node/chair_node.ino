/**
 * chair_node.ino
 * How Muddy? — Chair Node Firmware
 *
 * Hardware:  XIAO ESP32-C3 + FSR + 10kΩ resistor + 1S LiPo
 * Protocol:  ESP-NOW broadcast to table gateway
 *
 * Behavior:
 *   1. Wake up (or boot).
 *   2. Power on FSR, take DEBOUNCE_SAMPLES ADC readings.
 *   3. Determine if seat is occupied (sustained pressure above threshold).
 *   4. Broadcast an OccupancyPacket via ESP-NOW.
 *   5. Sleep for REPORT_INTERVAL_MS (light sleep) or deep-sleep if off-hours.
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

// ─── Packet Definition ────────────────────────────────────────────────────
// Keep this struct identical in chair_node and table_gateway.
struct OccupancyPacket {
  char     chair_id[8];    // e.g. "T1C1\0"
  bool     occupied;       // true = person seated
  uint16_t adc_raw;        // raw ADC value for debugging
  uint32_t uptime_ms;      // ms since last boot
  uint8_t  battery_pct;    // 0–100, estimated from ADC on battery pin
};

// ─── Globals ──────────────────────────────────────────────────────────────
static uint8_t gatewayMac[] = GATEWAY_MAC;
static bool    g_sendComplete = false;
static bool    g_sendSuccess  = false;

// ─── ESP-NOW send callback ────────────────────────────────────────────────
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  g_sendSuccess  = (status == ESP_NOW_SEND_SUCCESS);
  g_sendComplete = true;
#if DEBUG_ENABLED
  Serial.printf("[ESP-NOW] Send %s\n", g_sendSuccess ? "OK" : "FAIL");
#endif
}

// ─── FSR / ADC helpers ────────────────────────────────────────────────────

/** Power on FSR rail, wait for settling, then read ADC. */
uint16_t readFSR() {
  digitalWrite(FSR_VCC_PIN, HIGH);
  delay(5);  // settle time
  uint16_t val = analogRead(FSR_PIN);
  digitalWrite(FSR_VCC_PIN, LOW);
  return val;
}

/**
 * Return true if DEBOUNCE_SAMPLES consecutive readings all exceed
 * OCCUPIED_THRESHOLD_ADC.  Averages the readings for the adc_out param.
 */
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

/** Estimate battery percentage from ADC reading on the VBAT pin. */
uint8_t estimateBatteryPct() {
  // XIAO ESP32-C3 exposes VBAT on A0 via a 50% divider; full = 4.2V → ~2.1V
  // ADC_RESOLUTION = 4095 @ 3.3V ref.  2.1V/3.3V * 4095 ≈ 2606 for 100%.
  // 3.0V cutoff → 1.5V measured → ADC ≈ 1862.
  const uint16_t ADC_100 = 2606;
  const uint16_t ADC_0   = 1862;
  uint16_t raw = analogRead(A1);  // dedicated battery pin on XIAO
  if (raw >= ADC_100) return 100;
  if (raw <= ADC_0)   return 0;
  return (uint8_t)(((uint32_t)(raw - ADC_0) * 100) / (ADC_100 - ADC_0));
}

// ─── Time helpers ─────────────────────────────────────────────────────────

/** Return true if current local hour is within pub operating hours. */
bool isDuringOperatingHours() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return true;  // default to active if NTP fails
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

  // Pin setup
  pinMode(FSR_VCC_PIN, OUTPUT);
  digitalWrite(FSR_VCC_PIN, LOW);
  analogReadResolution(12);

  // WiFi in STA mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Optionally sync NTP for operating-hours gating (connect to known AP briefly)
  // Omitted here to keep cold-start fast; gateway handles time sync.

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
#if DEBUG_ENABLED
    Serial.println("[ESP-NOW] init failed — rebooting");
#endif
    ESP.restart();
  }
  esp_now_register_send_cb(onDataSent);

  // Register peer (gateway or broadcast)
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, gatewayMac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt  = false;
  esp_now_add_peer(&peer);
}

// ─── Main Loop ────────────────────────────────────────────────────────────
void loop() {
  uint16_t adcVal  = 0;
  bool     occupied = debounceOccupancy(adcVal);

#if DEBUG_ENABLED
  Serial.printf("[Chair %s] ADC=%u  occupied=%s\n",
                CHAIR_ID, adcVal, occupied ? "YES" : "NO");
#endif

  // Build packet
  OccupancyPacket pkt{};
  strncpy(pkt.chair_id, CHAIR_ID, sizeof(pkt.chair_id) - 1);
  pkt.occupied   = occupied;
  pkt.adc_raw    = adcVal;
  pkt.uptime_ms  = millis();
  pkt.battery_pct = estimateBatteryPct();

  // Send via ESP-NOW
  g_sendComplete = false;
  esp_now_send(gatewayMac, (uint8_t*)&pkt, sizeof(pkt));

  // Wait for callback (up to 200 ms)
  uint32_t t0 = millis();
  while (!g_sendComplete && (millis() - t0 < 200)) delay(10);

  // Sleep strategy:
  //   - During hours: light sleep for REPORT_INTERVAL_MS (keeps ESP-NOW fast)
  //   - Off hours: deep sleep to save battery
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
