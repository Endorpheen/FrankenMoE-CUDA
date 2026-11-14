#!/usr/bin/env python3
"""Mutation gate for P7-04 S3a: the unweighted row quantizers.

Twelve textual mutations of source/quant-rows.goose, six in each half. Each
mutant must still pass `goose check`, and the differential fixture must then
fail. A mutation the fixture does not catch is reported as MISS.

    tests/mutate.py --half 1|2|all

Every mutation targets a decision the port owns: a scale divisor, the nibble
clamp, the first-maximum selection, a 5th-bit position, the sign test, the
q8_0 rounding mode or the q2_0 pack shift.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant-rows.goose")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 q4_0 scale divisor",
     "        let d = mx / -8.0;",
     "        let d = mx / -16.0;"),
    ("A2 q4_0 nibble clamp",
     "            let q0 = qz_min_i(15, qz_i8(qz_f2i32(x0 + 8.5)));\n"
     "            let q1 = qz_min_i(15, qz_i8(qz_f2i32(x1 + 8.5)));",
     "            let q0 = qz_min_i(14, qz_i8(qz_f2i32(x0 + 8.5)));\n"
     "            let q1 = qz_min_i(14, qz_i8(qz_f2i32(x1 + 8.5)));"),
    ("A3 q4_0/q5_0 first maximum",
     "        if amax < a {",
     "        if amax <= a {"),
    ("A4 q4_1 scale divisor",
     "        let d = (mx - mn) / 15.0;",
     "        let d = (mx - mn) / 16.0;"),
    ("A5 q5_0 low-bit position",
     "            st_u8(yb + 6 + (j as u64), (u0 & 15) | ((u1 & 15) << 4));\n"
     "            qh = qh | (((u0 & 16) >> 4) << j);",
     "            st_u8(yb + 6 + (j as u64), (u0 & 15) | ((u1 & 15) << 4));\n"
     "            qh = qh | (((u0 & 16) >> 4) << (j + 1));"),
    ("A6 q5_0 nibble clamp",
     "            let q0 = qz_min_i(31, qz_i8(qz_f2i32(x0 + 16.5)));\n"
     "            let q1 = qz_min_i(31, qz_i8(qz_f2i32(x1 + 16.5)));",
     "            let q0 = qz_min_i(30, qz_i8(qz_f2i32(x0 + 16.5)));\n"
     "            let q1 = qz_min_i(30, qz_i8(qz_f2i32(x1 + 16.5)));"),
]

HALF2 = [
    ("B1 q5_1 scale divisor",
     "        let d = (mx - mn) / 31.0;",
     "        let d = (mx - mn) / 30.0;"),
    ("B2 q5_1 5th-bit position",
     "            qh = qh | (((u1 & 16) >> 4) << (j + QK5_0 / 2));",
     "            qh = qh | (((u1 & 16) >> 4) << j);"),
    ("B3 q8_0 scale divisor",
     "        let d = amax / 127.0;",
     "        let d = amax / 128.0;"),
    ("B4 q8_0 rounding mode",
     "            let x0 = ld_f32(xb + (j as u64) * 4) * id;\n"
     "            st_u8(yb + 2 + (j as u64), qz_u8(qz_round_f2i32(x0)));",
     "            let x0 = ld_f32(xb + (j as u64) * 4) * id;\n"
     "            st_u8(yb + 2 + (j as u64), qz_u8(qz_f2i32(x0)));"),
    ("B5 q1_0 sign test",
     "            if v >= 0 as f32 {",
     "            if v > 0 as f32 {"),
    ("B6 q2_0 pack shift",
     "            st_u8(at, qz_u8(ld_u8_at(at) | (q << ((j % 4) * 2))));",
     "            st_u8(at, qz_u8(ld_u8_at(at) | (q << (j % 4))));"),
]


def run_one(name, old, new, idx):
    src = open(SRC, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {name}: pattern occurs {src.count(old)} times")
        return False

    work = os.path.join(ROOT, "build", f"mut{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, "quant-rows.goose")
    with open(mutant, "w", encoding="utf-8") as f:
        f.write(src.replace(old, new))

    check = subprocess.run([GOOSE, "check", mutant], capture_output=True, text=True)
    if check.returncode != 0:
        print(f"SKIP {name}: goose check failed")
        return False

    env = dict(os.environ, GOOSE_SRC=mutant, OUT_DIR=work, OPT="-O0")
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
