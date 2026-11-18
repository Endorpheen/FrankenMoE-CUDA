#!/usr/bin/env python3
"""Find bridge objects linked into the DSO that no object references at all.

EXP1001 measured 169 dead bridge *symbols*. This asks the coarser and stronger question:
is there an object whose every export is uncalled? Such an object is pure weight - it
cannot serve any configuration of this build - and wording it as "dead code of unknown
provenance" is what rule 28 tells us not to do.

Two objects in this build provide the SAME decision under different names
(fg-q35.o is generated from the canonical unit, fg-q35-bridge.o from the frozen
experiment). That is the shape the unwired-module check found in the source tree; this is
the same check at the object level, where it decides what the shipped binary contains.

Usage:
  tools/check-dead-objects.py <build-dir> [--list]
Exit: 0 no fully dead object, 1 at least one
"""
import argparse
import os
import subprocess
import sys


def is_bridge(path):
    b = os.path.basename(path)
    return (b[0] == "b" and b[1].isdigit()) or b.startswith(("fg", "fx"))


def defined(path):
    out = subprocess.run(["nm", "-g", "--defined-only", path],
                         capture_output=True, text=True).stdout
    return {l.split()[-1] for l in out.splitlines() if l.strip()}


def undefined(path):
    out = subprocess.run(["nm", "-u", path], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in out.splitlines() if l.strip()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    objs = [os.path.join(a.build_dir, f) for f in os.listdir(a.build_dir)
            if f.endswith(".o")]
    if not objs:
        print("no objects in %s" % a.build_dir)
        return 1

    needed = set()
    for o in objs:
        needed |= undefined(o)

    dead_objects, partial = [], []
    for o in objs:
        if not is_bridge(o):
            continue
        syms = defined(o)
        if not syms:
            continue
        dead = {s for s in syms if s not in needed}
        if len(dead) == len(syms):
            dead_objects.append((os.path.basename(o), len(syms)))
        elif dead:
            partial.append((os.path.basename(o), len(dead), len(syms)))

    print("objects: %d | wholly dead bridge objects: %d | partly dead: %d"
          % (len(objs), len(dead_objects), len(partial)))
    if a.list:
        for n, k in sorted(partial):
            print("  partial %-24s %2d of %2d exports uncalled" % (n, k, k))
    for n, k in sorted(dead_objects):
        print("DEAD %s: all %d exports are uncalled" % (n, k))
    if dead_objects:
        print("result: %d object(s) linked but serving nothing" % len(dead_objects))
        return 1
    print("PASS every linked bridge object has at least one caller")
    return 0


if __name__ == "__main__":
    sys.exit(main())
