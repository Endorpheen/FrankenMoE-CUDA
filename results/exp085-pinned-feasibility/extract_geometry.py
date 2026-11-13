"""Extract pageable H2D copy geometry from a saved CUPTI trace.

Reads memcpy rows from an exp084-style profile SQLite database, filters them to
the prefill client window from an exp084-style analysis.json, and writes the
sorted byte sizes as a small JSON summary. Standard library only.
"""
import argparse
import json
import math
import os
import sqlite3
import sys

DB_QUERY = """
    SELECT bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY
    WHERE end > ? AND start < ? AND copyKind = 1 AND srcKind = 0
    ORDER BY start, end, correlationId
"""


def fail(message):
    sys.exit(f"error: {message}")


def nearest_rank_percentile(sorted_sizes, p):
    k = math.ceil(p * len(sorted_sizes)) - 1
    return sorted_sizes[k]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-db", required=True, help="path to the profile SQLite DB (read-only)")
    parser.add_argument("--analysis", required=True, help="path to the exp084 analysis.json")
    parser.add_argument("--output", required=True, help="path for the output JSON (must not already exist)")
    args = parser.parse_args()

    if not os.path.isfile(args.trace_db):
        fail(f"trace database not found: {args.trace_db}")
    if not os.path.isfile(args.analysis):
        fail(f"analysis file not found: {args.analysis}")
    if os.path.exists(args.output):
        fail(f"output path already exists: {args.output}")
    if not os.path.isdir(os.path.dirname(os.path.abspath(args.output))):
        fail(f"output directory does not exist: {args.output}")

    with open(args.analysis) as f:
        analysis = json.load(f)
    lo, hi = analysis["requests"]["cold"]["prefill_client_window_ns"]
    if lo > hi:
        fail(f"invalid client window in analysis: {lo} > {hi}")

    # mode=ro and no schema mutation guarantees the trace is never written.
    conn = sqlite3.connect("file:" + args.trace_db + "?mode=ro", uri=True)
    cursor = conn.execute(DB_QUERY, (lo, hi))
    sizes = [row[0] for row in cursor.fetchall()]
    conn.close()

    if not sizes:
        fail("no pageable H2D ranges fall inside the client window")

    # sizes_bytes keeps the trace order; percentiles are computed on a sorted
    # copy, so a re-run produces byte-identical output.
    sorted_sizes = sorted(sizes)
    n = len(sizes)
    total = sum(sizes)
    summary = {
        "schema_version": 1,
        "window_ns": [lo, hi],
        "sizes_bytes": sizes,
        "n_ranges": n,
        "sum_bytes": total,
        "max_bytes": sorted_sizes[-1],
        "p50_bytes": nearest_rank_percentile(sorted_sizes, 0.50),
        "p90_bytes": nearest_rank_percentile(sorted_sizes, 0.90),
        "p99_bytes": nearest_rank_percentile(sorted_sizes, 0.99),
        "source_description": "Measured pageable H2D size/order geometry; exact GGUF source offsets are unknown",
    }
    with open(args.output, "w") as f:
        json.dump(summary, f, indent=2)
        f.write("\n")
    print(f"{n} ranges, {total} bytes")


if __name__ == "__main__":
    main()
