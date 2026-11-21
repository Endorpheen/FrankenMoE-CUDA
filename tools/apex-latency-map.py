#!/usr/bin/env python3
"""Compare complete Apex 32K/512 CUDA traces and separate CPU clock profiles."""

import argparse
from bisect import bisect_left, bisect_right
from collections import Counter, defaultdict
from contextlib import closing
from dataclasses import dataclass, field
from decimal import Decimal
import json
from pathlib import Path
import re
import sqlite3
import subprocess


KERNEL_FAMILIES = {
    "MMQ": ("mul_mat_q",),
    "MMQ pack": ("quantize_mmq_q8_1", "goose_mmq_pack"),
    "MMQ fixup": ("mul_mat_q_stream_k_fixup", "goose_mmq_stream_fixup"),
    "MMVQ": ("mul_mat_vec_q", "mul_mat_vec_q_moe", "goose_mmvq_matvec"),
    "MMVQ pack": ("quantize_q8_1", "goose_mmvq_quantize"),
    "Dense and generic matmul": (
        "mul_mat_f", "mul_mat_vec_f", "dot_kernel", "goose_dense_mma", "goose_quantized_matmul",
    ),
    "Flash Attention MMA": (
        "flash_attn_ext_f16", "flash_attn_mask_to_KV_max", "flash_attn_stream_k_fixup",
        "dequantize_block_q4_0", "goose_flash_causal_mma", "goose_flash_stream",
        "goose_flash_fixup", "goose_flash_q4_stage",
    ),
    "Vector Attention": ("flash_attn_ext_vec", "flash_attn_combine_results", "goose_flash_vec"),
    "Other Attention": ("goose_q4_attention",),
    "GDN": ("gdn_single", "gated_delta_net_cuda", "goose_gdn", "goose_gated_delta_net"),
    "SSM convolution": ("ssm_conv", "goose_ssm_conv"),
    "RMSNorm": ("rms_norm_f32", "goose_rms_reference"),
    "Pointwise and GLU": ("k_bin_bcast", "unary_", "scale_f32", "goose_vector_ops", "goose_vector_flat"),
    "MoE router": ("topk_moe_cuda", "goose_route_"),
    "MoE grouping": ("mm_ids_helper",),
    "MoE weighted reduction": ("moe_weighted_reduction_f32", "goose_moe_weighted"),
    "Tensor gather and blit": ("k_get_rows", "concat_", "cpy_scalar", "goose_tensor_blit"),
    "KV writes": ("k_set_rows_quant", "goose_set_q4_rows"),
    "KV FWHT": ("fwht_cuda", "goose_kv_fwht"),
    "RoPE": ("rope_multi", "goose_rope_"),
}


def kernel_family(name, demangled):
    if "cutlass::Kernel" in demangled or "cublasGemv" in demangled:
        return "Dense and generic matmul"
    if name == "mul_mat_q" or name.startswith("goose_mmq_"):
        if "pack" not in name and "fixup" not in name:
            return "MMQ"
    for family, names in KERNEL_FAMILIES.items():
        if any(name == prefix or (
            prefix != "mul_mat_q" and name.startswith(prefix)
        ) for prefix in names):
            return family
    return "Other kernels"


@dataclass
class Activity:
    count: int = 0
    ns: int = 0
    bytes: int = 0
    intervals: list = field(default_factory=list)

    def add(self, start, end, size=0):
        self.count += 1
        self.bytes += size
        if end <= start:
            return
        self.ns += end - start
        if self.intervals and start <= self.intervals[-1][1]:
            left, right = self.intervals[-1]
            self.intervals[-1] = (left, max(right, end))
        else:
            self.intervals.append((start, end))

    def report(self, **extra):
        return {
            "count": self.count,
            "sum_ms": self.ns / 1e6,
            "mean_us": self.ns / (1000 * self.count) if self.count else 0,
            "union_ms": sum(end - start for start, end in self.intervals) / 1e6,
            "bytes": self.bytes,
            **extra,
        }


def exclusive(activities, left, right):
    """Return exclusive time by row, device busy time and idle intervals."""
    events = sorted(
        event
        for name, activity in activities.items()
        for start, end in activity.intervals
        for event in ((start, 1, name), (end, 0, name))
    )
    active, exposed, idle = set(), Counter(), []
    last = left
    for now, entering, name in events:
        if now > last:
            if not active:
                idle.append((last, now))
            elif len(active) == 1:
                exposed[next(iter(active))] += now - last
        if entering:
            active.add(name)
        else:
            active.remove(name)
        last = now
    if last < right:
        idle.append((last, right))
    busy = right - left - sum(b - a for a, b in idle)
    return exposed, busy, idle


def intersection(a, b):
    i = j = total = 0
    while i < len(a) and j < len(b):
        start = max(a[i][0], b[j][0])
        end = min(a[i][1], b[j][1])
        total += max(0, end - start)
        if a[i][1] < b[j][1]:
            i += 1
        else:
            j += 1
    return total


def api_family(name):
    if "Launch" in name:
        return "Graph launch" if "Graph" in name else "Direct launch"
    if "Memcpy" in name:
        return "Asynchronous memcpy API" if "Async" in name else "Synchronous memcpy API"
    if "Synchronize" in name or "WaitEvent" in name:
        return "CUDA synchronization and waits"
    if "Graph" in name or "Capture" in name:
        return "Graph setup and update"
    if "MemAlloc" in name or "MemFree" in name or "MemMap" in name:
        return "CUDA allocation and mapping"
    return "Other CUDA API"


def mmq_format(name, demangled):
    if "_q4_" in name or "(ggml_type)12," in demangled:
        return "Q4_K"
    if "_q6_" in name or "(ggml_type)14," in demangled:
        return "Q6_K"
    return "Unresolved format"


def decode_idle(db, pid, left, right, idle):
    """Partition idle wall time and relate memcpy calls to recorded device work."""
    cp = (pid, right, left)
    api_calls = list(db.execute("""
        select a.start,a.end,s.value,a.correlationId,a.globalTid,a.callchainId
        from CUPTI_ACTIVITY_KIND_RUNTIME a join StringIds s on a.nameId=s.id
        where (a.globalTid >> 24) & 16777215 = ? and a.start < ? and a.end > ?
        order by a.start,a.end
    """, cp))
    api_by_id = defaultdict(list)
    for call in api_calls:
        api_by_id[call[3]].append(call)

    kernels = list(db.execute("""
        select k.start,k.end,k.correlationId,s.value
        from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on k.shortName=s.id
        where (k.globalPid >> 24) & 16777215 = ? and k.start < ? and k.end > ?
        order by k.start,k.end
    """, cp))
    kernel_starts = [row[0] for row in kernels]
    kernel_ends = sorted((end, name) for _, end, _, name in kernels)
    end_times = [row[0] for row in kernel_ends]
    starts_at_gap_end = {end: set() for _, end in idle}
    ends_at_gap_start = {start: set() for start, _ in idle}
    device_range = {}

    def observe_device(start, end, correlation):
        if start in starts_at_gap_end:
            starts_at_gap_end[start].add(correlation)
        if end in ends_at_gap_start:
            ends_at_gap_start[end].add(correlation)
        old_start, old_end = device_range.get(correlation, (start, end))
        device_range[correlation] = min(start, old_start), max(end, old_end)

    for start, end, correlation, _ in kernels:
        observe_device(start, end, correlation)

    copies = defaultdict(list)
    for start, end, correlation, kind, size, source, dest, stream in db.execute("""
        select m.start,m.end,m.correlationId,m.copyKind,m.bytes,m.srcKind,m.dstKind,m.streamId
        from CUPTI_ACTIVITY_KIND_MEMCPY m
        where (m.globalPid >> 24) & 16777215 = ? and m.start < ? and m.end > ?
    """, cp):
        observe_device(start, end, correlation)
        if correlation in api_by_id:
            copies[correlation].append((start, end, kind, size, source, dest, stream))

    for start, end in db.execute("""
        select m.start,m.end from CUPTI_ACTIVITY_KIND_MEMSET m
        where (m.globalPid >> 24) & 16777215 = ? and m.start < ? and m.end > ?
    """, cp):
        observe_device(start, end, None)

    types = dict(db.execute("select id,label from ENUM_CUDA_MEMCPY_OPER"))
    kinds = dict(db.execute("select id,label from ENUM_CUDA_MEM_KIND"))
    copy_stats = defaultdict(Counter)
    copy_context = defaultdict(Counter)
    call_stats = Counter()
    call_rows = {}
    annotated = []
    for start, end, name, correlation, _, _ in api_calls:
        family = api_family(name)
        row = family
        copy = None
        if family == "Synchronous memcpy API":
            matches = copies.get(correlation, ())
            if len(matches) == 1 and len(api_by_id[correlation]) == 1:
                copy = matches[0]
                begins, done, kind, size, source, dest, stream = copy
                row = f"{types[kind]} {size} B {kinds[source]} to {kinds[dest]} (stream {stream})"
                previous = bisect_right(end_times, begins) - 1
                following = bisect_left(kernel_starts, done)
                context = (
                    kernel_ends[previous][1] if previous >= 0 else "Outside window",
                    kernels[following][3] if following < len(kernels) else "Outside window",
                )
                copy_context[row][context] += 1
                copy_stats[row]["device_ns"] += min(done, right) - max(begins, left)
                copy_stats[row]["returned_before_copy"] += end <= begins
            else:
                row = "Synchronous memcpy without unique correlation"
            copy_stats[row]["api_ns"] += min(end, right) - max(start, left)
            copy_stats[row]["calls"] += 1
        call_stats[row] += 1
        call_rows[correlation] = row
        annotated.append((start, end, row, copy, correlation))

    idle_totals, gap_counts, next_submission = Counter(), Counter(), Counter()
    graph_launch, offline = Counter(), Counter()
    thread_count = len({row[4] for row in api_calls})
    completed_calls = sorted((end, row) for _, end, row, _, _ in annotated) if thread_count == 1 else []
    completed_times = [end for end, _ in completed_calls]
    # Sweep only device-idle intervals. Concurrent API rows get one shared bin.
    pos = 0
    for gap_start, gap_end in idle:
        while pos < len(annotated) and annotated[pos][1] <= gap_start:
            pos += 1
        spans = []
        points = {gap_start, gap_end}
        index = pos
        while index < len(annotated) and annotated[index][0] < gap_end:
            start, end, row, copy, correlation = annotated[index]
            lo, hi = max(start, gap_start), min(end, gap_end)
            if lo < hi:
                spans.append((lo, hi, row, copy, correlation))
                points.update((lo, hi))
            index += 1
        boundary = sorted(points)
        gap_rows = set()
        linked = starts_at_gap_end[gap_end]
        upcoming = api_by_id.get(next(iter(linked)), ()) if len(linked) == 1 else ()
        for lo, hi in zip(boundary, boundary[1:]):
            hits = [(row, copy, correlation) for start, end, row, copy, correlation in spans
                    if start <= lo < end]
            n = hi - lo
            if len(hits) == 1:
                row, copy, correlation = hits[0]
                if copy:
                    if hi <= copy[0]:
                        copy_stats[row]["idle_before_copy_ns"] += n
                    elif lo >= copy[1]:
                        copy_stats[row]["idle_after_copy_ns"] += n
                    else:
                        raise ValueError("linked GPU copy inside a device-idle slice")
                if row == "Graph launch":
                    if correlation not in device_range:
                        graph_launch["No correlated device activity"] += n
                    else:
                        first, last = device_range[correlation]
                        before = max(0, min(hi, first) - lo)
                        after = max(0, hi - max(lo, last))
                        graph_launch["Before first correlated activity"] += before
                        graph_launch["After last correlated activity"] += after
                        graph_launch["Between correlated activities"] += n - before - after
            elif hits:
                row = "Overlapping CUDA APIs"
            elif len(upcoming) == 1:
                call = upcoming[0]
                if call[1] <= lo:
                    row = "No CUDA API, next activity already submitted"
                elif call[0] >= hi:
                    row = "No CUDA API, before next submission"
                else:
                    raise ValueError("linked CUDA API inside an uncovered idle slice")
                next_submission[row, api_family(call[2])] += n
                prior_index = bisect_right(completed_times, lo) - 1
                prior_row = completed_calls[prior_index][1] if prior_index >= 0 else "Unknown"
                same_submission = ends_at_gap_start[gap_start] == linked
                offline[row, call_rows[call[3]], prior_row, same_submission] += n
            else:
                row = "No CUDA API, next activity unlinked"
            idle_totals[row] += n
            gap_rows.add(row)
        gap_counts.update(gap_rows)
    if sum(idle_totals.values()) != sum(end - start for start, end in idle):
        raise ValueError("decode idle partition does not reconcile")

    return {
        "api_threads": thread_count,
        "api_calls_with_callchain": sum(row[5] is not None for row in api_calls),
        "rows": {
            row: {"idle_ns": idle_totals[row], "api_calls": call_stats.get(row),
                  "gaps_touched": gap_counts[row], **copy_stats.get(row, {})}
            for row in sorted(idle_totals.keys() | call_stats.keys())
        },
        "copy_kernel_neighbors": {
            row: [{"previous_kernel": prev, "next_kernel": after, "count": count}
                  for (prev, after), count in stats.most_common()]
            for row, stats in copy_context.items()
        },
        "next_submission": [
            {"idle_row": row, "next_api_family": family, "idle_ns": ns}
            for (row, family), ns in sorted(next_submission.items())
        ],
        "graph_launch_idle_ns": dict(graph_launch),
        "outside_cuda_api": [
            {"idle_row": row, "next_call": following, "last_completed_call": previous,
             "same_device_submission": same_submission, "idle_ns": ns}
            for (row, following, previous, same_submission), ns in offline.most_common()
        ],
    }


def read_phase(db, pid, left, right, include_decode_idle=False):
    gpu, names, cpu, cpu_names, os_waits = (defaultdict(Activity) for _ in range(5))
    mmq, shapes = defaultdict(Activity), defaultdict(Activity)
    graph, api_total = Activity(), Activity()
    kernel_sql = """
        select s.value,k.start,k.end,coalesce(k.graphNodeId,0),k.correlationId,k.demangledName,
               k.gridX,k.gridY,k.gridZ,k.blockX,k.blockY,k.blockZ
        from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on k.shortName=s.id
        where (k.globalPid >> 24) & 16777215 = ? and k.start < ? and k.end > ?
        order by k.start
    """
    symbols = dict(db.execute("select id,value from StringIds"))
    signatures = Counter()
    replay_nodes, replay_ns = Counter(), Counter()
    for name, start, end, node, correlation, symbol_id, *geometry in db.execute(kernel_sql, (pid, right, left)):
        start, end = max(left, start), min(right, end)
        names["Kernels", name].add(start, end)
        demangled = symbols.get(symbol_id, "")
        family = kernel_family(name, demangled)
        gpu[family].add(start, end)
        if family == "MMQ":
            quant = mmq_format(name, demangled)
            mmq[quant].add(start, end)
            shapes[quant, name, demangled, *geometry].add(start, end)
        if node:
            graph.add(start, end)
            signatures[name] += 1
            replay_nodes[correlation] += 1
            replay_ns[correlation] += end - start

    copies = {
        1: "H2D bulk",
        2: "D2H",
        8: "D2D",
    }
    for table in ("MEMCPY", "MEMSET"):
        detail = ",copyKind,bytes" if table == "MEMCPY" else ",-1,bytes"
        for start, end, kind, size in db.execute(
            f"""select start,end{detail} from CUPTI_ACTIVITY_KIND_{table}
                where (globalPid >> 24) & 16777215 = ? and start < ? and end > ? order by start""",
            (pid, right, left),
        ):
            name = copies.get(kind, "Memset" if kind == -1 else f"Copy kind {kind}")
            if kind == 1 and size <= 4096:
                name = "H2D packets up to 4 KiB"
            start, end = max(left, start), min(right, end)
            gpu[name].add(start, end, size)
            names["Transfers", name].add(start, end, size)

    exposed, busy, idle = exclusive(gpu, left, right)
    typed_gpu = {name: row for name, row in gpu.items() if name != "MMQ"}
    typed_gpu.update({f"MMQ {quant}": row for quant, row in mmq.items()})
    typed_exposure, typed_busy, _ = exclusive(typed_gpu, left, right)
    kernel_exposure, same_busy, _ = exclusive(names, left, right)
    if same_busy != busy or typed_busy != busy:
        raise ValueError("detail, typed MMQ, and family activity unions differ")
    for table in ("CUPTI_ACTIVITY_KIND_RUNTIME", "OSRT_API"):
        for name, start, end in db.execute(
            f"""select s.value,e.start,e.end from {table} e join StringIds s on e.nameId=s.id
                where (e.globalTid >> 24) & 16777215 = ? and e.start < ? and e.end > ?
                order by e.start""",
            (pid, right, left),
        ):
            start, end = max(left, start), min(right, end)
            if table == "OSRT_API":
                if any(part in name for part in ("wait", "poll", "sleep", "join", "recv")):
                    os_waits[name].add(start, end)
            else:
                api_total.add(start, end)
                cpu[api_family(name)].add(start, end)
                cpu_names[name].add(start, end)

    def host_report(rows):
        return {
            name: activity.report(gpu_idle_overlap_ms=intersection(activity.intervals, idle) / 1e6)
            for name, activity in rows.items()
        }

    size_counts, size_ns = Counter(replay_nodes.values()), Counter()
    for correlation, count in replay_nodes.items():
        size_ns[count] += replay_ns[correlation]
    result = {
        "start_ns": left,
        "end_ns": right,
        "window_ms": (right - left) / 1e6,
        "gpu_busy_ms": busy / 1e6,
        "gpu_idle_ms": (right - left - busy) / 1e6,
        "families": {name: row.report(exclusive_ms=exposed[name] / 1e6) for name, row in gpu.items()},
        "mmq_formats": {name: row.report(exclusive_ms=typed_exposure[f"MMQ {name}"] / 1e6) for name, row in mmq.items()},
        "mmq_launch_geometry": [
            {"format": quant, "name": name, "demangled": symbol, "grid": geometry[:3],
             "block": geometry[3:], **row.report()}
            for (quant, name, symbol, *geometry), row in shapes.items()
        ],
        "kernels_and_transfers": {
            name: row.report(exclusive_ms=kernel_exposure[kind, name] / 1e6)
            for (kind, name), row in names.items()
        },
        "graph_kernel_subset": graph.report(),
        "graph_kernel_names": dict(signatures),
        "graph_sizes": {
            count: {"replays": size_counts[count], "kernel_ms": size_ns[count] / 1e6}
            for count in size_counts
        },
        "cuda_api": host_report(cpu),
        "cuda_api_calls": host_report(cpu_names),
        "cuda_api_union": api_total.report(gpu_idle_overlap_ms=intersection(api_total.intervals, idle) / 1e6),
        "os_waits": host_report(os_waits),
    }
    if include_decode_idle:
        result["idle_attribution"] = decode_idle(db, pid, left, right, idle)
    return result


def traces(path, run, marker, rms, warmup_markers, include_decode_idle=False):
    pid = int((run / "pid.txt").read_text())
    response = json.loads((run / "r1.json").read_text())
    timings = response["timings"]
    if (timings["prompt_n"], timings["predicted_n"], timings.get("cache_n", 0)) != (32768, 512, 0):
        raise ValueError(f"unexpected workload in {run}")
    with closing(sqlite3.connect(f"{path.resolve().as_uri()}?mode=ro", uri=True)) as db:
        db.execute("pragma temp_store=memory")
        names = """
            from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on k.shortName=s.id
            where (k.globalPid >> 24) & 16777215 = ?
        """
        left = db.execute(f"select min(k.start) {names} and s.value=? and k.gridX=4096", (pid, rms)).fetchone()[0]
        ticks = [row[0] for row in db.execute(f"select k.start {names} and s.value=? order by k.start", (pid, marker))]
        if len(ticks) != 30 * 511 + warmup_markers:
            raise ValueError(f"incomplete decode trace in {path}: {len(ticks)} GDN markers")
        split = ticks[warmup_markers]
        end = db.execute("""
            select max(end) from (
              select max(end) as end from CUPTI_ACTIVITY_KIND_KERNEL where (globalPid >> 24) & 16777215 = ?
              union all
              select max(end) from CUPTI_ACTIVITY_KIND_MEMCPY where (globalPid >> 24) & 16777215 = ? and copyKind=2
            )""", (pid, pid)).fetchone()[0]
        if left is None or not left < split < end:
            raise ValueError(f"invalid phase landmarks in {path}")
        return {
            "timings": timings,
            "prefill": read_phase(db, pid, left, split),
            "decode": read_phase(db, pid, split, end, include_decode_idle),
        }


def cpu_family(name):
    if "libcuda" in name or "libnvidia" in name:
        return "CUDA driver"
    if "sched" in name or "graph" in name:
        return "Graph construction and dispatch"
    if any(part in name for part in ("expert", "slru", "ec_prepare")):
        return "Expert cache and SLRU"
    if "gather" in name or "get_rows" in name:
        return "CPU gather"
    if "sampl" in name or "FG171" in name:
        return "Sampling and token selection"
    if "libc" in name or "vdso" in name:
        return "System libraries"
    return "Other CPU functions"


def clock_profile(run):
    er = run / "cpu.er"
    response = json.loads((run / "r1.json").read_text())
    signals = re.findall(
        r'<event kind="sample" tstamp="([^"]+)" id="[^"]+" label="signal"/>',
        (er / "log.xml").read_text(errors="replace"),
    )
    if len(signals) != 2:
        raise ValueError(f"expected two request signals in {er}")
    start, end = (int(Decimal(s) * 10**9) for s in signals)
    split = start + round(response["timings"]["prompt_ms"] * 1e6)
    result = {"timings": response["timings"]}
    for name, lo, hi in (("prefill", start, split), ("decode", split, end)):
        report = subprocess.run(
            [
                "gprofng", "display", "text", "-filters", f"TSTAMP>={lo} && TSTAMP<{hi}",
                "-metrics", "e.totalcpu:i.totalcpu:name", "-limit", "0", "-functions", str(er),
            ],
            check=True, text=True, stdout=subprocess.PIPE,
        ).stdout
        rows = re.findall(r"^\s*(\d+\.\d+)\s+(\d+\.\d+)\s+(.+)$", report, re.MULTILINE)
        if not rows or rows[0][2] != "<Total>":
            raise ValueError(f"missing CPU time table in {er}")
        kinds = Counter()
        functions = []
        for exclusive_seconds, inclusive_seconds, symbol in rows[1:]:
            exclusive_ms = int(Decimal(exclusive_seconds) * 1000)
            inclusive_ms = int(Decimal(inclusive_seconds) * 1000)
            kinds[cpu_family(symbol)] += exclusive_ms
            functions.append({"symbol": symbol, "exclusive_ms": exclusive_ms, "inclusive_ms": inclusive_ms})
        total_ms = int(Decimal(rows[0][0]) * 1000)
        if sum(kinds.values()) != total_ms:
            raise ValueError(f"CPU symbol time does not reconcile in {er}")
        result[name] = {
            "start_ns": lo, "end_ns": hi, "total_ms": total_ms,
            "families_ms": dict(kinds), "functions": functions,
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decode-idle", action="store_true", help="include disjoint decode idle attribution")
    for runtime in ("hybrid", "goose"):
        parser.add_argument(f"--{runtime}-trace", required=True, type=Path)
        parser.add_argument(f"--{runtime}-gpu-run", required=True, type=Path)
        parser.add_argument(f"--{runtime}-cpu-run", required=True, type=Path)
    args = parser.parse_args()
    requests = [
        (getattr(args, f"{runtime}_{profile}_run") / "request.json").read_bytes()
        for runtime in ("hybrid", "goose")
        for profile in ("cpu", "gpu")
    ]
    if any(request != requests[0] for request in requests[1:]):
        raise ValueError("request bytes differ between the traces and CPU profiles")
    report = {}
    for runtime, marker, rms, warmup in (
        ("hybrid", "gated_delta_net_cuda", "rms_norm_f32", 90),
        ("goose", "goose_gdn_decode", "goose_rms_reference", 0),
    ):
        report[runtime] = {
            "gpu": traces(getattr(args, f"{runtime}_trace"), getattr(args, f"{runtime}_gpu_run"),
                          marker, rms, warmup, args.decode_idle),
            "cpu": clock_profile(getattr(args, f"{runtime}_cpu_run")),
        }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
