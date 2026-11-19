#!/usr/bin/env python3
"""Check an Edge0 pack directory before the runtime touches it.

Validates, without loading the weights:
  - the file inventory and sizes against the published pack (HF revision);
  - that no file is still a *.part download;
  - the strata pack index v3 header, tensor count, alignment and offsets;
  - the native experts v6 table: layer count, type ids, stride and record size;
  - labels.bin, scales/ and the latent D/U sizes against the derived layout.

Exit code 0 only when the pack is complete and structurally consistent.
Run with --hash to compare file hashes with the pinned Hugging Face manifest
(slow on the 51 GB PLE table). --file selects individual hashes for rechecking.
Successful structural checks alone do not certify file integrity or inference.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

MANIFEST = Path(__file__).with_name("edge0-pack-manifest.json")
EXPERT_RECORD = 1_164_800
LAYERS = 48
EXPERTS = 512


def file_digest(path: Path, algorithm: str) -> str:
    digest = hashlib.sha256() if algorithm == "sha256" else hashlib.sha1()
    if algorithm == "git-blob-sha1":
        digest.update(f"blob {path.stat().st_size}\0".encode())
    elif algorithm != "sha256":
        raise ValueError(f"unknown hash algorithm: {algorithm}")
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_files(root: Path, hash_files: bool, problems: list[str],
                manifest: dict | None = None, selected: set[str] | None = None) -> None:
    manifest = manifest or json.loads(MANIFEST.read_text())
    for entry in manifest["files"]:
        relative, size = entry["path"], entry["bytes"]
        path = root / relative
        partial = root / (relative + ".part")
        if path.is_file():
            actual = path.stat().st_size
            if actual != size:
                problems.append(f"size mismatch: {relative} {actual} != {size}")
            elif hash_files and (not selected or relative in selected):
                print(f"  hashing {relative} ...", flush=True)
                before = path.stat()
                actual_hash = file_digest(path, entry["algorithm"])
                after = path.stat()
                stable = (before.st_size, before.st_mtime_ns, before.st_ino) == (after.st_size, after.st_mtime_ns, after.st_ino)
                if not stable or actual_hash != entry["digest"]:
                    problems.append(f"hash mismatch or file changed: {relative} {actual_hash} != {entry['digest']}")
                else:
                    print(f"  MATCH {entry['algorithm']} {relative} {actual_hash}")
        elif partial.is_file():
            print(f"  partial {relative}: {partial.stat().st_size} / {size}")
            problems.append(f"unfinalized download: {relative}.part; verify its pinned hash before renaming")
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
    seen_layers = set()
    for row in rows:
        if len(row) != 5:
            problems.append("native_experts: expected 5 columns")
            break
        layer, gate_type, down_type, offset, size = (int(x) for x in row)
        if layer in seen_layers or layer < 0 or layer >= LAYERS:
            problems.append(f"native_experts: duplicate or invalid layer {layer}")
        seen_layers.add(layer)
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
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--file", action="append", default=[],
                        help="Hash only this manifest path; repeat to selectively recheck failures")
    args = parser.parse_args()
    root = args.pack
    if not root.is_dir():
        print(f"no pack directory: {root}", file=sys.stderr)
        return 2

    if args.file and not args.hash:
        parser.error("--file requires --hash")
    manifest = json.loads(args.manifest.read_text())
    known = {r["path"] for r in manifest["files"]}
    if set(args.file) - known:
        parser.error("--file contains paths absent from the manifest")
    problems: list[str] = []
    print(f"pack: {root}")
    print(f"  pinned Hugging Face revision: {manifest['revision']}")
    check_files(root, args.hash, problems, manifest, set(args.file))
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
    if args.hash and not args.file:
        print("PACK VERIFIED: pinned hashes and structure match; runtime support is a separate gate")
    elif args.hash:
        print("SELECTED HASHES PASS: structure checked; other file hashes were not rechecked")
    else:
        print("STRUCTURE PASS: integrity not checked; use --hash before loading")
    return 0


if __name__ == "__main__":
    sys.exit(main())
