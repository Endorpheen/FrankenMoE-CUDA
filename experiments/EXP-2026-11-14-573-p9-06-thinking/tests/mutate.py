#!/usr/bin/env python3
"""Mutation gate for P9-06 S3.

12 textual mutations of think.goose, six in each half. Each mutant must pass
`goose check`, then the fixture must fail.

    tests/mutate.py --half 1|2|all
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "think.goose")
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 flag-diff",
     "} else if !tc_eq(on, off) {",
     "} else if tc_eq(on, off) {"),
    ("A2 owns-span search",
     "let owns_span = start_tag.len > 0 && tc_find(source, 0, start_tag) >= 0;",
     "let owns_span = start_tag.len > 0 && tc_find(source, 0, start_tag) < 0;"),
    ("A3 prefill-lands",
     "if close_end >= 0 && tc_first_non_ws(prefilled, close_end) >= 0 {",
     "if close_end >= 0 && tc_first_non_ws(prefilled, close_end) < 0 {"),
    ("A4 span-closed",
     "return if reopen < 0 { 1 } else { 0 };",
     "return if reopen < 0 { 0 } else { 1 };"),
    ("A5 structural branch",
     "} else if !tc_eq(prefilled, off) && start_tag.len == 0 {",
     "} else if !tc_eq(prefilled, off) && start_tag.len > 0 {"),
    ("A6 name prefill",
     'return "prefill";',
     'return "prefil";'),
]

HALF2 = [
    ("B1 content empty",
     "clen[n] = 0;        // content stays empty",
     "clen[n] = 1;        // content stays empty"),
    ("B2 reasoning marker",
     "has_reason[n] = 1;  // reasoning_content is non-empty whitespace",
     "has_reason[n] = 0;  // reasoning_content is non-empty whitespace"),
    ("B3 continuation",
     "flags[TC_FLAG_CONTINUATION] = TC_CONT_CONTENT;",
     "flags[TC_FLAG_CONTINUATION] = TC_CONT_AUTO;"),
    ("B4 reasoning length",
     "return 2;\n}",
     "return 1;\n}"),
    ("B5 second newline",
     "out[1] = 10;",
     "out[1] = 32;"),
    ("B6 assistant role",
     "roles[n] = 1;       // assistant",
     "roles[n] = 0;       // assistant"),
]


def run_one(name, old, new, idx):
    src = open(SRC, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {name}: pattern occurs {src.count(old)} times")
        return False
    work = os.path.join(HERE, "build", f"mut{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, "think.goose")
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
        for line in (build.stdout + build.stderr).splitlines()[-8:]:
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
