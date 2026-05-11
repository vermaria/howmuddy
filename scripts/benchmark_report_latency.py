"""
scripts/benchmark_report_latency.py
Measure round-trip latency for POST /api/report (same HTTP path as the table gateway).

This is: time from sending the JSON body until the HTTP response is fully received.
It does NOT include ESP-NOW chair → gateway delay or gateway aggregation time; to
measure end-to-end from a chair, instrument the gateway firmware or use timestamps
in the payload plus server-side logging.

Interpreting the numbers (all in milliseconds):
    mean / median — typical round-trip for one POST after warmup.
    min / max     — best and worst single request (Wi‑Fi jitter, OS scheduling, Flask reload).
    p95           — “almost worst case” without one-off spikes; good for SLO-style reporting.
    stdev         — spread across runs; low means stable network and idle server.

Usage:
    python benchmark_report_latency.py
    python benchmark_report_latency.py --api http://10.29.143.185:5000 --iterations 50
    python benchmark_report_latency.py --api http://localhost:5001 --table-id T1 --chairs 4
"""
from __future__ import annotations

import argparse
import json
import statistics
import time
import urllib.error
import urllib.request


def build_payload(table_id: str, num_chairs: int) -> dict:
    chairs = []
    for i in range(1, num_chairs + 1):
        cid = f"{table_id}C{i}"
        chairs.append({
            "chair_id": cid,
            "occupied": (i % 2) == 0,
            "rssi": -50 - i * 2,
            "adc_raw": 1000 + i * 100,
            "battery_pct": max(0, 95 - i * 5),
        })
    return {"table_id": table_id, "chairs": chairs, "timestamp_ms": int(time.time() * 1000)}


def post_once(api_base: str, payload: dict) -> tuple[float, int]:
    url = f"{api_base.rstrip('/')}/api/report"
    body = json.dumps(payload).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    t0 = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            _ = resp.read()
            code = resp.getcode()
    except urllib.error.HTTPError as e:
        _ = e.read()
        code = e.code
    elapsed_ms = (time.perf_counter() - t0) * 1000.0
    return elapsed_ms, code


def percentile(sorted_vals: list[float], p: float) -> float:
    if not sorted_vals:
        return 0.0
    k = (len(sorted_vals) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(sorted_vals) - 1)
    if f == c:
        return sorted_vals[f]
    return sorted_vals[f] + (sorted_vals[c] - sorted_vals[f]) * (k - f)


def main() -> None:
    p = argparse.ArgumentParser(description="Benchmark POST /api/report latency.")
    p.add_argument("--api", default="http://localhost:5000", help="Backend base URL")
    p.add_argument("--table-id", default="T1", help="table_id in JSON body")
    p.add_argument("--chairs", type=int, default=4, help="Number of chairs in payload")
    p.add_argument("--iterations", type=int, default=30, help="Timed requests after warmup")
    p.add_argument("--warmup", type=int, default=3, help="Untimed requests first")
    args = p.parse_args()

    payload = build_payload(args.table_id, args.chairs)

    for _ in range(args.warmup):
        ms, code = post_once(args.api, payload)
        if code != 200:
            print(f"Warmup failed: HTTP {code} ({ms:.1f} ms)")
            return

    times: list[float] = []
    last_code = 200
    for _ in range(args.iterations):
        ms, last_code = post_once(args.api, payload)
        times.append(ms)
        if last_code != 200:
            print(f"Request failed: HTTP {last_code} ({ms:.1f} ms)")
            return

    times.sort()
    print(f"POST {args.api.rstrip('/')}/api/report")
    print(f"  payload: table_id={args.table_id}, chairs={args.chairs}")
    print(f"  iterations: {args.iterations} (warmup {args.warmup})")
    print()
    print(f"  min:    {min(times):8.2f} ms")
    print(f"  max:    {max(times):8.2f} ms")
    print(f"  mean:   {statistics.mean(times):8.2f} ms")
    print(f"  median: {statistics.median(times):8.2f} ms")
    print(f"  stdev:  {statistics.stdev(times):8.2f} ms" if len(times) > 1 else "  stdev:  n/a")
    print(f"  p95:    {percentile(times, 95):8.2f} ms")
    print()
    print("  (Gateway path only: JSON POST → Flask → DB → response. Not chair ESP-NOW.)")


if __name__ == "__main__":
    main()
