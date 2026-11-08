#!/usr/bin/env python3
"""Replay worker-1 Goose graph functions into a chosen copy of ggml.c.

This script performs exact, signature-checked body replacement only. It never
runs a compiler, never executes the result, never edits the source in place,
and never invents symbol names.

Usage:
  python3 replay.py --ggml path/to/ggml.c --anchors replay-anchors.json \
      --symbols replay-symbols.json [--output path/to/ggml.replayed.c] [--write]

Without --write this is a dry run that reports what would change.
--symbols must map each anchor to the emitter-produced symbol and a body
template; anchors without a mapping are reported UNRESOLVED and left intact.
The body template may use {symbol} and {args} and must preserve the original
signature (only the brace body is replaced). A legacy {bind_stmt} placeholder
is substituted with an empty string; typed externs need no registration.
"""

import argparse
import json
import re
import sys


def normalize(text):
    s = re.sub(r"\s+", " ", text).strip()
    s = re.sub(r"\(\s+", "(", s)
    s = re.sub(r"\s+\)", ")", s)
    s = re.sub(r"\s+,", ",", s)
    return s


def find_function(src, anchor, expect_signature):
    """Return (sig_start, body_open, body_close) for an exact signature."""
    pat = re.compile(r"\b" + re.escape(anchor) + r"\s*\(")
    matches = list(pat.finditer(src))
    if not matches:
        return None, "signature not found"
    want = normalize(expect_signature)
    for m in matches:
        # Locate the start of the declaration line(s) and the opening brace.
        line_start = src.rfind("\n", 0, m.start()) + 1
        brace = src.find("{", m.end())
        if brace < 0:
            continue
        sig = src[line_start:brace]
        if normalize(sig).endswith(normalize(expect_signature)):
            close = match_brace(src, brace)
            if close < 0:
                return None, "unbalanced braces"
            return (line_start, brace, close), None
    return None, "signature text did not match expected declaration"


def match_brace(src, open_pos):
    depth = 0
    i = open_pos
    n = len(src)
    state = None  # None, 'line', 'block', 'str', 'chr'
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


def insert_after_top_includes(src, block):
    lines = src.splitlines(keepends=True)
    last = 0
    for i, line in enumerate(lines):
        s = line.strip()
        if not s or s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        if s.startswith("#if"):
            break
        if s.startswith("#include"):
            last = i + 1
            continue
        if s.startswith("#"):
            continue
        break
    return "".join(lines[:last]) + block + "\n" + "".join(lines[last:])


def render_body(template, symbol, args, bind):
    # Typed externs need no graph registration. {bind_stmt} is kept as an
    # empty substitution so stale templates fail safe rather than emitting
    # an obsolete registry call.
    del bind
    return template.replace("{symbol}", symbol).replace("{args}", args).replace("{bind_stmt}", "")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ggml", required=True)
    ap.add_argument("--anchors", required=True)
    ap.add_argument("--symbols", required=True)
    ap.add_argument("--output")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--include", help="include line inserted after the last #include")
    ap.add_argument("--disable-flag", default="fg153_goose_enabled",
                    help="runtime default-on flag; original body runs when it returns 0")
    args = ap.parse_args()

    with open(args.ggml, "r", encoding="utf-8") as f:
        src = f.read()
    with open(args.anchors, "r", encoding="utf-8") as f:
        anchors = json.load(f)
    with open(args.symbols, "r", encoding="utf-8") as f:
        symbols = json.load(f)

    if not isinstance(anchors, list) or not isinstance(symbols, dict):
        print("ERROR: anchors must be a list, symbols must be an object", file=sys.stderr)
        return 2

    replacements = []
    unresolved = []
    missing = []
    for entry in anchors:
        anchor = entry["anchor"]
        expect = entry["expect_signature"]
        spec = symbols.get(anchor)
        if not spec or not spec.get("symbol") or not spec.get("body"):
            unresolved.append(anchor)
            continue
        loc, err = find_function(src, anchor, expect)
        if not loc:
            missing.append((anchor, err))
            continue
        start, open_pos, close_pos = loc
        body = render_body(spec["body"], spec["symbol"], entry.get("args", ""), spec.get("bind_graph", ""))
        original = src[open_pos + 1:close_pos]
        if args.disable_flag:
            body = "if (!%s()) { %s } else { %s }" % (args.disable_flag, original, body)
        replacements.append((anchor, start, open_pos, close_pos, body, spec.get("declaration", "")))

    for anchor, err in missing:
        print("MISSING anchor=%s reason=%s" % (anchor, err))
    for anchor in unresolved:
        print("UNRESOLVED anchor=%s (no emitter symbol/body mapping)" % anchor)

    if missing:
        print("ABORT: exact signature match failed for at least one anchor; no output written", file=sys.stderr)
        return 3

    out = src
    # Apply from the end so earlier offsets stay valid.
    for anchor, start, open_pos, close_pos, body, decl in sorted(replacements, key=lambda r: r[2], reverse=True):
        out = out[:open_pos] + "{ " + body + " }" + out[close_pos + 1:]

    decls = sorted({r[5] for r in replacements if r[5]})
    if args.include:
        decls = ['#include "%s"' % args.include] + decls
    if replacements and decls:
        out = insert_after_top_includes(out, "\n".join(decls))

    print("SUMMARY anchors=%d replaced=%d unresolved=%d" %
          (len(anchors), len(replacements), len(unresolved)))

    if not args.write:
        print("DRY-RUN: no file written (use --write --output PATH)")
        return 0

    if not args.output:
        print("ERROR: --write requires --output", file=sys.stderr)
        return 2
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(out)
    print("WROTE %s" % args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
