"""
backend/app/routes.py
REST API endpoints for How Muddy?

Endpoints
---------
POST /api/report          — receive occupancy data from a table gateway
GET  /api/status          — current pub occupancy (all tables)
GET  /api/status/<table_id>  — single-table status
GET  /api/wait            — estimated wait time (entire pub)
GET  /api/wait/<table_id> — estimated wait time for one table
GET  /api/history         — occupancy time series (last 60 min by default)
GET  /api/tables          — list all tables (pub layout)
POST /api/tables          — register a new table (admin)
GET  /api/health          — liveness check
"""
import logging
from datetime import datetime, timezone

from flask import Blueprint, jsonify, request, current_app

from . import db
from .models import Chair, Table
from .occupancy import (
    process_gateway_report,
    mark_stale_chairs,
    estimate_wait_minutes,
    occupancy_history,
)

api_bp = Blueprint("api", __name__)
logger = logging.getLogger(__name__)


# ─── Health ───────────────────────────────────────────────────────────────

@api_bp.get("/health")
def health():
    return jsonify({"status": "ok", "ts": datetime.now(timezone.utc).isoformat()}), 200


# ─── Gateway Report ───────────────────────────────────────────────────────

@api_bp.post("/report")
def report():
    """
    Receive an occupancy snapshot from a table gateway.

    Expected JSON:
    {
        "table_id": "T1",
        "chairs": [
            {"chair_id": "T1C1", "occupied": true, "rssi": -52,
             "adc_raw": 2100, "battery_pct": 85},
            ...
        ],
        "timestamp_ms": 1713800000000   // optional, gateway uptime-relative
    }
    """
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "invalid JSON"}), 400

    table_id    = data.get("table_id")
    chairs_data = data.get("chairs", [])

    if not table_id:
        return jsonify({"error": "table_id required"}), 422
    if not isinstance(chairs_data, list):
        return jsonify({"error": "chairs must be a list"}), 422

    try:
        snapshot = process_gateway_report(table_id, chairs_data)
    except Exception as exc:
        logger.exception("Error processing report for %s", table_id)
        return jsonify({"error": str(exc)}), 500

    return jsonify({"ok": True, "table": snapshot}), 200


# ─── Status ───────────────────────────────────────────────────────────────

@api_bp.get("/status")
def status_all():
    """Return occupancy status for all tables."""
    mark_stale_chairs()
    tables = Table.query.all()
    return jsonify({
        "ts":     datetime.now(timezone.utc).isoformat(),
        "tables": [t.to_dict() for t in tables],
    }), 200


@api_bp.get("/status/<string:table_id>")
def status_one(table_id: str):
    """Return occupancy status for a single table."""
    mark_stale_chairs()
    table = db.session.get(Table, table_id)
    if table is None:
        return jsonify({"error": f"table {table_id!r} not found"}), 404
    return jsonify(table.to_dict()), 200


# ─── Wait Time ────────────────────────────────────────────────────────────

@api_bp.get("/wait")
def wait_all():
    """Return estimated wait time for the entire pub."""
    mark_stale_chairs()
    result = estimate_wait_minutes()
    result["ts"] = datetime.now(timezone.utc).isoformat()
    return jsonify(result), 200


@api_bp.get("/wait/<string:table_id>")
def wait_one(table_id: str):
    """Return estimated wait time for a specific table."""
    mark_stale_chairs()
    table = db.session.get(Table, table_id)
    if table is None:
        return jsonify({"error": f"table {table_id!r} not found"}), 404
    result = estimate_wait_minutes(table_id=table_id)
    result["ts"] = datetime.now(timezone.utc).isoformat()
    result["table_id"] = table_id
    return jsonify(result), 200


# ─── History ──────────────────────────────────────────────────────────────

@api_bp.get("/history")
def history():
    """
    Return per-minute occupancy counts.
    Query params:
      minutes  (int, default 60) — how far back to look
    """
    minutes = request.args.get("minutes", 60, type=int)
    minutes = max(1, min(minutes, 1440))   # clamp to 1 min – 24 h
    data    = occupancy_history(minutes=minutes)
    return jsonify({"minutes": minutes, "data": data}), 200


# ─── Tables (layout) ─────────────────────────────────────────────────────

@api_bp.get("/tables")
def get_tables():
    """List all registered tables (pub layout)."""
    tables = Table.query.order_by(Table.id).all()
    return jsonify([t.to_dict() for t in tables]), 200


@api_bp.post("/tables")
def create_table():
    """
    Register a table (admin use / seeding).
    {
        "id": "T3",
        "label": "Corner Table",
        "capacity": 6,
        "section": "right",
        "x_pos": 0.8,
        "y_pos": 0.5
    }
    """
    data = request.get_json(silent=True)
    if not data or "id" not in data:
        return jsonify({"error": "id required"}), 422

    if db.session.get(Table, data["id"]):
        return jsonify({"error": "table already exists"}), 409

    table = Table(
        id       = data["id"],
        label    = data.get("label", data["id"]),
        capacity = data.get("capacity", 4),
        section  = data.get("section"),
        x_pos    = data.get("x_pos"),
        y_pos    = data.get("y_pos"),
    )
    db.session.add(table)
    db.session.commit()
    return jsonify(table.to_dict()), 201


# ─── Chairs (admin) ──────────────────────────────────────────────────────

@api_bp.get("/chairs")
def get_chairs():
    """List all known chairs."""
    chairs = Chair.query.order_by(Chair.id).all()
    return jsonify([c.to_dict() for c in chairs]), 200
