#!/usr/bin/env python3
"""Mutation gate for P9-08.

12 textual mutations per slice, six in each half. Every mutant must pass
`goose check`, then the slice's differential gate must fail when the mutant is
built at -O0.

    tests/mutate.py --module lga|ppl|kvp|all --half 1|2|all
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

SLICES = {
    "lga": ("lga-load-admission.goose", "fixture_lga.cpp"),
    "ppl": ("ppl-perplexity-plan.goose", "fixture_ppl.cpp"),
    "kvp": ("kvp-kv-turn-state.goose", "fixture_kvp.cpp"),
}

LGA_H1 = [
    ("A1 an equal hot-slot count is refused",
     "if hot_slots > 0 && hot_slots > n_expert {",
     "if hot_slots > 0 && hot_slots >= n_expert {"),
    ("A2 override unset arm narrowed",
     "if n_expert_used <= 0 {",
     "if n_expert_used < 0 {"),
    ("A3 dense arm accepts zero experts",
     "if n_expert <= 0 {",
     "if n_expert < 0 {"),
    ("A4 override range off by one",
     "if n_expert_used > n_expert {",
     "if n_expert_used >= n_expert {"),
    ("A5 override value bumped",
     "return n_expert_used;",
     "return n_expert_used + 1;"),
    ("A6 the plan runs past its first failure",
     "if fail[stage - 1] != 0 {",
     "if fail[stage - 1] == 0 {"),
]

LGA_H2 = [
    ("B1 mmap dropped from the model params",
     "var m: i64 = LGA_MP_MMAP + LGA_MP_NO_BUFT;",
     "var m: i64 = LGA_MP_NO_BUFT;"),
    ("B2 cpu-only list on a gpu session",
     "if n_gpu_layers == 0 && cpu_dev_present != 0 {",
     "if n_gpu_layers != 0 && cpu_dev_present != 0 {"),
    ("B3 expert override on a cpu session",
     "m = m + LGA_MP_EXPERT_BUFT;",
     "m = m + LGA_MP_EXPERT_BUFT + LGA_MP_CPU_ONLY;"),
    ("B4 load_mtp inverted",
     "if is_mtp != 0 {\n        m = m + LGA_MP_LOAD_MTP;",
     "if is_mtp == 0 {\n        m = m + LGA_MP_LOAD_MTP;"),
    ("B5 prepare callback armed without moe",
     "if hot_slots > 0 && moe_enabled != 0 {",
     "if hot_slots > 0 || moe_enabled != 0 {"),
    ("B6 hot-plan refusal reports the load code",
     "return 2; // hot_slots_from_budget's own error, or \"exceeds\" (690-697)",
     "return 3; // hot_slots_from_budget's own error, or \"exceeds\" (690-697)"),
]

PPL_H1 = [
    ("A1 short-text gate narrowed",
     "if n_tokens < 2 {",
     "if n_tokens < 3 {"),
    ("A2 a text of exactly n_ctx is refused",
     "if n_tokens > n_ctx {",
     "if n_tokens >= n_ctx {"),
    ("A3 prefix ceiling lets the last token through",
     "if s > n - 1 {",
     "if s > n {"),
    ("A4 prefix floor removed",
     "if s < 1 {\n        return 1;",
     "if s < 1 {\n        return 0;"),
    ("A5 choice bound one token too wide",
     "if has_choices != 0 {\n        return n;",
     "if has_choices != 0 {\n        return n + 1;"),
    ("A6 skip boundary inclusive",
     "if pos < skip {\n        return 0;",
     "if pos <= skip {\n        return 0;"),
]

PPL_H2 = [
    ("B1 successor bound one short",
     "if pos + 1 >= n {\n        return 0;",
     "if pos + 1 > n {\n        return 0;"),
    ("B2 decode-regime pair loses its row",
     "out[k] = pos * 64;",
     "out[k] = pos * 64 + 1;"),
    ("B3 batch pair row shifted",
     "out[k] = (i + j) * 64 + j;",
     "out[k] = (i + j) * 64 + j + 1;"),
    ("B4 choice row remainder miscomputed",
     "let rem = n - (n / n_batch) * n_batch;",
     "let rem = n - (n / (n_batch + 1)) * n_batch;"),
    ("B5 closing gate accepts an empty pass",
     "if scored <= 0 && has_choices == 0 {",
     "if scored <= 0 || has_choices == 0 {"),
    ("B6 mean divides by one too many",
     "return nll / (scored * 1.0);",
     "return nll / ((scored + 1) * 1.0);"),
]

KVP_H1 = [
    ("A1 token list survives a new chat",
     "var m: i64 = KVP_CL_TARGET + KVP_CL_HISTORY + KVP_CL_TOKENS;",
     "var m: i64 = KVP_CL_TARGET + KVP_CL_HISTORY;"),
    ("A2 draft clear only without a draft context",
     "if has_draft != 0 {\n        m = m + KVP_CL_DRAFT;",
     "if has_draft == 0 {\n        m = m + KVP_CL_DRAFT;"),
    ("A3 reuse consumes the whole prompt",
     "return n_tokens - 1;",
     "return n_tokens;"),
    ("A4 ceiling compared too late",
     "if n >= max_common {\n            return n;",
     "if n > max_common {\n            return n;"),
    ("A5 current list read past its end",
     "if n >= cur.len {\n            return n;",
     "if n > cur.len {\n            return n;"),
    ("A6 a refused removal keeps the prefix",
     "action = KVP_RW_CLEAR;\n            n = 0;",
     "action = KVP_RW_CLEAR;\n            n = common;"),
]

KVP_H2 = [
    ("B1 draft rewind claimed without a draft",
     "if has_draft == 0 {\n        return 0;",
     "if has_draft == 0 {\n        return 1;"),
    ("B2 rollback keep point off by one",
     "keep = n_common;\n        tokens_len = n_common;",
     "keep = n_common + 1;\n        tokens_len = n_common;"),
    ("B3 user message not popped",
     "if history_pushed != 0 {\n            popped = 1;",
     "if history_pushed != 0 {\n            popped = 0;"),
    ("B4 raw turn keeps a token list",
     "action = KVP_RW_CLEAR;\n        keep = 0;\n        tokens_len = 0;",
     "action = KVP_RW_CLEAR;\n        keep = 0;\n        tokens_len = n_common;"),
    ("B5 prefill span grows with the prefix",
     "return n_prompt - n_common;",
     "return n_prompt + n_common;"),
    ("B6 speculative token joins the list",
     "if speculative != 0 {\n        return 0;",
     "if speculative != 0 {\n        return 1;"),
]


def pick(module, half):
    tables = {
        ("lga", "1"): LGA_H1, ("lga", "2"): LGA_H2,
        ("ppl", "1"): PPL_H1, ("ppl", "2"): PPL_H2,
        ("kvp", "1"): KVP_H1, ("kvp", "2"): KVP_H2,
    }
    if half == "all":
        return tables[(module, "1")] + tables[(module, "2")]
    return tables[(module, half)]


def run_one(module, name, old, new, idx):
    src_name, fixture = SLICES[module]
    src_path = os.path.join(ROOT, "source", src_name)
    src = open(src_path, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {module}/{name}: pattern occurs {src.count(old)} times")
        return False

    work = os.path.join(HERE, "build", f"mut-{module}-{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, src_name)
    with open(mutant, "w", encoding="utf-8") as f:
        f.write(src.replace(old, new))

    check = subprocess.run([GOOSE, "check", mutant], capture_output=True, text=True)
    if check.returncode != 0:
        print(f"SKIP {module}/{name}: goose check failed")
        return False

    env = dict(os.environ, GOOSE_SRC=mutant, OUT_DIR=work, OPT="-O0")
    build = subprocess.run(["bash", os.path.join(HERE, "build.sh"), module],
                           env=env, capture_output=True, text=True)
    caught = build.returncode != 0
    print(f"{'CATCH' if caught else 'MISS '} {module}/{name}")
    if not caught:
        for line in (build.stdout + build.stderr).splitlines()[-8:]:
            print("   ", line)
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", choices=["lga", "ppl", "kvp", "all"], default="all")
    ap.add_argument("--half", choices=["1", "2", "all"], default="all")
    a = ap.parse_args()

    modules = list(SLICES) if a.module == "all" else [a.module]
    total = caught = 0
    for module in modules:
        muts = pick(module, a.half)
        got = 0
        for i, (name, old, new) in enumerate(muts, 1):
            if run_one(module, name, old, new, i):
                got += 1
        print(f"{module}: mutations {got}/{len(muts)} caught")
        total += len(muts)
        caught += got
    print(f"mutations {caught}/{total} caught")
    return 0 if caught == total and total > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
