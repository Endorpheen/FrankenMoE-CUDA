#!/usr/bin/env python3
"""Which Goose doors the production sources reach, per module.

`tools/door-callers.py` answers "is this door called anywhere". That is not the migration question:
a door called only from `integration/m1` is called by a harness, and a harness is not the runtime.
This separates the three cases in one pass, so the answer is about the shipped tree rather than
about every file that mentions a name.

  production  a call site under vendor/flint (the sources the server and the library are built from)
  harness     called only under integration/ (m1 bridges, edge0, standalone CLIs)
  none        no call site at all

Usage: python3 tools/production-doors.py [--list-none] [--module goose/model/foo.goose ...]
"""
import pathlib
import re
import sys

SKIP = ("_archive", "/build/", "/builds/", "/node_modules/")


def identifiers(root: pathlib.Path, base: str) -> set[str]:
    names: set[str] = set()
    for ext in ("*.cpp", "*.c", "*.h", "*.hpp"):
        for path in (root / base).rglob(ext):
            p = str(path)
            if any(s in p for s in SKIP):
                continue
            try:
                text = path.read_text(errors="replace")
            except OSError:
                continue
            names.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]{2,}", text))
    return names


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    src = (root / "tools/build-main-runtime.sh").read_text()
    mods = re.findall(r"^\s+(goose/[a-z0-9/_-]+\.goose)\s*$", src, re.M)
    if "--module" in sys.argv:
        i = sys.argv.index("--module")
        mods = sys.argv[i + 1:]
    list_none = "--list-none" in sys.argv

    prod = identifiers(root, "vendor/flint")
    harness = identifiers(root, "integration")

    totals = {"production": 0, "harness": 0, "none": 0}
    rows = []
    for m in mods:
        doors = [x.group(1) for x in re.finditer(r"(?m)^export fn ([a-z0-9_]+)", (root / m).read_text())]
        if not doors:
            continue
        p = h = 0
        absent = []
        for d in doors:
            if d in prod:
                p += 1
            elif d in harness:
                h += 1
            else:
                absent.append(d)
        totals["production"] += p
        totals["harness"] += h
        totals["none"] += len(absent)
        rows.append((m, len(doors), p, h, absent))

    rows.sort(key=lambda r: (-r[1], r[0]))
    print(f"{'module':52} {'doors':>6} {'prod':>5} {'harness':>8} {'none':>5}")
    for m, n, p, h, absent in rows:
        print(f"{m:52} {n:6} {p:5} {h:8} {len(absent):5}")
        if list_none and absent:
            print(f"    none: {', '.join(absent[:8])}{' ...' if len(absent) > 8 else ''}")
    print(f"\ntotals: production={totals['production']} harness-only={totals['harness']} "
          f"none={totals['none']}")

    # A second view was attempted and removed the same day it was written: it classified every fg*
    # reference from vendor/flint by where it is defined, and reported 203 of 209 as "defined nowhere".
    # That number was a fact about the scan, not about the tree - fg161_bulk, for instance, is an
    # `extern` host primitive declared inside goose/cpu/goose-allocator/allocator.goose, not a Goose
    # decision at all. Mixing substrate with decisions is the mistake rule 27 names, and the count
    # would have been quoted as coverage. The module view above is well defined; this one is not, so
    # it is not in the tool.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
