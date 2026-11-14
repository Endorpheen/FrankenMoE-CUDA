#!/usr/bin/env python3
"""Mutation gate for P7-04 S3b: K, IQ, MXFP4, NVFP4 and TQ row quantizers.

Twelve textual mutations of source/quant-rows2.goose, six in each half. Each
mutant must still pass `goose check`, and the differential fixture must then
fail. A mutation the fixture does not catch is reported as MISS.

    tests/mutate.py --half 1|2|all

Every mutation targets a decision the port owns: a scale divisor or sign, a
packing shift, a qh bit position, an exponent offset, a rounding bit, a fudge
factor.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant-rows2.goose")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 q2_K super-scale divisor",
     "            st_f16(yb + 80, max_scale / 15.0);",
     "            st_f16(yb + 80, max_scale / 16.0);"),
    ("A2 q4_K group level count",
     "            let sc = make_qkx2(32, 15, xb + ((j * 32) as u64) * 4, wa, L + (j * 32) as u64, laux, mnbuf,\n"
     "                               0 as f32 - 1.0, 0.1, 20, false);",
     "            let sc = make_qkx2(32, 16, xb + ((j * 32) as u64) * 4, wa, L + (j * 32) as u64, laux, mnbuf,\n"
     "                               0 as f32 - 1.0, 0.1, 20, false);"),
    ("A3 q3_K super-scale divisor",
     "            let isc = -32.0 / max_scale;",
     "            let isc = -31.0 / max_scale;"),
    ("A4 q3_K hmask bit step",
     "                hm = hm << 1;",
     "                hm = hm << 2;"),
    ("A5 q5_K qh second-bit mask",
     "        var m2 = 2 as i64;",
     "        var m2 = 4 as i64;"),
    ("A6 q6_K super-scale divisor",
     "        let iscale = -128.0 / max_scale;",
     "        let iscale = -127.0 / max_scale;"),
]

HALF2 = [
    ("B1 q8_K quant scale",
     "        let iscale = -127.0 / mx;",
     "        let iscale = -128.0 / mx;"),
    ("B2 tq1_0 ceiling division",
     "            q = (q * 256 + 242) / 243;\n"
     "            st_u8(yb + (m as u64), q);",
     "            q = (q * 256 + 241) / 243;\n"
     "            st_u8(yb + (m as u64), q);"),
    ("B3 mxfp4 E8M0 exponent offset",
     "            e = u8of(floor32(log232(amax)) - 2.0 + 127.0);",
     "            e = u8of(floor32(log232(amax)) - 3.0 + 127.0);"),
    ("B4 nvfp4 UE4M3 round bit",
     "    let round_bit = (bits >> 19) & 1;",
     "    let round_bit = (bits >> 18) & 1;"),
    ("B5 iq4_xs super-scale divisor",
     "        let sd = -max_scale / 32.0;",
     "        let sd = -max_scale / 16.0;"),
    ("B6 iq3_xxs scale fudge",
     "        st_f16(yb, d * 1.0125);",
     "        st_f16(yb, d * 1.0);"),
]


def run_one(name, old, new, idx):
    src = open(SRC, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {name}: pattern occurs {src.count(old)} times")
        return False

    work = os.path.join(ROOT, "build", f"mut{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, "quant-rows2.goose")
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
