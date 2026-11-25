"""Attribute two complete Bonsai 32K/512 Nsight Systems captures."""

import hashlib
import json
from pathlib import Path
import sqlite3
import sys


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def union_ns(rows):
    elapsed = 0
    rightmost = 0
    for start, end in sorted(rows):
        elapsed += max(0, end - max(start, rightmost))
        rightmost = max(rightmost, end)
    return elapsed


def read_capture(path):
    trace = path / "trace.sqlite"
    response = json.loads((path / "response.json").read_text())
    gate = json.loads((path / "cold.json").read_text())
    assert gate["cold_prefill"] == "PASS"
    assert gate["response_sha256"] == digest(path / "response.json")
    assert len(response["tokens"]) == 512
    del gate["process_identity"]
    result = {
        "client_s": float((path / "client-s").read_text()),
        "timings": response["timings"], "cold_prefill_gate": gate,
        "process_vram_after_mib": int((path / "after-vram").read_text().split(",")[-1]),
        "process_memory": dict(line.split(":", 1) for line in (path / "after-memory").read_text().splitlines()),
        "trace_sqlite_sha256": digest(trace),
    }
    with sqlite3.connect(trace.resolve().as_uri() + "?mode=ro", uri=True) as db:
        epoch, = db.execute("SELECT utcEpochNs FROM TARGET_INFO_SESSION_START_TIME").fetchone()
        start, end = [int((path / marker).read_text()) - epoch
                      for marker in ("request-start-ns", "request-end-ns")]
        copies = db.execute("""SELECT start, end, copyKind, bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY
                               WHERE start >= ? AND end <= ? ORDER BY start""", (start, end)).fetchall()
        readbacks = [(lo, hi) for lo, hi, kind, size in copies if kind == 2 and size == 993280]
        assert len(readbacks) == 512, (path, len(readbacks))
        result["full_vocabulary_readbacks"] = len(readbacks)
        result["response_tail_ns"] = end-readbacks[-1][1]
        for label, lo, hi in (("prefill", start, readbacks[0][1]),
                              ("decode", readbacks[0][1], readbacks[-1][1])):
            kernels = db.execute("""SELECT s.value, k.start, k.end FROM CUPTI_ACTIVITY_KIND_KERNEL k
                JOIN StringIds s ON s.id=k.shortName WHERE k.start >= ? AND k.end <= ?""", (lo, hi))
            spans, totals = [], {}
            for name, a, b in kernels:
                spans.append((a, b))
                item = totals.setdefault(name, {"launches": 0, "ns": 0})
                item["launches"] += 1
                item["ns"] += b-a
            copies_in_phase = [copy for copy in copies if copy[0] >= lo and copy[1] <= hi]
            phase_copies = {}
            for a, b, kind, size in copies_in_phase:
                item = phase_copies.setdefault(kind, {"count": 0, "ns": 0, "bytes": 0})
                item["count"] += 1
                item["ns"] += b-a
                item["bytes"] += size
            memsets = db.execute("""SELECT start, end FROM CUPTI_ACTIVITY_KIND_MEMSET
                                    WHERE start >= ? AND end <= ?""", (lo, hi)).fetchall()
            active = union_ns(spans + [row[:2] for row in copies_in_phase] + memsets)
            geometry = db.execute("""SELECT s.value, d.value, k.gridX, k.gridY, k.gridZ,
                k.blockX, k.blockY, k.blockZ, k.registersPerThread, k.staticSharedMemory,
                k.dynamicSharedMemory, COUNT(*), SUM(k.end-k.start)
                FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.shortName
                JOIN StringIds d ON d.id=k.demangledName WHERE k.start >= ? AND k.end <= ?
                AND (s.value LIKE 'bonsai_pq2%' OR s.value LIKE 'mul_mat_q%'
                   OR s.value IN ('bonsai_decode_gqa3', 'goose_q4_attention_merge',
                                  'flash_attn_ext_f16', 'flash_attn_stream_k_fixup_uniform'))
                GROUP BY 1,2,3,4,5,6,7,8,9,10,11 ORDER BY 13 DESC""", (lo, hi)).fetchall()
            graph_nodes, = db.execute("""SELECT COUNT(*) FROM CUPTI_ACTIVITY_KIND_KERNEL
                WHERE start >= ? AND end <= ? AND graphNodeId != 0""", (lo, hi)).fetchone()
            result[label] = {
                "wall_ns": hi-lo, "gpu_activity_union_ns": active, "gpu_gap_ns": hi-lo-active,
                "copies_by_cupti_kind": phase_copies, "cuda_graph_kernel_nodes": graph_nodes,
                "kernels": dict(sorted(totals.items(), key=lambda row: -row[1]["ns"])),
                "geometry_columns": ["kernel", "instance", "grid_x", "grid_y", "grid_z", "block_x",
                                     "block_y", "block_z", "registers", "static_smem", "dynamic_smem",
                                     "launches", "ns"],
                "hot_geometry": geometry,
            }
    return result, response


def main():
    prism_path, goose_path = map(Path, sys.argv[1:3])
    prism, prism_response = read_capture(prism_path)
    goose, goose_response = read_capture(goose_path)
    assert prism_response["tokens"] == goose_response["tokens"]
    assert prism_response["content"] == goose_response["content"]
    assert prism["cold_prefill_gate"]["request_sha256"] == goose["cold_prefill_gate"]["request_sha256"]
    print(json.dumps({"prism": prism, "goose": goose, "matched_ids": 512,
                      "matched_text": True}, indent=2))


if __name__ == "__main__":
    main()
