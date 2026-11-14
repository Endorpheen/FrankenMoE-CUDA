#!/usr/bin/env python3
"""Guard the canonical Goose tree against silent removals.

Parallel work on the same canonical runtime has already cost us once: a slice
built on a stale copy replaced `goose/jinja/runtime.goose` and dropped `permute`
and the cycle-aware dump without any test noticing. This check keeps a recorded
list of exported symbols and fails when one disappears.

Usage:
    python3 tools/check-goose-symbols.py            # verify
    python3 tools/check-goose-symbols.py --update   # accept the current set
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = ROOT / "goose"
RECORD = ROOT / "tools" / "goose-symbols.txt"
EXPORT = re.compile(r"^export\s+(?:recursive\s+)?fn\s+([A-Za-z0-9_]+)\s*\(", re.MULTILINE)


def current() -> dict[str, set[str]]:
    found: dict[str, set[str]] = {}
    for path in sorted(GOOSE.rglob("*.goose")):
        if "_archive" in path.parts:
            continue
        names = set(EXPORT.findall(path.read_text(errors="replace")))
        if names:
            found[path.relative_to(ROOT).as_posix()] = names
    return found


def recorded() -> dict[str, set[str]]:
    if not RECORD.exists():
        return {}
    out: dict[str, set[str]] = {}
    for line in RECORD.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        path, _, name = line.partition("\t")
        out.setdefault(path, set()).add(name)
    return out


def write(symbols: dict[str, set[str]]) -> None:
    lines = ["# Exported Goose symbols per canonical file.",
             "# Regenerate with: python3 tools/check-goose-symbols.py --update"]
    for path in sorted(symbols):
        for name in sorted(symbols[path]):
            lines.append(f"{path}\t{name}")
    RECORD.write_text("\n".join(lines) + "\n")


def main() -> int:
    now = current()
    if "--update" in sys.argv:
        write(now)
        print(f"recorded {sum(len(v) for v in now.values())} symbol(s) in {RECORD.relative_to(ROOT)}")
        return 0

    old = recorded()
    if not old:
        print("no symbol record; run with --update once", file=sys.stderr)
        return 2

    missing: list[str] = []
    added: list[str] = []
    for path, names in sorted(old.items()):
        gone = names - now.get(path, set())
        for name in sorted(gone):
            missing.append(f"{path}: {name}")
        new = now.get(path, set()) - names
        for name in sorted(new):
            added.append(f"{path}: {name}")
    for path in sorted(set(now) - set(old)):
        for name in sorted(now[path]):
            added.append(f"{path}: {name}")

    if added:
        print("new exported symbols (run --update to record):")
        for item in added:
            print("  +", item)
    if missing:
        print("MISSING exported symbols (a canonical file lost code):", file=sys.stderr)
        for item in missing:
            print("  -", item, file=sys.stderr)
        return 1
    print(f"goose symbols: {sum(len(v) for v in now.values())} present, none missing")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
