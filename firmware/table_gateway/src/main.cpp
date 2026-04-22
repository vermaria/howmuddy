/**
 * table_gateway — How Muddy? table hub
 *
 * Hardware: Seeed XIAO ESP32-C3
 *
 * Behavior:
 *   1. Listens for ESP-NOW OccupancyPacket broadcasts from chair nodes.
 *   2. Uses RSSI of each received packet to decide whether to "claim" the chair.
 *      A chair is assigned to the gateway with the strongest RSSI signal.
 *      Since gateways can't directly compare RSSI from each other, we use
 *      a threshold approach: accept chairs above RSSI_OWN_THRESHOLD and
 *      evict chairs that haven't been heard recently.
 *   3. Every PUBLISH_INTERVAL_MS, publishes a JSON payload to MQTT:
 *
 *      Topic:   howmuddy/occupancy/<table_id>
 *      Payload: {
 *        "table_id": "table_01",
 *        "timestamp": 1713800000,
 *        "chairs": [
 *          {"id": "chair_03", "occupied": true,  "rssi": -52, "battery_pct": 87},
 *          {"id": "chair_07", "occupied": false, "rssi": -61, "battery_pct": 91}
 *        ],
 *        "occupied_count": 1,
 *        "total_count": 2
 *      }
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "protocol.h"

// ── Chair record ──────────────────────────────────────────────────────────────
struct ChairRecord {
    char     id[16];
    bool     occupied;
    int      rssi;
    uint8_t  battery_pct;
    uint32_t last_seen_ms;
    bool     active;
};

// ── Globals ───────────────────────────────────────────────────────────────────
static char         g_table_id[16];
static ChairRecord  g_chairs[MAX_CHAIRS];

static WiFiClient   g_wifi_client;
static PubSubClient g_mqtt(g_wifi_client);

static uint32_t g_last_publish     = 0;
static uint32_t g_last_wifi_check  = 0;
static uint32_t g_last_mqtt_check  = 0;

static Preferences prefs;

// ── ESP-NOW receive callback ──────────────────────────────────────────────────
// Called from WiFi interrupt context — keep it fast.
void IRAM_ATTR onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(OccupancyPacket)) return;
    const OccupancyPacket *pkt = reinterpret_cast<const OccupancyPacket *>(data);
    if (pkt->magic != PACKET_MAGIC) return;

    int rssi = info->rx_ctrl->rssi;

    // Ignore chairs that are too far away
    if (rssi < RSSI_OWN_THRESHOLD) return;

    // Find existing record or empty slot
    int slot = -1;
    for (int i = 0; i < MAX_CHAIRS; i++) {
        if (g_chairs[i].active && strncmp(g_chairs[i].id, pkt->node_id, 15) == 0) {
            slot = i;
            break;
        }
        if (!g_chairs[i].active && slot == -1) {
            slot = i;
        }
    }
    if (slot == -1) return;  // table is full

    strncpy(g_chairs[slot].id, pkt->node_id, 15);
    g_chairs[slot].occupied     = pkt->occupied;
    g_chairs[slot].rssi         = rssi;
    g_chairs[slot].battery_pct  = pkt->battery_pct;
    g_chairs[slot].last_seen_ms = millis();
    g_chairs[slot].active       = true;
}

// ── WiFi helpers ──────────────────────────────────────────────────────────────
void ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.printf("[WiFi] Connecting to %s…\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
        // Sync NTP for timestamps
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    } else {
        Serial.println("\n[WiFi] Failed to connect.");
    }
}

// ── MQTT helpers ──────────────────────────────────────────────────────────────
void ensureMQTT() {
    if (g_mqtt.connected()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    Serial.printf("[MQTT] Connecting to %s:%d…\n", MQTT_HOST, MQTT_PORT);
    String client_id = String("gateway_") + g_table_id;
    bool ok = (strlen(MQTT_USER) > 0)
        ? g_mqtt.connect(client_id.c_str(), MQTT_USER, MQTT_PASS)
        : g_mqtt.connect(client_id.c_str());
    if (ok) {
        Serial.println("[MQTT] Connected.");
    } else {
        Serial.printf("[MQTT] Failed, rc=%d\n", g_mqtt.state());
    }
}

// ── Publish occupancy ─────────────────────────────────────────────────────────
void publishOccupancy() {
    if (!g_mqtt.connected()) return;

    // Evict stale chairs
    uint32_t now_ms = millis();
    for (int i = 0; i < MAX_CHAIRS; i++) {
        if (g_chairs[i].active && (now_ms - g_chairs[i].last_seen_ms) > CHAIR_TIMEOUT_MS) {
            Serial.printf("[Gateway] Evicting stale chair: %s\n", g_chairs[i].id);
            g_chairs[i].active = false;
        }
    }

    // Build JSON
    JsonDocument doc;
    doc["table_id"]  = g_table_id;
    doc["timestamp"] = (uint32_t)time(nullptr);

    JsonArray chairs = doc["chairs"].to<JsonArray>();
    int total = 0, occupied = 0;
    for (int i = 0; i < MAX_CHAIRS; i++) {
        if (!g_chairs[i].active) continue;
        total++;
        if (g_chairs[i].occupied) occupied++;
        JsonObject c = chairs.add<JsonObject>();
        c["id"]          = g_chairs[i].id;
        c["occupied"]    = g_chairs[i].occupied;
        c["rssi"]        = g_chairs[i].rssi;
        c["battery_pct"] = g_chairs[i].battery_pct;
    }
    doc["occupied_count"] = occupied;
    doc["total_count"]    = total;

    char topic[64];
    snprintf(topic, sizeof(topic), "%s/%s", MQTT_TOPIC_BASE, g_table_id);

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = g_mqtt.publish(topic, payload, /*retain=*/true);
    Serial.printf("[MQTT] Published to %s: %s (%s)\n", topic, payload, ok ? "OK" : "FAIL");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[HowMuddy] Table Gateway starting…");

    prefs.begin("muddy", true);
    String id = prefs.getString("table_id", DEFAULT_TABLE_ID);
    prefs.end();
    strncpy(g_table_id, id.c_str(), sizeof(g_table_id) - 1);
    Serial.printf("[HowMuddy] Table ID: %s\n", g_table_id);

    memset(g_chairs, 0, sizeof(g_chairs));

    // WiFi STA mode (required before ESP-NOW init)
    WiFi.mode(WIFI_STA);
    ensureWiFi();

    // Init ESP-NOW (works alongside WiFi on same channel)
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed — rebooting in 5 s");
        delay(5000);
        ESP.restart();
    }
    esp_now_register_recv_cb(onDataRecv);

    // MQTT
    g_mqtt.setServer(MQTT_HOST, MQTT_PORT);
    g_mqtt.setKeepAlive(MQTT_KEEPALIVE);
    ensureMQTT();

    Serial.println("[HowMuddy] Gateway ready.");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    // Reconnect WiFi if dropped
    if (now - g_last_wifi_check > WIFI_RECONNECT_MS) {
        g_last_wifi_check = now;
        ensureWiFi();
    }

    // Reconnect MQTT if dropped
    if (now - g_last_mqtt_check > MQTT_RECONNECT_MS) {
        g_last_mqtt_check = now;
        ensureMQTT();
    }

    g_mqtt.loop();

    // Publish occupancy snapshot
    if (now - g_last_publish >= PUBLISH_INTERVAL_MS) {
        g_last_publish = now;
        publishOccupancy();
    }
}
