#!/usr/bin/env python3
"""P10-07 mutation table: six mutations of the Goose parser, six of the adapter.

Usage: python3 tests/mutate.py 1|2 [--jobs N]

Each mutation is applied to a private copy of source/ under build/mut/, the gate
is rebuilt at -O0, and the run must either exit non-zero (the in-process
native-vs-goose diff) or produce a transcript different from the unmutated one.
A mutation the gate does not see is a gate defect and fails the half.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

EXP = Path(__file__).resolve().parent.parent
SRC = EXP / "source"
WORK = EXP / "build" / "mut"
REPO = Path(os.environ.get("FG_REPO", "/home/end0/AI/FrankenMoE-CUDA"))
LLAMA = Path(os.environ.get("FG_LLAMA", REPO / "upstream" / "llama.cpp"))
GOOSE = Path(os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"))
CC = os.environ.get("CC", "gcc-13")
CXX = os.environ.get("CXX", "g++-13")
OPT = "-O0"

HALVES = {
    1: [
        ("P1", "json-parser.goose",
         "        if e_klen[e] == key.len {\n            if eqb(sbuf[e_koff[e] .. e_koff[e] + e_klen[e]], key) {\n                e_val[e] = val;\n                return e;\n            }\n        }",
         "        if e_klen[e] == key.len {\n            if eqb(sbuf[e_koff[e] .. e_koff[e] + e_klen[e]], key) {\n                return add_entry(node, val, koff, klen, e_val, e_koff, e_klen, e_next, jh, st);\n            }\n        }",
         "duplicate-key policy: replace instead of append", "09-dupkeys"),
        ("P2", "json-parser.goose",
         "    if st[4] < text.len {\n        err_next(text, st, E_PARSE);\n        return -1;\n    }",
         "    if st[4] < 0 {\n        err_next(text, st, E_PARSE);\n        return -1;\n    }",
         "trailing garbage accepted", "14-trailing"),
        ("P3", "json-parser.goose",
         "                    st[10] = put_utf8(sbuf, st[10], cp);",
         "                    st[10] = appc(sbuf, st[10], cp as u8);",
         "\\uXXXX written as one byte, not UTF-8", "07-escapes"),
        ("P4", "json-parser.goose",
         "        jl[id] = len;",
         "        jl[id] = len - 1;",
         "string length one short", "01-basic"),
        ("P5", "json-parser.goose",
         "    if c == (32 as u8) || c == (9 as u8) || c == (10 as u8) || c == (13 as u8) {",
         "    if c == (32 as u8) || c == (9 as u8) || c == (13 as u8) {",
         "newline not whitespace", "19-tabs"),
        ("P6", "json-parser.goose",
         "            jn[i] = cnt;",
         "            jn[i] = cnt - 1;",
         "last array/object child dropped", "18-last-match"),
    ],
    2: [
        ("A1", "goose-json-adapter.cpp",
         "bool json::is_array() const  { return node_ref().kind == detail::K_ARRAY; }",
         "bool json::is_array() const  { return node_ref().kind == detail::K_ARRAY || node_ref().kind == detail::K_OBJECT; }",
         "is_array also true for objects", "04-siblings-object"),
        ("A2", "goose-json-adapter.cpp",
         "    for (int i = 0; i < nd.ccount; ++i) {\n        const detail::entry & e = doc_->entries[(std::size_t) (nd.cbegin + i)];\n        if (key_eq(e, key)) {\n            return json(doc_, e.cnode);\n        }\n    }\n    return json();",
         "    if (nd.ccount > 0) {\n        return json(doc_, doc_->entries[(std::size_t) nd.cbegin].cnode);\n    }\n    return json();",
         "operator[](key) returns the first child", "01-basic"),
        ("A3", "goose-json-adapter.h",
         "        json operator*() const {\n            const detail::entry & e = doc_->entries[(std::size_t) pos_];\n            return json(doc_, e.cnode);\n        }",
         "        json operator*() const {\n            const detail::entry & e = doc_->entries[(std::size_t) (pos_ + 1)];\n            return json(doc_, e.cnode);\n        }",
         "array iterator skips the first element", "02-split"),
        ("A4", "goose-json-adapter.cpp",
         "    if (node_ref().kind != detail::K_STRING) {\n        type_error(\"string\", kind_name(node_ref().kind));\n    }\n    const detail::node & nd = node_ref();\n    return doc_->sbuf.substr((std::size_t) nd.soff, (std::size_t) nd.slen);",
         "    if (node_ref().kind != detail::K_STRING) {\n        return std::string();\n    }\n    const detail::node & nd = node_ref();\n    return doc_->sbuf.substr((std::size_t) nd.soff, (std::size_t) nd.slen);",
         "get<string> returns empty on a wrong type", "16-rfilename-number"),
        ("A5", "goose-json-adapter.cpp",
         "    return doc_->sbuf.substr((std::size_t) nd.soff, (std::size_t) nd.slen);",
         "    return doc_->sbuf.substr((std::size_t) (nd.soff + 1), (std::size_t) nd.slen);",
         "string offset one byte late", "01-basic"),
        ("A6", "goose-json-adapter.cpp",
         "    if (root < 0) {\n        throw parse_error((int) st[1], st[2], st[3]);\n    }",
         "    if (root < 0) {\n        return json();\n    }",
         "parse error becomes a null document", "13-bad-json"),
    ],
}


def sh(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def prepare() -> None:
    if WORK.exists():
        shutil.rmtree(WORK)
    WORK.mkdir(parents=True)
    for f in ("json-parser.goose", "goose-json-adapter.h", "goose-json-adapter.cpp",
              "hf-pick.h", "hf-pick.cpp"):
        shutil.copy2(SRC / f, WORK / f)
    shutil.copy2(EXP / "tests" / "fixture.cpp", WORK / "fixture.cpp")


def build(tag: str, goose_rebuilt: bool) -> Path:
    out = WORK / ("bin-" + tag)
    out.mkdir(exist_ok=True)
    gen_c = WORK / "json-parser-gen.c"
    gen_h = WORK / "json-parser-gen.h"
    if goose_rebuilt or not gen_c.exists():
        r = sh([str(GOOSE), "--standalone", "-o", str(gen_c), "--header", str(gen_h),
                str(WORK / "json-parser.goose")])
        if r.returncode != 0:
            raise SystemExit(f"goose failed: {r.stderr[-2000:]}")
        r = sh([CC, OPT, "-std=c11", "-c", str(gen_c), "-o", str(out / "gen.o")])
        if r.returncode != 0:
            raise SystemExit(f"cc failed: {r.stderr[-2000:]}")
    objs = [out / "gen.o"]
    common = ["-std=c++17", OPT, "-DFG_GOOSE_JSON_REPLACE=1",
              f"-I{WORK}", f"-I{LLAMA / 'vendor'}"]
    for name, src in (("hf-pick.o", WORK / "hf-pick.cpp"),
                      ("adapter.o", WORK / "goose-json-adapter.cpp"),
                      ("fixture.o", WORK / "fixture.cpp")):
        r = sh([CXX, *common, "-c", str(src), "-o", str(out / name)])
        if r.returncode != 0:
            raise SystemExit(f"c++ failed for {name}: {r.stderr[-2000:]}")
        objs.append(out / name)
    r = sh([CXX, OPT, *[str(o) for o in objs], "-o", str(out / "gate")])
    if r.returncode != 0:
        raise SystemExit(f"link failed: {r.stderr[-2000:]}")
    return out / "gate"


def run_gate(binary: Path) -> tuple[int, str]:
    r = sh([str(binary), "--dump", "--cases", str(EXP / "tests" / "cases")])
    return r.returncode, r.stdout + r.stderr


def half(n: int) -> int:
    mutations = HALVES[n]
    prepare()
    base_bin = build("base", goose_rebuilt=True)
    base_rc, base_dump = run_gate(base_bin)
    if base_rc != 0:
        raise SystemExit(f"baseline gate failed rc={base_rc}")
    print(f"baseline: rc=0 lines={len(base_dump.splitlines())}")

    detected = 0
    for mid, rel, old, new, why, hint in mutations:
        prepare()  # pristine copies
        path = WORK / rel
        text = path.read_text(encoding="utf-8")
        if text.count(old) != 1:
            raise SystemExit(f"{mid}: anchor occurs {text.count(old)} times in {rel}")
        path.write_text(text.replace(old, new), encoding="utf-8")
        try:
            binary = build(mid, goose_rebuilt=(rel == "json-parser.goose"))
            rc, dump = run_gate(binary)
        except SystemExit as exc:
            print(f"{mid} {why}: build failed ({exc}) -> detected")
            detected += 1
            continue
        seen = rc != 0 or dump != base_dump
        first = ""
        for line in dump.splitlines():
            if line.startswith("DIFF"):
                first = " " + line
                break
        if not first:
            for a, b in zip(base_dump.splitlines(), dump.splitlines()):
                if a != b:
                    first = " first-change: " + b
                    break
        print(f"{mid} {why}: {'detected' if seen else 'NOT DETECTED'} "
              f"(rc={rc}, expected {hint}){first}")
        detected += 1 if seen else 0

    total = len(mutations)
    print(f"FG665_MUTATE half={n} detected={detected}/{total}")
    return 0 if detected == total else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("half", type=int, choices=[1, 2])
    ap.add_argument("--jobs", type=int, default=1, help="accepted for the repo convention; runs are serial")
    args = ap.parse_args()
    return half(args.half)


if __name__ == "__main__":
    sys.exit(main())
