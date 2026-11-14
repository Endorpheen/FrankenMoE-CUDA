#!/usr/bin/env python3
"""Mutation gate for P7-04 S1a.

12 textual mutations of source/quant.goose, six in each half. Each mutant must
still pass `goose check`, then the differential fixture must fail. A mutation
the fixture does not catch is reported.

    tests/mutate.py --half 1|2|all
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "quant.goose")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 gqa threshold",
     "new_type = if n_gqa >= 4 { T_Q4_K } else { T_Q3_K };",
     "new_type = if n_gqa >= 5 { T_Q4_K } else { T_Q3_K };"),
    ("A2 low-bit gqa/expert",
     "if n_gqa >= 4 || n_expert >= 4 {",
     "if n_gqa >= 5 || n_expert >= 5 {"),
    ("A3 expert-8 attn-v bump",
     "            if n_expert == 8 {\n                new_type = T_Q8_0;\n            }\n            st[S_I_ATTN] = st[S_I_ATTN] + 1;",
     "            if n_expert == 9 {\n                new_type = T_Q8_0;\n            }\n            st[S_I_ATTN] = st[S_I_ATTN] + 1;"),
    ("A4 use_more_bits",
     "return (i - n / 8) % 3 == 2;",
     "return (i - n / 8) % 3 == 1;"),
    ("A5 Q2_K ffn_down",
     "            if ftype == F_Q2_K {\n                new_type = T_Q3_K;\n            } else if ftype == F_Q2_K_S {",
     "            if ftype == F_Q2_K {\n                new_type = T_Q4_K;\n            } else if ftype == F_Q2_K_S {"),
    ("A6 token IQ2",
     "            if ftype == F_IQ2_XXS || ftype == F_IQ2_XS || ftype == F_IQ1_S || ftype == F_IQ1_M {\n                new_type = T_Q2_K;",
     "            if ftype == F_IQ2_XXS || ftype == F_IQ2_XS || ftype == F_IQ1_S || ftype == F_IQ1_M {\n                new_type = T_Q3_K;"),
]

HALF2 = [
    ("B1 policy IQ3_XXS",
     "if dst_type == T_IQ3_XXS || dst_type == T_IQ2_XXS",
     "if dst_type == T_IQ2_XXS"),
    ("B2 token exemption",
     'if qs_eq(name, "token_embd.weight") || qs_eq(name, "per_layer_token_embd.weight") ||',
     'if qs_eq(name, "tok_embd.weight") || qs_eq(name, "per_layer_token_embd.weight") ||'),
    ("B3 Q2_K_S condition",
     "return if ftype == F_Q2_K_S { 1 } else { 0 };",
     "return if ftype == F_Q2_K_S { 1 } else { 1 };"),
    ("B4 ggml policy IQ1_M",
     "if t == T_IQ2_XXS || t == T_IQ2_XS || t == T_IQ1_S {",
     "if t == T_IQ2_XXS || t == T_IQ2_XS || t == T_IQ1_S || t == T_IQ1_M {"),
    ("B5 token override",
     "if token_type < T_COUNT && cat == C_TOKEN_EMBD {",
     "if token_type < 0 && cat == C_TOKEN_EMBD {"),
    ("B6 mxfp4 3D",
     "            if ne2 > 1 {\n                new_type = T_MXFP4;",
     "            if ne2 > 100 {\n                new_type = T_MXFP4;"),
]


def run_one(name, old, new, idx):
    src = open(SRC, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {name}: pattern occurs {src.count(old)} times")
        return False
    work = os.path.join(ROOT, "build", f"mut{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, "quant.goose")
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
        tail = (build.stdout + build.stderr).splitlines()[-8:]
        for line in tail:
            print("   ", line)
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["1", "2", "all"], default="all")
    a = ap.parse_args()
    muts = (HALF1 if a.half == "1" else HALF2 if a.half == "2" else HALF1 + HALF2)
    caught = 0
    for i, (name, old, new) in enumerate(muts, 1):
        if run_one(name, old, new, i):
            caught += 1
    print(f"mutations {caught}/{len(muts)} caught")
    return 0 if caught == len(muts) else 1


if __name__ == "__main__":
    sys.exit(main())
