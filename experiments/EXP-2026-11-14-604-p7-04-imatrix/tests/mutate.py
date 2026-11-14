#!/usr/bin/env python3
"""Mutation gate for P7-04 S2 (imatrix loader, remap, prepare).

Twelve textual mutations of source/imatrix.goose, six in each half. Each
mutant must still pass `goose check`, then the differential driver must differ
from the recorded native output. A mutation the gate does not catch is
reported and makes the run exit non-zero.

    tests/mutate.py --half 1|2|all
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / "source" / "imatrix.goose"
GOOSE = os.environ.get("GOOSE_BIN",
                       "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

HALF1 = [
    ("A1 legacy chunk_size is not zeroed",
     "    meta[M_CHUNK] = chunk;\n    meta[M_CHUNKSZ] = 0;\n    return IM_OK;",
     "    meta[M_CHUNK] = chunk;\n    meta[M_CHUNKSZ] = 1;\n    return IM_OK;"),
    ("A2 legacy name keeps the bytes after a NUL",
     "        let nlen = nul_len(b, p, l);",
     "        let nlen = l;"),
    ("A3 legacy accepts an empty sums vector",
     "        if nval < 1 { return IM_ERR_COUNT; }",
     "        if nval < 0 { return IM_ERR_COUNT; }"),
    ("A4 a non-array datasets key becomes a dataset",
     "                if is_ds && is_arr {",
     "                if is_ds {"),
    ("A5 has_metadata ignores chunk_size",
     "    meta[M_HASMETA] = if has_ds != 0 && has_cc != 0 && has_cs != 0 { 1 } else { 0 };",
     "    meta[M_HASMETA] = if has_ds != 0 && has_cc != 0 { 1 } else { 0 };"),
    ("A6 alignment is not checked for a power of two",
     "    if align <= 0 || (align & (align - 1)) != 0 { return IM_ERR_TRUNC; }",
     "    if align <= 0 { return IM_ERR_TRUNC; }"),
]

HALF2 = [
    ("B1 per-expert groups are one element too wide",
     "            let ne0 = nval / ncnt;",
     "            let ne0 = nval / ncnt + 1;"),
    ("B2 counts truncate instead of rounding half away from zero",
     "    if d >= 0.0 { return (d + 0.5) as! i64; }",
     "    if d >= 0.0 { return d as! i64; }"),
    ("B3 a non-F32 sums/counts pair is accepted",
     "        if ttype[si] != T_F32 || ttype[ci] != T_F32 { return IM_ERR_TYPE; }",
     "        if false { return IM_ERR_TYPE; }"),
    ("B4 the excluded-weight filter is skipped",
     "            if sub_find(b, name_off[i], name_len[i], ex, ex_off[k], ex_len[k]) {\n                keep = false;",
     "            if sub_find(b, name_off[i], name_len[i], ex, ex_off[k], ex_len[k]) {\n                keep = true;"),
    ("B5 a repeated block is remapped again",
     "    let k = map_find(mapk, nmap[0], blk);\n    if k >= 0 {",
     "    let k = map_find(mapk, nmap[0], blk);\n    if false {"),
    ("B6 remap_imatrix reports a miss instead of aborting",
     "            o_len[0] = mid + (o + l - dend);\n            return 1;\n        }\n        i = i + 1;\n    }\n    return -1;\n}",
     "            o_len[0] = mid + (o + l - dend);\n            return 1;\n        }\n        i = i + 1;\n    }\n    return IM_OK;\n}"),
]


def run_one(label, old, new, idx):
    src = SRC.read_text()
    if src.count(old) != 1:
        print(f"SKIP {label}: anchor occurs {src.count(old)} times")
        return False
    work = ROOT / "build" / f"mut{idx:02d}"
    work.mkdir(parents=True, exist_ok=True)
    mutant = work / "imatrix.goose"
    mutant.write_text(src.replace(old, new))

    check = subprocess.run([GOOSE, "check", str(mutant)],
                           capture_output=True, text=True)
    if check.returncode != 0:
        print(f"SKIP {label}: goose check failed")
        return False

    env = dict(os.environ, GOOSE_SRC=str(mutant), OUT_DIR=str(work),
               OPT="-O0", GOOSE_ONLY="1")
    build = subprocess.run(["bash", str(HERE / "build.sh")],
                           env=env, capture_output=True, text=True)
    caught = build.returncode != 0
    print(f"{'CATCH' if caught else 'MISS '} {label}")
    if not caught:
        tail = (build.stdout + build.stderr).splitlines()[-6:]
        for line in tail:
            print("   ", line)
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", choices=["1", "2", "all"], default="all")
    a = ap.parse_args()
    muts = HALF1 if a.half == "1" else HALF2 if a.half == "2" else HALF1 + HALF2
    caught = 0
    for i, (label, old, new) in enumerate(muts, 1):
        if run_one(label, old, new, i):
            caught += 1
    print(f"mutations {caught}/{len(muts)} caught")
    return 0 if caught == len(muts) else 1


if __name__ == "__main__":
    sys.exit(main())
