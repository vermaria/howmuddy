"""
backend/app/occupancy.py
Occupancy state machine and wait-time estimation.
"""
from __future__ import annotations

import logging
from datetime import datetime, timezone, timedelta
from zoneinfo import ZoneInfo
from typing import List, Dict, Optional

from flask import current_app

from . import db
from .models import Chair, Table, ChairReport

logger = logging.getLogger(__name__)

EST = ZoneInfo("America/New_York")

# ─── RSSI hysteresis store ────────────────────────────────────────────────
# Maps chair_id -> { table_id, count }
# A chair must be reported by a new table N times before reassignment.
_rssi_challenge: Dict[str, Dict] = {}
REASSIGN_THRESHOLD = 5   # reports from new table before we reassign


# ─── Report Processing ────────────────────────────────────────────────────

def process_gateway_report(
    table_id: str,
    chairs_data: List[Dict],
    server_ts: Optional[datetime] = None,
) -> Dict:
    if server_ts is None:
        server_ts = datetime.now(timezone.utc)

    during_hours = _is_during_hours(server_ts)

    table = db.session.get(Table, table_id)
    if table is None:
        table = Table(id=table_id, label=f"Table {table_id}", capacity=len(chairs_data))
        db.session.add(table)
        logger.info("Auto-created table %s", table_id)

    for entry in chairs_data:
        chair_id    = entry["chair_id"]
        occupied    = bool(entry["occupied"])
        rssi        = entry.get("rssi")
        adc_raw     = entry.get("adc_raw")
        battery_pct = entry.get("battery_pct")

        chair = db.session.get(Chair, chair_id)
        if chair is None:
            chair = Chair(id=chair_id, table_id=table_id)
            db.session.add(chair)
            _rssi_challenge[chair_id] = {"table_id": table_id, "count": 0}

        # ── RSSI hysteresis: only reassign after REASSIGN_THRESHOLD reports ──
        current_table = chair.table_id
        if current_table is None or current_table == table_id:
            # Same table — reset challenge counter and assign normally
            _rssi_challenge[chair_id] = {"table_id": table_id, "count": 0}
            chair.table_id = table_id
        else:
            # Different table is claiming this chair
            state = _rssi_challenge.get(chair_id, {"table_id": table_id, "count": 0})
            if state["table_id"] == table_id:
                state["count"] += 1
            else:
                # New challenger — reset
                state = {"table_id": table_id, "count": 1}
            _rssi_challenge[chair_id] = state

            if state["count"] >= REASSIGN_THRESHOLD:
                logger.info("Reassigning chair %s from %s to %s",
                            chair_id, current_table, table_id)
                chair.table_id = table_id
                _rssi_challenge[chair_id] = {"table_id": table_id, "count": 0}
            # else: keep chair at its current table, ignore this report's table_id

        chair.is_occupied = occupied
        chair.last_rssi   = rssi
        chair.last_adc    = adc_raw
        chair.battery_pct = battery_pct
        chair.last_seen   = server_ts
        chair.is_online   = True

        report = ChairReport(
            chair_id     = chair_id,
            table_id     = chair.table_id,  # use resolved table, not reporter
            occupied     = occupied,
            rssi         = rssi,
            adc_raw      = adc_raw,
            battery_pct  = battery_pct,
            ts           = server_ts,
            during_hours = during_hours,
        )
        db.session.add(report)

    db.session.commit()
    return table.to_dict()


def mark_stale_chairs() -> int:
    threshold = datetime.now(timezone.utc) - timedelta(
        seconds=current_app.config["CHAIR_STALE_SECONDS"]
    )
    stale = (
        Chair.query
        .filter(Chair.is_online == True)   # noqa: E712
        .filter(Chair.last_seen < threshold)
        .all()
    )
    for chair in stale:
        chair.is_online   = False
        chair.is_occupied = False
        # clear hysteresis for stale chairs
        _rssi_challenge.pop(chair.id, None)
    if stale:
        db.session.commit()
        logger.info("Marked %d chairs stale", len(stale))
    return len(stale)


# ─── Wait-Time Estimation ─────────────────────────────────────────────────

def estimate_wait_minutes(table_id: Optional[str] = None) -> Dict:
    avg_turn = current_app.config["AVG_TABLE_TURN_MIN"]

    # Only count ONLINE chairs — offline chairs are neither free nor occupied
    query = Chair.query.filter(Chair.is_online == True)   # noqa: E712
    if table_id:
        query = query.filter(Chair.table_id == table_id)

    chairs = query.all()
    total_seats    = len(chairs)
    occupied_seats = sum(1 for c in chairs if c.is_occupied)

    if total_seats == 0:
        return {
            "estimated_wait_min": 0.0,
            "confidence": "low",
            "occupancy_ratio": 0.0,
            "occupied_seats": 0,
            "total_seats": 0,
        }

    occupancy_ratio = occupied_seats / total_seats
    estimated_wait  = round(occupancy_ratio * avg_turn, 1)

    if total_seats >= 6:
        confidence = "high"
    elif total_seats >= 3:
        confidence = "medium"
    else:
        confidence = "low"

    return {
        "estimated_wait_min": estimated_wait,
        "confidence":         confidence,
        "occupancy_ratio":    round(occupancy_ratio, 3),
        "occupied_seats":     occupied_seats,
        "total_seats":        total_seats,
    }


# ─── Historical Trend ─────────────────────────────────────────────────────

def occupancy_history(minutes: int = 60) -> List[Dict]:
    since = datetime.now(timezone.utc) - timedelta(minutes=minutes)

    rows = (
        db.session.query(
            ChairReport.ts,
            ChairReport.chair_id,
            ChairReport.occupied,
        )
        .filter(ChairReport.ts >= since)
        .filter(ChairReport.during_hours == True)   # noqa: E712
        .order_by(ChairReport.ts)
        .all()
    )

    buckets: Dict[str, Dict] = {}
    for row in rows:
        # Convert to EST for display
        ts_est = row.ts.replace(tzinfo=timezone.utc).astimezone(EST)
        key = ts_est.replace(second=0, microsecond=0).isoformat()
        if key not in buckets:
            buckets[key] = {"ts": key, "occupied": set(), "total": set()}
        buckets[key]["total"].add(row.chair_id)
        if row.occupied:
            buckets[key]["occupied"].add(row.chair_id)

    result = []
    for key in sorted(buckets):
        b = buckets[key]
        total    = len(b["total"])
        occupied = len(b["occupied"])
        result.append({
            "ts":       key,
            "occupied": occupied,
            "total":    total,
            "ratio":    round(occupied / total, 3) if total else 0.0,
        })
    return result


# ─── Internal helpers ─────────────────────────────────────────────────────

def _is_during_hours(ts: datetime) -> bool:
    open_h  = current_app.config["PUB_OPEN_HOUR"]
    close_h = current_app.config["PUB_CLOSE_HOUR"]
    h = ts.astimezone(EST).hour
    return open_h <= h < close_h
