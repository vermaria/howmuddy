"""
scripts/seed_db.py
Seed the How Muddy? database with the Muddy Charles Pub layout.

Run from the backend directory:
    cd backend
    python ../scripts/seed_db.py

The layout matches the proposal floor-plan diagram:
  - 10 tables total
  - Bar seating (bar stools)
  - Left section (small round tables)
  - Right section (larger tables)

Each table has x_pos/y_pos in [0, 1] fractional coordinates for the dashboard map.
"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'backend'))

from app import create_app, db
from app.models import Table, Chair

# ── Pub layout ──────────────────────────────────────────────────────────────
# (id, label, capacity, section, x_pos, y_pos)
TABLES = [
    # Bar stools (center-left)
    ("BAR", "Bar",         8,  "bar",   0.38, 0.30),
    # Left section
    ("T1",  "Table 1",     4,  "left",  0.12, 0.22),
    ("T2",  "Table 2",     4,  "left",  0.24, 0.22),
    ("T3",  "Table 3",     4,  "left",  0.12, 0.68),
    ("T4",  "Table 4",     4,  "left",  0.24, 0.68),
    # Right section — upper
    ("T5",  "Table 5",     6,  "right", 0.60, 0.15),
    ("T6",  "Table 6",     4,  "right", 0.74, 0.22),
    ("T7",  "Table 7",     4,  "right", 0.88, 0.22),
    # Right section — lower
    ("T8",  "Table 8",     6,  "right", 0.60, 0.55),
    ("T9",  "Table 9",     6,  "right", 0.74, 0.68),
    ("T10", "Table 10",    4,  "right", 0.88, 0.68),
]

# Pre-assign chairs to tables (chair_id format: <table_id>C<n>)
def chairs_for_table(table_id, capacity):
    return [
        {"id": f"{table_id}C{i+1}", "table_id": table_id}
        for i in range(capacity)
    ]


def seed():
    app = create_app("development")
    with app.app_context():
        db.create_all()

        added_tables  = 0
        added_chairs  = 0
        skipped       = 0

        for tid, label, cap, section, x, y in TABLES:
            if db.session.get(Table, tid):
                print(f"  skip  {tid} (already exists)")
                skipped += 1
                continue

            table = Table(id=tid, label=label, capacity=cap,
                          section=section, x_pos=x, y_pos=y)
            db.session.add(table)

            for c in chairs_for_table(tid, cap):
                chair = Chair(id=c["id"], table_id=c["table_id"],
                              is_occupied=False, is_online=False)
                db.session.add(chair)
                added_chairs += 1

            added_tables += 1
            print(f"  added {tid} '{label}' ({cap} chairs)")

        db.session.commit()
        print(f"\nDone: {added_tables} tables, {added_chairs} chairs added. "
              f"{skipped} skipped.")


if __name__ == "__main__":
    seed()
