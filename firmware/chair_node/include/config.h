#pragma once

// ── Hardware ──────────────────────────────────────────────────────────────────
// FSR is read via ADC on GPIO2 (A0 on XIAO ESP32-C3)
#define FSR_PIN              2

// Number of ADC readings to average for noise reduction
#define FSR_SAMPLES          8

// ADC value thresholds (12-bit, 0–4095)
// Below EMPTY_THRESHOLD  → definitely empty
// Above OCCUPIED_THRESHOLD → definitely occupied
// In between             → debounce / ignore transient
#define FSR_EMPTY_THRESHOLD      400
#define FSR_OCCUPIED_THRESHOLD   700

// Sustained-pressure debounce: seat must read occupied for this many
// consecutive samples before we declare it occupied (prevents bag detection).
#define OCCUPY_DEBOUNCE_TICKS    3   // × SAMPLE_INTERVAL_MS
#define VACATE_DEBOUNCE_TICKS    3

// ── Timing ────────────────────────────────────────────────────────────────────
#define SAMPLE_INTERVAL_MS       1000   // FSR polling period
#define BROADCAST_INTERVAL_MS    2000   // How often to broadcast via ESP-NOW
#define DEEP_SLEEP_ENABLED       false  // Set true to enable deep-sleep between reads

// ── Power ─────────────────────────────────────────────────────────────────────
// If DEEP_SLEEP_ENABLED, the node sleeps this long between wakeups.
// Keep longer than SAMPLE_INTERVAL_MS to save battery.
#define SLEEP_DURATION_US        (5ULL * 1000000ULL)   // 5 s

// ── Identity ──────────────────────────────────────────────────────────────────
// NODE_ID is flashed at provision time via NVS; this is only the fallback.
#define DEFAULT_NODE_ID          "chair_00"

// ── ESP-NOW ───────────────────────────────────────────────────────────────────
// Broadcast address — gateway nodes accept broadcasts from any peer.
static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ── Protocol ──────────────────────────────────────────────────────────────────
// Packet magic byte so gateways can sanity-check incoming messages.
#define PACKET_MAGIC 0xMD  // 'M' for Muddy
