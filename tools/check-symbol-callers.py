#!/usr/bin/env python3
"""Presence is not execution: does anything CALL the symbols that are exported?

EXP1003 corrected the overlap count from 0 to 12. That answered "what is in the
artifact" and said nothing about "what runs". A symbol can be exported, reachable by a
client, and called by nobody inside the library - which is exactly the shape rule 17
warns about, and the shape that made a bridge look alive for several cycles.

This reads the disassembly and reports, for each named symbol, whether some `call`
instruction targets it or its PLT stub.

Usage:
  tools/check-symbol-callers.py <library.so> [--only name1,name2] [--list]
Exit: 0 always (reports; it is evidence, not a gate)
"""
import argparse
import re
import subprocess
import sys

CALL = re.compile(r"\bcall\s+[0-9a-f]+\s+<([^>]+)>")


def disassemble(lib):
    """Instruction text only. Full disassembly of a 6 MB object is ~400k lines."""
    r = subprocess.run(["objdump", "-d", "--no-show-raw-insn", lib],
                       capture_output=True, text=True)
    return r.stdout


def call_targets(text):
    targets = set()
    for m in CALL.finditer(text):
        name = m.group(1)
        # "sym@plt" and "sym" both count; strip the version/offset decorations.
        name = name.split("@")[0]
        targets.add(name)
    return targets


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("library")
    ap.add_argument("--only", default="")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    text = disassemble(a.library)
    called = call_targets(text)
    print("call targets in %s: %d" % (a.library, len(called)))

    wanted = [s for s in a.only.split(",") if s]
    if not wanted:
        return 0

    alive, dead = [], []
    for s in wanted:
        (alive if s in called else dead).append(s)
    print("called: %d | exported but called by nobody: %d" % (len(alive), len(dead)))
    for s in alive:
        print("  CALLED %s" % s)
    if a.list:
        for s in dead:
            print("  UNCALLED %s" % s)
    else:
        for s in dead[:10]:
            print("  UNCALLED %s" % s)
        if len(dead) > 10:
            print("  ... %d more" % (len(dead) - 10))
    return 0


if __name__ == "__main__":
    sys.exit(main())
