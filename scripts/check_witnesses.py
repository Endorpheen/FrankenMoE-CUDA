#!/usr/bin/env python3
"""Compare the Goose witness lines of two Apex runs.

Why this exists: the acceptance script checks speed and correctness, but it
cannot know which Goose modules were *supposed* to execute. A component can
silently fall back to native and every gate still passes -- that is exactly how
EXP199 disappeared from the chain at EXP204 and stayed gone through EXP209,
while each run reported "tokens exact".

This compares the set of witness prefixes seen in the server log of a candidate
run against a baseline run, and fails when a prefix present in the baseline is
missing from the candidate.

Usage:
    check_witnesses.py --run <server.log|run-dir> [--baseline <...>]
                       [--expect FG199K_ FG204X_] [--json]

Exit code 0 = no regression, 1 = a witness disappeared, 2 = input error.
"""

import argparse
import json
import re
import sys
from pathlib import Path

# Witness prefixes carry their module number, e.g. FG199K_KV_CELLS, FG209I_KV_SEQPOS,
# FG203D_MMQ_DISPATCH. Two patterns are recognised: a numbered module tag followed by
# an optional name, and the shorter FG175C style tags.
PATTERNS = (
    re.compile(rb"FG\d{3}[A-Z]+_[A-Z_]+"),
    re.compile(rb"FG\d{3}[A-Z]\b"),
)


def read_log(path: Path) -> bytes:
    """Accept a log file, a results directory, or a run directory."""
    if path.is_file():
        return path.read_bytes()
    if not path.is_dir():
        raise FileNotFoundError(path)
    # prefer the newest server log under the directory tree
    logs = sorted(path.rglob("server.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not logs:
        logs = sorted(path.rglob("*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not logs:
        raise FileNotFoundError(f"no log under {path}")
    return logs[0].read_bytes()


def witnesses(data: bytes) -> set:
    out = set()
    for pat in PATTERNS:
        for m in pat.finditer(data):
            out.add(m.group(0).decode())
    return out


def normalise(tag: str) -> str:
    """Compare on the module tag, not on the individual counter name."""
    m = re.match(r"(FG\d{3}[A-Z]+)", tag)
    return m.group(1) if m else tag.split("_")[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", required=True, help="candidate log or results dir")
    ap.add_argument("--baseline", help="previous stage log or results dir")
    ap.add_argument("--expect", nargs="*", default=[],
                    help="witness prefixes that must be present regardless of the baseline")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    try:
        cur = witnesses(read_log(Path(args.run)))
        base = witnesses(read_log(Path(args.baseline))) if args.baseline else set()
    except (OSError, ValueError) as e:
        print(f"input error: {e}", file=sys.stderr)
        return 2

    cur_mod = {normalise(t) for t in cur}
    base_mod = {normalise(t) for t in base}

    missing = sorted(base_mod - cur_mod)
    absent_expected = [e for e in args.expect if normalise(e) not in cur_mod]
    new = sorted(cur_mod - base_mod)

    ok = not missing and not absent_expected
    verdict = "OK" if ok else "REGRESSION"

    if args.json:
        print(json.dumps({
            "verdict": verdict,
            "run": args.run,
            "baseline": args.baseline,
            "modules_present": sorted(cur_mod),
            "missing_vs_baseline": missing,
            "missing_expected": absent_expected,
            "new_vs_baseline": new,
        }, indent=2))
        return 0 if ok else 1

    print(f"modules executing in the candidate: {len(cur_mod)}")
    for m in sorted(cur_mod):
        mark = "new" if m in new else "   "
        print(f"  {mark} {m}")
    print()
    if missing:
        print("MISSING vs baseline (a component silently reverted):")
        for m in missing:
            print(f"  - {m}")
    if absent_expected:
        print("EXPECTED but absent:")
        for m in absent_expected:
            print(f"  - {m}")
    print(f"verdict: {verdict}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
