#!/usr/bin/env python3
"""Which ported modules actually reach the shipped library.

A module can be written, verified in isolation, listed in a build script and still not be part of the
runtime: the GGUF reader and the detokenizer were both built and called by nothing (EXP-2026-11-19-1314).
This reads the repository for `export fn` names and the built libraries for their symbols, and reports
per module how many of its doors are present.

Present is not the same as called - that is the next question, and the card for each module answers it.
"""
import re
import subprocess
import sys
import pathlib
from collections import defaultdict

BIN = sys.argv[1] if len(sys.argv) > 1 else "builds/flint-acc/bin"
ROOT = pathlib.Path(__file__).resolve().parent.parent

def library_symbols():
    syms = set()
    for so in pathlib.Path(BIN).glob("*.so*"):
        out = subprocess.run(f"nm -D --defined-only {so}", shell=True, capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[1] in ("T", "W"):
                syms.add(parts[2])
    return syms

def main():
    lib = library_symbols()
    if not lib:
        print(f"check-module-coverage: no symbols under {BIN}")
        return 2
    rows = []
    for f in sorted((ROOT / "goose").rglob("*.goose")):
        if "_archive" in f.parts:
            continue
        # staged copies are duplicates of a module that is counted where it lives
        if any("staged" in part for part in f.parts):
            continue
        names = re.findall(r"export fn ([A-Za-z_][A-Za-z0-9_]*)", f.read_text(errors="ignore"))
        if not names:
            continue
        present = [n for n in names if n in lib]
        rows.append((len(present), len(names), str(f.relative_to(ROOT))))
    rows.sort(reverse=True)
    wired = sum(1 for p, n, _ in rows if p == n)
    partial = sum(1 for p, n, _ in rows if 0 < p < n)
    none = sum(1 for p, n, _ in rows if p == 0)
    print(f"{'in lib / exports':>18}  module")
    for p, n, name in rows[:20]:
        print(f"{p:>10} / {n:<6}  {name}")
    print()
    print(f"modules with all doors in the library: {wired}")
    print(f"modules with some doors:               {partial}")
    print(f"modules with no doors in the library:  {none}")
    print(f"shipped symbols: {len(lib)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
