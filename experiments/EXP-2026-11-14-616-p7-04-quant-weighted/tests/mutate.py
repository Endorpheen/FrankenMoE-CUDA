#!/usr/bin/env python3
"""Mutation gate for P7-04 S4: weighted (imatrix) row quantizers.

Twelve textual mutations of source/quant-weighted.goose, six in each half.
Each mutant must still pass `goose check`, and the differential fixture must
then fail. A mutation the fixture does not catch is reported as MISS.

    tests/mutate.py --half 1|2|all

Every mutation targets a decision the weighted port owns: a group level count,
the per-element weight array, a scale clamp, the imatrix pointer of a call, the
super-scale search step, the sigma2 factor of an IQ weight, the neighbour-trial
count, or the final super-scale weights of IQ1_M.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant-weighted.goose")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 q2_K weighted group level count",
     "make_qkx3(16, 3, xb + ((16 * j) as u64) * 4, wa, L + ((16 * j) as u64),",
     "make_qkx3(16, 4, xb + ((16 * j) as u64) * 4, wa, L + ((16 * j) as u64),"),
    ("A2 q3_K weighted per-element weights replaced by group sums",
     "L + ((16 * j) as u64), 1, wa);",
     "L + ((16 * j) as u64), 1, sw);"),
    ("A3 q4_K weighted group level count",
     "make_qkx3(32, 15, xb + ((32 * j) as u64) * 4, wa, L + ((32 * j) as u64),",
     "make_qkx3(32, 16, xb + ((32 * j) as u64) * 4, wa, L + ((32 * j) as u64),"),
    ("A4 q5_K weighted scale clamp",
     "let lsv = imin(63, ld_u8(ls + (j as u64)));",
     "let lsv = imin(31, ld_u8(ls + (j as u64)));"),
    ("A5 q6_K weighted drops the imatrix weights",
     "L + ((16 * ib) as u64), 1, qw);",
     "L + ((16 * ib) as u64), 1, 0 as u64);"),
    ("A6 make_qp super-scale search step",
     "let isc = (0.1 * (is as f32) + (nmax as f32)) / mx;",
     "let isc = (0.2 * (is as f32) + (nmax as f32)) / mx;"),
]

HALF2 = [
    ("B1 q4_0 weighted level count",
     "let d = make_qx(32, 8, xb, L, 1, wa);",
     "let d = make_qx(32, 7, xb, L, 1, wa);"),
    ("B2 q4_1 weighted level count",
     "let d = make_qkx3(32, 15, xb, wa, L, mnbuf, laux, 0 as f32 - 0.9, 0.05, 36, false);",
     "let d = make_qkx3(32, 14, xb, wa, L, mnbuf, laux, 0 as f32 - 0.9, 0.05, 36, false);"),
    ("B3 iq4_nl weighted sigma2 factor",
     "iq4_nl_block(xb, L, wa, qw, 2.0 * sumx2 / 32.0, 7)",
     "iq4_nl_block(xb, L, wa, qw, 1.0 * sumx2 / 32.0, 7)"),
    ("B4 iq4_xs weighted neighbour trials",
     "L + (ib * 32) as u64, wa, qw, sigma2, 7);",
     "L + (ib * 32) as u64, wa, qw, sigma2, 6);"),
    ("B5 iq2_xxs weighted level count",
     "var scale = make_qp(32, 4, xv, L, wa);",
     "var scale = make_qp(32, 3, xv, L, wa);"),
    ("B6 iq1_m weighted super-scale drops the sqrt weight",
     "w = ld_f32(qwf + (i2 as u64) * 4) * sqrt32(sigma2 + xval * xval);",
     "w = ld_f32(qwf + (i2 as u64) * 4);"),
]


def run_one(name, old, new, idx):
    src = open(SRC, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {name}: pattern occurs {src.count(old)} times")
        return False

    work = os.path.join(ROOT, "build", f"mut{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, "quant-weighted.goose")
    with open(mutant, "w", encoding="utf-8") as f:
        f.write(src.replace(old, new))

    check = subprocess.run([GOOSE, "check", mutant], capture_output=True, text=True)
    if check.returncode != 0:
        print(f"SKIP {name}: goose check failed")
        return False

    env = dict(os.environ, GOOSE_SRC=mutant, OUT_DIR=work, OPT="-O0", GATE_ARG="--quick")
    build = subprocess.run(["bash", os.path.join(HERE, "build.sh")],
                           env=env, capture_output=True, text=True)
    caught = build.returncode != 0
    print(f"{'CATCH' if caught else 'MISS '} {name}")
    if not caught:
        for line in (build.stdout + build.stderr).splitlines()[-6:]:
            print("   ", line)
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["1", "2", "all"], default="all")
    a = ap.parse_args()
    muts = HALF1 if a.half == "1" else HALF2 if a.half == "2" else HALF1 + HALF2
    base = 6 if a.half == "2" else 0
    caught = 0
    for i, (name, old, new) in enumerate(muts, 1):
        if run_one(name, old, new, base + i):
            caught += 1
    print(f"mutations {caught}/{len(muts)} caught")
    return 0 if caught == len(muts) else 1


if __name__ == "__main__":
    sys.exit(main())
