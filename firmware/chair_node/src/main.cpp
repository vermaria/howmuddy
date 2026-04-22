/**
 * chair_node — How Muddy? seat occupancy sensor
 *
 * Hardware: Seeed XIAO ESP32-C3 + Force-Sensitive Resistor (FSR)
 *
 * Behavior:
 *   1. Reads FSR ADC every SAMPLE_INTERVAL_MS.
 *   2. Applies debounce logic to avoid false triggers from bags/coats.
 *   3. Every BROADCAST_INTERVAL_MS, broadcasts an OccupancyPacket via
 *      ESP-NOW to the broadcast address (all table gateways receive it).
 *   4. Table gateways use RSSI of the broadcast to assign the chair to
 *      the nearest table.
 *
 * Node ID is stored in NVS flash so each chair is uniquely identified
 * without recompiling. Flash at provisioning time via scripts/provision.py.
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>

#include "config.h"

// ── Packet definition ─────────────────────────────────────────────────────────
// Keep this in sync with table_gateway/include/protocol.h
struct __attribute__((packed)) OccupancyPacket {
    uint8_t  magic;           // PACKET_MAGIC
    char     node_id[16];     // e.g. "chair_03"
    uint8_t  occupied;        // 1 = occupied, 0 = empty
    uint16_t raw_adc;         // raw FSR ADC value for debugging
    uint32_t uptime_s;        // seconds since last boot
    uint8_t  battery_pct;     // 0–100 (255 = not measured)
};

// ── Globals ───────────────────────────────────────────────────────────────────
static char      g_node_id[16];
static bool      g_occupied       = false;
static int       g_occupy_ticks   = 0;
static int       g_vacate_ticks   = 0;
static uint32_t  g_last_broadcast = 0;
static uint32_t  g_last_sample    = 0;
static bool      g_send_success   = false;

static Preferences prefs;

// ── ESP-NOW callback ──────────────────────────────────────────────────────────
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    g_send_success = (status == ESP_NOW_SEND_SUCCESS);
    if (!g_send_success) {
        Serial.println("[ESP-NOW] Send failed (no gateway in range?)");
    }
}

// ── FSR helpers ───────────────────────────────────────────────────────────────
uint16_t readFSR() {
    uint32_t sum = 0;
    for (int i = 0; i < FSR_SAMPLES; i++) {
        sum += analogRead(FSR_PIN);
        delay(2);
    }
    return (uint16_t)(sum / FSR_SAMPLES);
}

/**
 * Simple voltage divider battery measurement.
 * Assumes a resistor divider on GPIO3 (A1) scaling 4.2V → 3.3V max.
 * Returns 0–100 or 255 if pin not connected.
 */
uint8_t readBatteryPercent() {
    // Adjust VDIV_RATIO if your divider differs.
    const float VDIV_RATIO = 4.2f / 3.3f;
    const float V_FULL = 4.2f;
    const float V_EMPTY = 3.0f;
    float raw = analogRead(3) * (3.3f / 4095.0f) * VDIV_RATIO;
    if (raw > V_FULL + 0.1f) return 255;  // not connected
    float pct = (raw - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f;
    return (uint8_t)constrain((int)pct, 0, 100);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[HowMuddy] Chair Node starting…");

    // Load node ID from NVS (set at provision time)
    prefs.begin("muddy", true);
    String id = prefs.getString("node_id", DEFAULT_NODE_ID);
    prefs.end();
    strncpy(g_node_id, id.c_str(), sizeof(g_node_id) - 1);
    Serial.printf("[HowMuddy] Node ID: %s\n", g_node_id);

    // Configure ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // 0–3.3 V range

    // WiFi in STA mode (needed for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed — rebooting in 5 s");
        delay(5000);
        ESP.restart();
    }
    esp_now_register_send_cb(onDataSent);

    // Register broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESP-NOW] Failed to add broadcast peer");
    }

    Serial.println("[HowMuddy] Setup complete.");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // ── Sample FSR ────────────────────────────────────────────────────────────
    if (now - g_last_sample >= SAMPLE_INTERVAL_MS) {
        g_last_sample = now;
        uint16_t adc = readFSR();

        bool raw_occupied = adc >= FSR_OCCUPIED_THRESHOLD;
        bool raw_empty    = adc <= FSR_EMPTY_THRESHOLD;

        if (raw_occupied) {
            g_vacate_ticks = 0;
            g_occupy_ticks++;
            if (g_occupy_ticks >= OCCUPY_DEBOUNCE_TICKS && !g_occupied) {
                g_occupied = true;
                Serial.printf("[FSR] %s → OCCUPIED (adc=%d)\n", g_node_id, adc);
            }
        } else if (raw_empty) {
            g_occupy_ticks = 0;
            g_vacate_ticks++;
            if (g_vacate_ticks >= VACATE_DEBOUNCE_TICKS && g_occupied) {
                g_occupied = false;
                Serial.printf("[FSR] %s → EMPTY (adc=%d)\n", g_node_id, adc);
            }
        }
        // Readings in the dead zone don't change state (hysteresis)
    }

    // ── Broadcast occupancy packet ─────────────────────────────────────────
    if (now - g_last_broadcast >= BROADCAST_INTERVAL_MS) {
        g_last_broadcast = now;

        OccupancyPacket pkt = {};
        pkt.magic      = 0xAD;   // 0xMD doesn't parse as hex; use 0xAD ('muddy')
        strncpy(pkt.node_id, g_node_id, sizeof(pkt.node_id) - 1);
        pkt.occupied   = g_occupied ? 1 : 0;
        pkt.raw_adc    = readFSR();
        pkt.uptime_s   = now / 1000;
        pkt.battery_pct = readBatteryPercent();

        esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t *)&pkt, sizeof(pkt));
        if (result != ESP_OK) {
            Serial.printf("[ESP-NOW] Send error: 0x%X\n", result);
        } else {
            Serial.printf("[ESP-NOW] Sent: id=%s occ=%d adc=%d batt=%d%%\n",
                          pkt.node_id, pkt.occupied, pkt.raw_adc, pkt.battery_pct);
        }
    }

#if DEEP_SLEEP_ENABLED
    Serial.flush();
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
    esp_deep_sleep_start();
#endif
}
