#!/usr/bin/env python3
"""Apply the worker-8 NUMA affinity hook to a copy of the real ggml-cpu.c.

Reads inputs/native/ggml/src/ggml-cpu/ggml-cpu.c, inserts a default-on
dispatch at the top of ggml_numa_init that runs numa-init.goose, copies the
typed result back into g_state.numa, and keeps the original Linux enumeration
as an explicit rollback (GGML_GOOSE_CPU_NUMA=0).  Writes the candidate source
and a hash-bound report into worker-8/threadpool/.

This script never edits shared inputs and never invents symbol names.  A
missing anchor or unbalanced body aborts without writing.

Usage:
  python3 replay-numa-init.py [--check]
"""

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path("/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/helpers/omo5")
SOURCE = ROOT / "inputs" / "native" / "ggml" / "src" / "ggml-cpu" / "ggml-cpu.c"
OUTDIR = ROOT / "worker-8" / "threadpool"
OUT = OUTDIR / "ggml-cpu.numa-closured.c"
REPORT = OUTDIR / "numa-hook-report.json"

ANCHOR = "ggml_numa_init"
SIGNATURE = "void ggml_numa_init(enum ggml_numa_strategy numa_flag)"
MARKER = "/* worker-8 threadpool NUMA hook */"
INCLUDE = '#include "numa-init-bridge.h" ' + MARKER

DISPATCH = """\
    if (fg153_numa_enabled()) {
        struct fg153_numa_result r;
        memset(&r, 0, sizeof(r));
        int code = (int) fg153_numa_run((intptr_t) &r, (intptr_t) numa_flag, (intptr_t) g_state.numa.n_nodes);
        if (code == FG153_NUMA_ALREADY) {
            return;
        }
        g_state.numa.numa_strategy = (enum ggml_numa_strategy) r.strategy;
        g_state.numa.total_cpus    = r.total_cpus;
        g_state.numa.current_node  = r.current_node;
        g_state.numa.cpuset        = r.cpuset;
        if (code == FG153_NUMA_OK) {
            g_state.numa.n_nodes = r.n_nodes;
            for (uint32_t n = 0; n < r.n_nodes; ++n) {
                g_state.numa.nodes[n].n_cpus = r.nodes[n].n_cpus;
                for (uint32_t c = 0; c < r.nodes[n].n_cpus; ++c) {
                    g_state.numa.nodes[n].cpus[c] = r.nodes[n].cpus[c];
                }
            }
        } else {
            g_state.numa.n_nodes = 0;
        }
        return;
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    src = SOURCE.read_text(encoding="utf-8")
    report = {
        "source": str(SOURCE),
        "source_sha256": hashlib.sha256(src.encode()).hexdigest(),
        "anchor": ANCHOR,
        "signature": SIGNATURE,
        "marker": MARKER,
        "hook_default": "on",
        "rollback": "GGML_GOOSE_CPU_NUMA=0",
        "candidate": str(OUT),
    }

    if MARKER in src:
        report["status"] = "ALREADY_APPLIED"
        print(json.dumps(report, indent=2))
        return 0

    pos = src.find(SIGNATURE)
    if pos < 0:
        report["status"] = "FAILED"
        report["error"] = "signature anchor not found"
        print(json.dumps(report, indent=2))
        return 3

    brace = src.find("{", pos + len(SIGNATURE))
    if brace < 0:
        report["status"] = "FAILED"
        report["error"] = "opening brace not found"
        print(json.dumps(report, indent=2))
        return 3
    close = match_brace(src, brace)
    if close < 0:
        report["status"] = "FAILED"
        report["error"] = "unbalanced body"
        print(json.dumps(report, indent=2))
        return 3

    out = src[:pos] + INCLUDE + "\n" + src[pos:brace] + "{\n" + DISPATCH + src[brace + 1:]

    checks = {
        "hook_present": "fg153_numa_run((intptr_t) &r" in out,
        "gate_present": "fg153_numa_enabled()" in out,
        "include_present": INCLUDE in out,
        "rollback_body_present": "#if defined(__gnu_linux__)" in out and "g_state.numa.n_nodes = 0;" in out,
        "single_function": out.count("void " + ANCHOR + "(") == 1,
    }
    report["checks"] = checks
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
