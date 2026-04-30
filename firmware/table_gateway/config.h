#pragma once

// ─── Table Identity ───────────────────────────────────────────────────────
#define TABLE_ID "T1"   // unique per gateway (T1, T2, … T10)

// ─── WiFi Credentials ─────────────────────────────────────────────────────
#define WIFI_SSID     "MITsecure"    // MIT on-campus network
#define WIFI_PASSWORD ""             // Open/cert networks: leave blank, see docs

// ─── Backend API ──────────────────────────────────────────────────────────
#define API_HOST    "10.31.134.241"
#define API_PORT    5000
#define API_PATH    "/api/report"
#define UPLOAD_INTERVAL_MS  5000    // batch-send every 5 seconds

// ─── RSSI-Based Proximity ─────────────────────────────────────────────────
// Number of RSSI samples to average per chair before assigning it to a table
#define RSSI_SMOOTHING_N    8
// A chair is "ours" if its smoothed RSSI is stronger than this threshold (dBm)
// Typical: chairs at same table ≈ -50 dBm, neighboring table ≈ -75 dBm
#define RSSI_CLAIM_THRESHOLD   -65  // dBm
// If a chair's best-seen gateway changes, reassign after this many reports
#define REASSIGN_HYSTERESIS     3

// ─── ESP-NOW ──────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL  1
#define MAX_CHAIRS      16   // max chairs tracked simultaneously

// ─── Debug ────────────────────────────────────────────────────────────────
#define SERIAL_BAUD   115200
#define DEBUG_ENABLED true
