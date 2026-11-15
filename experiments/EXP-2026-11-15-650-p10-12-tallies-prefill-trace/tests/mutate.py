#!/usr/bin/env python3
"""P10-12 mutation run: 12 mutations per slice, two halves of six.

    python3 tests/mutate.py 1            # tl 6 + pf 6 + tr 6 + mp 6
    python3 tests/mutate.py 2 --jobs 4   # the second half, four workers

Each mutation rewrites one decision in one Goose module, regenerates that
module at -O0 (the gate itself is compiled at -O2), relinks its fixture and
reruns the gate. A mutation that still passes is a hole in the gate, and it is
reported as such rather than relabelled.
"""

import concurrent.futures
import os
import subprocess
import sys
from pathlib import Path

B = Path(__file__).resolve().parent.parent
OUT = B / "build"
WORK = OUT / "mut"
GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
REPO = Path(os.environ.get("FG_REPO", "/home/end0/AI/FrankenMoE-CUDA"))
CC = os.environ.get("CC", "gcc-13")
CXX = os.environ.get("CXX", "g++-13")

SRC = {
    "tl": B / "source" / "tallies.goose",
    "pf": B / "source" / "prefill-plan.goose",
    "tr": B / "source" / "trace-frame-order.goose",
    "mp": B / "source" / "moe-predict-report.goose",
}
FIX = {
    "tl": B / "tests" / "tl_fixture.cpp",
    "pf": B / "tests" / "pf_fixture.cpp",
    "tr": B / "tests" / "tr_fixture.cpp",
    "mp": B / "tests" / "mp_fixture.cpp",
}

# ── half 1: the per-token decomposition, the plan start, the frame head ─────
M1 = [
    # tallies
    ("tl", "the millisecond conversion is centiseconds",
     "let wall_ms: f64 = wall * 1000.0;", "let wall_ms: f64 = wall * 100.0;"),
    ("tl", "the fault bytes are dropped from the MiB column",
     "let majflt_mib: f64 = (majflt as f64) * fault_bytes / TL_MIB;",
     "let majflt_mib: f64 = (majflt as f64) / TL_MIB;"),
    ("tl", "the compute residual is no longer clamped",
     "    if compute_ms < 0.0 {\n        compute_ms = 0.0;\n    }",
     "    if compute_ms < -1000.0 {\n        compute_ms = 0.0;\n    }"),
    ("tl", "the no-cache sentinel becomes a zero hit rate",
     "    var cache_hit_pct: f64 = 0.0 - 1.0;", "    var cache_hit_pct: f64 = 0.0;"),
    ("tl", "the cache fraction is reported unscaled",
     "        cache_hit_pct = 100.0 * st_cache_hits / st_cache_lookups;",
     "        cache_hit_pct = st_cache_hits / st_cache_lookups;"),
    ("tl", "the overlap residual subtracts io instead of stall",
     "        compute_ms = wall_ms - stall_ms - mgmt_ms;", "        compute_ms = wall_ms - io_ms - mgmt_ms;"),

    # prefill plan
    ("pf", "the plan starts one token past the reused prefix",
     "    var i: i64 = n_common;", "    var i: i64 = n_common + 1;"),
    ("pf", "the loop advances two batches at a time",
     "        i = i + n_batch;", "        i = i + n_batch * 2;"),
    ("pf", "every chunk is a full batch, tail included",
     "        if left < n_batch {", "        if left < 0 {"),
    ("pf", "the tail chunk is one token too long",
     "            chunk = left;", "            chunk = left + 1;"),
    ("pf", "the loop plans the token at n_prompt",
     "    while i < n_prompt {", "    while i <= n_prompt {"),
    ("pf", "each pass counts twice",
     "        n = n + 1;", "        n = n + 2;"),

    # trace frame
    ("tr", "the phase reaches the hook after the trace calls",
     "    at = tr_put(out, at, TR_SET_PHASE, phase, 0, 0, 0);\n"
     "    at = tr_put(out, at, TR_BEGIN_DET, base_pos, n_tokens, phase, 0);",
     "    at = tr_put(out, at, TR_BEGIN_DET, base_pos, n_tokens, phase, 0);\n"
     "    at = tr_put(out, at, TR_SET_PHASE, phase, 0, 0, 0);"),
    ("tr", "the stamp step is the first position, not the last",
     "        out[1] = base_pos + n_tokens - 1;", "        out[1] = base_pos;"),
    ("tr", "the compute interval is not closed when a route trace is on",
     "    if compute != 0 {\n        at = tr_put(out, at, TR_END_COMPUTE, 0, 0, 0, 0);\n    }",
     "    if compute != 0 && route == 0 {\n        at = tr_put(out, at, TR_END_COMPUTE, 0, 0, 0, 0);\n    }"),
    ("tr", "an empty route frame still hands rows over",
     "        if route_rows > 0 {", "        if route_rows >= 0 {"),
    ("tr", "compute rows leave even when the compute trace is off",
     "    if compute != 0 && compute_rows > 0 {", "    if compute_rows > 0 {"),
    ("tr", "the empty io take is not part of the plan",
     "            at = tr_put(out, at, TR_IO_TAKE, 0, 0, 0, 0);\n        }",
     "        }"),

    # moe-predict report
    ("mp", "the table column is five wide",
     "    return mp_pad_f1(out, pos, 100.0 * f, 6);", "    return mp_pad_f1(out, pos, 100.0 * f, 5);"),
    ("mp", "the dash cell is one column short",
     '        return mp_put_lit(out, pos, "     -");', '        return mp_put_lit(out, pos, "    -");'),
    ("mp", "the layer column is four wide",
     "            p = mp_pad_i64(out, p, il, 5);", "            p = mp_pad_i64(out, p, il, 4);"),
    ("mp", "the routings column is five wide",
     "            p = mp_pad_i64(out, p, a_rows, 6);", "            p = mp_pad_i64(out, p, a_rows, 5);"),
    ("mp", "the first aggregate line loses a word",
     '    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 0), 1);\n'
     '    p = mp_put_lit(out, p, "% of routed slots (");',
     '    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 0), 1);\n'
     '    p = mp_put_lit(out, p, "% of routed slot (");'),
    ("mp", "the two-layer horizon always prints",
     "    if stale2[MP_HIT] > 0 {", "    if stale2[MP_HIT] >= 0 {"),
]

# ── half 2: totals, cursors, prefill bracket, summary, frame tail, rows ─────
M2 = [
    # tallies
    ("tl", "the drain delta is taken against the stall cursor",
     "    let drain_ms: f64 = (st_drain_s - prev_drain_s) * 1000.0;",
     "    let drain_ms: f64 = (st_drain_s - prev_stall_s) * 1000.0;"),
    ("tl", "the byte delta adds the cursor instead of subtracting",
     "    let read_delta: i64 = (st_read_bytes as i64) - prev_bytes;",
     "    let read_delta: i64 = (st_read_bytes as i64) + prev_bytes;"),
    ("tl", "the io total keeps milliseconds",
     "    out_f[12] = acc_io_s + io_ms / 1000.0;", "    out_f[12] = acc_io_s + io_ms;"),
    ("tl", "the prefill read total is in KiB",
     "    out[1] = diff / TL_MIB;", "    out[1] = diff / 1024.0;"),
    ("tl", "the prefill bracket reads counters with streaming off",
     "    if moe_on != 0 {", "    if moe_on >= 0 {"),
    ("tl", "the summary's fault average uses the cpu total",
     "        out[0] = (majflt as f64) / n_gen;", "        out[0] = cpu_seconds / n_gen;"),

    # prefill plan
    ("pf", "the plan refuses a single-token batch",
     "    if n_batch < 1 {", "    if n_batch < 2 {"),
    ("pf", "the plan swaps start and length",
     "            out[2 * n] = i;\n            out[2 * n + 1] = chunk;",
     "            out[2 * n] = chunk;\n            out[2 * n + 1] = i;"),
    ("pf", "a full chunk is sized from the remaining prompt",
     "        var chunk: i64 = n_batch;", "        var chunk: i64 = n_prompt - i;"),
    ("pf", "the plan reports one chunk less than it wrote",
     "    return n;\n}", "    return n - 1;\n}"),
    ("pf", "the tail is measured from the prompt start, not the cursor",
     "        let left: i64 = n_prompt - i;", "        let left: i64 = n_prompt - n_common;"),
    ("pf", "the written length is the remaining prompt",
     "            out[2 * n + 1] = chunk;", "            out[2 * n + 1] = left;"),

    # trace frame
    ("tr", "the io rows are stamped with phase zero",
     "            at = tr_put(out, at, TR_IO_STAMP, io_rows, turn, phase, step);",
     "            at = tr_put(out, at, TR_IO_STAMP, io_rows, turn, 0, step);"),
    ("tr", "the io rows lose the turn they belong to",
     "            at = tr_put(out, at, TR_IO_STAMP, io_rows, turn, phase, step);",
     "            at = tr_put(out, at, TR_IO_STAMP, io_rows, 0, phase, step);"),
    ("tr", "the compute frame is attributed to the first position",
     "        at = tr_put(out, at, TR_BEGIN_COMPUTE, base_pos + n_tokens - 1, phase, turn, 0);",
     "        at = tr_put(out, at, TR_BEGIN_COMPUTE, base_pos, phase, turn, 0);"),
    ("tr", "the route frame is opened from the compute flag",
     "    if route != 0 {\n        at = tr_put(out, at, TR_BEGIN_ROUTE, base_pos, n_tokens, phase, turn);",
     "    if compute != 0 {\n        at = tr_put(out, at, TR_BEGIN_ROUTE, base_pos, n_tokens, phase, turn);"),
    ("tr", "the deterministic batch is one token short",
     "    at = tr_put(out, at, TR_BEGIN_DET, base_pos, n_tokens, phase, 0);",
     "    at = tr_put(out, at, TR_BEGIN_DET, base_pos, n_tokens - 1, phase, 0);"),
    ("tr", "the route frame loses its turn",
     "        at = tr_put(out, at, TR_BEGIN_ROUTE, base_pos, n_tokens, phase, turn);",
     "        at = tr_put(out, at, TR_BEGIN_ROUTE, base_pos, n_tokens, phase, 0);"),

    # moe-predict report
    ("mp", "the warning threshold drops to 0.998",
     "            if mp_hit(head, 8) < 0.999 {", "            if mp_hit(head, 8) < 0.998 {"),
    ("mp", "the warning prints without a scored control",
     "    if head[8] > 0 {", "    if head[8] >= 0 {"),
    ("mp", "a dense layer prints a row",
     "        if !(a_rows == 0 && b_rows == 0 && c_rows == 0) {", "        if a_rows >= 0 {"),
    ("mp", "the stale exact fraction is the slot fraction",
     "    p = mp_pad_f1(out, p, 100.0 * mp_exact(head, 0), 1);",
     "    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 0), 1);"),
    ("mp", "the table header gains a space",
     '    p = mp_put_lit(out, p, "  layer   stale    prev   ctrl   routings\\n");',
     '    p = mp_put_lit(out, p, "  layer   stale    prev    ctrl   routings\\n");'),
    ("mp", "the unscored count is not reported",
     "    p = mp_put_i64(out, p, unscored);", "    p = mp_put_i64(out, p, 0);"),
]


def link_and_run(name, mut_dir):
    obj = mut_dir / (name + ".o")
    gate = mut_dir / ("gate-" + name)
    inc = [CXX, "-O2", "-std=c++17", "-I", str(mut_dir), "-I", str(OUT), "-I", str(B / "tests"),
           "-I", str(REPO / "runtime/core/include")]
    cmd = inc + [str(FIX[name]), str(obj), "-lm", "-o", str(gate)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return "link-fail", r.stderr
    try:
        r = subprocess.run([str(gate)], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return "hang", "the gate did not finish in 60 s"
    if r.returncode == 0:
        return "survived", r.stdout
    return "caught", r.stdout + r.stderr


def one(name, desc, old, new, idx, half):
    mut_dir = WORK / ("%s-h%s-%02d" % (name, half, idx))
    mut_dir.mkdir(parents=True, exist_ok=True)
    text = SRC[name].read_text()
    n = text.count(old)
    if n != 1:
        return name, desc, "BROKEN", "anchor occurs %d times" % n
    src = mut_dir / (name + ".goose")
    src.write_text(text.replace(old, new, 1))
    gen_c = mut_dir / (name + ".c")
    gen_h = mut_dir / (name + ".h")
    obj = mut_dir / (name + ".o")
    r = subprocess.run([GOOSE, "-O0", "--standalone", "-o", str(gen_c), "--header", str(gen_h), str(src)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return name, desc, "gen-fail", r.stderr[-300:]
    r = subprocess.run([CC, "-O0", "-std=c11", "-w", "-c", str(gen_c), "-o", str(obj)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return name, desc, "cc-fail", r.stderr[-300:]
    verdict, detail = link_and_run(name, mut_dir)
    print("%-9s %-3s %-56s %s" % (verdict, name, desc, "" if verdict == "caught" else detail[-200:]), flush=True)
    return name, desc, verdict, "" if verdict == "caught" else detail[-400:]


def main():
    half = sys.argv[1] if len(sys.argv) > 1 else "1"
    jobs = 4
    if "--jobs" in sys.argv:
        jobs = int(sys.argv[sys.argv.index("--jobs") + 1])
    muts = M1 if half == "1" else M2
    WORK.mkdir(parents=True, exist_ok=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(one, name, desc, old, new, idx, half) for idx, (name, desc, old, new) in enumerate(muts)]
        for f in concurrent.futures.as_completed(futs):
            results.append(f.result())
    caught = 0
    for name, desc, verdict, detail in results:
        if verdict == "caught":
            caught += 1
    per_slice = {s: 0 for s in ("tl", "pf", "tr", "mp")}
    for name, desc, verdict, detail in results:
        if verdict == "caught":
            per_slice[name] += 1
    print("FG649_MUT half=%s caught=%d/%d  tl=%d/6 pf=%d/6 tr=%d/6 mp=%d/6"
          % (half, caught, len(results), per_slice["tl"], per_slice["pf"], per_slice["tr"], per_slice["mp"]))
    return 0 if caught == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
