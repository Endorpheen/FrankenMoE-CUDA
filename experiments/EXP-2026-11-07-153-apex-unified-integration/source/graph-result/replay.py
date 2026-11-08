#!/usr/bin/env python3
"""worker-7: signature/hash-bound replay for the Apex graph-result/input block.

The native bodies are never deleted. Each owned method gets an early-return hook
that calls the strong Goose export; compiling with -DFG153_NATIVE_ROLLBACK
removes the hooks and restores the untouched native implementation.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_anchors(path: Path):
    data = json.loads(path.read_text())
    anchors = data.get("anchors")
    if not isinstance(anchors, list) or not anchors:
        raise SystemExit("anchors must be a non-empty list")
    for a in anchors:
        for key in ("name", "signature", "hook"):
            if not isinstance(a.get(key), str) or not a[key]:
                raise SystemExit(f"anchor missing {key}: {a!r}")
    return data, anchors


def locate(source: str, anchor) -> int:
    sig = anchor["signature"]
    first = source.find(sig)
    if first < 0:
        raise SystemExit(f"anchor signature not found: {anchor['name']}\n{sig}")
    if source.find(sig, first + 1) >= 0:
        raise SystemExit(f"anchor signature is not unique: {anchor['name']}\n{sig}")
    line_start = source.rfind("\n", 0, first) + 1
    if source[line_start:first].strip():
        raise SystemExit(f"anchor signature is not on its own line: {anchor['name']}")
    line_end = source.find("\n", first)
    if line_end < 0:
        line_end = len(source)
    line = source[first:line_end]
    brace_off = line.find("{")
    if brace_off < 0:
        raise SystemExit(f"anchor body brace not found on signature line: {anchor['name']}")
    if line[brace_off + 1:].strip():
        raise SystemExit(f"anchor body brace is not the end of the signature line: {anchor['name']}")
    return first + brace_off


def verify(source: str, anchors, expected_sha: str | None) -> None:
    digest = sha256_text(source)
    if expected_sha and digest != expected_sha:
        raise SystemExit(f"source sha256 mismatch: expected {expected_sha}, got {digest}")
    seen = set()
    for anchor in anchors:
        brace = locate(source, anchor)
        if brace in seen:
            raise SystemExit(f"two anchors share a body brace: {anchor['name']}")
        seen.add(brace)
        # signature-local hash so an edit to a single owned signature is detected
        sig_digest = sha256_text(anchor["signature"])
        if anchor.get("signature_sha256") and anchor["signature_sha256"] != sig_digest:
            raise SystemExit(f"signature sha256 mismatch for {anchor['name']}")
    print(f"verified {len(anchors)} anchors, source sha256={digest}")


def emit(source: str, anchors, include_after: str) -> str:
    if '#include "graph-bridge.h"' not in source:
        at = source.find(include_after)
        if at < 0:
            at = source.find("\n", source.find("#include", 0) if "#include" in source else 0)
        if at < 0:
            raise SystemExit("cannot locate include insertion point")
        line_end = source.find("\n", at)
        if line_end < 0:
            line_end = len(source)
        source = source[:line_end + 1] + '#include "graph-bridge.h"\n' + source[line_end + 1:]

    # re-locate after the include insertion
    inserts = []
    for anchor in anchors:
        brace = locate(source, anchor)
        inserts.append((brace + 1, anchor["hook"]))
    for pos, hook in sorted(inserts, reverse=True):
        source = source[:pos] + "\n" + hook + "\n" + source[pos:]
    return source


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["hash", "verify", "emit"])
    ap.add_argument("--source", required=True)
    ap.add_argument("--anchors", required=True)
    ap.add_argument("--expected-sha256")
    ap.add_argument("--output")
    args = ap.parse_args()

    src_path = Path(args.source)
    source = src_path.read_text()
    meta, anchors = load_anchors(Path(args.anchors))
    expected_sha = args.expected_sha256 or meta.get("source_sha256")

    if args.mode == "hash":
        print(sha256_text(source))
        return 0
    if args.mode == "verify":
        verify(source, anchors, expected_sha)
        return 0

    if not args.output:
        raise SystemExit("emit requires --output")
    verify(source, anchors, expected_sha)
    out = emit(source, anchors, '#include "llama-cparams.h"')
    Path(args.output).write_text(out)
    print(f"emitted {args.output} from {args.source}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
