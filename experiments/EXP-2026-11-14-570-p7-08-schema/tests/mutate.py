#!/usr/bin/env python3
"""Mutation runner for the integrated schema converter.

Builds are serial: one mutation at a time, then the fixture. On the gate:
    python3 tests/mutate.py --half first
    python3 tests/mutate.py --half second
"""
from pathlib import Path
import argparse
import subprocess
import sys

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "source" / "format-grammar.goose"
BUILD = HERE / "build.sh"
ORIG = SRC.read_text()

M = [
    # first half 0:6
    ("identical rule text is not reused",
     "if found >= 0 && slice_eq(buf, toff[found], tlen[found], text_off, text_len) {",
     "if false && slice_eq(buf, toff[found], tlen[found], text_off, text_len) {"),
    ("primitive text length is an end offset",
     "    let tl = end - at;",
     "    let tl = end;"),
    ("space text is one byte too long",
     "    let sl = end - at;",
     "    let sl = end - at + 1;"),
    ("pattern wrapper reads one byte past its literal",
     '    var p = app(buf, at, "\\"\\\\\\"\\" (", 0, 6);',
     '    var p = app(buf, at, "\\"\\\\\\"\\" (", 0, 7);'),
    ("pattern literal is not flushed on break",
     """            if have_lit && st[ST_PSTAT] == 0 {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }

            if !consumed && st[ST_PSTAT] == 0 {""",
     """            if false && st[ST_PSTAT] == 0 {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }

            if !consumed && st[ST_PSTAT] == 0 {"""),
    ("trie rejects share one depth slot",
     "    let rb = 1024 + pre_len * 512;",
     "    let rb = 1024;"),

    # second half 6:12
    ("allOf enum keeps the complement of the intersection",
     "        if total == vn {",
     "        if total != vn {"),
    ("allOf anyOf alternatives become required",
     "                allof_collect(vals[vf + i], 0, 0, meta, props, vals, mprops, st);",
     "                allof_collect(vals[vf + i], 1, 0, meta, props, vals, mprops, st);"),
    ("union alternatives lose the alternative- prefix",
     """            if name.len == 0 {
                ap = app(buf, ap, "alternative-", 0, 12);
            } else {""",
     """            if false {
                ap = app(buf, ap, "alternative-", 0, 12);
            } else {"""),
    ("tuple separator loses a byte",
     '                p = app(buf, p, " \\",\\" space ", 0, 11);',
     '                p = app(buf, p, " \\",\\" space ", 0, 10);'),
    ("time format uses the date-string rule",
     '            prim = "time-string";',
     '            prim = "date-string";'),
    ("integer bounds are swapped",
     "        p = build_mm_at(buf, p, mn, mx, 16, 1, sbuf, ast);",
     "        p = build_mm_at(buf, p, mx, mn, 16, 1, sbuf, ast);"),
]


def run_fixture():
    try:
        r = subprocess.run(
            ["bash", str(BUILD)],
            text=True, capture_output=True, timeout=900, errors="replace",
        )
    except subprocess.TimeoutExpired:
        return True, "timeout"
    line = [x for x in r.stdout.splitlines() if x.startswith("FG573_SCHEMA")]
    if line:
        caught = "failures=0" not in line[-1] or r.returncode != 0
        return caught, line[-1]
    last = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "no stderr"
    return True, "build/run failure: " + last[:160]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["first", "second"], required=True)
    args = ap.parse_args()
    lo, hi = (0, 6) if args.half == "first" else (6, 12)

    caught = 0
    try:
        for i in range(lo, hi):
            label, old, new = M[i]
            if old not in ORIG:
                print(f"[skip] {i} {label}: pattern not found")
                continue
            SRC.write_text(ORIG.replace(old, new, 1))
            ok, detail = run_fixture()
            caught += ok
            print(f'[{"caught" if ok else "MISSED"}] {i} {label}: {detail}')
    finally:
        SRC.write_text(ORIG)
    print(f"{args.half} {caught}/{hi - lo} caught")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
