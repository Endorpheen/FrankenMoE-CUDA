#!/usr/bin/env python3
"""Fail if a source file the runtime build compiles is not tracked by git.

Check the active standalone, tensor substrate and composed Goose build inputs
against the index so a fresh clone has the same source set.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The archived reconstructed builder names the removed native sources.
SOURCES = [
    ROOT / "tools/build-standalone-runtime.sh",
    ROOT / "tools/build-goose-tensor-core.sh",
    ROOT / "tools/build-main-runtime.sh",
]

PATTERNS = [
    (r'\$ROOT/([^\s"$]+\.c)(?=["\s])', ""),
    (r'\$BASE/([^\s"$]+\.c)(?=["\s])', "vendor/flint/ggml/"),
    (r'^\s+(goose/[^\s]+\.goose)[ \t]*$', ""),
]


def tracked() -> set[str]:
    out = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "-z"], text=True)
    return set(out.split("\0"))


def main() -> int:
    have = tracked()
    missing: dict[str, str] = {}
    for src in SOURCES:
        text = src.read_text()
        for pat, prefix in PATTERNS:
            for name in re.findall(pat, text, re.MULTILINE):
                cand = prefix + name
                if cand not in have:
                    missing.setdefault(cand, str(src.relative_to(ROOT)))

    if missing:
        print(f"build sources not tracked by git: {len(missing)}", file=sys.stderr)
        for cand, where in missing.items():
            print(f"  {cand}   (named by {where})", file=sys.stderr)
        print("a fresh clone would build without these; commit them or remove them from the build",
              file=sys.stderr)
        return 1
    print("build sources: all tracked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
