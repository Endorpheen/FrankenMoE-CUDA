#!/usr/bin/env python3
"""Mutation table for the P7-11 S4/S5 slices.

Twelve semantic mutations in two halves:

    half 1 (0..6)   source/log-policy.goose     -> FG_LOG must stop reporting
                                                   failures=0
    half 2 (6..12)  source/console-editing.goose -> FG_CONSOLE must stop
                                                   reporting failures=0

A mutation is CAUGHT when the gate for that module reports a non-zero failure
count, the run aborts, or the build fails. The gate keeps its -O1 native
oracle; the mutated port runs at -O0 (GATE_OPT).

Usage:
    tests/mutate.py            # both halves, sequential
    tests/mutate.py 0 6        # half 1
    tests/mutate.py 6 12       # half 2
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
G = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

LOG = "source/log-policy.goose"
CON = "source/console-editing.goose"

# (module, label, anchor, replacement)
HALF_1 = [
    (LOG, "the INFO verbosity becomes info instead of trace",
     "    if level == LP_INFO {\n        return LP_V_TRACE;",
     "    if level == LP_INFO {\n        return LP_V_INFO;"),
    (LOG, "the DEBUG stream gate also drops the threshold at 5",
     "if level == LP_DEBUG && thold < LP_DEFAULT_DEBUG {",
     "if level == LP_DEBUG && thold <= LP_DEFAULT_DEBUG {"),
    (LOG, "the reset is not emitted for DEBUG lines",
     "let reset = level == LP_WARN || level == LP_ERROR || level == LP_DEBUG;",
     "let reset = level == LP_WARN || level == LP_ERROR;"),
    (LOG, "the timestamp minute field divides by 61",
     "p = lp_put_uint(out, p, timestamp / 1000000 / 60, 1);",
     "p = lp_put_uint(out, p, timestamp / 1000000 / 61, 1);"),
    (LOG, "an empty debug filter set matches nothing",
     "    if count == 0 {\n        return 1;\n    }",
     "    if count == 0 {\n        return 0;\n    }"),
    (LOG, "the NaN abort fires on a finite sum instead",
     "if abort_on_nan == 1 && sum != sum {",
     "if abort_on_nan == 1 && sum == sum {"),
]

HALF_2 = [
    (CON, "a two-byte sequence advances three bytes",
     "        adv[0] = 2;\n        return ((c & 31) << 6) | (c1 & 63);",
     "        adv[0] = 3;\n        return ((c & 31) << 6) | (c1 & 63);"),
    (CON, "the ctrl-arrow modifier is 3 instead of 5",
     "if !bad && value == 5 {",
     "if !bad && value == 3 {"),
    (CON, "word-left treats the space run as the word",
     "        if con_is_space(cp) == 0 {\n            break;\n        }\n        move = move + con_width(cp);\n        cpos = cpos - 1;\n        bpos = prev;",
     "        if con_is_space(cp) == 1 {\n            break;\n        }\n        move = move + con_width(cp);\n        cpos = cpos - 1;\n        bpos = prev;"),
    (CON, "backspace moves the cursor right before erasing",
     "    con_cursor(0 - w, out, olen);\n    pos[0] = pos[0] - 1;",
     "    con_cursor(w, out, olen);\n    pos[0] = pos[0] - 1;"),
    (CON, "a mid-line insert leaves the cursor after the tail",
     "    con_cursor(0 - tail, out, olen);\n    pos[0] = pos[0] + 1;",
     "    con_cursor(tail, out, olen);\n    pos[0] = pos[0] + 1;"),
    (CON, "history keeps duplicates of different length only",
     "        if len[last] == tlen {",
     "        if len[last] != tlen {"),
]

M = HALF_1 + HALF_2
MODULE_LINE = {LOG: "FG_LOG", CON: "FG_CONSOLE"}


def run_gate(tree: Path, out_dir: Path):
    env = dict(os.environ, GATE_OPT="-O0", OPT="-O0", FG_SKIP_NATIVE="1",
               OUT_DIR=str(out_dir), GOOSE_BIN=G)
    try:
        r = subprocess.run(["bash", str(tree / "tests" / "build.sh")],
                           capture_output=True, text=True, timeout=300,
                           errors="replace", env=env)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    return r, (r.stdout or "") + (r.stderr or "")


def one(idx: int) -> dict:
    module, label, old, new = M[idx]
    orig = (S / module).read_text()
    if orig.count(old) != 1:
        return {"idx": idx, "label": label, "status": "ANCHOR",
                "detail": "anchor count %d" % orig.count(old)}

    tree = S / "build-mut" / ("m%02d" % idx)
    shutil.rmtree(tree, ignore_errors=True)
    (tree / "source").mkdir(parents=True)
    (tree / "tests").mkdir()
    for f in (S / "source").glob("*.goose"):
        shutil.copy2(f, tree / "source" / f.name)
    shutil.copy2(S / "tests" / "gen_corpus.py", tree / "tests" / "gen_corpus.py")
    shutil.copy2(S / "tests" / "build.sh", tree / "tests" / "build.sh")
    (tree / module).write_text(orig.replace(old, new, 1))

    r, out = run_gate(tree, S / "build")
    shutil.rmtree(tree, ignore_errors=True)

    target = MODULE_LINE[module]
    m = re.search(r"%s cases=(\d+) failures=(\d+)" % target, out)
    if r is not None and r.returncode == 0 and m and int(m.group(2)) == 0:
        return {"idx": idx, "label": label, "status": "MISSED",
                "detail": "%s failures=0" % target}
    if m:
        return {"idx": idx, "label": label, "status": "CAUGHT",
                "detail": "%s failures=%s" % (target, m.group(2))}
    tail = [x for x in out.strip().splitlines() if x.strip()]
    return {"idx": idx, "label": label, "status": "CAUGHT",
            "detail": "abort" + (": " + tail[-1][:120] if tail else "")}


def main() -> int:
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    (S / "build-mut").mkdir(exist_ok=True)
    rows = []
    for i in range(lo, hi):
        r = one(i)
        rows.append(r)
        print("[%s] %d %s: %s" % (r["status"], r["idx"], r["label"], r["detail"]), flush=True)
    caught = sum(1 for r in rows if r["status"] == "CAUGHT")
    missed = [r["idx"] for r in rows if r["status"] != "CAUGHT"]
    print("mutations %d/%d caught (range %d:%d) missed=%s" % (caught, hi - lo, lo, hi, missed))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
