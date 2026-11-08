#!/usr/bin/env python3
"""Default-on exact signature replay for APX016/015 backend control.

Default: replace the in-scope function bodies in a chosen copy of
active-ggml-backend.cpp with calls to fg153_bc_* wrappers.
--native-rollback: write the original source unchanged (native rollback).
--dry-run: report only, write nothing.
Never edits the source in place; never runs a compiler. Missing exact
signature aborts without output.
"""
import argparse
import json
import re
import sys


def norm(s):
    t = re.sub(r"\s+", " ", s).strip()
    t = re.sub(r"\(\s+", "(", t)
    t = re.sub(r"\s+\)", ")", t)
    t = re.sub(r"\s+,", ",", t)
    return t


def match_brace(src, pos):
    depth = 0
    i = pos
    state = None
    while i < len(src):
        c = src[i]
        n = src[i + 1] if i + 1 < len(src) else ""
        if state is None:
            if c == "/" and n == "/":
                state = "line"; i += 2; continue
            if c == "/" and n == "*":
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
            if c == "*" and n == "/":
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


def locate(src, anchor, expect):
    for m in re.finditer(r"\b" + re.escape(anchor) + r"\s*\(", src):
        line = src.rfind("\n", 0, m.start()) + 1
        brace = src.find("{", m.end())
        if brace < 0:
            continue
        if norm(src[line:brace]).endswith(norm(expect)):
            close = match_brace(src, brace)
            if close < 0:
                return None
            return line, brace, close
    return None


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--anchors", required=True)
    ap.add_argument("--include", default="backend-replay-wrappers.h")
    ap.add_argument("--disable-flag", default="fg153_bc_enabled")
    ap.add_argument("--native-rollback", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    if args.source == args.output:
        print("ERROR: refusing in-place edit", file=sys.stderr)
        return 2

    src = open(args.source, encoding="utf-8").read()
    if args.native_rollback:
        if not args.dry_run:
            open(args.output, "w", encoding="utf-8").write(src)
        print("NATIVE-ROLLBACK written=%d" % (0 if args.dry_run else 1))
        return 0

    anchors = json.load(open(args.anchors, encoding="utf-8"))
    repl = []
    for e in anchors:
        loc = locate(src, e["anchor"], e["expect_signature"])
        if not loc:
            print("ABORT: signature mismatch %s" % e["anchor"], file=sys.stderr)
            return 3
        start, op, cl = loc
        body = e["body"].replace("{symbol}", e["symbol"]).replace("{args}", e["args"])
        if args.disable_flag:
            body = "if (!%s()) { %s } else { %s }" % (args.disable_flag, src[op + 1:cl], body)
        repl.append((op, cl, body))

    out = src
    for op, cl, body in sorted(repl, key=lambda r: r[0], reverse=True):
        out = out[:op] + "{ " + body + " }" + out[cl + 1:]

    inc = '#include "%s"' % args.include
    if inc not in out:
        out = insert_after_top_includes(out, inc)

    print("SUMMARY anchors=%d replaced=%d" % (len(anchors), len(repl)))
    if args.dry_run:
        print("DRY-RUN: no file written")
        return 0
    open(args.output, "w", encoding="utf-8").write(out)
    print("WROTE %s" % args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
