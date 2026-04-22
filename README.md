# How Muddy? 🍺

**Estimating Dining Wait Times with Force and Proximity Sensors**  
*6.1820 Mobile and Sensor Computing — Spring 2026*  
*Ria Verma · Elaine Wang · Eileen Zu*

---

## Overview

How Muddy? is a low-cost, passive sensing system that continuously estimates seat occupancy at the Muddy Charles Pub and exposes that data via a public-facing dashboard and REST API.

### System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Chair Nodes                          │
│         FSR + ESP32-C3  (one per chair)                 │
│   Reads weight → broadcasts occupancy over ESP-NOW      │
└────────────────────┬────────────────────────────────────┘
                     │ ESP-NOW (RSSI used for proximity)
┌────────────────────▼────────────────────────────────────┐
│                  Table Gateways                         │
│         ESP32-C3  (one per table)                       │
│  Aggregates chair occupancy, assigns chairs by RSSI,    │
│  forwards to backend over WiFi (HTTP POST)              │
└────────────────────┬────────────────────────────────────┘
                     │ HTTP/WiFi
┌────────────────────▼────────────────────────────────────┐
│              Backend Server (Python/Flask)              │
│  REST API  ·  SQLite DB  ·  Occupancy state machine     │
│  Endpoints: POST /report · GET /status · GET /history   │
└────────────────────┬────────────────────────────────────┘
                     │ REST API
┌────────────────────▼────────────────────────────────────┐
│              Public Dashboard (React)                   │
│  Live seat map · Wait time estimate · Trend chart       │
└─────────────────────────────────────────────────────────┘
```

---

## Repository Layout

```
howmuddy/
├── firmware/
│   ├── chair_node/          # ESP32-C3 firmware: FSR + ESP-NOW transmit
│   │   ├── chair_node.ino
│   │   └── config.h
│   └── table_gateway/       # ESP32-C3 firmware: receive + WiFi upload
│       ├── table_gateway.ino
│       └── config.h
├── backend/
│   ├── app/
│   │   ├── __init__.py      # Flask app factory
│   │   ├── models.py        # SQLAlchemy models
│   │   ├── routes.py        # REST API routes
│   │   ├── occupancy.py     # Occupancy state machine & wait-time logic
│   │   └── config.py        # App configuration
│   ├── tests/
│   │   ├── test_routes.py
│   │   └── test_occupancy.py
│   ├── run.py               # Entry point
│   └── requirements.txt
├── dashboard/               # React frontend
│   ├── src/
│   │   ├── App.jsx
│   │   ├── main.jsx
│   │   ├── index.css
│   │   ├── components/
│   │   │   ├── SeatMap.jsx
│   │   │   ├── TableCard.jsx
│   │   │   ├── WaitTimeBadge.jsx
│   │   │   └── TrendChart.jsx
│   │   ├── hooks/
│   │   │   └── useOccupancy.js
│   │   └── utils/
│   │       └── api.js
│   ├── index.html
│   └── package.json
├── scripts/
│   ├── simulate_sensors.py  # Inject fake sensor data for testing
│   └── seed_db.py           # Seed DB with pub layout
└── docs/
    ├── API.md               # Full API reference
    └── HARDWARE.md          # Wiring diagrams and BOM notes
```

---

## Quick Start

### 1. Backend

```bash
cd backend
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python run.py
# API available at http://localhost:5000
```

### 2. Dashboard

```bash
cd dashboard
npm install
npm run dev
# Dashboard at http://localhost:5173
```

### 3. Simulate Sensors (no hardware required)

```bash
cd scripts
python simulate_sensors.py --api http://localhost:5000 --tables 2 --chairs 8
```

### 4. Flash Firmware

See [`docs/HARDWARE.md`](docs/HARDWARE.md) for wiring and flashing instructions.

---

## Hardware BOM

| Item | Source | Unit Cost | Qty | Total |
|------|--------|-----------|-----|-------|
| XIAO ESP32-C3 | DigiKey | $4.99 | 12 | $59.88 |
| FSR (Force Sensitive Resistor) | Adafruit | $3.95 | 8 | $31.60 |
| 1S LiPo Battery | Amazon | $4.75 | 10 | $47.50 |
| VL53L0X ToF Sensor | Adafruit | $14.95 | 2 | $29.90 |
| 1S LiPo Charger | Amazon | $32.99 | 1 | $32.99 |
| **Total** | | | | **$201.87** |

---

## Metrics Targets

| Metric | Target |
|--------|--------|
| Occupancy accuracy | ≥ 95% vs. ground truth |
| Update latency | ≤ 10 seconds |
| API response time | ≤ 300 ms |
| Sensor uptime | ≥ 99% over 6 hours |
| Battery life | ≥ 8 hours |
| Wait time error | ≤ 5 minutes |

---

## AI Usage Disclosure

*During development of this codebase, Claude Sonnet (Anthropic) was used to scaffold the initial project structure, generate boilerplate firmware templates, and suggest Flask API patterns. All sensor logic, RSSI-based proximity assignment, occupancy state machine design, and system architecture decisions were directed by the student team.*
