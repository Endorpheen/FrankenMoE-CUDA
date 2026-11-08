#!/usr/bin/env python3
"""Hash-bound JSONL comparator for batch-allocr traces.

This tool only compares JSONL. It does not build, launch, link, or otherwise
connect the C++ allocr to Goose. The fixture is responsible for executing the
native and generated-Goose paths and for printing rows with an "engine" field.

Usage:
    python3 replay.py --pin
    python3 replay.py --check
    python3 replay.py --trace out.jsonl

Trace row: {"engine":"native"|"goose","case":..,"ub":..,"n_tokens":..,
            "n_seq_tokens":..,"n_seqs":..,"equal":..,"batch_idxs":[..],
            "seq_ids":[[..]],"token":[..],"pos":[..],"embd_sum":..}
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent          # helpers/omo5
LOCK = HERE / "source-lock.json"
SRC = HERE / "llama-batch-copy.cpp"
HDR = HERE / "llama-batch-copy.h"

KEYS = ("n_tokens", "n_seq_tokens", "n_seqs", "equal",
        "batch_idxs", "seq_ids", "token", "pos", "embd_sum")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_lock() -> dict:
    return json.loads(LOCK.read_text(encoding="utf-8")) if LOCK.is_file() else {}


def save_lock(lock: dict) -> None:
    LOCK.write_text(json.dumps(lock, indent=2) + "\n", encoding="utf-8")


def pin() -> int:
    for p in (SRC, HDR):
        if not p.is_file():
            print(f"error: missing {p}", file=sys.stderr)
            return 2
    lock = load_lock()
    lock.update({
        "source": str(SRC.relative_to(ROOT)),
        "header": str(HDR.relative_to(ROOT)),
        "source_sha256": sha256(SRC),
        "header_sha256": sha256(HDR),
        "status": "PINNED",
    })
    save_lock(lock)
    print(json.dumps(lock, indent=2))
    return 0


def check_lock() -> int:
    lock = load_lock()
    if lock.get("status") != "PINNED" or not lock.get("source_sha256"):
        print("REFUSED: source-lock.json is not pinned; run replay.py --pin", file=sys.stderr)
        return 2
    if sha256(SRC) != lock["source_sha256"]:
        print("REFUSED: inputs/llama-batch.cpp hash changed", file=sys.stderr)
        return 2
    if lock.get("header_sha256") and sha256(HDR) != lock["header_sha256"]:
        print("REFUSED: inputs/llama-batch.h hash changed", file=sys.stderr)
        return 2
    return 0


def load_rows(path: Path) -> dict[tuple[str, int], dict[str, dict]]:
    by_key: dict[tuple[str, int], dict[str, dict]] = {}
    with path.open(encoding="utf-8") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise SystemExit(f"{path}:{lineno}: bad JSON: {exc}")
            engine = row.get("engine")
            if engine not in ("native", "goose"):
                raise SystemExit(f"{path}:{lineno}: missing/unknown engine")
            key = (row.get("case", ""), int(row.get("ub", 0)))
            by_key.setdefault(key, {})[engine] = row
    return by_key


def compare(path: Path) -> int:
    by_key = load_rows(path)
    bad = 0
    for (case, ub), eng in sorted(by_key.items()):
        if "native" not in eng or "goose" not in eng:
            print(f"MISSING ENGINE case={case} ub={ub} have={sorted(eng)}", file=sys.stderr)
            bad += 1
            continue
        a, b = eng["native"], eng["goose"]
        for k in KEYS:
            if a.get(k) != b.get(k):
                bad += 1
                print(f"MISMATCH case={case} ub={ub} field={k}\n  native {a.get(k)}\n  goose  {b.get(k)}", file=sys.stderr)
    if bad:
        return 1
    print(f"OK: {len(by_key)} ubatch pairs identical")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pin", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--trace", help="JSONL emitted by the fixture (both engines)")
    args = ap.parse_args()

    if args.pin:
        return pin()
    rc = check_lock()
    if rc != 0:
        return rc
    if args.check and not args.trace:
        print("source lock OK")
        return 0
    if not args.trace:
        print("nothing to compare: supply --trace", file=sys.stderr)
        return 2
    return compare(Path(args.trace))


if __name__ == "__main__":
    sys.exit(main())
