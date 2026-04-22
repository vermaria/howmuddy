#pragma once
#include <stdint.h>

/**
 * OccupancyPacket — broadcast by each chair node every BROADCAST_INTERVAL_MS.
 * Must stay in sync with chair_node/src/main.cpp.
 */
struct __attribute__((packed)) OccupancyPacket {
    uint8_t  magic;           // 0xAD
    char     node_id[16];     // null-terminated chair identifier, e.g. "chair_03"
    uint8_t  occupied;        // 1 = occupied, 0 = empty
    uint16_t raw_adc;         // raw FSR reading (0–4095)
    uint32_t uptime_s;        // seconds since last boot (for diagnostics)
    uint8_t  battery_pct;     // 0–100; 255 = not measured
};
