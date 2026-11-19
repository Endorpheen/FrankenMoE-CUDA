#!/usr/bin/env python3
"""Which doors a Goose module exports, and where each one is called from.

This exists because the same mistake was made five times on 2026-11-19: a claim of "nothing calls
it" made by grepping `vendor/flint/src` and `runtime/` for `*.cpp` only - missing headers (where the
model-manager wiring lives) and missing `integration/` (where the harness bridges live). Five ledger
records had to be corrected.

The rule this encodes: a count is a fact about a search, and the search is part of the fact.

Usage:
  python3 tools/door-callers.py <goose/module.goose> ...   # every door of each module
  python3 tools/door-callers.py --door <name> ...          # specific doors

Searches *.cpp, *.c, *.h, *.hpp under vendor/flint, runtime and integration, skipping archive
directories and build outputs.
"""
import pathlib
import re
import subprocess
import sys

ROOTS = ["vendor/flint", "runtime", "integration"]
EXT = ["*.cpp", "*.c", "*.h", "*.hpp"]
SKIP = ("_archive", "/build/", "/builds/")


def doors_of(module: pathlib.Path) -> list[str]:
    return [m.group(1) for m in re.finditer(r"(?m)^export fn ([a-z0-9_]+)", module.read_text())]


def callers(door: str, root: pathlib.Path) -> list[str]:
    hits = []
    for rel in ROOTS:
        base = root / rel
        if not base.is_dir():
            continue
        for ext in EXT:
            for path in base.rglob(ext):
                p = str(path)
                if any(s in p for s in SKIP):
                    continue
                try:
                    text = path.read_text(errors="replace")
                except OSError:
                    continue
                if re.search(r"\b" + re.escape(door) + r"\b", text):
                    hits.append(str(path.relative_to(root)))
    return sorted(set(hits))


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__.strip())
        return 2
    root = pathlib.Path(__file__).resolve().parents[1]

    doors: list[tuple[str, str]] = []
    if args[0] == "--door":
        for name in args[1:]:
            doors.append(("<named>", name))
    else:
        for a in args:
            module = root / a
            if not module.is_file():
                print(f"no such module: {a}", file=sys.stderr)
                return 2
            for door in doors_of(module):
                doors.append((a, door))

    wired = 0
    for module, door in doors:
        files = callers(door, root)
        if files:
            wired += 1
            print(f"{door:28} {len(files)} call site(s)")
            for f in files[:4]:
                print(f"    {f}")
            if len(files) > 4:
                print(f"    ... and {len(files) - 4} more")
        else:
            print(f"{door:28} no call site")
    print(f"\n{len(doors)} door(s), {wired} with a call site, {len(doors) - wired} without")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
