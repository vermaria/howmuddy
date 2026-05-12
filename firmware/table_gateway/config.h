#pragma once

// ─── Table Identity ───────────────────────────────────────────────────────
#define TABLE_ID     "T2"
#define TABLE_ID_NUM 2

// ─── WiFi Credentials ─────────────────────────────────────────────────────
#define WIFI_SSID     "MIT"            // your hotspot SSID
#define WIFI_PASSWORD "RJ@cC_3UFZ" // your hotspot password

// ─── Backend API ──────────────────────────────────────────────────────────
#define API_HOST    "10.29.139.235"          // your laptop's IP on the hotspot - need to change this when running backend
#define API_PORT    5000
#define API_PATH    "/api/report"
#define UPLOAD_INTERVAL_MS  5000

// ─── RSSI-Based Proximity ─────────────────────────────────────────────────
#define RSSI_SMOOTHING_N      8
#define RSSI_CLAIM_THRESHOLD  -65
#define REASSIGN_HYSTERESIS   3

// ─── ESP-NOW ──────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL    1
#define MAX_CHAIRS        16
#define BEACON_INTERVAL_MS 200

// ─── Debug ────────────────────────────────────────────────────────────────
#define SERIAL_BAUD     115200
#define DEBUG_ENABLED   1
