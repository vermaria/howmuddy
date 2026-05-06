# How Muddy? 🍺

**Estimating Dining Wait Times with Force and Proximity Sensors**  
*6.1820 Mobile and Sensor Computing — Spring 2026*  
*Ria Verma · Elaine Wang · Eileen Zu*

---

## Overview

How Muddy? is a low-cost sensing system that estimates seat occupancy at the Muddy Charles Pub and exposes it through a dashboard and REST API.

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
# Dashboard at the printed Vite URL (usually http://localhost:5173)
```

### 3. Simulate Sensors (no hardware required)

```bash
cd scripts
python simulate_sensors.py --api http://localhost:5000 --tables 2 --chairs 8
```

### 4. Flash Firmware

See [`docs/HARDWARE.md`](docs/HARDWARE.md) for wiring and flashing instructions.

---

## Pilot Demo (2 tables, 8 chairs, no hardware)

Our initial build targets a subsection of the pub — 2 tables with 4 chairs each — before scaling to the full floor plan. To run the whole stack locally, use four terminals:

```bash
# Terminal 1 — backend (use PORT=5001 if macOS AirPlay has 5000)
cd backend && source .venv/bin/activate
PORT=5001 python run.py

# Terminal 2 — seed the pilot layout
cd scripts && source ../backend/.venv/bin/activate
python seed_db.py --profile pilot --reset

# Terminal 3 — fake sensor traffic
cd scripts && source ../backend/.venv/bin/activate
python simulate_sensors.py --api http://localhost:5001 --tables 2 --chairs 8

# Terminal 4 — dashboard
cd dashboard && npm run dev
```

Open the Vite URL shown in terminal (usually [http://localhost:5173](http://localhost:5173)).  
When you're ready for the full pub, reseed with `python seed_db.py --profile full --reset`.

> **Note:** if you run the backend on port 5000 (no AirPlay conflict), drop the `PORT=5001` and change `--api` / `dashboard/vite.config.js` back to `5000`.

---

## Hardware

- **XIAO ESP32-C3** × 12
- **FSR (Force Sensitive Resistor)** × 8
- **1S LiPo Battery** × 10
- **1S LiPo Charger** × 1

---

## Metrics Targets

- **Occupancy accuracy** — ≥ 95% vs. ground truth
- **Update latency** — ≤ 10 seconds
- **API response time** — ≤ 300 ms
- **Sensor uptime** — ≥ 99% over 6 hours
- **Battery life** — ≥ 8 hours
- **Wait time error** — ≤ 5 minutes

---

## Runtime Status Notes

- Tables are shown as online only when a recent gateway report is received.
- Chairs are shown as online only when recent chair data appears in gateway uploads.
- Staleness is controlled in backend config (`TABLE_STALE_SECONDS`, `CHAIR_STALE_SECONDS`).

---

## AI Usage Disclosure

*During development of this codebase, Claude Sonnet (Anthropic) was used to scaffold the initial project structure, generate boilerplate firmware templates, and suggest Flask API patterns. All sensor logic, RSSI-based proximity assignment, occupancy state machine design, and system architecture decisions were directed by the student team.*
