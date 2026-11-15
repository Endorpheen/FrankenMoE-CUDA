#!/usr/bin/env python3
"""Materialise a module state for the EXP642 gate.

    python3 tests/mutate.py materialize <label>
    python3 tests/mutate.py list

Labels:
    before      source/quant-rows2.goose (canon 1de2effc + the accepted EXP641
                qgrid patch), i.e. the state in which iq1_m is still wrong
    fixed       before + patches/iq1m-split.diff
    mut-<id>    fixed + one mutation

A mutation is one line; when that line occurs more than once the surrounding
context is widened until it is unique and the substitution is applied inside
that context only.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "source" / "quant-rows2.goose"
PATCH = ROOT / "patches" / "iq1m-split.diff"
WORK = ROOT / "results" / "work"


MUTATIONS = [
    # ---- half A: the split search structure -------------------------------
    ("A1", "A",
     "                                sx1 += w * pp * xv2;",
     "                                sx1 += w * mm * xv2;",
     "h=0 branch: group 1 uses the negative level set"),

    ("A2", "A",
     "                                sx2 += w * pp * xv2;",
     "                                sx2 += w * mm * xv2;",
     "h=1 branch: group 2 swaps the level sign"),

    ("A3", "A",
     "                            if ii2 < 8 {",
     "                            if ii2 < 7 {",
     "half-block boundary 8 -> 7 in the level assignment"),

    ("A4", "A",
     "                        else if g == 2 { pp = 1.125; mm = 0.875; }",
     "                        else if g == 2 { pp = 0.875; mm = 1.125; }",
     "group 2 level constants swapped"),

    ("A5", "A",
     "                                s21 += w * pp * pp;",
     "                                s21 += w * pp;",
     "squared level becomes a linear level in one sum"),

    ("A6", "A",
     "                        else if g == 1 { lo = i1; hi = i2b; }",
     "                        else if g == 1 { lo = 0; hi = i2b; }",
     "middle group starts at 0 instead of i1"),

    # ---- half B: the arithmetic around the split -------------------------
    ("B1", "B",
     "                            scale = sumqx / sumq2;",
     "                            scale = sumqx / (2.0 * sumq2);",
     "scale estimate halved"),

    ("B2", "B",
     "                        if sumq2 > 0 as f32 && sumqx * sumqx > best_score * sumq2 {",
     "                        if sumq2 > 0 as f32 && 0.5 * sumqx * sumqx > best_score * sumq2 {",
     "score comparison scaled by 0.5"),

    ("B3", "B",
     "                        best_score = scale * sumqx;",
     "                        best_score = scale * sumq2;",
     "best score built from the wrong factor"),

    ("B4", "B",
     "            if sh == 1 { mask = 128; }",
     "            if sh == 1 { mask = 64; }",
     "shift mask 0x80 -> 0x40"),

    ("B5", "B",
     "        st_f16(tmp(S_TMP + 64), dd * 1.1125);",
     "        st_f16(tmp(S_TMP + 64), dd * 1.125);",
     "final scale fudge factor 1.1125 -> 1.125"),

    ("B6", "B",
     "        let d0 = max_scale / 15.0;",
     "        let d0 = max_scale / 16.0;",
     "super-scale denominator 15 -> 16"),
]

BY_ID = {m[0]: m for m in MUTATIONS}


def patch_unique(text: str, anchor: str, repl: str) -> str:
    n = text.count(anchor)
    if n == 0:
        raise SystemExit("anchor not found: %r" % anchor[:70])
    idx = text.find(anchor)
    start, end = idx, idx + len(anchor)
    while text.count(text[start:end]) > 1:
        prev_nl = text.rfind("\n", 0, start - 1)
        next_nl = text.find("\n", end + 1)
        if prev_nl < 0 or next_nl < 0:
            break
        start, end = prev_nl + 1, next_nl
    ctx = text[start:end]
    if text.count(ctx) != 1 or ctx.count(anchor) != 1:
        raise SystemExit("could not isolate a unique site for %r" % anchor[:70])
    return text[:start] + ctx.replace(anchor, repl) + text[end:]


def apply_fixed() -> str:
    text = SRC.read_text()
    if not PATCH.exists():
        return text
    WORK.mkdir(parents=True, exist_ok=True)
    tmp = WORK / "quant-rows2.patch-in.goose"
    tmp.write_text(text)
    res = subprocess.run(["patch", str(tmp), str(PATCH)],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if res.returncode != 0:
        raise SystemExit("patch failed:\n" + res.stdout)
    return tmp.read_text()


def materialize(label: str) -> str:
    WORK.mkdir(parents=True, exist_ok=True)
    if label == "before":
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
    if sys.argv[1] == "materialize":
        materialize(sys.argv[2] if len(sys.argv) > 2 else "fixed")
        return 0
    if sys.argv[1] == "list":
        for mid, half, _, _, mech in MUTATIONS:
            print("%-3s %s  %s" % (mid, half, mech))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
