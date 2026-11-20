#!/usr/bin/env python3
"""Count how much of today's composed Goose unit an artefact actually carries.

The question behind P11-01 and P11-07: the shipped library is a build of the unit as it stood when
it was built, and the unit has grown since. This reports the two sizes instead of leaving the gap to
be guessed: every `export fn` of the composed unit that is in the module list, against the symbols
the artefact exports.

A door that is absent is not necessarily a defect - it may be newer than the artefact. A door that
is present is evidence the unit reached the artefact at build time.

Usage: python3 tools/unit-carry.py <artefact> [--list-absent]

The artefact is a shared library or an object; anything `nm -D` or `nm -g --defined-only` can read.
"""
import pathlib
import re
import runpy
import subprocess
import sys


def unit_exports(root: pathlib.Path) -> set[str]:
    src = (root / "tools/build-main-runtime.sh").read_text()
    mods = re.findall(r"^\s+(goose/[a-z0-9/_-]+\.goose)\s*$", src, re.M)
    if not mods:
        sys.exit("no module list found in tools/build-main-runtime.sh")
    renames = runpy.run_path(str(root / "tools/compose-goose.py"))["RENAMES"]
    out = set()
    for m in mods:
        module = root / m
        mapping = renames.get(module.name, {})
        for name in re.findall(r"(?m)^export fn (\w+)", module.read_text()):
            out.add(mapping.get(name, name))
    return out


def symbols(artefact: pathlib.Path) -> set[str]:
    for args in (["nm", "-D", "--defined-only"], ["nm", "-g", "--defined-only"]):
        r = subprocess.run(args + [str(artefact)], capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            return {line.split()[-1] for line in r.stdout.splitlines() if len(line.split()) >= 3}
    return set()


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip())
        return 2
    root = pathlib.Path(__file__).resolve().parents[1]
    art = pathlib.Path(sys.argv[1])
    if not art.exists():
        print(f"no such artefact: {art}")
        return 2
    exports = unit_exports(root)
    have = symbols(art)
    present = exports & have
    absent = sorted(exports - have)
    pct = (100 * len(present)) // max(1, len(exports))
    print(f"artefact: {art}")
    print(f"composed unit exports:            {len(exports)}")
    print(f"exported by this artefact:        {len(present)} ({pct}%)")
    print(f"absent (newer than, or not wired): {len(absent)}")
    if "--list-absent" in sys.argv:
        for name in absent:
            print(f"  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
