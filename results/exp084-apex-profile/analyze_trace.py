"""Read the EXP-084 capture; never launch a model or issue requests."""

import json
from pathlib import Path
import re
import sqlite3
import time

from apex_interval_union import complement, duration, merge_intervals, summarize


ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "results/archive/EXP-2026-10-30-084/raw"


def trace_window(name, epoch):
    anchor = json.loads((RAW / f"{name}-start.json").read_text())
    day = time.localtime(anchor["wall_ns"] // 10**9)
    midnight = int(time.mktime((*day[:3], 0, 0, 0, 0, 0, -1))) * 10**9
    current = None
    begin = first = finish = None
    for line in (RAW / f"{name}.trace").read_text().splitlines():
        stamp = re.match(r"(\d\d):(\d\d):(\d\d)\.(\d{6}) ", line)
        if stamp:
            h, m, s, micro = map(int, stamp.groups())
            current = midnight + (h * 3600 + m * 60 + s) * 10**9 + micro * 1000
            if "=> Send header" in line and begin is None:
                begin = current
            if "<= Recv data" in line:
                finish = current
        if first is None and re.search(r'"content"\s*:\s*"[^"\s]', line):
            first = current
    assert begin is not None and first is not None and finish is not None, name
    events = [json.loads(line[6:]) for line in (RAW / f"{name}.sse").read_text().splitlines()
              if line.startswith("data: {")]
    final = next(event for event in reversed(events) if event.get("timings"))
    assert final["timings"]["predicted_n"] == 128 and not final.get("truncated")
    return {
        "window_ns": [begin - epoch, finish - epoch],
        "prefill_client_window_ns": [begin - epoch, first - epoch],
        "decode_client_window_ns": [first - epoch, finish - epoch],
        "client_ttft_s": (first - begin) / 10**9,
        "client_total_s": (finish - begin) / 10**9,
        "server_timings": final["timings"],
    }


def phase(conn, window):
    lo, hi = window
    intervals = {}
    for key, table in [("kernels", "KERNEL"), ("copies", "MEMCPY"), ("memsets", "MEMSET")]:
        intervals[key] = conn.execute(
            f"SELECT start,end FROM CUPTI_ACTIVITY_KIND_{table} WHERE end>? AND start<?", (lo, hi)
        ).fetchall()
    result = summarize(window, intervals["kernels"], intervals["copies"])
    busy = merge_intervals(intervals["kernels"] + intervals["copies"] + intervals["memsets"], window)
    result["gpu_with_memset_union_ns"] = duration(busy)
    result["window_minus_gpu_ns"] = hi - lo - duration(busy)
    result["memset_count"] = len(intervals["memsets"])
    gaps = complement(busy, window)
    result["max_gpu_gap_ns"] = max((e - s for s, e in gaps), default=0)
    result["gpu_gaps_above_1ms"] = sum(e - s > 10**6 for s, e in gaps)
    result["copies_by_kind"] = []
    for kind, count, size in conn.execute(
        "SELECT copyKind,COUNT(*),SUM(bytes) FROM CUPTI_ACTIVITY_KIND_MEMCPY "
        "WHERE end>? AND start<? GROUP BY copyKind", (lo, hi)
    ):
        spans = conn.execute("SELECT start,end FROM CUPTI_ACTIVITY_KIND_MEMCPY "
                             "WHERE end>? AND start<? AND copyKind=?", (lo, hi, kind)).fetchall()
        union = duration(merge_intervals(spans, window))
        result["copies_by_kind"].append({"kind": kind, "count": count, "bytes": size, "union_ns": union})
    result["h2d_host_memory"] = [dict(kind=k, count=n, bytes=b) for k, n, b in conn.execute(
        "SELECT srcKind,COUNT(*),SUM(bytes) FROM CUPTI_ACTIVITY_KIND_MEMCPY "
        "WHERE copyKind=1 AND end>? AND start<? GROUP BY srcKind", (lo, hi))]
    result["top_kernels"] = [dict(name=n, count=c, summed_ns=t) for n, c, t in conn.execute(
        "SELECT s.value,COUNT(*),SUM(MIN(k.end,?)-MAX(k.start,?)) "
        "FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id=k.demangledName "
        "WHERE k.end>? AND k.start<? GROUP BY s.value ORDER BY 3 DESC LIMIT 8", (hi, lo, lo, hi)
    )]
    result["top_cuda_apis"] = [dict(name=n, count=c, summed_ns=t, max_ns=m) for n, c, t, m in conn.execute(
        "SELECT s.value,COUNT(*),SUM(MIN(r.end,?)-MAX(r.start,?)),MAX(r.end-r.start) "
        "FROM CUPTI_ACTIVITY_KIND_RUNTIME r JOIN StringIds s ON s.id=r.nameId "
        "WHERE r.end>? AND r.start<? GROUP BY s.value ORDER BY 3 DESC LIMIT 12", (hi, lo, lo, hi)
    )]
    return result


def main():
    with sqlite3.connect(f"file:{RAW / 'profile.sqlite'}?mode=ro", uri=True) as conn:
        epoch = conn.execute("SELECT utcEpochNs FROM TARGET_INFO_SESSION_START_TIME").fetchone()[0]
        output = {"session_epoch_ns": epoch, "clock_mapping": "curl local wall time minus Nsight UTC epoch",
                  "window_limit": "client prefill includes transport and first sampled-token delivery",
                  "attribution_limit": "GPU gaps do not prove CPU computation; sums may overlap and are not additive savings",
                  "requests": {}}
        output["profiler_warnings"] = [text for (text,) in conn.execute(
            "SELECT text FROM DIAGNOSTIC_EVENT WHERE severity>1")]
        for name in ["cold", "branch", "repeat"]:
            request = trace_window(name, epoch)
            request["phases"] = {key: phase(conn, request[field]) for key, field in [
                ("whole", "window_ns"), ("prefill", "prefill_client_window_ns"),
                ("decode", "decode_client_window_ns")]}
            output["requests"][name] = request
    (Path(__file__).parent / "analysis.json").write_text(json.dumps(output, indent=2) + "\n")
    for name, request in output["requests"].items():
        print(name, "TTFT", round(request["client_ttft_s"], 6), "total", round(request["client_total_s"], 6))
        for phase_name in ["prefill", "decode"]:
            item = request["phases"][phase_name]
            print(phase_name, {k: round(item[k] / 10**9, 6) for k in [
                "window_ns", "kernel_union_ns", "copy_union_ns", "overlap_ns", "window_minus_gpu_ns"]})
            print("copies", item["copies_by_kind"])
            print("kernels", [(x["name"][:90], round(x["summed_ns"] / 10**9, 6)) for x in item["top_kernels"][:4]])
            print("APIs", item["top_cuda_apis"][:4])


if __name__ == "__main__":
    main()
