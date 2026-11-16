#!/usr/bin/env python3
"""Fail if a source file the runtime build compiles is not tracked by git.

Why: seven files build.py compiles were absent from the repository - including llama-vocab.cpp, whose 157
untracked lines carry four Goose bridge families. A fresh clone would have built a runtime with those
calls gone and nothing would have reported the loss.

The check is one `git ls-files` comparison over the file lists build.py and the build scripts name.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The file lists the build reads. Keep this in step with build.py's `changed` list and the scripts'
# explicit sources.
SOURCES = [
    ROOT / "runtime/reconstructed/build.py",
    ROOT / "tools/build-standalone-runtime.sh",
    ROOT / "tools/build-main-runtime.sh",
]

PATTERNS = [
    r"'([a-z0-9_\-]+\.cpp)'",       # build.py changed list
    r'\$ROOT/integration/[^\s"]+\.(?:c|cpp)',   # scripts' explicit sources
]


def tracked() -> set[str]:
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files"], capture_output=True, text=True).stdout
    return set(out.splitlines())


# vendor/flint supplies these two files; they are tracked under vendor/ and referenced from there, but the checker
# resolves a bare file name to runtime/reconstructed/src/. Listing them explicitly keeps the check strict elsewhere.
VENDOR_SOURCED = {"llama.cpp", "unicode-data.cpp"}


def main() -> int:
    have = tracked()
    missing: list[tuple[str, str]] = []
    for src in SOURCES:
        if not src.exists():
            continue
        text = src.read_text()
        for pat in PATTERNS:
            for m in re.finditer(pat, text):
                name = m.group(1) if m.lastindex else m.group(0)
                rel = name.replace("$ROOT/", "")
                # a bare file name means runtime/reconstructed/src/<name>
                if name in VENDOR_SOURCED:
                    continue
                cand = rel if "/" in rel else f"runtime/reconstructed/src/{rel}"
                # build.py compiles from an external tree; the repository copy is what must be tracked
                # Flag it whether or not the repo copy exists: a file the build compiles but the repository
                # does not carry is the failure this checks for. Skipping nonexistent ones is what made the
                # first version of this script report "all tracked" while seven were missing.
                if cand not in have and all(cand != m2 for m2, _ in missing):
                    missing.append((cand, str(src.relative_to(ROOT))))

    if missing:
        print(f"build sources not tracked by git: {len(missing)}", file=sys.stderr)
        for cand, where in missing:
            print(f"  {cand}   (named by {where})", file=sys.stderr)
        print("a fresh clone would build without these; commit them or remove them from the build",
              file=sys.stderr)
        return 1
    print("build sources: all tracked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
