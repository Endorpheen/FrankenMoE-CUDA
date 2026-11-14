#!/usr/bin/env python3
"""Mutation table for the ggml-opt slice (P4-12).

Each mutation edits one decision in source/ggml-opt.goose, rebuilds the port at
-O0, links it against the already built native oracle and expects the gate to
fail. A mutation that still passes is SURVIVED and makes the run exit non-zero.

Two halves of six: --half A or B. Builds are serial by default.
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "ggml-opt.goose")
OUT = os.path.join(ROOT, "build")

GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
LLAMA = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda")

# (id, half, slice, description, old, new)
MUTATIONS = [
    ("M01", "A", "s1", "batch count off by one",
     "    return ndata / ndata_batch;",
     "    return ndata / ndata_batch + 1;"),
    ("M02", "A", "s1", "negative split keeps no training data",
     "    if split < 0 {\n        split = ndata;\n    }",
     "    if split < 0 {\n        split = 0;\n    }"),
    ("M03", "A", "s1", "validation split drops the physical period",
     "let ibatch_split: i64 = (scaled as i64) * period;",
     "let ibatch_split: i64 = (scaled as i64);"),
    ("M04", "A", "s1", "shard bytes invert the shard ratio",
     "out[0] = nbytes_data * ndata_shard / ndata;",
     "out[0] = nbytes_data * ndata / ndata_shard;"),
    ("M05", "A", "s1", "shuffle limit not divided by the shard size",
     "    return idata / ndata_shard;",
     "    return idata;"),
    ("M06", "A", "s1", "batch shard index transposed",
     "    return perm[ibatch * shards_per_batch + k];",
     "    return perm[k * shards_per_batch + ibatch];"),

    ("M07", "B", "s2", "optimizer graph selected on the wrong step index",
     "    if nxt == 0 {",
     "    if nxt == 1 {"),
    ("M08", "B", "s2", "step index does not wrap",
     "    return (opt_i + 1) % opt_period;",
     "    return opt_i + 1;"),
    ("M09", "B", "s2", "period-one static graph accumulates anyway",
     "    if static_graphs != 0 && build_type_alloc == 30 && opt_period == 1 {",
     "    if 0 != 0 {"),
    ("M10", "B", "s2", "cross-entropy ignores the opt period",
     "            return 1.0 / opt_period;",
     "            return 1.0;"),
    ("M11", "B", "s3", "per-datapoint loss is not rescaled",
     "            vf = vf * opt_period;",
     "            vf = vf;"),
    ("M12", "B", "s3", "undefined accuracy sentinel is not sticky",
     "    if has_ncorrect == 0 || cur < 0 {",
     "    if has_ncorrect == 0 {"),
]


def case_name(line):
    tok = line.split()
    if "FAIL" in tok:
        i = tok.index("FAIL")
        if i + 1 < len(tok):
            return tok[i + 1]
    return "nonzero-exit"


def run_one(m):
    mid, half, slice_id, desc, old, new = m
    work = os.path.join(OUT, "mut", mid)
    os.makedirs(work, exist_ok=True)
    text = open(SRC, encoding="utf-8").read()
    count = text.count(old)
    if count != 1:
        return mid, half, slice_id, "BADPATTERN", f"{count} matches", desc

    mut_src = os.path.join(work, "ggml-opt.goose")
    with open(mut_src, "w", encoding="utf-8") as fh:
        fh.write(text.replace(old, new, 1))

    gen_c = os.path.join(work, "ggml-opt-gen.c")
    gen_h = os.path.join(work, "ggml-opt-gen.h")
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
    cmd = ["g++-13", os.path.join(OUT, "fixture.o"), os.path.join(OUT, "oracle.o"), gen_o,
           "-I" + work,
           "-L" + os.path.join(LLAMA, "build-cuda", "bin"), "-lggml-base", "-lggml-cpu", "-lm",
           "-Wl,-rpath," + os.path.join(LLAMA, "build-cuda", "bin"), "-o", binp]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return mid, half, slice_id, "BUILDFAIL", r.stderr.strip()[-200:], desc

    r = subprocess.run([binp], capture_output=True, text=True)
    failed = [ln for ln in r.stdout.splitlines() if " FAIL " in ln]
    if r.returncode != 0:
        return mid, half, slice_id, "KILLED", case_name(failed[0]) if failed else "nonzero-exit", desc
    return mid, half, slice_id, "SURVIVED", "", desc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["A", "B"], default="A")
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()

    if not os.path.exists(os.path.join(OUT, "oracle.o")):
        sys.exit("build the gate first: bash tests/build.sh")

    table = [m for m in MUTATIONS if m[1] == args.half]
    results = [run_one(m) for m in table]
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
