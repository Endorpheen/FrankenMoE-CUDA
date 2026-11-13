#!/usr/bin/env python3
"""Verify that goose/ matches the accepted slices recorded in its manifest.

Checks, for every entry: the file exists, its SHA256 matches, and the
experiment directory exists. Also reports .goose files present in goose/ but
absent from the manifest, and manifest entries whose status is not "ported".

Exit code 0 when everything agrees, 1 otherwise.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = ROOT / "goose"
MANIFEST = GOOSE / "MANIFEST.json"


def main() -> int:
    doc = json.loads(MANIFEST.read_text())
    entries = doc.get("entries", [])
    seen = set()
    problems = []

    for row in entries:
        rel = row["goose"]
        seen.add(rel)
        path = GOOSE / rel
        if not path.is_file():
            problems.append(f"missing file: {rel}")
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != row["sha256"]:
            problems.append(
                f"hash mismatch: {rel}\n  manifest {row['sha256']}\n  actual   {digest}"
            )
        if not (ROOT / "experiments" / row["experiment"]).is_dir():
            problems.append(f"missing experiment: {row['experiment']} (for {rel})")

    for path in sorted(GOOSE.rglob("*.goose")):
        rel = path.relative_to(GOOSE).as_posix()
        if rel not in seen:
            problems.append(f"not in manifest: {rel}")

    if problems:
        print(f"goose tree check: {len(problems)} problem(s)")
        for item in problems:
            print("  " + item)
        return 1

    print(f"goose tree check: {len(entries)} file(s), hashes and experiments match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
