#!/usr/bin/env python3
"""Build the hash-bound matrix-closure patches from the frozen /inputs tree.

The parent has the previous matrix candidate applied (real mmvq.cu seam).  This
script does not touch that seam: it adds Goose q8_1 activation packing and MMQ
prefill routing in quantize.cu and mmq.cu.
"""
import difflib, hashlib, json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OMO = HERE.parents[1]
INPUTS = OMO / "inputs/native/ggml/src/ggml-cuda"
OUT = HERE / "patched"
PATCHES = HERE / "patches"

Q_INC_OLD = '#include "quantize.cuh"\n'
Q_INC_NEW = Q_INC_OLD + '#include "apex_closure_glue.h"\n'
Q_ROW_ANCHOR = '''    GGML_ASSERT(!ids);
    GGML_ASSERT(ne0 % QK8_1 == 0);
'''
Q_ROW_HOOK = Q_ROW_ANCHOR + '''
    if (fgc_cuda_enabled() && (type_src0 == GGML_TYPE_Q4_K || type_src0 == GGML_TYPE_Q6_K)) {
        if (fgc_cuda_quant_row_q8_1(x, vy, ne00, s01, s02, s03, ne0, ne1, ne2, ne3, stream) == 0) {
            return;
        }
    }
'''
Q_MMQ_ANCHOR = '''    GGML_ASSERT(ne00 % 4 == 0);
    GGML_ASSERT(ne0 % QK8_1_MMQ == 0);
'''
Q_MMQ_HOOK = Q_MMQ_ANCHOR + '''
    if (!ids && (type_src0 == GGML_TYPE_Q4_K || type_src0 == GGML_TYPE_Q6_K) && fgc_cuda_enabled()) {
        const mmq_q8_1_ds_layout ds = mmq_get_q8_1_ds_layout(type_src0);
        const int64_t layout = ds == MMQ_Q8_1_DS_LAYOUT_DS4 ? 0 : (ds == MMQ_Q8_1_DS_LAYOUT_D4 ? 1 : -1);
        if (layout >= 0 && fgc_cuda_quant_mmq_q8_1(x, vy, ne00, s01, s02, s03, ne0, ne1, ne2, ne3, layout, stream) == 0) {
            return;
        }
    }
'''
M_INC_OLD = '#include "mmid.cuh"\n'
M_INC_NEW = M_INC_OLD + '#include "apex_closure_glue.h"\n'
M_ANCHOR = '''    const bool fallback = ggml_cuda_mmq_needs_fallback(ne01);

    const ggml_prec prec_src1 = ggml_cuda_mmq_get_prec_src1(src0, dst, cc);
'''
M_HOOK = M_ANCHOR + '''
    if (!ids && (src0->type == GGML_TYPE_Q4_K || src0->type == GGML_TYPE_Q6_K) && fgc_cuda_enabled()) {
        const int64_t kind = src0->type == GGML_TYPE_Q4_K ? 12 : 14;
        const int64_t s01v = src0->nb[1] / ts_src0;
        const int64_t s11v = src1->nb[1] / ts_src1;
        const int64_t s12v = src1->nb[2] / ts_src1;
        const int64_t s13v = src1->nb[3] / ts_src1;
        const int64_t dstride = dst->nb[1] / ts_dst;
        if (fgc_cuda_mmq_dense(kind, src0_d, src1_d, dst_d, ne00, ne01, ne11,
                               s01v, dstride, s11v, s12v, s13v, ne12, ne13,
                               cc, (int64_t) smpbo, ggml_cuda_info().devices[id].nsm, stream) == 0) {
            return;
        }
    }
'''

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def edit_quantize(s):
    assert s.count(Q_INC_OLD) == 1
    s = s.replace(Q_INC_OLD, Q_INC_NEW, 1)
    assert s.count(Q_ROW_ANCHOR) == 1
    s = s.replace(Q_ROW_ANCHOR, Q_ROW_HOOK, 1)
    assert s.count(Q_MMQ_ANCHOR) >= 1
    s = s.replace(Q_MMQ_ANCHOR, Q_MMQ_HOOK, 1)
    return s

M_POLICY_ANCHOR = '''bool ggml_cuda_should_use_mmq(enum ggml_type type, int cc, int64_t ne11, int64_t n_experts) {
#ifdef GGML_CUDA_FORCE_CUBLAS
    return false;
#endif // GGML_CUDA_FORCE_CUBLAS

    bool mmq_supported;
'''
M_POLICY_HOOK = '''bool ggml_cuda_should_use_mmq(enum ggml_type type, int cc, int64_t ne11, int64_t n_experts) {
#ifdef GGML_CUDA_FORCE_CUBLAS
    return false;
#endif // GGML_CUDA_FORCE_CUBLAS

    // Goose owns the dense Q4_K/Q6_K prefill routing decision when it can
    // serve the geometry; otherwise the native policy below is unchanged.
    if (n_experts == 0 && (type == GGML_TYPE_Q4_K || type == GGML_TYPE_Q6_K) && fgc_cuda_enabled()) {
        const int id = ggml_cuda_get_device();
        int64_t reason = 0;
        if (fgc_cuda_route(type == GGML_TYPE_Q4_K ? 12 : 14, ne11, 1, cc,
                           (int64_t) ggml_cuda_info().devices[id].smpbo, 256, ne11,
                           ggml_cuda_info().devices[id].nsm, &reason) == 3) {
            return true;
        }
    }

    bool mmq_supported;
'''

def edit_mmq(s):
    assert s.count(M_INC_OLD) == 1
    s = s.replace(M_INC_OLD, M_INC_NEW, 1)
    assert s.count(M_ANCHOR) == 1
    s = s.replace(M_ANCHOR, M_HOOK, 1)
    assert s.count(M_POLICY_ANCHOR) == 1
    s = s.replace(M_POLICY_ANCHOR, M_POLICY_HOOK, 1)
    return s

def main():
    OUT.mkdir(exist_ok=True)
    PATCHES.mkdir(exist_ok=True)
    files = {
        "quantize.cu": (INPUTS / "quantize.cu", edit_quantize, "0001-goose-q8_1-packing.patch"),
        "mmq.cu": (INPUTS / "mmq.cu", edit_mmq, "0002-goose-mmq-prefill.patch"),
    }
    manifest = {"hash_algorithm": "sha256", "files": {}}
    for name, (src, fn, patch_name) in files.items():
        raw = src.read_text()
        out = fn(raw)
        (OUT / name).write_text(out)
        a = raw.splitlines(keepends=True)
        b = out.splitlines(keepends=True)
        diff = ''.join(difflib.unified_diff(a, b,
            fromfile="a/ggml/src/ggml-cuda/" + name,
            tofile="b/ggml/src/ggml-cuda/" + name))
        (PATCHES / patch_name).write_text(diff)
        manifest["files"][name] = {
            "base_sha256": sha(src),
            "patched_sha256": sha(OUT / name),
            "patch": "matrix-closure/patches/" + patch_name,
            "patch_sha256": sha(PATCHES / patch_name),
        }
    manifest["install"] = "python3 matrix-closure/apply_closure.py --root <llama.cpp-copy>"
    manifest["rollback"] = "GGML_GOOSE_CUDA_MATRIX=0"
    (HERE / "patch_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    ours = [
        "closure.goose", "closure_generated.c", "closure_generated.cu",
        "closure_device.cu", "prims.cuh", "prims.c", "emitter_ids.h",
        "emitter_ids.json", "verify_emitter.py", "qualify.py", "nvcc_iso.py",
        "apex_closure_glue.h", "closure_fixture.c", "apply_closure.py",
        "make_patches.py", "compat/bits/mathcalls.h",
    ]
    sh = {
        "generated_by": "worker-11/matrix-closure",
        "hash_algorithm": "sha256",
        "base_inputs": {name: manifest["files"][name]["base_sha256"] for name in manifest["files"]},
        "patched": {name: manifest["files"][name]["patched_sha256"] for name in manifest["files"]},
        "goose_and_units": {p: sha(HERE / p) for p in ours if (HERE / p).is_file()},
        "install": manifest["install"],
        "rollback": manifest["rollback"],
    }
    (HERE / "source-hashes.json").write_text(json.dumps(sh, indent=2) + "\n")
    print(json.dumps(manifest["files"], indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
