#!/usr/bin/env python3
# Sanity check for the parse gate: plant a few defects, rebuild, and require the
# parse gate to fail. Proves the gate is not vacuous.
import os, subprocess, json
from pathlib import Path

ROOT = str(Path(__file__).resolve().parent.parent)
BASE = f"{ROOT}/source/runtime.goose"
W = f"{ROOT}/build/pm"
G = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
L = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda"

MUT = [
    ("M01 constructed start marker", "tool_call_start_marker\", \"<tool_call>\")", "tool_call_start_marker\", \"<toolcall>\")"),
    ("M05 json digits min 1 -> 0", "st, \"[0-9]\", 1, 0 - 1);", "st, \"[0-9]\", 0, 0 - 1);"),
    ("M06 flat fields sorted descending", "if fkey[b - 1] > ki {", "if fkey[b - 1] < ki {"),
    ("M15 exponent reuses digits0", "expo = peg_seq2(kind, cfirst, ccount, children, sidx, st, expo, digits);",
     "expo = peg_seq2(kind, cfirst, ccount, children, sidx, st, expo, digits0);"),
    ("X1 constructed param closer typo", "mval_blob, \"parameter_closer\", \"</param>\"", "mval_blob, \"parameter_closer\", \"</paramm>\""),
]


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def main():
    os.makedirs(W, exist_ok=True)
    src = open(BASE).read()
    out = []
    for name, needle, repl in MUT:
        if src.count(needle) == 0:
            out.append({"mutation": name, "status": "NOT-APPLIED"})
            continue
        open(f"{W}/m.goose", "w").write(src.replace(needle, repl, 1))
        r = sh(f"timeout 300 {G} --standalone -o {W}/m.c --header {W}/m.h {W}/m.goose")
        if r.returncode != 0:
            out.append({"mutation": name, "status": "goose-fail"})
            continue
        r = sh(f"timeout 300 gcc-13 -O0 -std=c11 -w -c {W}/m.c -o {W}/m.o")
        if r.returncode != 0:
            out.append({"mutation": name, "status": "gcc-fail"})
            continue
        r = sh(f"g++-13 {ROOT}/build/parse_fixture.o {W}/m.o -L{L}/build-cuda/bin -lllama-common "
               f"-Wl,-rpath,{L}/build-cuda/bin -o {W}/gate")
        if r.returncode != 0:
            out.append({"mutation": name, "status": "link-fail"})
            continue
        r = sh(f"timeout 300 {W}/gate")
        fails = [ln.split()[0] for ln in r.stdout.splitlines() if " FAIL" in ln]
        out.append({"mutation": name, "status": "caught" if fails else "missed", "failing_cases": fails})
        print(json.dumps(out[-1]), flush=True)
    with open(f"{ROOT}/build/parse_mutations.json", "w") as f:
        json.dump(out, f, indent=1)


main()
