#!/usr/bin/env python3
"""Convert run telemetry into a compact JSON document."""

import csv
import json
import pathlib
import sys


def value(text: str):
    try:
        return float(text) if any(c in text for c in ".eE") else int(text)
    except ValueError:
        return text


source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
summary = {}
for line in source.read_text(encoding="utf-8").splitlines():
    if line.startswith("# summary "):
        summary = {k: value(v) for k, v in (item.split("=", 1) for item in line[10:].split())}
if not summary:
    raise SystemExit(f"No summary line found in {source}")

if len(sys.argv) >= 4:
    monitor = pathlib.Path(sys.argv[3])
    rows = []
    if monitor.is_file():
        with monitor.open(encoding="utf-8", newline="") as stream:
            rows = list(csv.DictReader(stream))
    if rows:
        def integers(name: str) -> list[int]:
            return [int(row[name]) for row in rows if row.get(name, "").strip()]

        rss = integers("rss_kib")
        process_swap = integers("process_swap_kib")
        gpu = integers("gpu_used_mib")
        temperatures = integers("gpu_temp_c")
        reads = integers("read_bytes")
        swap_in = integers("system_pswpin")
        swap_out = integers("system_pswpout")
        summary["monitor"] = {
            "samples": len(rows),
            "rss_peak_mib": round(max(rss) / 1024, 3),
            "process_swap_peak_mib": round(max(process_swap) / 1024, 3),
            "gpu_total_start_mib": gpu[0],
            "gpu_total_peak_mib": max(gpu),
            "gpu_delta_peak_mib": max(gpu) - gpu[0],
            "gpu_temp_peak_c": max(temperatures),
            "process_read_gib": round((max(reads) - min(reads)) / 2**30, 3),
            "system_swapin_delta_mib": round((swap_in[-1] - swap_in[0]) * 4 / 1024, 3),
            "system_swapout_delta_mib": round((swap_out[-1] - swap_out[0]) * 4 / 1024, 3),
        }
target.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
