"""Attribute Bonsai prefill from Nsight SQLite exports and real responses."""

import argparse
import hashlib
import json
from pathlib import Path
import sqlite3


def read_trace(path, prism):
    trace = path / "trace.sqlite"
    with sqlite3.connect(trace.resolve().as_uri() + "?mode=ro", uri=True) as db:
        rows = db.execute(
            """select s.value,k.gridX,k.start,k.end
               from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName
               order by k.start"""
        ).fetchall()

    if prism:
        first_mmq = next(i for i, row in enumerate(rows) if row[0] == "quantize_mmq_q8_1")
        start = max(i for i, row in enumerate(rows[:first_mmq]) if row[0] == "k_get_rows")
        heads = {("mul_mat_vec_q", 248320)}
    else:
        start = next(i for i, row in enumerate(rows) if row[0] == "bonsai_embed_batch")
        heads = {("bonsai_pq2_mvq_q8_1", 62080), ("bonsai_pq2_matvec_rows2", 31040)}
    end = [i for i in range(start, len(rows)) if rows[i][:2] in heads]
    result = json.loads((path / "response.json").read_text())
    ids = result["tokens"]
    timing = result["timings"]
    if len(end) != 16 or len(ids) != 16 or timing["prompt_n"] != 32768 or timing["cache_n"] != 0:
        raise ValueError(f"{path}: expected a complete uncached 32K/16 trace")

    kernels = {}
    for name, _, begin, finish in rows[start:end[0] + 1]:
        kernel = kernels.setdefault(name, {"ns": 0, "calls": 0})
        kernel["ns"] += finish - begin
        kernel["calls"] += 1
    with trace.open("rb") as file:
        digest = hashlib.file_digest(file, "sha256").hexdigest()
    return ids, {
        "trace_sqlite_sha256": digest,
        "timings": timing,
        "gpu_prefill_span_ns": rows[end[0]][3] - rows[start][2],
        "prefill": kernels,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("prism", "exp2019", "exp2049"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    ids, prism = read_trace(args.prism, True)
    ids2019, exp2019 = read_trace(args.exp2019, False)
    ids2049, exp2049 = read_trace(args.exp2049, False)
    if ids != ids2019 or ids != ids2049:
        raise ValueError("The three traces returned different token IDs")
    print(json.dumps({
        "token_ids": ids,
        "prism": prism,
        "exp2019": exp2019,
        "exp2049": exp2049,
    }, indent=2))


if __name__ == "__main__":
    main()
