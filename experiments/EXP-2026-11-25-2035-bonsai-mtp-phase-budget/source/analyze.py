"""Assign traced GPU events to NVTX submission ranges and account for wall time."""
from bisect import bisect_right
from collections import defaultdict
import json
from pathlib import Path
import sqlite3
import sys


def union(spans):
    end = total = 0
    for start, stop in sorted(spans):
        if stop > end:
            total += stop - max(start, end)
            end = stop
    return total


def classify(name, stage, matrix):
    if matrix:
        return "matrix." + matrix.split(".")[-1]
    if name.startswith("bonsai_mtp_"):
        return "draft.projections"
    if name.startswith(("goose_gdn_", "bonsai_decay_", "bonsai_conv_")):
        return "gdn." + ("recurrence" if name.startswith("goose") else name.split("_")[1])
    if name.startswith(("bonsai_flash_", "bonsai_verify_q4_", "bonsai_decode_gqa", "goose_q4_attention")):
        return "attention.tiles_merge"
    if name.startswith("bonsai_choice_"):
        return "choice"
    return stage + ".other"


class Ranges:
    def __init__(self, ranges):
        self.items = ranges
        self.starts = [x[0] for x in ranges]

    def at(self, time):
        idx = bisect_right(self.starts, time) - 1
        if idx >= 0 and time < self.items[idx][1]:
            return self.items[idx][2]
        return None


def account(events, api, prefill, decode, passes, matrices):
    spans = {"prefill": prefill, "decode": decode}
    activity = {name: [] for name in spans}
    records = {name: defaultdict(lambda: [0, 0, 0]) for name in spans}
    for kind, a, b, corr, name, size, grid in events:
        launch = api.get(corr)
        for phase, (start, end) in spans.items():
            first, last = max(a, start), min(b, end)
            if first >= last:
                continue
            assert launch is not None, (kind, corr)
            stage = passes.at(launch) or "outside_child_range"
            matrix = matrices.at(launch)
            group = classify(name, stage, matrix) if kind == "kernel" else kind
            activity[phase].append((first, last, kind))
            row = records[phase][(stage, group, name, grid)]
            row[0] += last - first
            row[1] += 1
            row[2] += size
    results = {}
    for phase, (start, end) in spans.items():
        by_stage = defaultdict(lambda: defaultdict(int))
        by_group = defaultdict(int)
        by_kernel = defaultdict(int)
        copy_payload = defaultdict(int)
        for (stage, group, name, grid), (ns, calls, size) in records[phase].items():
            by_stage[stage][group] += ns
            by_group[group] += ns
            by_kernel[name] += ns
            if group == "copy":
                copy_payload[name] += size
        active_ns = union((a, b) for a, b, _ in activity[phase])
        kernels_ns = union((a, b) for a, b, k in activity[phase] if k == "kernel")
        copies_ns = union((a, b) for a, b, k in activity[phase] if k == "copy")
        results[phase] = {
            "wall_ms": (end - start) / 1e6,
            "gpu_active_union_ms": active_ns / 1e6,
            "gpu_kernel_union_ms": kernels_ns / 1e6,
            "gpu_copy_union_ms": copies_ns / 1e6,
            "gpu_gap_ms": (end - start - active_ns) / 1e6,
            "stage_group_event_ms": {stage: {group: ns / 1e6 for group, ns in groups.items()}
                                     for stage, groups in sorted(by_stage.items())},
            "group_event_ms": {group: ns / 1e6 for group, ns in sorted(by_group.items(), key=lambda x: -x[1])},
            "kernel_event_ms": {name: ns / 1e6 for name, ns in sorted(by_kernel.items(), key=lambda x: -x[1])},
            "copy_payload_bytes": dict(copy_payload),
            "detail": [dict(pass_name=stage, group=group, kernel_or_copy=name, grid_x=grid,
                            event_ms=ns / 1e6, calls=calls, bytes=size)
                       for (stage, group, name, grid), (ns, calls, size)
                       in sorted(records[phase].items(), key=lambda x: -x[1][0])],
        }
    return results


def main(path):
    db = sqlite3.connect(f"file:{path.resolve()}?mode=ro", uri=True)
    ranges = db.execute("""SELECT start, end, text FROM NVTX_EVENTS
        WHERE end IS NOT NULL AND text IS NOT NULL ORDER BY start""").fetchall()
    phases = {name: (first, last) for first, last, name in ranges if name.startswith("bonsai.")}
    pass_rows = [(a, b, n) for a, b, n in ranges if n.startswith("pass.")]
    matrix_rows = [(a, b, n) for a, b, n in ranges if n.startswith("matrix.")]
    assert list(phases) == ["bonsai.prefill", "bonsai.decode"]
    assert sum(n == "pass.target_verify" for _, _, n in pass_rows) == 185
    api_rows = db.execute("""SELECT start, end, correlationId, s.value FROM CUPTI_ACTIVITY_KIND_RUNTIME r
        JOIN StringIds s ON s.id = r.nameId ORDER BY start""").fetchall()
    api = {corr: a for a, _, corr, _ in api_rows}
    assert len(api) == len(api_rows)
    gpu = db.execute("""SELECT 'kernel', k.start, k.end, k.correlationId, s.value, 0, k.gridX
        FROM CUPTI_ACTIVITY_KIND_KERNEL k JOIN StringIds s ON s.id = k.shortName
        UNION ALL SELECT 'copy', m.start, m.end, m.correlationId, d.label, m.bytes, 0
        FROM CUPTI_ACTIVITY_KIND_MEMCPY m JOIN ENUM_CUDA_MEMCPY_OPER d ON d.id = m.copyKind
        UNION ALL SELECT 'memset', start, end, correlationId, 'memset', bytes, 0
        FROM CUPTI_ACTIVITY_KIND_MEMSET""").fetchall()
    report = account(gpu, api, phases["bonsai.prefill"], phases["bonsai.decode"],
                     Ranges(pass_rows), Ranges(matrix_rows))
    for phase, (start, end) in phases.items():
        a = report[phase.split(".")[1]]
        a["nvtx_pass_host_wall_ms"] = {
            n: sum(b - s for s, b, x in pass_rows if x == n and start <= s < b <= end) / 1e6
            for n in sorted({n for s, _, n in pass_rows if start <= s < end})
        }
        api_names = defaultdict(list)
        for s, t, _, name in api_rows:
            if max(s, start) < min(t, end):
                api_names[name].append((max(s, start), min(t, end)))
        a["cuda_api_wall_ms"] = {name: union(items) / 1e6 for name, items in api_names.items()}
    report["traced_gpu_events"] = len(gpu)
    report["traced_gpu_kernels"] = sum(row[0] == "kernel" for row in gpu)
    report["all_kernels_linked_to_cuda_api"] = all(e[3] in api for e in gpu if e[0] == "kernel")
    report["explicit_device_allocations_bytes"] = [r[0] for r in db.execute(
        "SELECT bytes FROM CUDA_GPU_MEMORY_USAGE_EVENTS WHERE memKind = 2 AND memoryOperationType = 0 ORDER BY start")]
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main(Path(sys.argv[1]))
