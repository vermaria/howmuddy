"""
scripts/simulate_sensors.py
Simulate chair node → gateway → backend data flow without any hardware.

Usage:
    python simulate_sensors.py                       # defaults
    python simulate_sensors.py --api http://localhost:5000 --tables 2 --chairs 4
    python simulate_sensors.py --scenario rush       # simulate a rush hour
    python simulate_sensors.py --once                # send one batch and exit

Scenarios:
    normal  — random occupancy varying slowly over time (default)
    rush    — occupancy ramps up to ~90% over 10 minutes then drains
    empty   — all chairs empty (smoke-test)
    full    — all chairs occupied
"""
import argparse
import json
import math
import random
import time
import urllib.request
import urllib.error
from datetime import datetime


def parse_args():
    p = argparse.ArgumentParser(description="How Muddy? sensor simulator")
    p.add_argument("--api",      default="http://localhost:5000", help="Backend base URL")
    p.add_argument("--tables",   type=int, default=2,   help="Number of tables to simulate")
    p.add_argument("--chairs",   type=int, default=4,   help="Chairs per table")
    p.add_argument("--interval", type=float, default=5, help="Seconds between reports")
    p.add_argument("--scenario", choices=["normal", "rush", "empty", "full"],
                   default="normal")
    p.add_argument("--once", action="store_true", help="Send one report and exit")
    return p.parse_args()


# ── Occupancy models ────────────────────────────────────────────────────────

def occupancy_normal(t: float, chair_index: int) -> bool:
    """Smooth sinusoidal variation with per-chair phase offset."""
    phase = chair_index * 0.7
    base  = 0.5 + 0.35 * math.sin(t / 120 + phase)
    return random.random() < base


def occupancy_rush(t: float, chair_index: int) -> bool:
    """Ramp to ~90% over 10 min then drain over 5 min."""
    ramp_s  = 600
    drain_s = 300
    if t < ramp_s:
        prob = 0.1 + 0.8 * (t / ramp_s)
    elif t < ramp_s + drain_s:
        prob = 0.9 - 0.8 * ((t - ramp_s) / drain_s)
    else:
        prob = 0.1
    return random.random() < prob


def occupancy_empty(_t, _i): return False
def occupancy_full(_t, _i):  return True


SCENARIOS = {
    "normal": occupancy_normal,
    "rush":   occupancy_rush,
    "empty":  occupancy_empty,
    "full":   occupancy_full,
}


# ── HTTP helper ─────────────────────────────────────────────────────────────

def post_report(api_base: str, payload: dict) -> bool:
    url  = f"{api_base.rstrip('/')}/api/report"
    body = json.dumps(payload).encode()
    req  = urllib.request.Request(url, data=body,
                                  headers={"Content-Type": "application/json"},
                                  method="POST")
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status == 200
    except urllib.error.URLError as e:
        print(f"  [!] POST failed: {e.reason}")
        return False


# ── Main ─────────────────────────────────────────────────────────────────────

def build_report(table_id: str, n_chairs: int, occ_fn, t: float) -> dict:
    chairs = []
    for i in range(n_chairs):
        chair_id = f"{table_id}C{i+1}"
        occupied = occ_fn(t, i)
        # Simulate realistic ADC values
        adc_raw  = random.randint(1800, 3200) if occupied else random.randint(50, 400)
        rssi     = random.randint(-65, -45)    # nearby chair
        batt     = random.randint(70, 100)
        chairs.append({
            "chair_id":    chair_id,
            "occupied":    occupied,
            "rssi":        rssi,
            "adc_raw":     adc_raw,
            "battery_pct": batt,
        })
    return {"table_id": table_id, "chairs": chairs}


def main():
    args    = parse_args()
    occ_fn  = SCENARIOS[args.scenario]
    t_start = time.monotonic()

    table_ids = [f"T{i+1}" for i in range(args.tables)]

    print(f"How Muddy? Sensor Simulator")
    print(f"  API:      {args.api}")
    print(f"  Tables:   {table_ids}")
    print(f"  Chairs:   {args.chairs} per table")
    print(f"  Scenario: {args.scenario}")
    print(f"  Interval: {args.interval}s")
    print()

    iteration = 0
    while True:
        t = time.monotonic() - t_start
        ts = datetime.now().strftime("%H:%M:%S")

        for tid in table_ids:
            payload = build_report(tid, args.chairs, occ_fn, t)
            ok      = post_report(args.api, payload)
            occ     = sum(1 for c in payload["chairs"] if c["occupied"])
            status  = "✓" if ok else "✗"
            print(f"  [{ts}] {status} {tid}: {occ}/{args.chairs} occupied")

        iteration += 1
        if args.once:
            break

        try:
            time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\nSimulator stopped.")
            break


if __name__ == "__main__":
    main()
