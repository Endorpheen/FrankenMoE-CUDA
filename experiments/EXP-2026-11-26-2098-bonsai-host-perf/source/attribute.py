"""Align CPU perf samples with GPU activity for a single cold Bonsai request."""

import bisect
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import sys


SAMPLE = re.compile(r'^.+?\s+(\d+)/(\d+)\s+(\d+)\.(\d{9}): cpu-clock:')
FRAME = re.compile(r'^\s+([0-9a-f]+) (.*) \((.*)\)$')
GUARD_NS = 10_000


def load_samples(path, offset_ns, pid):
    with path.open() as stream:
        active = None
        for line in stream:
            header = SAMPLE.match(line)
            if header:
                if active is not None:
                    yield active
                process, tid, sec, nanos = map(int, header.groups())
                active = (sec * 1_000_000_000 + nanos + offset_ns, tid, None) if process == pid else None
                continue
            frame = FRAME.match(line.rstrip("\n"))
            if active is not None and active[2] is None and frame:
                active = (*active[:2], frame.groups())
        if active is not None:
            yield active


def union(spans):
    result = []
    for lo, hi in sorted(spans):
        if result and lo <= result[-1][1]:
            result[-1] = (result[-1][0], max(result[-1][1], hi))
        else:
            result.append((lo, hi))
    return result


def complement(spans, lo, hi):
    gaps = []
    for start, stop in spans:
        if start > lo:
            gaps.append((lo, start))
        lo = max(lo, stop)
    if lo < hi:
        gaps.append((lo, hi))
    return gaps


def clip(rows, lo, hi):
    return [(max(start, lo), min(end, hi), *other) for start, end, *other in rows
            if start < hi and lo < end]


def overlap_ns(spans, gaps):
    i = j = total = 0
    while i < len(spans) and j < len(gaps):
        start, stop = spans[i]
        lo, hi = gaps[j]
        total += max(0, min(stop, hi) - max(start, lo))
        if stop <= hi:
            i += 1
        else:
            j += 1
    return total


def classify(frame):
    if frame is None:
        return "missing_frame", "missing"
    ip, symbol, path = frame
    dso = Path(path).name
    if ip.startswith("ffff") or "kernel" in dso:
        return "kernel", symbol
    if dso.startswith("libcuda.so"):
        return "cuda_driver", symbol
    if dso == "bonsai-server":
        return "frankenmoe", symbol
    if "ToolsInjection" in dso or "cupti" in dso.lower():
        return "profiler", symbol
    if dso.startswith("libc.so"):
        return "libc", symbol
    return dso, symbol


def phase(lo, hi, active, apis, samples, main_thread):
    gpu = union((a, b) for a, b in clip(active, lo, hi))
    gaps = complement(gpu, lo, hi)
    gap_starts = [a for a, _ in gaps]
    api_rows = clip(apis, lo, hi)
    api_starts = [a for a, _, _ in api_rows]
    api_ns = Counter()
    for name in sorted({name for _, _, name in api_rows}):
        api_ns[name] = overlap_ns(union((a,b) for a,b,n in api_rows if n == name), gaps)
    phase_samples = Counter()
    all_leaf = Counter()
    gap_leaf = Counter()
    gap_api = Counter()
    outside_cuda_gap_leaf = Counter()
    for at, tid, frame in samples:
        if not lo <= at < hi:
            continue
        if tid != main_thread:
            phase_samples["other_threads"] += 1
            continue
        dso, symbol = classify(frame)
        phase_samples["main_thread"] += 1
        all_leaf[dso] += 1
        i = bisect.bisect_right(gap_starts, at) - 1
        if i < 0 or at >= gaps[i][1]:
            continue
        start, end = gaps[i]
        if at - start < GUARD_NS or end - at <= GUARD_NS:
            phase_samples["uncertain_gap_boundary"] += 1
            continue
        phase_samples["inside_gpu_gap"] += 1
        gap_leaf[dso] += 1
        j = bisect.bisect_right(api_starts, at) - 1
        api = api_rows[j][2] if j >= 0 and at < api_rows[j][1] else "outside_cuda_api"
        gap_api[api] += 1
        if api == "outside_cuda_api":
            outside_cuda_gap_leaf[f"{dso}: {symbol}"] += 1
    return {
        "wall_ns": hi-lo, "gpu_gap_ns": sum(end-start for start,end in gaps),
        "gpu_gap_cuda_api_overlap_ns": dict(api_ns),
        "gpu_gap_cuda_api_union_overlap_ns": overlap_ns(union((a,b) for a,b,_ in api_rows), gaps),
        "gpu_gap_interior_ns": sum(max(0, end-start-2*GUARD_NS) for start,end in gaps),
        "samples": dict(phase_samples), "main_thread_leaf_group": dict(all_leaf),
        "gpu_gap_leaf_group": dict(gap_leaf), "gpu_gap_cuda_api_samples": dict(gap_api),
        "outside_cuda_api_gap_top_leaf": dict(outside_cuda_gap_leaf.most_common(20)),
    }


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file,"sha256").hexdigest()


def main():
    directory = Path(sys.argv[1])
    clocks = [json.loads((directory / f"clock-{name}.json").read_text()) for name in ("before","after")]
    offsets = [c["utc_ns"] - c["monotonic_ns"] for c in clocks]
    assert abs(offsets[0]-offsets[1]) < GUARD_NS
    trace = directory / "trace.sqlite"
    with sqlite3.connect(trace.resolve().as_uri() + "?mode=ro", uri=True) as db:
        epoch, = db.execute("SELECT utcEpochNs FROM TARGET_INFO_SESSION_START_TIME").fetchone()
        start,end = [int((directory / marker).read_text())-epoch for marker in ("request-start-ns","request-end-ns")]
        copies = db.execute("""SELECT start,end,copyKind,bytes FROM CUPTI_ACTIVITY_KIND_MEMCPY
                               WHERE start >= ? AND end <= ? ORDER BY start""",(start,end)).fetchall()
        readbacks = [hi for _,hi,kind,size in copies if kind==2 and size==993280]
        assert len(readbacks)==512
        active = [row[:2] for row in copies]
        for table in ("CUPTI_ACTIVITY_KIND_KERNEL","CUPTI_ACTIVITY_KIND_MEMSET"):
            active.extend(db.execute(f"SELECT start,end FROM {table} WHERE start >= ? AND end <= ?",(start,end)))
        thread, = db.execute("""SELECT r.globalTid FROM CUPTI_ACTIVITY_KIND_RUNTIME r
            JOIN StringIds s ON s.id=r.nameId WHERE s.value='cuCtxCreate_v2' ORDER BY r.start LIMIT 1""").fetchone()
        apis = db.execute("""SELECT r.start,r.end,s.value FROM CUPTI_ACTIVITY_KIND_RUNTIME r
            JOIN StringIds s ON s.id=r.nameId WHERE r.globalTid=? AND r.start < ? AND r.end > ?
            ORDER BY r.start""",(thread,end,start)).fetchall()
    pid = int((directory/"server.pid").read_text())
    samples = list(load_samples(directory/"perf.txt",sum(offsets)//2-epoch,pid))
    assert samples
    print(json.dumps({
        "clock_calibration": clocks, "utc_epoch_ns": epoch, "guard_ns": GUARD_NS,
        "samples_read":len(samples), "perf_data_sha256":digest(directory/"perf.data"),
        "trace_sqlite_sha256":digest(trace),
        "prefill":phase(start,readbacks[0],active,apis,samples,pid),
        "decode":phase(readbacks[0],readbacks[-1],active,apis,samples,pid),
    },indent=2))


if __name__ == "__main__":
    main()
