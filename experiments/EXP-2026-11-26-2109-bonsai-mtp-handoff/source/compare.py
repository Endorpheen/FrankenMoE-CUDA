"""Measure four-row verifier work from the saved Bonsai MTP artifacts."""

from bisect import bisect_right
from collections import defaultdict
from hashlib import file_digest
import json
from pathlib import Path
import re
import sqlite3
from statistics import mean
import sys


def digest(path):
    with path.open("rb") as file:
        return file_digest(file, "sha256").hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def archived_cli(path, http_ids):
    records = []
    for arm in ("b1", "b2"):
        log, output = (path / f"{arm}.{ext}" for ext in ("log", "jsonl"))
        raw = log.read_text()
        block, proposed, accepted, draft_ms, target_ms, catchup_ms = re.search(
            r"mtp_blocks=(\d+) proposed=(\d+) accepted=(\d+).*? draft_ms=([\d.]+)"
            r" target_ms=([\d.]+) catchup_ms=([\d.]+)", raw).groups()
        prefill_ms, decode_ms = re.search(r"prefill_ms=([\d.]+) decode_n=512 decode_ms=([\d.]+)", raw).groups()
        tokens = [json.loads(line)["token"] for line in output.read_text().splitlines()]
        assert len(tokens) == len(http_ids) == 512 and tokens == http_ids
        row = dict(zip(("blocks", "proposed", "accepted"), map(int, (block, proposed, accepted))))
        row.update(zip(("draft_ms", "target_ms", "catchup_ms", "prefill_ms", "decode_ms"),
                       map(float, (draft_ms, target_ms, catchup_ms, prefill_ms, decode_ms))))
        row.update(log_sha256=digest(log), output_sha256=digest(output))
        records.append(row)
    return {"arms": records, "ids_matching_http": 512, "raw_cuda_trace_available": False,
            "mean_target_host_ms_per_block": mean(row["target_ms"] / row["blocks"] for row in records)}


def http_trace(path):
    resp = read_json(path / "response.json")
    cold = read_json(path / "cold.json")
    assert cold["cold_prefill"] == "PASS" and cold["response_sha256"] == digest(path / "response.json")
    assert cold["computed_prompt_tokens"] == 32768 and cold["cached_prompt_tokens"] == 0
    trace = path / "trace.sqlite"
    with sqlite3.connect(trace.resolve().as_uri() + "?mode=ro", uri=True) as db:
        epoch, = db.execute("SELECT utcEpochNs FROM TARGET_INFO_SESSION_START_TIME").fetchone()
        lo, hi = [int((path / f"request-{part}-ns").read_text()) - epoch for part in ("start", "end")]
        copies = db.execute("""SELECT start, end, bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY
            WHERE copyKind=2 AND start>=? AND end<=? ORDER BY start""", (lo, hi)).fetchall()
        assert copies[0][2] == 993280 and copies[-1][2] == 12
        assert (len(copies) - 2) % 4 == 0
        windows = []
        for i in range(1, len(copies) - 1, 4):
            chunk = copies[i:i + 4]
            assert [row[2] for row in chunk] == [12, 12, 12, 48]
            windows.append((chunk[2][1], chunk[3][1]))
        start = [a for a, _ in windows]
        data = defaultdict(lambda: {"launches": 0, "ns": 0, "graph_nodes": 0})
        for name, a, b, graph in db.execute("""SELECT s.value, k.start, k.end, k.graphNodeId
            FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.shortName
            WHERE k.start>=? AND k.end<=?""", (copies[0][1], hi)):
            i = bisect_right(start, a) - 1
            if i >= 0 and b <= windows[i][1]:
                phase = "four_row_verifier"
            else:
                phase = "ordinary_tail" if a >= windows[-1][1] else "draft_or_repair"
            item = data[phase, name]
            item["launches"] += 1
            item["ns"] += b - a
            item["graph_nodes"] += bool(graph)
    phases = {}
    for (phase, name), record in sorted(data.items(), key=lambda row: -row[1]["ns"]):
        phases.setdefault(phase, {})[name] = record
    graph_nodes = {phase: sum(item["graph_nodes"] for item in kernels.values())
                   for phase, kernels in phases.items()}
    counters = re.findall(r"\[bonsai-mtp\] proposed=(\d+) accepted=(\d+)",
                          (path / "server.log").read_text())
    assert len(windows) == len(counters) and graph_nodes["four_row_verifier"] == 0
    return {"trace_sha256": digest(path / "cuda.nsys-rep"), "sqlite_sha256": digest(trace),
            "request_sha256": cold["request_sha256"], "timings": resp["timings"],
            "verifier_blocks": len(windows), "proposed": sum(int(n) for n, _ in counters),
            "accepted": sum(int(a) for _, a in counters),
            "verifier_readback_window_sum_ms": sum(b-a for a,b in windows) / 1e6,
            "graph_nodes": graph_nodes, "kernel_phases": phases}, resp["tokens"]


if __name__ == "__main__":
    cli, http = map(Path, sys.argv[1:])
    trace, ids = http_trace(http)
    print(json.dumps({"exp2047": archived_cli(cli, ids), "exp2106_diagnostic": trace}, indent=2))
