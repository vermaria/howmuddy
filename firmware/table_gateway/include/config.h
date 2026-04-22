#pragma once
#include <stdint.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
// Edit these before flashing each gateway.
#define WIFI_SSID       "MIT"       // On-campus MIT WiFi
#define WIFI_PASSWORD   ""          // Open network — leave blank

// ── MQTT ──────────────────────────────────────────────────────────────────────
// The backend server hosts an MQTT broker (Mosquitto) on the same network.
// Change MQTT_HOST to the IP of your Raspberry Pi / EC2 / laptop running the backend.
#define MQTT_HOST       "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_USER       ""           // leave blank if no auth
#define MQTT_PASS       ""
#define MQTT_TOPIC_BASE "howmuddy/occupancy"   // full topic: howmuddy/occupancy/<table_id>
#define MQTT_KEEPALIVE  60

// ── Table identity ────────────────────────────────────────────────────────────
// TABLE_ID is loaded from NVS at runtime; this is the fallback.
#define DEFAULT_TABLE_ID  "table_01"

// ── Chair tracking ────────────────────────────────────────────────────────────
// Maximum number of distinct chair nodes that a single gateway will track.
#define MAX_CHAIRS          16

// RSSI ownership threshold: a chair is "owned" by this gateway if its RSSI
// (from this gateway's perspective) is above RSSI_OWN_THRESHOLD AND
// better than any other gateway's RSSI that the chair has reported.
// Since we only receive from our own radio, we use a simpler heuristic:
// accept a chair if its RSSI is above this value (dBm, negative).
#define RSSI_OWN_THRESHOLD  -75    // dBm — chairs further than ~2 m typically drop below this

// If a chair hasn't been heard from in this many ms, evict it from the table.
#define CHAIR_TIMEOUT_MS    10000

// ── Protocol ──────────────────────────────────────────────────────────────────
#define PACKET_MAGIC  0xAD

// ── Timing ────────────────────────────────────────────────────────────────────
#define PUBLISH_INTERVAL_MS   2000   // How often to publish state to MQTT
#define WIFI_RECONNECT_MS     5000
#define MQTT_RECONNECT_MS     3000
