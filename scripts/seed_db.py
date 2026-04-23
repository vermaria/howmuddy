"""
scripts/seed_db.py
Seed the How Muddy? database with the Muddy Charles Pub layout.

Run from the backend directory:
    cd backend
    python ../scripts/seed_db.py --profile pilot   # 2 tables, 8 chairs (hardware pilot)
    python ../scripts/seed_db.py --profile full    # full 10-table pub
    python ../scripts/seed_db.py --profile pilot --reset   # wipe + reseed

Each table has x_pos/y_pos in [0, 1] fractional coordinates for the dashboard map.
"""
import argparse
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'backend'))

from app import create_app, db
from app.models import Table, Chair

# ── Pub layouts ─────────────────────────────────────────────────────────────
# (id, label, capacity, section, x_pos, y_pos)

# Pilot: 2 neighboring tables in the left section — matches the small squares
# in the upper-left of the proposal floor-plan. 4 chairs each = 8 chairs total.
PILOT_TABLES = [
    ("T1", "Table 1", 4, "left", 0.35, 0.30),
    ("T2", "Table 2", 4, "left", 0.65, 0.30),
]

# Full pub: 10 tables + bar, matching the proposal floor-plan diagram.
FULL_TABLES = [
    ("BAR", "Bar",      8, "bar",   0.38, 0.30),
    ("T1",  "Table 1",  4, "left",  0.12, 0.22),
    ("T2",  "Table 2",  4, "left",  0.24, 0.22),
    ("T3",  "Table 3",  4, "left",  0.12, 0.68),
    ("T4",  "Table 4",  4, "left",  0.24, 0.68),
    ("T5",  "Table 5",  6, "right", 0.60, 0.15),
    ("T6",  "Table 6",  4, "right", 0.74, 0.22),
    ("T7",  "Table 7",  4, "right", 0.88, 0.22),
    ("T8",  "Table 8",  6, "right", 0.60, 0.55),
    ("T9",  "Table 9",  6, "right", 0.74, 0.68),
    ("T10", "Table 10", 4, "right", 0.88, 0.68),
]

PROFILES = {
    "pilot": PILOT_TABLES,
    "full":  FULL_TABLES,
}

# Pre-assign chairs to tables (chair_id format: <table_id>C<n>)
def chairs_for_table(table_id, capacity):
    return [
        {"id": f"{table_id}C{i+1}", "table_id": table_id}
        for i in range(capacity)
    ]


def seed(profile: str = "pilot", reset: bool = False):
    tables = PROFILES[profile]
    app = create_app("development")
    with app.app_context():
        db.create_all()

        if reset:
            deleted_chairs = Chair.query.delete()
            deleted_tables = Table.query.delete()
            db.session.commit()
            print(f"  reset: removed {deleted_tables} tables, "
                  f"{deleted_chairs} chairs")

        print(f"Seeding profile '{profile}' "
              f"({len(tables)} tables, "
              f"{sum(cap for _, _, cap, *_ in tables)} chairs):")

        added_tables = 0
        added_chairs = 0
        skipped      = 0

        for tid, label, cap, section, x, y in tables:
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
    parser = argparse.ArgumentParser(description="Seed the How Muddy? database.")
    parser.add_argument(
        "--profile", choices=PROFILES.keys(), default="pilot",
        help="Which pub layout to seed (default: pilot).",
    )
    parser.add_argument(
        "--reset", action="store_true",
        help="Delete all existing tables/chairs before seeding.",
    )
    args = parser.parse_args()
    seed(profile=args.profile, reset=args.reset)
