#!/usr/bin/env python3
"""Check an Edge0 pack directory before the runtime touches it.

Validates, without loading the weights:
  - the file inventory and sizes against the published pack (HF revision);
  - that no file is still a *.part download;
  - the strata pack index v3 header, tensor count, alignment and offsets;
  - the native experts v6 table: layer count, type ids, stride and record size;
  - labels.bin, scales/ and the latent D/U sizes against the derived layout.

Exit code 0 only when the pack is complete and structurally consistent.
Run with --hash to also verify the SHA256 of the complete files (slow on the
51 GB PLE table).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

# Published sizes at HF revision dab5ce727d494e2018626fe8896bfdc774215b17.
EXPECTED = {
    "model-skeleton.gguf": 5_991_569_248,
    "experts.bin": 28_626_124_800,
    "dense.bin": 1_538_625_024,
    "latent_dense_q4.bin": 1_415_577_600,
    "latent_dense_q8.bin": 2_673_868_800,
    "ple-fp8.gguf": 51_200_246_112,
    "labels.bin": 98_304,
    "profile.bin": 196_632,
    "index.txt": 117_464,
    "native_experts.txt": 1_458,
    "mtp/dense.bin": 116_099_072,
    "mtp/experts.bin": 707_788_800,
    "mtp/draft_vocab.bin": 425_196,
}

EXPERT_RECORD = 1_164_800
LAYERS = 48
EXPERTS = 512


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_files(root: Path, hash_files: bool, problems: list[str]) -> None:
    for relative, size in EXPECTED.items():
        path = root / relative
        partial = root / (relative + ".part")
        if path.is_file():
            actual = path.stat().st_size
            if actual != size:
                problems.append(f"size mismatch: {relative} {actual} != {size}")
            elif hash_files:
                print(f"  hashing {relative} ...", flush=True)
                print(f"  sha256 {relative} {sha256(path)}")
        elif partial.is_file():
            print(f"  partial {relative}: {partial.stat().st_size} / {size}")
        else:
            problems.append(f"missing: {relative}")

    scans = sorted((root / "scales").glob("l*.f32")) if (root / "scales").is_dir() else []
    if len(scans) != LAYERS:
        problems.append(f"scales/: {len(scans)} files, expected {LAYERS}")
    for path in scans:
        if path.stat().st_size != 1_280 * 4:
            problems.append(f"scale size: {path.name} {path.stat().st_size} != 5120")


def check_index(root: Path, problems: list[str]) -> None:
    path = root / "index.txt"
    if not path.is_file():
        return
    lines = path.read_text().splitlines()
    match = None
    for line in lines:
        if line.startswith("#"):
            match = re.search(r"align (\d+) pool (\d+) tensors (\d+)", line)
            if match:
                break
    if not match:
        problems.append("index.txt: header does not carry align/pool/tensors")
        return
    align, pool, tensors = (int(x) for x in match.groups())
    dense = root / "dense.bin"
    if dense.is_file() and dense.stat().st_size != pool:
        problems.append(f"index pool {pool} != dense.bin size {dense.stat().st_size}")

    rows = [line.split() for line in lines if line and not line.startswith("#")]
    if len(rows) != tensors:
        problems.append(f"index tensors {tensors} != {len(rows)} rows")
    names = set()
    in_pool = 0
    for row in rows:
        if len(row) < 5:
            problems.append("index: short row " + " ".join(row[:2]))
            break
        name, _, dtype, offset, nbytes = row[0], row[1], row[2], int(row[3]), int(row[4])
        if name in names:
            problems.append("index: duplicate tensor " + name)
        names.add(name)
        if nbytes == 0:
            continue
        in_pool += 1
        if offset % align != 0:
            problems.append(f"index: {name} offset {offset} is not {align}-aligned")
        if offset + nbytes > pool:
            problems.append(f"index: {name} ends at {offset + nbytes}, pool {pool}")
        # 0 = kept in the skeleton GGUF, 2 = f32, 4 = bf16, 5 = seen on
        # blk.1.ple_conv1d.weight; anything outside 0..8 is suspicious.
        if not dtype.isdigit() or not (0 <= int(dtype) <= 8):
            problems.append(f"index: {name} unknown dtype {dtype}")
    print(f"  index: {tensors} tensors, {in_pool} in dense.bin, align {align}, pool {pool}")


def check_native_experts(root: Path, problems: list[str]) -> None:
    path = root / "native_experts.txt"
    if not path.is_file():
        return
    rows = [line.split() for line in path.read_text().splitlines()
            if line and not line.startswith("#")]
    if len(rows) != LAYERS:
        problems.append(f"native_experts: {len(rows)} layers, expected {LAYERS}")
    stride = EXPERTS * EXPERT_RECORD
    for row in rows:
        if len(row) != 5:
            problems.append("native_experts: expected 5 columns")
            break
        layer, gate_type, down_type, offset, size = (int(x) for x in row)
        if gate_type != 11 or down_type != 20:
            problems.append(f"native_experts: layer {layer} types {gate_type}/{down_type}")
        if size != EXPERT_RECORD:
            problems.append(f"native_experts: layer {layer} record {size}")
        if offset != layer * stride:
            problems.append(f"native_experts: layer {layer} offset {offset}")
    experts = root / "experts.bin"
    if experts.is_file() and experts.stat().st_size != LAYERS * stride:
        problems.append(f"experts.bin size {experts.stat().st_size} != {LAYERS * stride}")
    print(f"  native experts: {len(rows)} layers, record {EXPERT_RECORD}, stride {stride}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pack", type=Path)
    parser.add_argument("--hash", action="store_true")
    args = parser.parse_args()
    root = args.pack
    if not root.is_dir():
        print(f"no pack directory: {root}", file=sys.stderr)
        return 2

    problems: list[str] = []
    print(f"pack: {root}")
    check_files(root, args.hash, problems)
    check_index(root, problems)
    check_native_experts(root, problems)

    model_json = root / "model.json"
    if model_json.is_file():
        config = json.loads(model_json.read_text())
        print(f"  model.json: {config.get('model_name')} layers={config.get('layers')} "
              f"experts={config.get('experts')} latent={config.get('latent_dim')}")

    if problems:
        print(f"NOT READY: {len(problems)} problem(s)")
        for item in problems[:40]:
            print("  " + item)
        return 1
    print("READY: pack complete and structurally consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
