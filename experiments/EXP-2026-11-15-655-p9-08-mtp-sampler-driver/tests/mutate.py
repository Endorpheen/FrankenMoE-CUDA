#!/usr/bin/env python3
"""P9-08 G4/G5/G7 mutation run: 12 mutations per slice, two halves of six.

    python3 tests/mutate.py 1            # sch 6 + mw 6 + drv 6
    python3 tests/mutate.py 2 --jobs 4   # the second half, four workers

Each mutation rewrites one decision in one Goose module, regenerates that module
at -O0 (the gate itself is compiled at -O2), relinks its fixture and reruns the
gate. A mutation that still passes is a hole in the gate, and it is reported as
such rather than relabelled.
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
    "sch": B / "source" / "sampler-chain.goose",
    "mw": B / "source" / "mtp-capture-warmup.goose",
    "drv": B / "source" / "cli-driver-loop.goose",
}
FIX = {
    "sch": B / "tests" / "sch_fixture.cpp",
    "mw": B / "tests" / "mw_fixture.cpp",
    "drv": B / "tests" / "drv_fixture.cpp",
}

# ── half 1: the gate condition, the stage order, the warm-up shape ─────────
M1 = [
    # sampler chain
    ("sch", "the opt-in is inclusive of temp zero",
     "export fn sch_on(temp: f64) -> i64 {\n    if temp > 0.0 {",
     "export fn sch_on(temp: f64) -> i64 {\n    if temp >= 0.0 {"),
    ("sch", "the stage count ignores temp",
     "export fn sch_entries(temp: f64) -> i64 {\n    if temp > 0.0 {\n        return 4;",
     "export fn sch_entries(temp: f64) -> i64 {\n    if temp >= 0.0 {\n        return 4;"),
    ("sch", "top_p is added before top_k",
     "    n = sch_put(out, outf, n, SCH_TOP_K, top_k, 0, 0.0);\n"
     "    n = sch_put(out, outf, n, SCH_TOP_P, 0, 1, top_p);",
     "    n = sch_put(out, outf, n, SCH_TOP_P, 0, 1, top_p);\n"
     "    n = sch_put(out, outf, n, SCH_TOP_K, top_k, 0, 0.0);"),
    ("sch", "top_p loses its min_keep",
     "n = sch_put(out, outf, n, SCH_TOP_P, 0, 1, top_p);",
     "n = sch_put(out, outf, n, SCH_TOP_P, 0, 0, top_p);"),
    ("sch", "dist is handed a zero seed",
     "n = sch_put(out, outf, n, SCH_DIST, seed, 0, 0.0);",
     "n = sch_put(out, outf, n, SCH_DIST, 0, 0, 0.0);"),
    ("sch", "the chain stops before dist",
     "    n = sch_put(out, outf, n, SCH_DIST, seed, 0, 0.0);\n    return n;",
     "    return n;"),

    # mtp capture warm-up
    ("mw", "a negative bos is passed to the decode",
     "export fn mw_warm_token(bos: i64) -> i64 {\n    if bos < 0 {",
     "export fn mw_warm_token(bos: i64) -> i64 {\n    if bos < -1 {"),
    ("mw", "the verify batch is one position wide at pos 0 with logits",
     "at = mw_put3(out, at, MW_FILL_VERIFY, 0, 1);",
     "at = mw_put3(out, at, MW_FILL_VERIFY, 0, 0);"),
    ("mw", "the draft context is cleared only when absent",
     "    if has_draft != 0 {\n        at = mw_put3(out, at, MW_CLEAR_DRAFT, 0, 0);",
     "    if has_draft == 0 {\n        at = mw_put3(out, at, MW_CLEAR_DRAFT, 0, 0);"),
    ("mw", "the warm-up decode failure is only a positive return",
     "export fn mw_warm_fail(decode_rc: i64, process_ok: i64, mtp: i64) -> i64 {\n    if decode_rc != 0 {",
     "export fn mw_warm_fail(decode_rc: i64, process_ok: i64, mtp: i64) -> i64 {\n    if decode_rc > 0 {"),
    ("mw", "the dense path demands a draft process pass",
     "    if mtp != 0 && process_ok == 0 {",
     "    if process_ok == 0 {"),
    ("mw", "an unbound layer is walked instead of skipped",
     "export fn mw_slot_step(bound: i64, used: i64, has_tensor: i64, has_off: i64) -> i64 {\n    if bound == 0 {",
     "export fn mw_slot_step(bound: i64, used: i64, has_tensor: i64, has_off: i64) -> i64 {\n    if bound != 0 {"),

    # cli driver loop
    ("drv", "the model flag loses its short name",
     'if drv_eq(name, "-m") != 0 || drv_eq(name, "--model") != 0 {',
     'if drv_eq(name, "-M") != 0 || drv_eq(name, "--model") != 0 {'),
    ("drv", "a boolean flag is assumed to consume a value",
     "    if k == DRV_K_TRUE || k == DRV_K_FALSE || k == DRV_K_ACTION ||",
     "    if k == DRV_K_FALSE || k == DRV_K_ACTION ||"),
    ("drv", "--chatml is parsed as a value-carrying flag",
     "    if id == DRV_CHATML || id == DRV_PROGRESS || id == DRV_SESSION || id == DRV_MOE_STREAM ||",
     "    if id == DRV_PROGRESS || id == DRV_SESSION || id == DRV_MOE_STREAM ||"),
    ("drv", "the streaming flag writes the row-stream field",
     "    if id == DRV_MOE_STREAM {\n        return DRV_F_MOE_ENABLED;",
     "    if id == DRV_MOE_STREAM {\n        return DRV_F_ROW_STREAM;"),
    ("drv", "a draft-source conflict is not a refusal",
     "export fn drv_draft_conflict(cur: i64, want: i64) -> i64 {\n    if cur != 0 && cur != want {",
     "export fn drv_draft_conflict(cur: i64, want: i64) -> i64 {\n    if cur != want {"),
    ("drv", "an unreadable list is not a usage-class exit",
     "    if kind == 6 {\n        return DRV_X_PPL_LIST_OPEN;",
     "    if kind == 6 {\n        return DRV_X_WORKLOAD_CONFLICT;"),
]

# ── half 2: arguments, harvest rules, refusals and the --ppl arithmetic ────
M2 = [
    # sampler chain
    ("sch", "the temperature stage carries the nucleus cutoff",
     "n = sch_put(out, outf, n, SCH_TEMP, 0, 0, temp);",
     "n = sch_put(out, outf, n, SCH_TEMP, 0, 0, top_p);"),
    ("sch", "top_k is passed a zero",
     "n = sch_put(out, outf, n, SCH_TOP_K, top_k, 0, 0.0);",
     "n = sch_put(out, outf, n, SCH_TOP_K, 0, 0, 0.0);"),
    ("sch", "the stage count is one short",
     "export fn sch_entries(temp: f64) -> i64 {\n    if temp > 0.0 {\n        return 4;",
     "export fn sch_entries(temp: f64) -> i64 {\n    if temp > 0.0 {\n        return 3;"),
    ("sch", "dist is labelled as top_k",
     "const SCH_DIST: i64 = 4;",
     "const SCH_DIST: i64 = 1;"),
    ("sch", "the default seed drifts by one",
     "const SCH_DEFAULT_SEED: i64 = 4294967295;",
     "const SCH_DEFAULT_SEED: i64 = 4294967294;"),
    ("sch", "the seed gate is inverted",
     "export fn sch_seed_gate(seed: i64) -> i64 {\n    if seed == SCH_DEFAULT_SEED {",
     "export fn sch_seed_gate(seed: i64) -> i64 {\n    if seed != SCH_DEFAULT_SEED {"),

    # mtp capture warm-up
    ("mw", "the verify decode is planned on the dense path",
     "    if mtp != 0 {\n        at = mw_put3(out, at, MW_FILL_VERIFY, 0, 1);",
     "    if mtp == 0 {\n        at = mw_put3(out, at, MW_FILL_VERIFY, 0, 1);"),
    ("mw", "the capture closes before the KV wipe",
     "    at = mw_put3(out, at, MW_END_CAPTURE, 0, 0);\n    at = mw_put3(out, at, MW_CLEAR_TARGET, 0, 0);",
     "    at = mw_put3(out, at, MW_CLEAR_TARGET, 0, 0);\n    at = mw_put3(out, at, MW_END_CAPTURE, 0, 0);"),
    ("mw", "the expert count is accepted without a cross-check",
     "    if ne2 == cur {\n        return MW_CHECK_COUNT;\n    }",
     "    if ne2 > 0 {\n        return MW_CHECK_COUNT;\n    }"),
    ("mw", "an architecture-unused slot is demanded",
     "    if used == 0 {\n        return MW_BAD_SLOT;\n    }",
     "    if used < 0 {\n        return MW_BAD_SLOT;\n    }"),
    ("mw", "a device-resident weight enters the dense list",
     "    if dev_non_cpu != 0 {\n        return MW_DROP;\n    }",
     "    if dev_non_cpu < 0 {\n        return MW_DROP;\n    }"),
    ("mw", "the row window is sized in decimal megabytes",
     "    return v * 1024 * 1024;",
     "    return v * 1000 * 1000;"),

    # cli driver loop
    ("drv", "the temperature flag writes top_k",
     "    if id == DRV_TEMP {\n        return DRV_F_TEMP;",
     "    if id == DRV_TEMP {\n        return DRV_F_TOP_K;"),
    ("drv", "the --ppl-skip flag writes the step switch",
     "    if id == DRV_PPL_SKIP {\n        return DRV_F_PPL_SKIP;",
     "    if id == DRV_PPL_SKIP {\n        return DRV_F_PPL_STEP;"),
    ("drv", "the trace-layers flag writes the compute-trace path",
     "    if id == DRV_COMPUTE_TRACE || id == DRV_COMPUTE_TRACE_LAYERS {\n        return DRV_F_COMPUTE_TRACE;",
     "    if id == DRV_COMPUTE_TRACE || id == DRV_COMPUTE_TRACE_LAYERS {\n        return DRV_F_IO_TRACE;"),
    ("drv", "a bad dense value is a usage-class exit",
     "    if kind == 4 {\n        return DRV_X_DENSE_ENUM;",
     "    if kind == 4 {\n        return DRV_X_MISSING_VALUE;"),
    ("drv", "the perplexity list is not a mode",
     "    if has_ppl != 0 || has_list != 0 {\n        return 2;",
     "    if has_ppl != 0 {\n        return 2;"),
    ("drv", "the workload rejection ignores the ppl list",
     "    if session != 0 || has_ppl != 0 || has_list != 0 {",
     "    if session != 0 || has_ppl != 0 {"),
]

# The third block is a second half-2 for drv: the --ppl arithmetic is its own
# decision set and deserves its own mutations. Split 6/6 per slice stays exactly
# as the task requires; these extend the corpus to the same total by pairing
# with M1/M2 above, so they live in a separate list used only with --extra.
M3 = [
    ("drv", "the csv field count drops the trailing field",
     "export fn drv_csv_count(commas: i64) -> i64 {\n    if commas < 0 {\n        return 1;\n    }\n    return commas + 1;",
     "export fn drv_csv_count(commas: i64) -> i64 {\n    if commas < 0 {\n        return 1;\n    }\n    return commas;"),
    ("drv", "the separator search looks for a dot",
     "        if s[i] == 44 {",
     "        if s[i] == 46 {"),
    ("drv", "a comment line is kept",
     "    if line[0] == 35 {",
     "    if line[0] == 59 {"),
    ("drv", "the line trim keeps the CR",
     "        if c == 13 || c == 32 {",
     "        if c == 32 {"),
    ("drv", "the percentage assumes a scored token",
     "    if n_scored > 0 {\n        let num: f64 = n_top1;\n        let den: f64 = n_scored;",
     "    if n_scored >= 0 {\n        let num: f64 = n_top1;\n        let den: f64 = n_scored;"),
    ("drv", "the single-text label always prints",
     "export fn drv_ppl_label(n_paths: i64) -> i64 {\n    if n_paths > 1 {",
     "export fn drv_ppl_label(n_paths: i64) -> i64 {\n    if n_paths >= 1 {"),
]

ORDER = {"sch": 0, "mw": 1, "drv": 2}


def link_and_run(name, mut_dir):
    obj = mut_dir / (name + ".o")
    gate = mut_dir / ("gate-" + name)
    inc = [CXX, "-O2", "-std=c++17", "-I", str(mut_dir), "-I", str(OUT),
           "-I", str(REPO / "runtime/core/include")]
    cmd = inc + [str(FIX[name]), str(obj), "-lm", "-o", str(gate)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return "link-fail", r.stderr
    try:
        r = subprocess.run([str(gate)], capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        return "hang", "the gate did not finish in 120 s"
    if r.returncode == 0:
        return "survived", r.stdout
    return "caught", r.stdout + r.stderr


def one(name, desc, old, new, idx, half):
    mut_dir = WORK / ("%s-h%s-%02d" % (name, half, idx))
    mut_dir.mkdir(parents=True, exist_ok=True)
    text = SRC[name].read_text()
    n = text.count(old)
    if n != 1:
        print("%-9s %-3s %-56s anchor occurs %d times" % ("BROKEN", name, desc, n), flush=True)
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
    if half == "1":
        muts = M1
    elif half == "2":
        muts = M2
    else:
        muts = M3
    WORK.mkdir(parents=True, exist_ok=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(one, name, desc, old, new, idx, half) for idx, (name, desc, old, new) in enumerate(muts)]
        for f in concurrent.futures.as_completed(futs):
            results.append(f.result())
    per_slice = {s: 0 for s in ("sch", "mw", "drv")}
    caught = 0
    for name, desc, verdict, detail in results:
        if verdict == "caught":
            caught += 1
            per_slice[name] += 1
    total = len(results)
    print("FG654_MUT half=%s caught=%d/%d  sch=%d/6 mw=%d/6 drv=%d/6"
          % (half, caught, total, per_slice["sch"], per_slice["mw"], per_slice["drv"]))
    return 0 if caught == total else 1


if __name__ == "__main__":
    sys.exit(main())
