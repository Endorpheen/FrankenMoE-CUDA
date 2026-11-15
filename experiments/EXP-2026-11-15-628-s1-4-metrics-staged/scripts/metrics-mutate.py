#!/usr/bin/env python3
"""One-decision mutations of the staged metrics module, for gate sensitivity only.

Each mutation rewrites exactly one decision of goose/metrics/metrics-traces.goose so the
S1.4 gate must observe it. Run scripts/metrics-mutation.sh, which applies a mutation,
regenerates, relinks and restores the file.

    python3 scripts/metrics-mutate.py 1    # comma dropped from the quote predicate
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "goose" / "metrics" / "metrics-traces.goose"

MUTATIONS = {
    "1": (
        "comma dropped from the field quote predicate",
        "        if c == ',' || c == '\"' || c == '\\n' {",
        "        if c == '\"' || c == '\\n' {",
    ),
    "2": (
        "newline written raw inside a quoted field",
        "        } else if c == '\\n' {\n            p = mt_put(out, p, ' ');",
        "        } else if c == '\\n' {\n            p = mt_put(out, p, c);",
    ),
    "3": (
        "io row prints the requested size as the read size",
        "    p = mt_put_u64(out, p, read_bytes as u64);",
        "    p = mt_put_u64(out, p, req_bytes as u64);",
    ),
    "4": (
        "NaN route weight takes the numeric arm",
        "        p = mt_put_lit(out, p, \"nan\");",
        "        p = p + mt_fmt_g6(out, p, weight);",
    ),
}


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in MUTATIONS:
        print("usage: metrics-mutate.py " + "|".join(sorted(MUTATIONS)))
        return 2
    name, old, new = MUTATIONS[sys.argv[1]]
    text = SRC.read_text()
    if text.count(old) != 1:
        print(f"mutation {sys.argv[1]}: target appears {text.count(old)} times")
        return 1
    SRC.write_text(text.replace(old, new, 1))
    print(f"mutation {sys.argv[1]}: {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
