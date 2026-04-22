"""
backend/app/occupancy.py
Occupancy state machine and wait-time estimation.

The OccupancyEngine handles:
  - Applying incoming gateway reports to the Chair/Table state in the DB.
  - Marking chairs stale (offline) when no report arrives within threshold.
  - Estimating wait time from current occupancy + historical turn-time data.
"""
from __future__ import annotations

import logging
from datetime import datetime, timezone, timedelta
from typing import List, Dict, Optional

from flask import current_app

from . import db
from .models import Chair, Table, ChairReport

logger = logging.getLogger(__name__)


# ─── Report Processing ────────────────────────────────────────────────────

def process_gateway_report(
    table_id: str,
    chairs_data: List[Dict],
    server_ts: Optional[datetime] = None,
) -> Dict:
    """
    Apply a single gateway report to the database state.

    Parameters
    ----------
    table_id    : Table identifier (e.g. "T1")
    chairs_data : List of dicts with keys: chair_id, occupied, rssi, adc_raw, battery_pct
    server_ts   : Override timestamp (used in tests); defaults to UTC now

    Returns
    -------
    dict with updated table snapshot
    """
    if server_ts is None:
        server_ts = datetime.now(timezone.utc)

    during_hours = _is_during_hours(server_ts)

    table = db.session.get(Table, table_id)
    if table is None:
        # Auto-create table if not seeded yet (useful for ad-hoc testing)
        table = Table(id=table_id, label=f"Table {table_id}", capacity=len(chairs_data))
        db.session.add(table)
        logger.info("Auto-created table %s", table_id)

    updated_chairs = []
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

        # Update mutable state
        chair.table_id    = table_id   # may reassign if RSSI claims changed
        chair.is_occupied = occupied
        chair.last_rssi   = rssi
        chair.last_adc    = adc_raw
        chair.battery_pct = battery_pct
        chair.last_seen   = server_ts
        chair.is_online   = True

        # Append time-series record
        report = ChairReport(
            chair_id     = chair_id,
            table_id     = table_id,
            occupied     = occupied,
            rssi         = rssi,
            adc_raw      = adc_raw,
            battery_pct  = battery_pct,
            ts           = server_ts,
            during_hours = during_hours,
        )
        db.session.add(report)
        updated_chairs.append(chair)

    db.session.commit()

    if not during_hours:
        logger.debug("Report from %s received outside operating hours", table_id)

    return table.to_dict()


def mark_stale_chairs() -> int:
    """
    Mark any chair offline whose last_seen is older than CHAIR_STALE_SECONDS.
    Returns the count of newly-stale chairs.
    """
    threshold = datetime.now(timezone.utc) - timedelta(
        seconds=current_app.config["CHAIR_STALE_SECONDS"]
    )
    stale = (
        Chair.query
        .filter(Chair.is_online == True)  # noqa: E712
        .filter(Chair.last_seen < threshold)
        .all()
    )
    for chair in stale:
        chair.is_online   = False
        chair.is_occupied = False
    if stale:
        db.session.commit()
        logger.info("Marked %d chairs stale", len(stale))
    return len(stale)


# ─── Wait-Time Estimation ────────────────────────────────────────────────

def estimate_wait_minutes(table_id: Optional[str] = None) -> Dict:
    """
    Estimate the current wait time in minutes for a given table (or the pub overall).

    Algorithm
    ---------
    1. Count fully-occupied tables (all chairs occupied).
    2. Estimate how many groups are waiting (queue = occupied_groups - capacity_groups).
       - Since the Muddy doesn't have a formal queue, we use occupancy ratio as a proxy.
    3. Multiply expected wait groups by historical average turn time.

    For the prototype, we use a simple linear model:
        wait_min ≈ occupancy_ratio * AVG_TABLE_TURN_MIN

    where occupancy_ratio = occupied_seats / total_seats across all online chairs.

    Returns
    -------
    {
        "estimated_wait_min": float,
        "confidence": "low" | "medium" | "high",
        "occupancy_ratio": float,
        "occupied_seats": int,
        "total_seats": int,
    }
    """
    avg_turn = current_app.config["AVG_TABLE_TURN_MIN"]

    query = Chair.query.filter(Chair.is_online == True)  # noqa: E712
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

    # Simple linear model:  wait ≈ ratio * avg_turn
    # When fully packed (ratio=1), wait ≈ avg_turn (one full cycle).
    # At 50% occupancy, wait ≈ avg_turn / 2, etc.
    estimated_wait = round(occupancy_ratio * avg_turn, 1)

    # Confidence based on how many chairs are online
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


# ─── Historical Trend ────────────────────────────────────────────────────

def occupancy_history(minutes: int = 60) -> List[Dict]:
    """
    Return per-minute occupancy counts for the past `minutes` minutes.
    Used by the dashboard trend chart.
    """
    since = datetime.now(timezone.utc) - timedelta(minutes=minutes)

    rows = (
        db.session.query(
            ChairReport.ts,
            ChairReport.chair_id,
            ChairReport.occupied,
        )
        .filter(ChairReport.ts >= since)
        .filter(ChairReport.during_hours == True)  # noqa: E712
        .order_by(ChairReport.ts)
        .all()
    )

    # Bucket into 1-minute bins
    buckets: Dict[str, Dict] = {}
    for row in rows:
        # Truncate to minute
        key = row.ts.replace(second=0, microsecond=0).isoformat()
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
            "ts":           key,
            "occupied":     occupied,
            "total":        total,
            "ratio":        round(occupied / total, 3) if total else 0.0,
        })
    return result


# ─── Internal helpers ─────────────────────────────────────────────────────

def _is_during_hours(ts: datetime) -> bool:
    open_h  = current_app.config["PUB_OPEN_HOUR"]
    close_h = current_app.config["PUB_CLOSE_HOUR"]
    h = ts.astimezone(tz=None).hour  # local time
    return open_h <= h < close_h
