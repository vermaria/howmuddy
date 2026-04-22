# How Muddy? — Hardware Guide

## Bill of Materials

| Item | Source | Unit Cost | Qty | Total |
|------|--------|-----------|-----|-------|
| XIAO ESP32-C3 | DigiKey | $4.99 | 12 | $59.88 |
| FSR 402 (square, 0.5") | Adafruit #166 | $3.95 | 8 | $31.60 |
| 1S LiPo 500mAh | Amazon | $4.75 | 10 | $47.50 |
| VL53L0X ToF sensor | Adafruit #3317 | $14.95 | 2 | $29.90 |
| 1S LiPo charger (USB-C) | Amazon | $32.99 | 1 | $32.99 |
| 10kΩ resistors (pack) | DigiKey | ~$1 | 1 | $1.00 |
| Breadboard / perfboard | — | — | — | on hand |
| **Total** | | | | **$202.87** |

---

## Chair Node Wiring

```
3.3V ─── FSR ─── A0 (ADC)
                  │
                 10kΩ
                  │
                GND
```

- **FSR_VCC_PIN (D10)** is toggled HIGH only during a reading to save power.  
- **Battery**: Connect 1S LiPo to XIAO BAT+ / BAT- pads.  
- No additional components needed; XIAO has an onboard LiPo charge circuit.

### Pinout (XIAO ESP32-C3)

| Signal | Pin |
|--------|-----|
| FSR ADC | A0 |
| FSR power switch | D10 |
| Battery voltage sense | A1 (internal) |
| ESP-NOW radio | Internal (WiFi chip) |

---

## Table Gateway Wiring

The gateway uses the same XIAO ESP32-C3 board but is powered permanently (USB-C or
wall adapter via the LiPo pads). No FSR needed.

For WiFi on MIT campus (MITsecure), you will need to register the ESP32's MAC address
via `https://mit.edu/netreg`. The gateway MAC is printed to Serial on boot.

---

## Arduino IDE Setup

1. Install **Arduino IDE 2.x**.
2. Add ESP32 board package:
   - File → Preferences → Additional board URLs:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board Manager → search "esp32" → install **Espressif Systems esp32 3.x**.
3. Install libraries (Tools → Manage Libraries):
   - `ArduinoJson` by Benoit Blanchon (≥ 7.x)
4. Select board: **Tools → Board → esp32 → XIAO_ESP32C3**
5. Select port: whichever `/dev/cu.usbmodem*` or `COM*` appears when plugged in.

---

## Flashing Chair Nodes

1. Edit `firmware/chair_node/config.h`:
   - Set `CHAIR_ID` uniquely (e.g. `"T1C1"`, `"T1C2"`, …).
   - Set `GATEWAY_MAC` to the MAC address printed by the table gateway on boot.
2. Open `firmware/chair_node/chair_node.ino` in Arduino IDE.
3. Upload (Ctrl+U).
4. Open Serial Monitor at 115200 baud to verify FSR readings and ESP-NOW sends.

### Calibrating FSR Threshold

With nothing on the chair, read `ADC=` values in Serial Monitor.  
Sit on the chair and observe the new ADC value.  
Set `OCCUPIED_THRESHOLD_ADC` in `config.h` to ~halfway between the two values.

Typical values:
- Empty: 50–400 ADC counts
- Occupied (adult): 1500–3500 ADC counts
- Recommended threshold: **1200**

---

## Flashing Table Gateways

1. Edit `firmware/table_gateway/config.h`:
   - Set `TABLE_ID` (e.g. `"T1"`).
   - Set `WIFI_SSID` / `WIFI_PASSWORD` (leave password blank for MITsecure open auth).
   - Set `API_HOST` to your backend server's IP on MIT-SECURE (or `192.168.x.x` for local).
2. Open `firmware/table_gateway/table_gateway.ino` and upload.
3. On boot the gateway prints its MAC address — record this for chair node config.

---

## Battery Life Estimate

- ESP32-C3 active current: ~80 mA  
- Deep sleep current: ~5 µA  
- Sample + send cycle: ~300 ms active, then 2 s light sleep  
- Effective average: ~80 mA × (0.3/2.3) + negligible sleep ≈ **~10 mA**  
- 500 mAh LiPo → ~50 hours continuous  
- With off-hours deep sleep (12 h/day): weeks of runtime  
- **Target ≥ 8 hours easily met.**

---

## ToF Door Counter (stretch goal)

Two VL53L0X sensors mounted 10 cm apart above the doorway.  
Connect to a dedicated ESP32-C3 via I²C:

| VL53L0X Pin | XIAO Pin |
|-------------|----------|
| VIN | 3.3V |
| GND | GND |
| SDA | D4 |
| SCL | D5 |
| XSHUT (sensor 1) | D2 |
| XSHUT (sensor 2) | D3 |

Direction is inferred from the order of beam-break events (sensor 1 then 2 = entry;
sensor 2 then 1 = exit). The counter resets at open time each day.

Firmware for the ToF counter is in `firmware/tof_counter/` (to be implemented in milestone 2).
