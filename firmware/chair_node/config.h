#pragma once

// ─── Chair Identity ────────────────────────────────────────────────────────
// Set a unique ID per chair before flashing (e.g., "T1C1" = Table 1, Chair 1)
#define CHAIR_ID "T1C2"

// ─── FSR Configuration ────────────────────────────────────────────────────
#define FSR_PIN          A0      // ADC pin wired to FSR voltage divider
#define FSR_VCC_PIN      D10     // GPIO used to power FSR (saves battery)
#define FSR_SERIES_OHMS  10000  // Series resistor value in the divider (10 kΩ)
#define ADC_RESOLUTION   4095   // 12-bit ADC on ESP32-C3

// Occupancy thresholds
// FSR reads higher ADC value when weight is applied (lower resistance → higher V)
#define OCCUPIED_THRESHOLD_ADC  1200   // ~1V on a 3.3V rail with 10kΩ divider
#define DEBOUNCE_SAMPLES        5      // consecutive samples required to flip state
#define SAMPLE_INTERVAL_MS      200    // ms between individual ADC reads
#define REPORT_INTERVAL_MS      2000   // ms between ESP-NOW broadcasts

// ─── Power Management ─────────────────────────────────────────────────────
// Deep sleep duration when the pub is closed (outside operating hours)
#define SLEEP_DURATION_US       (30ULL * 1000000ULL)  // 30 seconds
// Operating hours (24h, local time — set timezone in firmware)
#define PUB_OPEN_HOUR   11
#define PUB_CLOSE_HOUR  23

// ─── ESP-NOW ──────────────────────────────────────────────────────────────
// MAC address of the table gateway this chair reports to.
// Set to broadcast (FF:FF:FF:FF:FF:FF) for auto-discovery mode during setup.
#define GATEWAY_MAC   { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

// Channel must match the gateway
#define ESPNOW_CHANNEL  6   // must match the channel the gateway prints on boot

// ─── Debug ────────────────────────────────────────────────────────────────
#define SERIAL_BAUD     115200
#define DEBUG_ENABLED   true
