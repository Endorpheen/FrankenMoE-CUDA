#!/usr/bin/env python3
"""Which bridge symbols are dead in a built DSO, and which are the only live path.

EXP1000 found 94 new symbols in a fresh build and noted that "both paths are in the
binary". That is only half a fact: a bridge object may be linked and never called, and
the difference matters because a dead bridge is removable debt while a live one is the
only implementation some models reach.

The measurement is the object graph, not the source text: a symbol is DEAD when no
object that needs it lists it as undefined, including the bridge objects themselves.
Source-text grep is what produced two wrong answers earlier in this series, so it is
not used.

Usage:
  tools/audit-bridge-symbols.py <build-dir> [--list]
Exit: 0 always - this reports, it does not gate (nothing is removed on its word)
"""
import argparse
import os
import subprocess
import sys

BRIDGE_PREFIXES = ("b1", "b2", "b3", "b4", "fg", "fx")


def defined(path):
    out = subprocess.run(["nm", "-g", "--defined-only", path],
                         capture_output=True, text=True).stdout
    return {l.split()[-1] for l in out.splitlines() if l.strip()}


def undefined(path):
    out = subprocess.run(["nm", "-u", path], capture_output=True, text=True).stdout
    return {l.split()[-1] for l in out.splitlines() if l.strip()}


def is_bridge(name):
    base = os.path.basename(name)
    return base[0] == "b" and base[1].isdigit() or base.startswith(("fg", "fx"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    objs = [os.path.join(a.build_dir, f) for f in os.listdir(a.build_dir)
            if f.endswith(".o")]
    bridges = [o for o in objs if is_bridge(os.path.basename(o))]
    others = [o for o in objs if o not in bridges]

    bridge_syms = {}
    for o in bridges:
        for s in defined(o):
            bridge_syms.setdefault(s, o)

    # Every symbol some object needs, from bridges and non-bridges alike. A bridge
    # symbol another bridge calls is alive: those are internal helpers of the
    # generated programs, and counting them dead would be wrong.
    needed = set()
    for o in objs:
        needed |= undefined(o)

    dead = sorted(s for s in bridge_syms if s not in needed)
    live_from_outside = sorted(s for s in bridge_syms
                               if s in needed and any(s in undefined(o) for o in others))

    print("bridge objects: %d | their exports: %d" % (len(bridges), len(bridge_syms)))
    print("only reachable from non-bridge objects: %d" % len(live_from_outside))
    print("referenced by nobody: %d" % len(dead))

    groups = {}
    for s in dead:
        groups[s.split("_")[0]] = groups.get(s.split("_")[0], 0) + 1
    for g, n in sorted(groups.items(), key=lambda kv: -kv[1]):
        print("  %-10s %3d dead of %d exports"
              % (g, n, sum(1 for s in bridge_syms if s.split("_")[0] == g)))

    if a.list:
        for s in dead:
            print("  dead %s  (%s)" % (s, os.path.basename(bridge_syms[s])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
