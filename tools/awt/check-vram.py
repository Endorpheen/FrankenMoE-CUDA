#!/usr/bin/env python3
"""Verdict on a run's VRAM profile against the recorded baseline.

Three questions, which are the ones the investigation needed and could not answer:
  constant  - does this server's peak exceed the baseline's by more than the tolerance?
  growth    - does it grow across the run, which is the shape of a leak?
  neighbours- does the rest of the card move, which would mean the difference is not ours?

usage: check-vram.py <run-dir> [--baseline file] [--update]
"""
import json
import pathlib
import sys

DEFAULT_BASELINE = pathlib.Path("docs/goose-migration/VRAM-BASELINE.json")
TOLERANCE_PEAK_MIB = 300
TOLERANCE_GROWTH_MIB = 512


def read_trace(path):
    rows = []
    for line in path.read_text().strip().split("\n"):
        if not line:
            continue
        t, total, server, other = line.split("\t")
        rows.append({"t": int(t), "total": int(total), "server": int(server), "other": int(other)})
    return rows


def profile(run_dir):
    tsv = next(iter(sorted(run_dir.glob("*.tsv"))), None)
    if tsv is None:
        return None
    rows = read_trace(tsv)
    if not rows:
        return None
    return {
        "samples": len(rows),
        "seconds": rows[-1]["t"],
        "server_start": rows[0]["server"],
        "server_peak": max(r["server"] for r in rows),
        "other_start": rows[0]["other"],
        "other_peak": max(r["other"] for r in rows),
        "growth": rows[-1]["server"] - rows[0]["server"],
    }


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print("usage: check-vram.py <run-dir> [--baseline file] [--update]")
        return 2
    run_dir = pathlib.Path(args[0])
    baseline_path = DEFAULT_BASELINE
    update = "--update" in args
    if "--baseline" in args:
        baseline_path = pathlib.Path(args[args.index("--baseline") + 1])

    prof = profile(run_dir)
    if prof is None:
        print(f"check-vram: FAIL, no samples in {run_dir}")
        return 1

    print(f"check-vram: samples={prof['samples']} seconds={prof['seconds']}")
    print(f"check-vram: this server start={prof['server_start']} peak={prof['server_peak']} growth={prof['growth']} MiB")
    print(f"check-vram: everything else start={prof['other_start']} peak={prof['other_peak']} MiB")

    base = json.loads(baseline_path.read_text()) if baseline_path.exists() else None
    if update or base is None:
        payload = {"recorded_from": run_dir.name, **prof}
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n")
        print(f"check-vram: baseline written to {baseline_path} ({payload['server_peak']} MiB peak, "
              f"{payload['growth']} MiB growth)")
        return 0

    fail = []
    d_peak = prof["server_peak"] - base["server_peak"]
    print(f"check-vram: peak versus baseline {base['server_peak']} MiB = {d_peak:+d} MiB "
          f"(tolerance {TOLERANCE_PEAK_MIB})")
    if d_peak > TOLERANCE_PEAK_MIB:
        fail.append(f"peak grew by {d_peak} MiB over the baseline")

    print(f"check-vram: growth {prof['growth']:+d} MiB (tolerance {TOLERANCE_GROWTH_MIB})")
    if prof["growth"] > TOLERANCE_GROWTH_MIB:
        fail.append(f"the server grew by {prof['growth']} MiB during the run")

    d_other = prof["other_peak"] - base["other_start"]
    if d_other > TOLERANCE_PEAK_MIB:
        print(f"check-vram: note, the rest of the card moved by {d_other:+d} MiB, so the compare is not clean")

    if fail:
        for f in fail:
            print(f"check-vram: FAIL, {f}")
        return 1
    print("check-vram: ok, peak and growth are within the baseline")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
