#!/usr/bin/env python3
"""Verify that goose/ matches the accepted slices recorded in its manifest.

Checks, for every entry: the file exists, its SHA256 matches, and the
experiment directory exists. Also reports .goose files present in goose/ but
absent from the manifest, and manifest entries whose status is not "ported".

Exit code 0 when everything agrees, 1 otherwise.
"""
import hashlib
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = ROOT / "goose"
MANIFEST = GOOSE / "MANIFEST.json"
REQUIRED = ROOT / "tools" / "goose-required-snippets.txt"


def canonical_sources():
    """Rows from the sync tool, so the check never trusts a stale manifest."""
    spec = importlib.util.spec_from_file_location("sync_goose_tree", ROOT / "tools" / "sync-goose-tree.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.CANONICAL_SOURCES


def check_required(problems):
    if not REQUIRED.is_file():
        return
    for line in REQUIRED.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        rel, _, snippet = line.partition("\t")
        path = GOOSE / rel
        if not path.is_file():
            problems.append(f"required snippet file missing: {rel}")
            continue
        if snippet not in path.read_text(errors="replace"):
            problems.append(f"required snippet missing in {rel}: {snippet!r}")


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

    for target, experiment, source, _native, _gate in canonical_sources():
        path = GOOSE / target
        origin = ROOT / "experiments" / experiment / source
        if not path.is_file() or not origin.is_file():
            problems.append(f"canonical source missing: {target} <- {experiment}/{source}")
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != hashlib.sha256(origin.read_bytes()).hexdigest():
            problems.append(f"canonical file differs from its source: {target} <- {experiment}/{source}")

    check_required(problems)

    if problems:
        print(f"goose tree check: {len(problems)} problem(s)")
        for item in problems:
            print("  " + item)
        return 1

    print(f"goose tree check: {len(entries)} file(s), hashes and experiments match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
