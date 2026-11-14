#!/usr/bin/env python3
"""Mutation gate for the dequant closure (P7-04 S5b). 12 edits, two halves.

Each mutation is a single semantic change in source/quant-dequant2.goose. The
mutated module is rebuilt at -O0 (tests/build.sh, OPT=-O0) and the gate must
fail: a passing gate means the corpus does not exercise that decode step.

    python3 tests/mutate.py 0 6
    python3 tests/mutate.py 6 12
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant-dequant2.goose")
WORK = os.path.join(ROOT, "build-mut")
MUT = os.path.join(ROOT, "source", "m.goose")

M = [
    ("q2_K reads the minimum nibble as the scale",
     "                let dl0 = d * ((sc0 & 15) as f32);",
     "                let dl0 = d * ((sc0 >> 4) as f32);"),
    ("q4_K high nibble shifts by 3",
     "                out[y + 32 + l] = d2 * ((u8at(src, qb + l) >> 4) as f32) - m2;",
     "                out[y + 32 + l] = d2 * ((u8at(src, qb + l) >> 3) as f32) - m2;"),
    ("q6_K q2 takes the third high bit",
     "                let q2 = (((u8at(src, ql + l + 32) & 15) | (((u8at(src, qh + l) >> 2) & 3) << 4)) - 32) as f32;",
     "                let q2 = (((u8at(src, ql + l + 32) & 15) | (((u8at(src, qh + l) >> 3) & 3) << 4)) - 32) as f32;"),
    ("q8_K treats int8 as unsigned",
     "            out[i * QK_K + j] = d * (i8of(u8at(src, o + 4 + j)) as f32);",
     "            out[i * QK_K + j] = d * ((u8at(src, o + 4 + j)) as f32);"),
    ("tq1_0 tier decode uses 512 buckets",
     "                    let xi = (q * 3) >> 8;\n                    out[y] = ((xi - 1) as f32) * d;\n                    y = y + 1;\n                    m = m + 1;\n                }\n                nn = nn + 1;\n            }\n            j = j + 32;",
     "                    let xi = (q * 3) >> 9;\n                    out[y] = ((xi - 1) as f32) * d;\n                    y = y + 1;\n                    m = m + 1;\n                }\n                nn = nn + 1;\n            }\n            j = j + 32;"),
    ("mxfp4 e8m0 exponent bias is off by one",
     "        b = (e - 1) << 23;",
     "        b = (e - 2) << 23;"),
    ("nvfp4 sub-block stride is 4 bytes",
     "                let qb = u8at(src, o + 4 + s * 8 + j);",
     "                let qb = u8at(src, o + 4 + s * 4 + j);"),
    ("iq4_xs scale high bits use the wrong pair",
     "            let ls = ((u8at(src, o + 4 + ib / 2) >> (4 * (ib & 1))) & 15) | (((sh >> (2 * ib)) & 3) << 4);",
     "            let ls = ((u8at(src, o + 4 + ib / 2) >> (4 * (ib & 1))) & 15) | (((sh >> (2 * ib + 1)) & 3) << 4);"),
    ("iq3_xxs second grid takes the wrong sign bit",
     "                    out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((signs & (1 << (j + 4))) != 0);",
     "                    out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((signs & (1 << (j + 3))) != 0);"),
    ("iq2_s grid mask keeps 0x200 instead of 0x300",
     "                let gv = iq2_grid(3, u8at(src, qs + ib32 * 4 + l) | ((u8at(src, qh + ib32) << (8 - 2 * l)) & 0x300));",
     "                let gv = iq2_grid(3, u8at(src, qs + ib32 * 4 + l) | ((u8at(src, qh + ib32) << (8 - 2 * l)) & 0x200));"),
    ("f16 encode clamps the subnormal bias one exponent early",
     "    if bias < 0x71000000 {",
     "    if bias < 0x70000000 {"),
    ("q8_0 rounding treats a tie as a round up",
     "    if frac > 0.5 {",
     "    if frac >= 0.5 {"),
]


def run(cmd, timeout=600):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    orig = open(SRC, encoding="utf-8").read()
    os.makedirs(WORK, exist_ok=True)
    caught = 0
    for idx in range(lo, hi):
        label, old, new = M[idx]
        if orig.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor count {orig.count(old)}")
            continue
        open(MUT, "w", encoding="utf-8").write(orig.replace(old, new, 1))
        r = run(f"OPT=-O0 OUT_DIR={WORK} GOOSE_SRC={MUT} {HERE}/build.sh")
        if r.returncode == 0:
            print(f"[MISSED] {idx} {label}")
        else:
            tail = (r.stdout.strip().splitlines() or ["no output"])[-1]
            print(f"[caught] {idx} {label}: {tail[:110]}")
            caught += 1
        if os.path.exists(MUT):
            os.remove(MUT)
    print(f"mutations {caught}/{hi - lo} caught (range {lo}:{hi})")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
