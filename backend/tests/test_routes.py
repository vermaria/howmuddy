"""
backend/tests/test_routes.py
Integration tests for the How Muddy? REST API.

Run with:  pytest backend/tests/ -v
"""
import pytest
from app import create_app, db as _db
from app.models import Table, Chair


# ─── Fixtures ─────────────────────────────────────────────────────────────

@pytest.fixture()
def app():
    app = create_app("testing")
    with app.app_context():
        _db.create_all()
        # Seed a table
        t = Table(id="T1", label="Test Table", capacity=4)
        _db.session.add(t)
        _db.session.commit()
        yield app
        _db.drop_all()


@pytest.fixture()
def client(app):
    return app.test_client()


# ─── Health ───────────────────────────────────────────────────────────────

def test_health(client):
    r = client.get("/api/health")
    assert r.status_code == 200
    data = r.get_json()
    assert data["status"] == "ok"
    assert "ts" in data


# ─── Report ───────────────────────────────────────────────────────────────

VALID_REPORT = {
    "table_id": "T1",
    "chairs": [
        {"chair_id": "T1C1", "occupied": True,  "rssi": -52, "adc_raw": 2100, "battery_pct": 90},
        {"chair_id": "T1C2", "occupied": False, "rssi": -49, "adc_raw":  300, "battery_pct": 88},
    ]
}


def test_report_success(client):
    r = client.post("/api/report", json=VALID_REPORT)
    assert r.status_code == 200
    data = r.get_json()
    assert data["ok"] is True
    assert data["table"]["id"] == "T1"


def test_report_creates_chairs(client, app):
    client.post("/api/report", json=VALID_REPORT)
    with app.app_context():
        c1 = _db.session.get(Chair, "T1C1")
        c2 = _db.session.get(Chair, "T1C2")
        assert c1 is not None
        assert c1.is_occupied is True
        assert c2.is_occupied is False


def test_report_missing_table_id(client):
    r = client.post("/api/report", json={"chairs": []})
    assert r.status_code == 422


def test_report_invalid_json(client):
    r = client.post("/api/report", data="not-json",
                    content_type="text/plain")
    assert r.status_code == 400


def test_report_auto_creates_unknown_table(client):
    payload = {
        "table_id": "T99",
        "chairs": [{"chair_id": "T99C1", "occupied": False, "rssi": -60}]
    }
    r = client.post("/api/report", json=payload)
    assert r.status_code == 200


# ─── Status ───────────────────────────────────────────────────────────────

def test_status_all_empty(client):
    r = client.get("/api/status")
    assert r.status_code == 200
    data = r.get_json()
    assert "tables" in data
    # T1 was seeded but has no chairs yet
    assert any(t["id"] == "T1" for t in data["tables"])


def test_status_one(client):
    client.post("/api/report", json=VALID_REPORT)
    r = client.get("/api/status/T1")
    assert r.status_code == 200
    data = r.get_json()
    assert data["id"] == "T1"
    assert data["occupied_seats"] == 1
    assert data["total_seats"] == 2


def test_status_not_found(client):
    r = client.get("/api/status/TXXX")
    assert r.status_code == 404


# ─── Wait Time ────────────────────────────────────────────────────────────

def test_wait_all(client):
    client.post("/api/report", json=VALID_REPORT)
    r = client.get("/api/wait")
    assert r.status_code == 200
    data = r.get_json()
    assert "estimated_wait_min" in data
    assert "occupancy_ratio" in data
    assert data["total_seats"] == 2
    assert data["occupied_seats"] == 1


def test_wait_full_occupancy(client):
    """When all chairs are occupied, wait should approach AVG_TABLE_TURN_MIN."""
    full_report = {
        "table_id": "T1",
        "chairs": [
            {"chair_id": "T1C1", "occupied": True, "rssi": -50},
            {"chair_id": "T1C2", "occupied": True, "rssi": -50},
            {"chair_id": "T1C3", "occupied": True, "rssi": -50},
            {"chair_id": "T1C4", "occupied": True, "rssi": -50},
        ]
    }
    client.post("/api/report", json=full_report)
    r = client.get("/api/wait")
    data = r.get_json()
    assert data["occupancy_ratio"] == 1.0
    # Default AVG_TABLE_TURN_MIN = 45
    assert data["estimated_wait_min"] == pytest.approx(45.0, abs=1.0)


def test_wait_no_chairs(client):
    r = client.get("/api/wait")
    assert r.status_code == 200
    data = r.get_json()
    assert data["estimated_wait_min"] == 0.0


def test_wait_one_not_found(client):
    r = client.get("/api/wait/TXXX")
    assert r.status_code == 404


# ─── History ──────────────────────────────────────────────────────────────

def test_history_empty(client):
    r = client.get("/api/history")
    assert r.status_code == 200
    data = r.get_json()
    assert "data" in data
    assert isinstance(data["data"], list)


def test_history_after_report(client, app):
    # POST a report; /history filters to during_hours=True rows so we
    # verify the endpoint works by seeding a row directly.
    from datetime import datetime, timezone
    from app import db as _db2
    from app.models import ChairReport
    client.post("/api/report", json=VALID_REPORT)
    with app.app_context():
        _db2.session.add(ChairReport(
            chair_id="T1C1", table_id="T1",
            occupied=True, ts=datetime.now(timezone.utc), during_hours=True
        ))
        _db2.session.commit()
    r = client.get("/api/history?minutes=5")
    assert r.status_code == 200
    data = r.get_json()
    assert len(data["data"]) >= 1
    bucket = data["data"][0]
    assert "ts" in bucket
    assert "occupied" in bucket
    assert "total" in bucket


def test_history_minutes_clamped(client):
    # Requesting 9999 minutes should be clamped to 1440
    r = client.get("/api/history?minutes=9999")
    data = r.get_json()
    assert data["minutes"] == 1440


# ─── Tables ───────────────────────────────────────────────────────────────

def test_get_tables(client):
    r = client.get("/api/tables")
    assert r.status_code == 200
    tables = r.get_json()
    assert any(t["id"] == "T1" for t in tables)


def test_create_table(client):
    r = client.post("/api/tables", json={
        "id": "T2", "label": "Bar Corner", "capacity": 6,
        "section": "bar", "x_pos": 0.5, "y_pos": 0.2
    })
    assert r.status_code == 201
    data = r.get_json()
    assert data["id"] == "T2"
    assert data["capacity"] == 6


def test_create_table_duplicate(client):
    r = client.post("/api/tables", json={"id": "T1", "label": "Dup"})
    assert r.status_code == 409


def test_create_table_missing_id(client):
    r = client.post("/api/tables", json={"label": "No ID"})
    assert r.status_code == 422
