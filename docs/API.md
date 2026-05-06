# How Muddy? — API Reference

Base URL: `http://<host>:5000/api`

All responses are JSON. Timestamps are ISO-8601 UTC strings.

---

## `GET /health`

Liveness probe.

**Response 200**
```json
{ "status": "ok", "ts": "2026-04-22T14:00:00+00:00" }
```

---

## `POST /report`

Receive an occupancy snapshot from a table gateway.

**Request body**
```json
{
  "table_id": "T1",
  "chairs": [
    {
      "chair_id":    "T1C1",
      "occupied":    true,
      "rssi":        -52,
      "adc_raw":     2100,
      "battery_pct": 90
    }
  ]
}
```

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `table_id` | string | ✓ | e.g. `"T1"` |
| `chairs` | array | ✓ | Can be empty |
| `chairs[].chair_id` | string | ✓ | e.g. `"T1C1"` |
| `chairs[].occupied` | bool | ✓ | |
| `chairs[].rssi` | int | – | dBm, used for proximity verification |
| `chairs[].adc_raw` | int | – | Raw FSR ADC value |
| `chairs[].battery_pct` | int | – | 0–100 |

**Response 200**
```json
{
  "ok": true,
  "table": { /* table snapshot, see GET /status/<id> */ }
}
```

**Errors**: `400` bad JSON · `422` missing table_id · `500` internal

---

## `GET /status`

Current occupancy for all tables.

**Response 200**
```json
{
  "ts": "2026-04-22T14:00:00+00:00",
  "tables": [
    {
      "id": "T1",
      "label": "Window Table",
      "capacity": 4,
      "section": "left",
      "x_pos": 0.12,
      "y_pos": 0.22,
      "is_online": true,
      "last_seen": "2026-04-22T14:00:00+00:00",
      "occupied_seats": 2,
      "total_seats": 4,
      "chairs": [ /* see chair object below */ ]
    }
  ]
}
```

**Table fields**

| Field | Type | Notes |
|---|---|---|
| `is_online` | bool | True if the backend has seen a recent gateway report for this table |
| `last_seen` | string \| null | Timestamp of last gateway report for this table |

**Chair object**
```json
{
  "id":          "T1C1",
  "table_id":    "T1",
  "is_occupied": true,
  "last_rssi":   -52,
  "battery_pct": 90,
  "is_online":   true,
  "last_seen":   "2026-04-22T14:00:00+00:00"
}
```

---

## `GET /status/<table_id>`

Single table snapshot.

**Response 200** — same as one table entry in `/status`  
**Response 404** if table not found

---

## `GET /wait`

Estimated wait time for the entire pub.

**Response 200**
```json
{
  "estimated_wait_min": 22.5,
  "confidence":         "high",
  "occupancy_ratio":    0.5,
  "occupied_seats":     4,
  "total_seats":        8,
  "ts":                 "2026-04-22T14:00:00+00:00"
}
```

| `confidence` | Condition |
|---|---|
| `"low"` | < 3 online chairs |
| `"medium"` | 3–5 online chairs |
| `"high"` | ≥ 6 online chairs |

---

## `GET /wait/<table_id>`

Wait time for one table. Same schema as `/wait`, plus `"table_id"` field.

---

## `GET /history`

Per-minute occupancy counts for trend display.

**Query params**

| Param | Default | Range | Description |
|-------|---------|-------|-------------|
| `minutes` | 60 | 1–1440 | How far back to look |

**Response 200**
```json
{
  "minutes": 60,
  "data": [
    { "ts": "2026-04-22T13:05:00+00:00", "occupied": 3, "total": 8, "ratio": 0.375 },
    ...
  ]
}
```

---

## `GET /tables`

List all registered tables (pub layout).

---

## `POST /tables`

Register a table (admin / seeding use).

**Request body**
```json
{
  "id":       "T3",
  "label":    "Corner Table",
  "capacity": 6,
  "section":  "right",
  "x_pos":    0.8,
  "y_pos":    0.5
}
```

**Response 201** — table object  
**Response 409** if table already exists  
**Response 422** if `id` missing

---

## `GET /chairs`

List all known chairs with current state.
