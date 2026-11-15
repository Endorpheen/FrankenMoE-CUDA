#!/usr/bin/env python3
"""Materialise a module state for the EXP636 gate.

    python3 tests/mutate.py materialize <label>
    python3 tests/mutate.py list

Labels:
    canon        goose/model/quant-rows2.goose at 4930d479 (via source/), unpatched
    fixed        canon + patches/quant-rows2-iq-qgrid.diff
    mut-<id>     fixed + one mutation

A mutation is one line; when that line occurs more than once the surrounding
context is widened until it is unique, and the substitution is applied inside
that context only. This keeps every mutation a single-site edit without long
hand-written context strings.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "source" / "quant-rows2.goose"
PATCH = ROOT / "patches" / "quant-rows2-iq-qgrid.diff"
WORK = ROOT / "results" / "work"

# id, half, anchor line, replacement line, mechanism
MUTATIONS = [
    # ---- half A: the grid view itself ------------------------------------
    ("A1", "A",
     'extern "qz2_iq2_qgrid" fn iq2_qgrid(g: i64, i: i64) -> i64;',
     'extern "qz2_iq2_grid" fn iq2_qgrid(g: i64, i: i64) -> i64;',
     "iq2 quantizers read the static (dequant) grid again"),

    ("A2", "A",
     'extern "qz2_iq3_qgrid" fn iq3_qgrid(g: i64, i: i64) -> i64;',
     'extern "qz2_iq3_grid" fn iq3_qgrid(g: i64, i: i64) -> i64;',
     "iq3 quantizers read the static (dequant) grid again"),

    ("A3", "A",
     "    let gv = iq2_qgrid(g, gi);",
     "    let gv = iq2_grid(g, gi);",
     "iq2_best_nb scores neighbours on the wrong grid"),

    ("A4", "A",
     "    let gv = iq3_qgrid(g, gi);",
     "    let gv = iq3_grid(g, gi);",
     "iq3_best_nb scores neighbours on the wrong grid"),

    ("A5", "A",
     "                        let gv = iq3_qgrid(0, gidx);",
     "                        let gv = iq3_grid(0, gidx);",
     "iq3_xxs decodes the chosen neighbour on the wrong grid"),

    ("A6", "A",
     "                let gi = iq2_map(3, u);",
     "                let gi = iq2_map(2, u);",
     "iq2_s looks its final group up in the IQ1 table"),

    # ---- half B: the iq3_xxs arithmetic around the grid ------------------
    ("B1", "B",
     "                        let gv = iq3_qgrid(0, gidx);",
     "                        let gv = iq3_qgrid(0, gidx + 1);",
     "iq3_xxs decodes the neighbour one grid entry too far"),

    ("B2", "B",
     "                let id = (15.0 + (is as f32) * 0.2) / mx;",
     "                let id = (15.0 + (is as f32) * 0.1) / mx;",
     "iq3_xxs scale search step 0.2 -> 0.1"),

    ("B3", "B",
     "                if nflip % 2 == 1 {",
     "                if nflip % 2 == 0 {",
     "iq3_xxs flips the parity rule for the sign correction"),

    ("B4", "B",
     "                        st_u8(laux + ((4 * kk3 + i4) as u64), imax(0 as i64, imin(7, l0)));",
     "                        st_u8(laux + ((4 * kk3 + i4) as u64), imax(0 as i64, imin(6, l0)));",
     "iq3_xxs level clamp 7 -> 6"),

    ("B5", "B",
     "                    let q = 2.0 * (ld_u8(laux + (i4 as u64)) as f32) + 1.0;",
     "                    let q = 2.0 * (ld_u8(laux + (i4 as u64)) as f32);",
     "iq3_xxs level-to-value map drops the +1 offset"),

    ("B6", "B",
     "        let d = max_scale / 31.0;",
     "        let d = max_scale / 32.0;",
     "iq3_xxs super-scale denominator 31 -> 32"),
]

BY_ID = {m[0]: m for m in MUTATIONS}


def patch_unique(text: str, anchor: str, repl: str) -> str:
    """Replace `anchor` with `repl` at one site, widening context if needed."""
    n = text.count(anchor)
    if n == 0:
        raise SystemExit("anchor not found: %r" % anchor[:60])
    idx = text.find(anchor)
    start, end = idx, idx + len(anchor)
    while text.count(text[start:end]) > 1:
        prev_nl = text.rfind("\n", 0, start - 1)
        next_nl = text.find("\n", end + 1)
        if prev_nl < 0 or next_nl < 0:
            break
        start, end = prev_nl + 1, next_nl
    ctx = text[start:end]
    if text.count(ctx) != 1:
        raise SystemExit("could not isolate a unique site for %r" % anchor[:60])
    if ctx.count(anchor) != 1:
        raise SystemExit("anchor is not unique inside its context")
    return text[:start] + ctx.replace(anchor, repl) + text[end:]


def apply_fixed() -> str:
    text = SRC.read_text()
    if not PATCH.exists():
        return text
    tmp = WORK / "quant-rows2.patch-in.goose"
    tmp.parent.mkdir(parents=True, exist_ok=True)
    tmp.write_text(text)
    res = subprocess.run(["patch", str(tmp), str(PATCH)],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if res.returncode != 0:
        raise SystemExit("patch failed:\n" + res.stdout)
    return tmp.read_text()


def materialize(label: str) -> str:
    WORK.mkdir(parents=True, exist_ok=True)
    if label == "canon":
        text = SRC.read_text()
    elif label == "fixed":
        text = apply_fixed()
    elif label.startswith("mut-"):
        mid = label[4:]
        if mid not in BY_ID:
            raise SystemExit("unknown mutation %s" % mid)
        _, _, anchor, repl, _ = BY_ID[mid]
        text = patch_unique(apply_fixed(), anchor, repl)
    else:
        raise SystemExit("unknown label %s" % label)

    out = WORK / ("quant-rows2.%s.goose" % label)
    out.write_text(text)
    print("materialised %s -> %s" % (label, out))
    return str(out)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    if cmd == "materialize":
        materialize(sys.argv[2] if len(sys.argv) > 2 else "fixed")
        return 0
    if cmd == "list":
        for mid, half, _, _, mech in MUTATIONS:
            print("%-3s %s  %s" % (mid, half, mech))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
