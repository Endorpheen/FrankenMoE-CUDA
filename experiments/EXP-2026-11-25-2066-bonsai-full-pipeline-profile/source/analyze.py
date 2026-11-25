"""Measure server CUDA and CPU interval overlap from a complete Bonsai trace."""

import hashlib
import json
import os
from pathlib import Path
import sqlite3
import sys


def union(spans):
    result = []
    for lo, hi in sorted(spans):
        if result and lo <= result[-1][1]:
            result[-1] = (result[-1][0], max(result[-1][1], hi))
        else:
            result.append((lo, hi))
    return result


def duration(spans):
    return sum(hi - lo for lo, hi in spans)


def intersection(a, b):
    i = j = total = 0
    while i < len(a) and j < len(b):
        lo, hi = a[i]
        start, stop = b[j]
        total += max(0, min(hi, stop)-max(lo, start))
        if hi <= stop:
            i += 1
        else:
            j += 1
    return total


def clip(rows, start, end):
    return [(max(start, lo), min(end, hi), *rest) for lo, hi, *rest in rows
            if lo < end and start < hi]


def named_summary(rows):
    result = {}
    for lo, hi, name, *_ in rows:
        item = result.setdefault(name, {"count": 0, "ns": 0})
        item["count"] += 1
        item["ns"] += hi-lo
    return result


def group_kernel(name):
    if name.startswith(("bonsai_pq2", "bonsai_pack_half", "bonsai_pack_q8", "bonsai_mv_pack")):
        return "matrix_and_pack"
    if name.startswith(("bonsai_flash", "goose_q4_attention", "bonsai_decode_gqa")):
        return "attention"
    if name.startswith(("goose_gdn", "bonsai_decay", "bonsai_conv")):
        return "gdn_decay_conv"
    return "remaining"


def phase(start, end, kernel, copies, memset, driver, runtime, alloc):
    kernel = clip(kernel, start, end)
    copies = clip(copies, start, end)
    memset = clip(memset, start, end)
    driver = clip(driver, start, end)
    runtime = clip(runtime, start, end)
    gpu = union((lo, hi) for rows in (kernel, copies, memset) for lo, hi, *_ in rows)
    activity = duration(gpu)
    host = union((lo, hi) for lo, hi, *_ in driver)
    overlap = intersection(gpu, host)
    jobs = named_summary(kernel)
    groups = {}
    for name, value in jobs.items():
        group = group_kernel(name)
        groups[group] = groups.get(group, 0) + value["ns"]
    transfer = named_summary(copies)
    for name, item in transfer.items():
        item["bytes"] = sum(size for _, _, kind, size in copies if kind == name)
    return {
        "wall_ns": end-start,
        "gpu_active_ns": activity,
        "gpu_trace_idle_ns": end-start-activity,
        "gpu_kernel_groups_ns": groups,
        "gpu_top_kernel_sums": dict(sorted(jobs.items(), key=lambda item: -item[1]["ns"])[:12]),
        "gpu_kernel_count": len(kernel),
        "gpu_graph_node_count": sum(graph is not None and graph != 0 for *_, graph in kernel),
        "gpu_transfers": transfer,
        "gpu_memset_ns": sum(hi-lo for lo, hi, *_ in memset),
        "host_cuda_api": named_summary(driver),
        "host_cuda_api_union_ns": duration(host),
        "host_cuda_api_gpu_busy_overlap_ns": overlap,
        "host_cuda_api_gpu_idle_overlap_ns": duration(host)-overlap,
        "host_osrt_api": named_summary(runtime),
        "cuda_allocations": [{"offset_ns": at-start, "bytes": size, "type": op}
                             for at, size, op in alloc if start <= at < end],
    }


def proc_stat(path):
    fields = path.read_text().rsplit(")", 1)[1].split()
    return int(fields[11]), int(fields[12]), int(fields[7]), int(fields[9])


def analyze(directory, reference):
    trace = directory / "trace.sqlite"
    with sqlite3.connect(trace.resolve().as_uri() + "?mode=ro", uri=True) as db:
        epoch, = db.execute("select utcEpochNs from TARGET_INFO_SESSION_START_TIME").fetchone()
        pid, = db.execute("select distinct globalPid from CUPTI_ACTIVITY_KIND_KERNEL").fetchone()
        kernel = db.execute("""select k.start,k.end,s.value,k.gridX,k.gridY,k.graphNodeId
            from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.shortName
            where k.globalPid=? order by k.start""", (pid,)).fetchall()
        copies = db.execute("""select m.start,m.end,e.label,m.bytes
            from CUPTI_ACTIVITY_KIND_MEMCPY m join ENUM_CUDA_MEMCPY_OPER e on m.copyKind=e.id
            where m.globalPid=? order by m.start""", (pid,)).fetchall()
        memset = db.execute("""select start,end,bytes from CUPTI_ACTIVITY_KIND_MEMSET
            where globalPid=? order by start""", (pid,)).fetchall()
        alloc = db.execute("""select start,bytes,memoryOperationType from CUDA_GPU_MEMORY_USAGE_EVENTS
            where globalPid=? order by start""", (pid,)).fetchall()
        thread, = db.execute("""select r.globalTid from CUPTI_ACTIVITY_KIND_RUNTIME r
            join StringIds s on s.id=r.nameId where s.value='cuCtxCreate_v2'
            order by r.start limit 1""").fetchone()
        driver = db.execute("""select r.start,r.end,s.value from CUPTI_ACTIVITY_KIND_RUNTIME r
            join StringIds s on s.id=r.nameId where r.globalTid=? order by r.start""", (thread,)).fetchall()
        runtime = db.execute("""select r.start,r.end,s.value from OSRT_API r
            join StringIds s on s.id=r.nameId where r.globalTid=? order by r.start""", (thread,)).fetchall()

    def elapsed(name):
        return int((directory / name).read_text())-epoch

    launch, request, request_end = map(elapsed, ("launch-ns", "request-start-ns", "request-end-ns"))
    ready = next(lo for lo, _, name in runtime if name == "accept")
    heads = {"bonsai_pq2_mvq_q8_1": 62080, "bonsai_pq2_matvec_rows2": 31040}
    vocab = [(lo, hi) for lo, hi, name, grid, _, _ in kernel if request <= lo and heads.get(name) == grid]
    readback = [(lo, hi) for lo, hi, kind, size in copies
                if request <= lo and kind == "Device-to-Host" and size == 248320*4]
    assert len(vocab) == len(readback) == 512
    assert all(vocab[i][1] <= readback[i][0] and
               (i == 511 or readback[i][1] < vocab[i+1][0]) for i in range(512))
    response = json.loads((directory / "response.json").read_text())
    reference = json.loads(reference.read_text())
    timings = response["timings"]
    assert timings["cache_n"] == 0 and timings["prompt_n"] == 32768
    assert response["tokens"] == reference["tokens"] and response["content"] == reference["content"]

    fields = ("user_ticks", "system_ticks", "minor_faults", "major_faults")
    counters = dict(zip(fields, (b-a for a, b in zip(proc_stat(directory/"ready-stat"),
                                                    proc_stat(directory/"after-stat")))))
    counters["ticks_per_second"] = os.sysconf("SC_CLK_TCK")
    prefill_end, decode_end = readback[0][1], readback[-1][1]
    result = {
        "matched_ids": len(vocab),
        "server_timings": timings,
        "client_seconds": float((directory/"client-s").read_text()),
        "client_marker_span_ns": request_end-request,
        "response_tail_after_last_readback_ns": request_end-decode_end,
        "waiting_for_request_after_ready_ns": request-ready,
        "proc_request_window_counters": counters,
        "process_vram_after_mib": int((directory/"after-vram").read_text().split(",")[-1]),
        "ready_memory": (directory/"ready-memory").read_text().splitlines(),
        "after_memory": (directory/"after-memory").read_text().splitlines(),
    }
    for label, lo, hi in (("startup", launch, ready), ("prefill", request, prefill_end),
                          ("decode", prefill_end, decode_end)):
        result[label] = phase(lo, hi, kernel, copies, memset, driver, runtime, alloc)
    generic = {}
    for lo, hi, name, x, y, _ in clip(kernel, request, prefill_end):
        if name != "bonsai_pq2_cached_fp16_acc_k32":
            continue
        row = generic.setdefault((x, y), {"grid_x": x, "grid_y": y, "count": 0, "ns": 0})
        row["count"] += 1
        row["ns"] += hi-lo
    result["generic_fp16_partial_dispatches"] = sorted(generic.values(), key=lambda row: -row["ns"])
    with trace.open("rb") as file:
        result["trace_sqlite_sha256"] = hashlib.file_digest(file, "sha256").hexdigest()
    return result


if __name__ == "__main__":
    directory, reference = map(Path, sys.argv[1:])
    print(json.dumps(analyze(directory, reference), indent=2))
