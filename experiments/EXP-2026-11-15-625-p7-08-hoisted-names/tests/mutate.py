#!/usr/bin/env python3
# Mutation runner for the rebased tool-builder port. Each mutation is applied to a
# scratch copy of base/rebased.goose, rebuilt with goose and gcc -O0, then the
# structural gate reports which cases caught it. One build at a time.
import os, subprocess, sys, json
from pathlib import Path

ROOT = str(Path(__file__).resolve().parent.parent)
BASE = f"{ROOT}/source/runtime.goose"
WORK = f"{ROOT}/build/mut"
GEN = f"{WORK}/runtime-gen.c"
HDR = f"{WORK}/runtime-gen.h"
G = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
L = "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda"

MUT = [
    ("M01 constructed default start marker", "tool_call_start_marker\", \"<tool_call>\")", "tool_call_start_marker\", \"<toolcall>\")"),
    ("M02 constructed parallel one_or_more->zero_or_more", "mx, st, rep_body, 1, 0 - 1);", "mx, st, rep_body, 0, 0 - 1);"),
    ("M03 constructed arg name drops single quotes", "let names = peg_choice3(kind, cfirst, ccount, children, sidx, st, bare, dq, sq);", "let names = peg_choice2(kind, cfirst, ccount, children, sidx, st, bare, dq);"),
    ("M04 constructed param suffix uses param prefix", "let suffix = peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_suffix);", "let suffix = peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_prefix);"),
    ("M05 json digits CharRepeat min 1 -> 0", "st, \"[0-9]\", 1, 0 - 1);", "st, \"[0-9]\", 0, 0 - 1);"),
    ("M06 flat fields sorted descending", "if fkey[b - 1] > ki {", "if fkey[b - 1] < ki {"),
    ("M07 constructed tool-args tag renamed", "\"tool-args\", args);", "\"tool-arguments\", args);"),
    ("M08 function_is_key dispatch never taken", "if function_is_key != 0 {", "if function_is_key != 1 {"),
    ("M09 openai wrapper emits eps", "if accept_openai_wrapper == 0 {", "if accept_openai_wrapper == 1 {"),
    ("M10 nested name keeps the full dot key", "nested_name_field = key_field(eff_name_key);", "nested_name_field = eff_name_key;"),
    ("M11 array wrapper closes with a space", "peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, \"]\"));", "peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, \"] \"));"),
    ("M12 flat string call id loses the number branch", "                                            sb, call_id_key, 1);", "                                            sb, call_id_key, 0);"),
    ("M13 force=false still forced (first builder)", "    if force != 0 {", "    if force == 0 {"),
    ("M14 dump prints CharRepeat max unbounded literal", "        p = peg_app(out, p, \"unbounded\");", "        p = peg_app(out, p, \"unbounded!\");"),
    ("M15 json exponent reuses digits0 (dump-invisible)", "expo = peg_seq2(kind, cfirst, ccount, children, sidx, st, expo, digits);", "expo = peg_seq2(kind, cfirst, ccount, children, sidx, st, expo, digits0);"),
    # --- hoisted-name slice (GBNF parity) ------------------------------------
    ("M16 effective kind is the node id again", "    return kind[cur];\n}", "    return cur;\n}"),
    ("M17 seq always writes the separator", "            if wrote != 0 {\n                dst[p] = 32;\n                start = p + 1;\n            }", "            dst[p] = 32;\n            start = p + 1;"),
    ("M18 seq inverted group-paren test", "            if gb_need(ch, kind, children, cfirst) != 0 {", "            if gb_need(ch, kind, children, cfirst) == 0 {"),
    ("M19 seq treats an empty child as written", "            if q != body {", "            if q >= body {"),
    ("M20 choice always writes the separator", "            if wrote != 0 {\n                q = gb_w(dst, q, \" | \");\n            }", "            q = gb_w(dst, q, \" | \");"),
    ("M21 choice inverted group-paren test", "            if gb_eff(ch, kind, children, cfirst) == PEG_CHOICE {", "            if gb_eff(ch, kind, children, cfirst) == PEG_SEQ {"),
    ("M22 gb_need does not unwrap Tag/Atomic", "    if kind[id] == PEG_TAG || kind[id] == PEG_ATOMIC {\n        return gb_need(children[cfirst[id]], kind, children, cfirst);\n    }", "    if kind[id] == PEG_TAG || kind[id] == PEG_ATOMIC {\n        return 1;\n    }"),
    ("M23 gb_need never groups a seq/choice", "    if kind[id] == PEG_SEQ || kind[id] == PEG_CHOICE {\n        return 1;\n    }", "    if kind[id] == PEG_SEQ || kind[id] == PEG_CHOICE {\n        return 0;\n    }"),
    ("M24 schema emits its kind, not the rule name", "        let sid = mx[id];", "        let sid = sidx[id];"),
    ("M25 schema child is not collected", "        return gb_collect(children[cfirst[id]], kind, cfirst, ccount, children, sidx, dstart, dcount, pool, vis, rids, hids, meta);", "        return 0;"),
    ("M26 rule ref slot not allocated", "    return peg_node(kind, cfirst, ccount, sidx, st, PEG_REF, 0 - 1);", "    return 0 - 1;"),
    ("M27 arg-rule suffix built before the until", "    let val = peg_tag_impl(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st,\n                           \"tool-arg-value\",\n                           peg_until_marker(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_closer));\n    let suffix = peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_suffix);", "    let suffix = peg_literal_impl(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_suffix);\n    let val = peg_tag_impl(kind, cfirst, ccount, children, sidx, dstart, dcount, pool, st,\n                           \"tool-arg-value\",\n                           peg_until_marker(kind, cfirst, ccount, sidx, dstart, dcount, pool, st, param_closer));"),
]


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def gate(src_text, tag):
    with open(f"{WORK}/runtime.goose", "w") as f:
        f.write(src_text)
    r = run(f"timeout 300 {G} --standalone -o {GEN} --header {HDR} {WORK}/runtime.goose")
    if r.returncode != 0:
        return None, "goose-fail"
    r = run(f"timeout 300 gcc-13 -O0 -std=c11 -w -c {GEN} -o {WORK}/gen.o")
    if r.returncode != 0:
        return None, "gcc-fail"
    r = run(f"g++-13 {ROOT}/build/fixture.o {WORK}/gen.o -L{L}/build-cuda/bin "
            f"-lllama-common -Wl,-rpath,{L}/build-cuda/bin -o {WORK}/{tag}")
    if r.returncode != 0:
        return None, "link-fail"
    r = run(f"timeout 300 {WORK}/{tag}")
    fails = [ln.split()[0] for ln in r.stdout.splitlines()
             if ("FAIL" in ln or "gbnf=DIFF" in ln) and not ln.startswith("cases=")]
    tail = [ln for ln in r.stdout.splitlines() if ln.startswith("cases=")]
    return fails, (tail[0] if tail else r.stdout[-200:])


def main():
    lo, hi = int(sys.argv[1]), int(sys.argv[2])
    os.makedirs(WORK, exist_ok=True)
    src = open(BASE).read()
    out = []
    for idx in range(lo, hi):
        name, needle, repl = MUT[idx]
        if src.count(needle) == 0:
            out.append({"id": f"M{idx+1:02d}", "mutation": name, "status": "NOT-APPLIED"})
            print(json.dumps(out[-1]), flush=True)
            continue
        fails, tail = gate(src.replace(needle, repl, 1), f"m{idx+1:02d}")
        status = "caught" if (fails is not None and len(fails) > 0) else ("missed" if fails is not None else tail)
        out.append({"id": f"M{idx+1:02d}", "mutation": name, "status": status,
                    "failing_cases": fails or [], "gate": tail})
        print(json.dumps(out[-1]), flush=True)
    path = f"{ROOT}/build/mutations_h{lo+1}_{hi}.json"
    with open(path, "w") as f:
        json.dump(out, f, indent=1)
    print("wrote", path)


main()
