#!/usr/bin/env python3
"""Extract Goose emitter IDs from generated C. Never guess.

Input: a real generated translation unit (normally generated/batch-allocr.c)
produced by the Goose emitter for batch-allocr.goose.

The emitter emits one `gg_function_<n>` per Goose function in emission order.
This tool only reports a mapping when the generated file is present, parses
cleanly, and ids are contiguous/unique. Otherwise it prints UNKNOWN and exits 3.

Usage:
    python3 compute_emitter_ids.py --generated generated/batch-allocr.c
    python3 compute_emitter_ids.py --generated ... --json
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

FUNC_RE = re.compile(r'\bgg_function_(\d+)\s*\(')
# Wrapper entry ids are expected to be the ids of the generated entry functions
# called by the unit wrapper (gg_function_1(mode,...)). We do not assume which
# Goose function that is unless the generated file names it.
NAME_RE = re.compile(r'\bgg_function_(\d+)\s*\([^)]*\)\s*\{')

GOOSE_NAMES = [
    "init", "split_reset", "split_simple", "split_equal", "split_seq",
    "ubatch_reserve", "ubatch_add", "clear", "seq_pos_min", "seq_pos_max",
    "entry",
]


def main(argv: list[str]) -> int:
    as_json = "--json" in argv
    args = [a for a in argv if a != "--json"]
    gen = None
    if "--generated" in args:
        i = args.index("--generated")
        if i + 1 >= len(args):
            print("error: --generated needs a path", file=sys.stderr)
            return 2
        gen = Path(args[i + 1])
    if gen is None or not gen.is_file():
        out = {
            "status": "UNKNOWN_PENDING_GENERATED_C",
            "reason": "generated C not supplied or not readable",
            "generated": str(gen) if gen else None,
            "mapping": None,
        }
        print(json.dumps(out, indent=2) if as_json else
              "UNKNOWN_PENDING_GENERATED_C: provide --generated <generated/batch-allocr.c>")
        return 3

    text = gen.read_text(encoding="utf-8", errors="replace")
    ids = sorted({int(m.group(1)) for m in FUNC_RE.finditer(text)})
    if not ids:
        print("UNKNOWN: no gg_function_<n> definitions found", file=sys.stderr)
        return 3
    if ids != list(range(ids[0], ids[0] + len(ids))):
        print("UNKNOWN: non-contiguous emitter ids; refusing to map", file=sys.stderr)
        return 3

    # Without the emitter's symbol table we can only bind ids to names when the
    # generated file carries a name table or comments. Report the raw id list.
    names = {}
    for m in NAME_RE.finditer(text):
        start = m.end()
        end = text.find('\n}', start)
        body = text[start:end if end != -1 else len(text)]
        for g in GOOSE_NAMES:
            if re.search(r'\b' + re.escape(g) + r'\s*\(', body):
                names.setdefault(int(m.group(1)), g)
                break

    out = {
        "status": "PARSED" if names else "IDS_ONLY_UNPROVEN_NAMES",
        "generated": str(gen),
        "ids": ids,
        "name_binding": names or None,
        "entry_id": names.get(1),
        "note": "Name binding is heuristic; the emitter's own symbol table is authoritative.",
    }
    print(json.dumps(out, indent=2) if as_json else out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
