#!/usr/bin/env python3
"""Mutation gate for the basic dequantizers (P7-04 S5a). 12 edits, two halves."""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant-dequant.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
L = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda")

M = [
    ("q4_0 offset is -7", "                    x0 = x0 - 8;\n                    x1 = x1 - 8;",
     "                    x0 = x0 - 7;\n                    x1 = x1 - 7;"),
    ("q4 high nibble shifts by 3", "                var x1 = (q >> 4) & 15;", "                var x1 = (q >> 3) & 15;"),
    ("q4 treats q4_1 as q5_1 for the minimum", "            if t == T_Q4_1 {\n                m = dq_half(dq_u16(src, o + 2));",
     "            if t == T_Q5_1 {\n                m = dq_half(dq_u16(src, o + 2));"),
    ("q4 scale reads the minimum slot", "            let d = dq_half(dq_u16(src, o));\n            var m: f32 = 0.0;\n            if t == T_Q4_1 {\n                m = dq_half(dq_u16(src, o + 2));",
     "            var d = dq_half(dq_u16(src, o));\n            var m: f32 = 0.0;\n            if t == T_Q4_1 {\n                d = dq_half(dq_u16(src, o + 2));"),
    ("q5 high-bit mask is 0x20", "                let xh0 = ((qh >> j) << 4) & 16;", "                let xh0 = ((qh >> j) << 4) & 32;"),
    ("q5 second high bit uses j+11", "                let xh1 = (qh >> (j + 12)) & 16;", "                let xh1 = (qh >> (j + 11)) & 16;"),
    ("q5_0 offset is -15", "                    x0 = x0 - 16;\n                    x1 = x1 - 16;",
     "                    x0 = x0 - 15;\n                    x1 = x1 - 15;"),
    ("q5_1 reads its minimum from the qh slot", "                qs_at = o + 8;", "                qs_at = o + 7;"),
    ("q8_0 treats int8 as unsigned", "                let v = (q - (q & 128) * 2) as f32;", "                let v = q as f32;"),
    ("q4_0 block size is 20", "    if t == T_Q4_0 { return 18; }", "    if t == T_Q4_0 { return 20; }"),
    ("half exponent bias is 16", "        var e = exp - 15;", "        var e = exp - 16;"),
    ("q8_0 reads its quants one byte early", "                let q = dq_u8(src, o + 2 + j);",
     "                let q = dq_u8(src, o + 1 + j);"),
    # EXP633: the signed-zero fix must be protected by its own mutations.
    ("half negation flips zero sign", "        return -v;", "        return 0.0 - v;"),
    ("q4_0 branch adds a zero min", "                if t == T_Q4_0 {\n                    out[i * QK + j] = (x0 as f32) * d;",
     "                if t == T_Q5_0 {\n                    out[i * QK + j] = (x0 as f32) * d;"),
    ("q5_0 branch always adds the min", "                if t == T_Q5_0 {\n                    out[i * QK + j] = (x0 as f32) * d;",
     "                if t == T_Q4_0 {\n                    out[i * QK + j] = (x0 as f32) * d;"),
]


def run(cmd, timeout=300):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    orig = open(SRC, encoding="utf-8").read()
    caught = 0
    for idx in range(lo, hi):
        label, old, new = M[idx]
        if orig.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor {orig.count(old)}")
            continue
        open(os.path.join(ROOT, "source", "m.goose"), "w", encoding="utf-8").write(orig.replace(old, new, 1))
        g = run(f"{GOOSE} --standalone -o {HERE}/m.c --header {HERE}/g.h {ROOT}/source/m.goose")
        if g.returncode != 0:
            print(f"[{idx}] BUILD-FAIL {label}")
            caught += 1
            continue
        c = run(f"gcc-13 -O0 -std=c11 -w -c {HERE}/m.c -o {HERE}/m.o")
        l = run(f"g++-13 -O1 -std=c++17 -I{HERE} -I{L}/ggml/include {HERE}/fixture.cpp {HERE}/m.o "
                f"-L{L}/build-cuda/bin -lggml-base -Wl,-rpath,{L}/build-cuda/bin -o {HERE}/mgate")
        if c.returncode or l.returncode:
            print(f"[{idx}] LINK-FAIL {label}")
            caught += 1
            continue
        r = run(f"{HERE}/mgate")
        ok = r.returncode != 0
        tail = (r.stdout.strip().splitlines() or ["no output"])[-1]
        print(f"[{'caught' if ok else 'MISSED'}] {idx} {label}: {tail}")
        caught += ok
    print(f"mutations {caught}/{hi - lo} caught (range {lo}:{hi})")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
