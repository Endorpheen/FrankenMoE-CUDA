#!/usr/bin/env python3
"""How many of the composed unit's exported functions exist in a library.

This replaces an inline heredoc in migration-status.sh whose regex silently matched
nothing: a `\\b` in a non-raw heredoc became a backspace character, so the pattern
required a literal 0x08 and the report printed "0 of the header's functions" while 12
were actually present. A number that is wrong in the reassuring direction is worse than
no number, so the measurement moved into a file where it can be tested.

Usage:
  tools/unit-overlap.py <library.so> <header.h> [--list]
Exit: 0 always; prints "N|name,name,..."
"""
import re
import subprocess
import sys

# A declaration ends in ';' and has a parenthesised parameter list. Written as a raw
# string so no escape can be reinterpreted by whatever includes this file.
DECL = re.compile(r"^\s*(?:[A-Za-z_]\w*[\s*]+)+(\w+)\s*\([^;]*\)\s*;\s*$", re.M)


def header_names(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return set(DECL.findall(f.read()))
    except OSError:
        return set()


def defined(lib):
    out = subprocess.run(["nm", "-D", "--defined-only", lib],
                         capture_output=True, text=True).stdout
    return {l.split()[-1] for l in out.splitlines() if l.strip()}


def main():
    if len(sys.argv) < 3:
        print("usage: unit-overlap.py <library.so> <header.h> [--list]", file=sys.stderr)
        return 2
    names = header_names(sys.argv[2])
    have = defined(sys.argv[1])
    shared = sorted(names & have)
    print("%d|%s" % (len(shared), ",".join(shared)))
    if "--list" in sys.argv:
        for n in shared:
            print("  %s" % n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
