#!/usr/bin/env python3
"""Mutation table for EXP P4-12.

Each mutation edits one decision in source/runtime.goose, rebuilds the port at
-O0, links it against the already built oracle fixture and expects the gate to
fail. A mutation that still passes is reported as SURVIVED and makes the run
exit non-zero.

Two halves of six: --half A or B. Builds are serial by default (--jobs 1).
"""

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "runtime.goose")
OUT = os.path.join(ROOT, "build")

GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
LLAMA = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda")

# (id, half, slice, description, old, new)
MUTATIONS = [
    ("M01", "A", "mb", "meta device name drops the M prefix",
     "w = put_byte(out, w, 77); // M", "w = put_byte(out, w, 65); // A"),
    ("M02", "A", "mb", "caps seed forgets the async flag",
     "var m: i64 = 17;", "var m: i64 = 16;"),
    ("M03", "A", "mb", "free memory sum starts at -1",
     "export fn mb_free_sum(vals: const i64[:]) -> i64 {\n    var t: i64 = 0;",
     "export fn mb_free_sum(vals: const i64[:]) -> i64 {\n    var t: i64 = -1;"),
    ("M04", "A", "mb", "alignment takes the minimum instead of the maximum",
     "        if al[i] > m {\n            m = al[i];",
     "        if al[i] < m {\n            m = al[i];"),
    ("M05", "A", "mb", "max buffer size takes the maximum instead of the minimum",
     "        if v < m {", "        if v > m {"),
    ("M06", "A", "mb", "host buffer type ignores a mismatching member",
     "        if ids[i] != first {", "        if 0 != 0 {"),

    ("M07", "B", "mb", "supports_buft ignores device order",
     "    if a_meta == 0 || b_meta == 0 {\n        return 0;\n    }\n    if a.len != b.len {\n        return 0;\n    }\n    var i: i64 = 0;\n    while i < a.len {\n        if a[i] != b[i] {",
     "    if a_meta == 0 || b_meta == 0 {\n        return 0;\n    }\n    if a.len != b.len {\n        return 0;\n    }\n    var i: i64 = 0;\n    while i < a.len {\n        if 0 != 0 {"),
    ("M08", "B", "th", "params_default polls at 0 instead of 50",
     "    out[2] = 50;", "    out[2] = 0;"),
    ("M09", "B", "th", "cpumask wrap resets the index to 0",
     "            idx = idx - 512;", "            idx = 0;"),
    ("M10", "B", "th", "plan clamp no longer applied",
     "    if n > pool {\n        n = pool;\n    }", "    if n > pool {\n        n = n;\n    }"),
    ("M11", "B", "rt", "io_threads upper bound drops to 7",
     "if v < 1 || v > 8 {", "if v < 1 || v > 7 {"),
    ("M12", "B", "rg", "registry key drops the userdata check",
     "if a_cb != b_cb || a_ud != b_ud {", "if a_cb != b_cb {"),
]


def run_one(m, keep=False):
    mid, half, slice_id, desc, old, new = m
    work = os.path.join(OUT, "mut", mid)
    os.makedirs(work, exist_ok=True)
    text = open(SRC, encoding="utf-8").read()
    count = text.count(old)
    if count != 1:
        return mid, half, slice_id, "BADPATTERN", f"{count} matches", desc
    mut_src = os.path.join(work, "runtime.goose")
    with open(mut_src, "w", encoding="utf-8") as fh:
        fh.write(text.replace(old, new, 1))

    gen_c = os.path.join(work, "runtime-gen.c")
    gen_h = os.path.join(work, "runtime-gen.h")
    r = subprocess.run([GOOSE, "--standalone", "-o", gen_c, "--header", gen_h, mut_src],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return mid, half, slice_id, "BUILDFAIL", r.stderr.strip()[-200:], desc

    gen_o = os.path.join(work, "gen.o")
    r = subprocess.run(["gcc-13", "-O0", "-std=c11", "-c", gen_c, "-o", gen_o],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return mid, half, slice_id, "BUILDFAIL", r.stderr.strip()[-200:], desc

    binp = os.path.join(work, "gate")
    cmd = ["g++-13", os.path.join(OUT, "fixture.o"), gen_o, os.path.join(OUT, "config.o"),
           "-L" + os.path.join(LLAMA, "build-cuda", "bin"), "-lggml", "-lggml-base",
           "-Wl,-rpath," + os.path.join(LLAMA, "build-cuda", "bin"), "-o", binp]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return mid, half, slice_id, "BUILDFAIL", r.stderr.strip()[-200:], desc

    r = subprocess.run([binp], capture_output=True, text=True)
    out = r.stdout
    failed = [ln for ln in out.splitlines() if " FAIL " in ln]
    if r.returncode != 0:
        first = failed[0].split("case ")[1].split(" ")[0] if failed else "nonzero-exit"
        return mid, half, slice_id, "KILLED", first, desc
    return mid, half, slice_id, "SURVIVED", "", desc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["A", "B"], default="A")
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()

    if not os.path.exists(os.path.join(OUT, "fixture.o")):
        sys.exit("build the gate first: bash tests/build.sh")

    table = [m for m in MUTATIONS if m[1] == args.half]
    results = []
    # Serial by default: the repository measures one build at a time.
    for m in table:
        results.append(run_one(m))
    bad = 0
    for mid, half, slice_id, status, detail, desc in results:
        if status != "KILLED":
            bad += 1
        print(f"{mid} half={half} slice={slice_id} {status:9s} {detail:32s} {desc}")
    killed = sum(1 for r in results if r[3] == "KILLED")
    print(f"half {args.half}: killed {killed}/{len(results)}")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
