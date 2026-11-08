#!/usr/bin/env python3
"""Hash/signature-bound replay for the llm_graph_context metadata patch.

Verifies that the patched llama-graph.cpp/.h were generated from the exact
input snapshot and the accepted Goose metadata module, and optionally runs the
synthetic constructor corpus. No model, no weights, no network.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent  # helpers/omo5
MANIFEST = HERE / "patch-manifest.json"
SCHEMA = "fg153.graph-metadata-patch/1"

REQUIRED_CPP = [
    "fg153_graph_meta_fill",
    "llm_graph_context(params, fg153_meta_for(params))",
    "llm_graph_context(const llm_graph_params & params, const fg153_graph_meta & meta)",
    "#ifndef FG153_NATIVE_ROLLBACK",
    "#else",
    "res->set_params(params);",
]
REQUIRED_H = [
    "struct fg153_graph_meta;",
    "llm_graph_context(const llm_graph_params & params, const fg153_graph_meta & meta);",
]


def sha256(p):
    h = hashlib.sha256()
    with p.open("rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def fail(msg, code=2):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(code)


def load():
    if not MANIFEST.is_file():
        fail("patch-manifest.json missing; run make_patched.py", code=4)
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def check():
    m = load()
    if m.get("schema") != SCHEMA:
        fail("unexpected manifest schema")
    base_cpp = ROOT / m["base_cpp"]
    base_h = ROOT / m["base_h"]
    patched_cpp = ROOT / m["patched_cpp"]
    patched_h = ROOT / m["patched_h"]
    for p in (base_cpp, base_h, patched_cpp, patched_h):
        if not p.is_file():
            fail(f"missing bound file: {p}")
    if sha256(base_cpp) != m["base_cpp_sha256"]:
        fail("active-llama-graph.cpp changed since the patch was generated", 3)
    if sha256(base_h) != m["base_h_sha256"]:
        fail("llama-graph.h changed since the patch was generated", 3)
    if sha256(patched_cpp) != m["patched_cpp_sha256"]:
        fail("patched llama-graph.cpp changed without regenerating", 3)
    if sha256(patched_h) != m["patched_h_sha256"]:
        fail("patched llama-graph.h changed without regenerating", 3)
    accepted = m.get("accepted_module", {})
    if not accepted:
        fail("manifest has no accepted-module binding")
    for name, digest in accepted.items():
        p = ROOT / "inputs/accepted-graph-result" / name
        if not p.is_file() or sha256(p) != digest:
            fail(f"accepted Goose module changed: {name}", 3)
    cpp = patched_cpp.read_text(encoding="utf-8")
    hdr = patched_h.read_text(encoding="utf-8")
    for s in REQUIRED_CPP:
        if s not in cpp:
            fail(f"patched cpp missing marker: {s}")
    for s in REQUIRED_H:
        if s not in hdr:
            fail(f"patched header missing marker: {s}")
    if cpp.count(m["constructor_anchor"]) < 1:
        fail("constructor anchor not found in patched cpp")
    print(f"OK: metadata patch bound to {len(accepted)} accepted files")
    return 0


def driver(binary):
    check()
    try:
        r = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as e:
        fail(f"cannot run corpus: {e}")
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    if r.returncode != 0 or '"status":"PASS"' not in r.stdout:
        fail("constructor corpus did not pass")
    print("OK: synthetic constructor corpus passed")
    return 0


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", action="store_true")
    g.add_argument("--driver")
    a = ap.parse_args()
    return check() if a.check else driver(a.driver)


if __name__ == "__main__":
    sys.exit(main())
