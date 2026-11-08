#!/usr/bin/env python3
"""Apply the worker-8 allocator closure hook to a copy of active-ggml-alloc.c.

This script never edits shared inputs.  It reads inputs/active-ggml-alloc.c,
replaces the owned size loop of ggml_gallocr_reserve_n_size with a default-on
dispatch to the generated Goose closure, keeps the native loop as an explicit
rollback (GGML_GOOSE_ALLOC_CLOSURE=0), and writes the candidate source plus a
machine-readable report into worker-8/.

It performs exact anchor/signature checks.  A missing anchor or an unbalanced
body aborts without writing.  Running it twice is a no-op (marker detected).

Usage:
  python3 replay-allocator-closure.py            # write candidate + report
  python3 replay-allocator-closure.py --check     # verify only
"""

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path("/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/helpers/omo5")
SOURCE = ROOT / "inputs" / "active-ggml-alloc.c"
OUT = ROOT / "worker-8" / "active-ggml-alloc.closured.c"
REPORT = ROOT / "worker-8" / "hook-report.json"

ANCHOR = "ggml_gallocr_reserve_n_size"
SIGNATURE = (
    "void ggml_gallocr_reserve_n_size( "
    "ggml_gallocr_t galloc, struct ggml_cgraph * graph, "
    "const int * node_buffer_ids, const int * leaf_buffer_ids, size_t * sizes)"
)
MARKER = "/* worker-8 allocator closure hook */"

INCLUDE_LINE = '#include "allocator-closure-bridge.h" ' + MARKER + "\n"

DISPATCH = """\
    GGML_ASSERT(ggml_gallocr_reserve_n_impl(galloc, graph, node_buffer_ids, leaf_buffer_ids, /*no_alloc =*/ true));
    if (fg153_alloc_closure_enabled()) {
        fg153_alloc_closure(0, (intptr_t) galloc, (intptr_t) sizes, 0);
        return;
    }
    for (int i = 0; i < galloc->n_buffers; i++) {
        sizes[i] = 0;
        for (int c = 0; c < galloc->buf_tallocs[i]->n_chunks; c++) {
            sizes[i] += galloc->buf_tallocs[i]->chunks[c]->max_size;
        }
    }
"""


def match_brace(src, open_pos):
    depth = 0
    i = open_pos
    n = len(src)
    state = None
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                state = "line"; i += 2; continue
            if c == "/" and nxt == "*":
                state = "block"; i += 2; continue
            if c == '"':
                state = "str"; i += 1; continue
            if c == "'":
                state = "chr"; i += 1; continue
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return i
        elif state == "line":
            if c == "\n":
                state = None
        elif state == "block":
            if c == "*" and nxt == "/":
                state = None; i += 2; continue
        elif state == "str":
            if c == "\\":
                i += 2; continue
            if c == '"':
                state = None
        elif state == "chr":
            if c == "\\":
                i += 2; continue
            if c == "'":
                state = None
        i += 1
    return -1


def normalize(text):
    return " ".join(text.split())


def locate(src):
    pos = src.find("void " + ANCHOR + "(")
    if pos < 0:
        return None, "anchor not found"
    brace = src.find("{", pos)
    if brace < 0:
        return None, "opening brace not found"
    sig = src[pos:brace]
    if normalize(sig) != normalize(SIGNATURE):
        return None, "signature mismatch: " + normalize(sig)
    close = match_brace(src, brace)
    if close < 0:
        return None, "unbalanced body"
    return (pos, brace, close), None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    src = SOURCE.read_text(encoding="utf-8")
    report = {
        "source": str(SOURCE),
        "source_sha256": hashlib.sha256(src.encode()).hexdigest(),
        "anchor": ANCHOR,
        "marker": MARKER,
        "rollback": "GGML_GOOSE_ALLOC_CLOSURE=0",
        "hook_default": "on",
        "residual_native_loop": "kept as explicit rollback only",
    }

    if MARKER in src:
        report["status"] = "ALREADY_APPLIED"
        print(json.dumps(report, indent=2))
        return 0

    loc, err = locate(src)
    if not loc:
        report["status"] = "FAILED"
        report["error"] = err
        print(json.dumps(report, indent=2))
        return 3

    pos, brace, close = loc
    out = src[:pos] + INCLUDE_LINE + "\n" + src[pos:brace] + "{\n" + DISPATCH + "}" + src[close + 1:]

    # Verify invariants in the produced text.
    checks = {
        "hook_present": "fg153_alloc_closure(0, (intptr_t) galloc" in out,
        "gate_present": "fg153_alloc_closure_enabled()" in out,
        "include_present": INCLUDE_LINE.strip() in out,
        "fallback_loop_present": "sizes[i] += galloc->buf_tallocs[i]->chunks[c]->max_size;" in out,
        "single_function": out.count("void " + ANCHOR + "(") == 1,
    }
    report["checks"] = checks
    report["output"] = str(OUT)
    report["output_sha256"] = hashlib.sha256(out.encode()).hexdigest()

    if not all(checks.values()):
        report["status"] = "FAILED"
        print(json.dumps(report, indent=2))
        return 4

    report["status"] = "CHECK_PASS" if args.check else "WROTE"
    print(json.dumps(report, indent=2))
    if not args.check:
        OUT.write_text(out, encoding="utf-8")
        REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
