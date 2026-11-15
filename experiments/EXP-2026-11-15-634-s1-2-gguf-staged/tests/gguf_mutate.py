#!/usr/bin/env python3
"""Twelve mutations of the composed GGUF Goose unit, two halves of six.

Every mutation is one textual change in the composed unit
(composed/gguf-stack.goose, built from goose/model/gguf-{metadata,writer,
tensors,split}.goose). The mutated unit is generated to mut/unit.goose,
compiled at -O0 into a separate build tree (build-mut), and both gates run:

  fg-gguf-smoke            the writer gate and the layout/predicate check
  scripts/split-gate.sh    the split tool differential, OFF (native) against
                           the mutated ON build

A mutation is DETECTED when either gate exits non-zero. Half A covers the KV
list, the scalar/string/array encoding, the alignment pad and the metadata
framing; half B covers the tensor descriptors, the offsets, the data-section
padding, both split thresholds and the plan bounds.

Usage: python3 tests/gguf_mutate.py [--work DIR] [--jobs N]
Writes evidence/mutations.txt and prints the summary.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOOSE = os.environ.get(
    "GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
)

# (id, half, origin module, mechanism, needle, replacement, occurrence)
MUTATIONS = [
    (
        "A1",
        "A",
        "gguf-writer",
        "KV order: keys emitted in reverse while the values stay in place",
        "        p = w_str(out, p, pool, key_off[i], key_len[i]);\n"
        "        let t = types[i];",
        "        p = w_str(out, p, pool, key_off[st[S_N] - (1 as i64) - i],\n"
        "                  key_len[st[S_N] - (1 as i64) - i]);\n"
        "        let t = types[st[S_N] - (1 as i64) - i];",
        2,  # 1 = s2_write (unused by the bridge), 2 = w_meta (the live path)
    ),
    (
        "A2",
        "A",
        "gguf-writer",
        "scalar type size: f32 written as 8 bytes",
        "    if t == 4 as i64 || t == 5 as i64 || t == 6 as i64 {\n"
        "        return 4 as i64;\n"
        "    }\n"
        "    return 8 as i64;",
        "    if t == 4 as i64 || t == 5 as i64 {\n"
        "        return 4 as i64;\n"
        "    }\n"
        "    return 8 as i64;",
        1,
    ),
    (
        "A3",
        "A",
        "gguf-writer",
        "string length prefix: u32 instead of u64",
        "fn w_str(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {\n"
        "    var q = w_u64(out, p, len);",
        "fn w_str(out: u8[:], p: i64, src: const u8[:], off: i64, len: i64) -> i64 {\n"
        "    var q = w_u32(out, p, len);",
        1,
    ),
    (
        "A4",
        "A",
        "gguf-writer",
        "array tag: ARRAY(9) written as STRING(8) in the live KV writer",
        "            p = w_u32(out, p, 9 as i64);",
        "            p = w_u32(out, p, 8 as i64);",
        2,  # 1 = s2_write (unused by the bridge), 2 = w_meta (the live path)
    ),
    (
        "A5",
        "A",
        "gguf-tensors",
        "alignment pad: always pads one extra block",
        "fn w_pad(out: u8[:], p: i64, alignment: i64) -> i64 {\n"
        "    var q = p;",
        "fn w_pad(out: u8[:], p: i64, alignment: i64) -> i64 {\n"
        "    var q = p + alignment;",
        1,
    ),
    (
        "A6",
        "A",
        "gguf-tensors",
        "metadata framing: n_kv written as n_kv + 1",
        "    p = w_u64(out, p, n_tensors);\n"
        "    p = w_u64(out, p, st[S_N]);",
        "    p = w_u64(out, p, n_tensors);\n"
        "    p = w_u64(out, p, st[S_N] + (1 as i64));",
        1,
    ),
    (
        "B1",
        "B",
        "gguf-tensors",
        "tensor descriptor: n_dims always 1",
        "fn t_dims(tne: const i64[:], i: i64) -> i64 {\n"
        "    var d: i64 = 3 as i64;\n"
        "    while d >= 1 as i64 {\n"
        "        if tne[i * 4 as i64 + d] > 1 as i64 {\n"
        "            return d + (1 as i64);\n"
        "        }\n"
        "        d -= 1 as i64;\n"
        "    }\n"
        "    return 1 as i64;\n"
        "}",
        "fn t_dims(tne: const i64[:], i: i64) -> i64 {\n"
        "    return 1 as i64;\n"
        "}",
        1,
    ),
    (
        "B2",
        "B",
        "gguf-tensors",
        "tensor offsets: cumulative stride without the alignment pad",
        "                trec[p * TR_STRIDE + TR_OFFSET] + t_pad(nb, tst[TS_ALIGN]);",
        "                trec[p * TR_STRIDE + TR_OFFSET] + nb;",
        1,
    ),
    (
        "B3",
        "B",
        "gguf-tensors",
        "data section: no pad after each tensor's data",
        "        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);\n"
        "        p = w_pad(out, p, a);",
        "        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);",
        1,
    ),
    (
        "B4",
        "B",
        "gguf-split",
        "split threshold by size: >= instead of >",
        "    if mode == 2 as i64 {\n"
        "        if next_size > n_bytes_split {",
        "    if mode == 2 as i64 {\n"
        "        if next_size >= n_bytes_split {",
        1,
    ),
    (
        "B5",
        "B",
        "gguf-split",
        "split threshold by tensor count: boundary shifted by one",
        "           i_tensor % n_split_tensors == 0 as i64 {",
        "           (i_tensor + 1 as i64) % n_split_tensors == 0 as i64 {",
        1,
    ),
    (
        "B6",
        "B",
        "gguf-split",
        "plan bounds: last shard ends at n_tensors - 1",
        "            t1[s2] = n_tensors;",
        "            t1[s2] = n_tensors - (1 as i64);",
        1,
    ),
]


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True, **kw)


def compose_canonical() -> str:
    """Rebuild the composed unit from the canonical modules and return its text."""
    dest = ROOT / "mut" / "canon.goose"
    dest.parent.mkdir(parents=True, exist_ok=True)
    r = run([sys.executable, "tools/compose-goose.py", str(dest),
             "goose/model/gguf-metadata.goose", "goose/model/gguf-writer.goose",
             "goose/model/gguf-tensors.goose", "goose/model/gguf-split.goose"])
    if r.returncode != 0:
        sys.exit("compose failed:\n" + r.stderr)
    return dest.read_text()


def generate(unit_goose: Path, unit_c: Path, unit_h: Path) -> None:
    r = run([GOOSE, "--standalone", "-o", str(unit_c), "--header", str(unit_h),
             str(unit_goose)])
    if r.returncode != 0:
        sys.exit("goose failed:\n" + r.stdout + r.stderr)


def build(jobs: int) -> None:
    r = run(["cmake", "--build", "build-mut", "-j", str(jobs),
             "--target", "fg-gguf-smoke", "fg-gguf-make", "fg-gguf-split"])
    if r.returncode != 0:
        sys.exit("build-mut failed:\n" + r.stdout[-4000:] + r.stderr[-4000:])


def first_failure(text: str) -> str:
    for line in text.splitlines():
        if line.startswith("FAIL"):
            return line.replace("\t", " ")
    return ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default=str(ROOT / "mut"))
    ap.add_argument("--jobs", type=int, default=32)
    args = ap.parse_args()

    work = Path(args.work)
    canon_text = compose_canonical()

    # The mutation build compiles the mutated unit at -O0; the canonical builds
    # are untouched.
    unit_goose = work / "unit.goose"
    unit_c = work / "unit.c"
    unit_h = work / "unit.h"
    for p in (unit_goose, unit_c, unit_h):
        p.parent.mkdir(parents=True, exist_ok=True)

    # Canonical unit through the same pipeline, so a mutation is the only change.
    # It has to exist before the first configure: the unit is a source file.
    unit_goose.write_text(canon_text)
    generate(unit_goose, unit_c, unit_h)

    r = run(["cmake", "-S", ".", "-B", "build-mut", "-DCMAKE_BUILD_TYPE=Release",
             "-DBUILD_SHARED_LIBS=ON", "-DFG_GOOSE_JSON_SCHEMA=OFF",
             "-DFG_GOOSE_GGUF=ON", "-DFG_GGUF_MUT=ON",
             f"-DFG_GGUF_UNIT={unit_c}"])
    if r.returncode != 0:
        sys.exit("configure of build-mut failed:\n" + r.stdout[-4000:] + r.stderr[-4000:])
    build(args.jobs)
    base_smoke = run([str(ROOT / "build-mut" / "fg-gguf-smoke")])
    base_split = run(["bash", "scripts/split-gate.sh", str(work / "gate-canon")],
                     env={**os.environ, "OFF_DIR": str(ROOT / "build-off"),
                          "ON_DIR": str(ROOT / "build-mut")})
    if base_smoke.returncode != 0 or base_split.returncode != 0:
        sys.exit("the unmutated build-mut does not pass its own gates:\n"
                 + base_smoke.stdout[-2000:] + base_split.stdout[-2000:])

    rows = []
    for mid, half, module, mechanism, needle, repl, occurrence in MUTATIONS:
        count = canon_text.count(needle)
        if count < occurrence:
            sys.exit(f"{mid}: anchor found {count} time(s), need occurrence {occurrence}:\n{needle}")
        # replace only the requested occurrence
        idx = -1
        for _ in range(occurrence):
            idx = canon_text.find(needle, idx + 1)
        mutated = canon_text[:idx] + repl + canon_text[idx + len(needle):]
        if mutated == canon_text:
            sys.exit(f"{mid}: mutation did not change the unit")

        unit_goose.write_text(mutated)
        generate(unit_goose, unit_c, unit_h)
        build(args.jobs)

        smoke = run([str(ROOT / "build-mut" / "fg-gguf-smoke")])
        split = run(["bash", "scripts/split-gate.sh", str(work / f"gate-{mid}")],
                    env={**os.environ, "OFF_DIR": str(ROOT / "build-off"),
                         "ON_DIR": str(ROOT / "build-mut")})
        detected = smoke.returncode != 0 or split.returncode != 0
        where = []
        if smoke.returncode != 0:
            where.append("smoke: " + (first_failure(smoke.stdout) or "non-zero exit"))
        if split.returncode != 0:
            where.append("split: " + (first_failure(split.stdout) or "non-zero exit"))
        rows.append((mid, half, module, mechanism, detected, "; ".join(where)))
        print(f"{mid}\t{half}\t{'DETECTED' if detected else 'UNDETECTED'}\t{mechanism}")
        sys.stdout.flush()

    # restore the canonical unit so a later build of build-mut is clean again
    unit_goose.write_text(canon_text)
    generate(unit_goose, unit_c, unit_h)
    build(args.jobs)

    half_a = sum(1 for r in rows if r[1] == "A" and r[4])
    half_b = sum(1 for r in rows if r[1] == "B" and r[4])
    total = sum(1 for r in rows if r[4])

    lines = ["FG630_MUTATIONS"]
    lines.append(f"unit: composed/gguf-stack.goose (4 modules), mutated unit built at -O0, "
                 f"native build-off as the oracle")
    lines.append("")
    for mid, half, module, mechanism, detected, where in rows:
        lines.append(f"{mid}\t{half}\t{module}\t{'DETECTED' if detected else 'UNDETECTED'}"
                     f"\t{mechanism}\t{where}")
    lines.append("")
    lines.append(f"half A: caught {half_a}/6")
    lines.append(f"half B: caught {half_b}/6")
    lines.append(f"total: caught {total}/12")
    text = "\n".join(lines) + "\n"
    (ROOT / "evidence" / "mutations.txt").write_text(text)
    print(text)
    return 0 if total == len(MUTATIONS) else 1


if __name__ == "__main__":
    sys.exit(main())
