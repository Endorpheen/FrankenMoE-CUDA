#!/usr/bin/env python3
"""Hash-bound installer for the worker-11 Goose CUDA matrix seam.

    python3 apply_patch.py --root /path/to/llama.cpp-copy [--check-only]

It refuses to touch a tree whose ggml/src/ggml-cuda/mmvq.cu does not match the
recorded base hash, and it verifies the post-edit hash.  The same two anchor
edits are described by patches/0001-goose-cuda-matrix.patch; using anchors
keeps the operation exact even if the caller has a different patch utility.
"""
import argparse, hashlib, json, shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
HASHES = json.loads((HERE / "source-hashes.json").read_text())

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--check-only", action="store_true")
    a = ap.parse_args()
    root = Path(a.root)
    mmvq = root / "ggml/src/ggml-cuda/mmvq.cu"
    if not mmvq.is_file():
        print(f"missing {mmvq}", file=sys.stderr)
        return 2
    cur = sha(mmvq)
    if cur == HASHES["seam"]["patched_sha256"]:
        print("already installed")
        return 0
    if cur != HASHES["seam"]["base_sha256"]:
        print("refusing: mmvq.cu base hash mismatch", file=sys.stderr)
        return 3
    s = mmvq.read_text()
    inc = '#include "mmvq.cuh"\n'
    if 'apex_matrix_glue.h' in s:
        print("already installed", file=sys.stderr)
    else:
        assert s.count(inc) == 1
        s = s.replace(inc, inc + '#include "apex_matrix_glue.h"\n', 1)
        anchor = '''    const int stride_row_x = ne00 / ggml_blck_size(src0->type);
    const int stride_col_y = src1_padded_row_size / QK8_1;

    ggml_cuda_mm_fusion_args_device fusion_local{};'''
        hook = '''    const int stride_row_x = ne00 / ggml_blck_size(src0->type);
    const int stride_col_y = src1_padded_row_size / QK8_1;

    // Goose default-on Q4_K/Q6_K single-column matrix-vector path.  Only the
    // dense (non-expert, single-slice) layout is taken here; MAC/MoE and
    // multi-GPU row splits keep the native kernel.  GGML_GOOSE_CUDA_MATRIX=0
    // restores the native path unconditionally.
    if (src1_ncols == 1 && src0->ne[2] == 1 && src0->ne[3] == 1 &&
        (src0->type == GGML_TYPE_Q4_K || src0->type == GGML_TYPE_Q6_K) &&
        fgm_cuda_enabled()) {
        const int64_t kind = src0->type == GGML_TYPE_Q4_K ? 12 : 14;
        if (fgm_cuda_mul_mat_vec_kq(kind, src0_dd_i, src1_ddq_i, dst_dd_i, ne00,
                                    row_low, row_high, stride_row_x, stride_col_y,
                                    stream) == 0) {
            return;
        }
    }

    ggml_cuda_mm_fusion_args_device fusion_local{};'''
        assert s.count(anchor) == 1
        s = s.replace(anchor, hook, 1)
        if a.check_only:
            print("check-only: anchors found, hash would match")
        else:
            mmvq.write_text(s)
    if not a.check_only:
        if sha(mmvq) != HASHES["seam"]["patched_sha256"]:
            print("refusing: patched hash mismatch after edit", file=sys.stderr)
            return 4
        for row in HASHES["copy_map"]:
            src = HERE / row["from"].split("/", 1)[1]
            dst = root / row["to"]
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            if sha(dst) != sha(src):
                print(f"copy hash mismatch {dst}", file=sys.stderr)
                return 5
            print(f"installed {dst}")
        frag = root / "ggml/src/ggml-cuda/goose/build_fragment.cmake"
        frag.write_text(
            "# Append to ggml/src/ggml-cuda/CMakeLists.txt (the CUDA backend source list).\n"
            "list(APPEND GGML_CUDA_SOURCES\n"
            "    ${CMAKE_CURRENT_SOURCE_DIR}/goose/apex_matrix_device.cu)\n")
        print(f"wrote {frag}")
        print("add goose/apex_matrix_device.cu to the CUDA backend source list")
    return 0

if __name__ == "__main__":
    sys.exit(main())
