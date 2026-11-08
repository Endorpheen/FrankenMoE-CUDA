#!/usr/bin/env python3
"""Hash-bound installer for matrix-closure (quantize.cu + mmq.cu).

    python3 apply_closure.py --root /path/to/llama.cpp-copy [--check-only]

The tree must already contain the previous matrix candidate (its real mmvq.cu
seam is left untouched).  Base and result hashes are verified against
patch_manifest.json.
"""
import argparse, hashlib, json, shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_patches as mp  # same edit functions used to build the patch files

MAN = json.loads((HERE / "patch_manifest.json").read_text())

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--check-only", action="store_true")
    a = ap.parse_args()
    root = Path(a.root)
    edits = {"quantize.cu": mp.edit_quantize, "mmq.cu": mp.edit_mmq}
    for name, fn in edits.items():
        dst = root / "ggml/src/ggml-cuda" / name
        if not dst.is_file():
            print(f"missing {dst}", file=sys.stderr); return 2
        cur = sha(dst)
        want = MAN["files"][name]["patched_sha256"]
        base = MAN["files"][name]["base_sha256"]
        if cur == want:
            print(f"{name}: already installed")
            continue
        if cur != base:
            print(f"{name}: base hash mismatch", file=sys.stderr); return 3
        out = fn(dst.read_text())
        if a.check_only:
            if hashlib.sha256(out.encode()).hexdigest() != want:
                print(f"{name}: anchors produce wrong result", file=sys.stderr); return 4
            print(f"{name}: check-only anchors ok")
            continue
        dst.write_text(out)
        if sha(dst) != want:
            print(f"{name}: patched hash mismatch", file=sys.stderr); return 5
        print(f"patched {dst}")
    if a.check_only:
        return 0
    cd = root / "ggml/src/ggml-cuda"
    gd = cd / "goose_closure"
    gd.mkdir(parents=True, exist_ok=True)
    copies = [
        ("apex_closure_glue.h", cd / "apex_closure_glue.h"),
        ("apex_closure_glue.h", gd / "apex_closure_glue.h"),
        ("prims.cuh", gd / "prims.cuh"),
        ("emitter_ids.h", gd / "emitter_ids.h"),
        ("closure_generated.c", gd / "closure_generated.c"),
        ("closure_generated.cu", gd / "closure_generated.cu"),
        ("closure_device.cu", gd / "closure_device.cu"),
    ]
    for src, dst in copies:
        shutil.copy2(HERE / src, dst)
        if sha(dst) != sha(HERE / src):
            print(f"copy hash mismatch {dst}", file=sys.stderr); return 6
        print(f"installed {dst}")
    (gd / "build_fragment.cmake").write_text(
        "# Append to ggml/src/ggml-cuda/CMakeLists.txt (CUDA backend sources).\n"
        "list(APPEND GGML_CUDA_SOURCES\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/goose_closure/closure_device.cu)\n")
    print("add goose_closure/closure_device.cu to the CUDA backend source list")
    return 0

if __name__ == "__main__":
    sys.exit(main())
