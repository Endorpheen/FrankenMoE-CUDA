#!/usr/bin/env python3
# replay.py - source-hash-bound exact patch of the native ggml-cuda MoE routing
# dispatch to the Goose-emitted kernels.  Inputs are read-only snapshots.
import difflib, hashlib, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = "/inputs/native/ggml/src/ggml-cuda"
OUT = os.path.join(HERE, "patched")
HEADER = os.path.join(HERE, "fg153-moe-cuda.h")
LOCK = os.path.join(HERE, "source-lock.json")

ARGSORT = "argsort.cu"
TOPK = "top-k.cu"
RED = "moe-weighted-reduction.cu"
LOCKED = [ARGSORT, TOPK, RED, "argsort.cuh", "top-k.cuh", "moe-weighted-reduction.cuh"]

INC_ARGSORT_A = '#include "argsort.cuh"\n'
INC_ARGSORT = INC_ARGSORT_A + '#include "moe-routing/fg153-moe-cuda.h"\n'
BODY_ARGSORT_A = "    enum ggml_sort_order order = (enum ggml_sort_order) dst->op_params[0];\n"
BODY_ARGSORT = BODY_ARGSORT_A + """
    // FG153 Goose argsort default-on path; GGML_CUDA_GOOSE_ARGSORT=0 rolls back.
    if (fg153_goose_argsort_launch(src0_d, (int *) dst_d, ncols, nrows, (int) order, stream)) {
        return;
    }
"""

INC_TOPK_A = '#include "top-k.cuh"\n'
INC_TOPK = INC_TOPK_A + '#include "moe-routing/fg153-moe-cuda.h"\n'
BODY_TOPK_A = "    const int64_t    k     = dst->ne[0];\n"
BODY_TOPK = BODY_TOPK_A + """
    // FG153 Goose top-k default-on path; GGML_CUDA_GOOSE_TOP_K=0 rolls back.
    if (fg153_goose_top_k_launch(src0_d, dst_d, ncols, nrows, k, stream)) {
        return;
    }
"""

INC_RED_A = '#include "moe-weighted-reduction.cuh"\n'
INC_RED = INC_RED_A + '#include "moe-routing/fg153-moe-cuda.h"\n'
BODY_RED_A = "    cudaStream_t  stream        = ctx.stream();\n"
BODY_RED = BODY_RED_A + """
    // FG153 Goose MoE reduction default-on path; GGML_CUDA_GOOSE_MOE_REDUCTION=0 rolls back.
    if (fg153_goose_moe_reduction_launch((const float *) experts->data,
            expert_scale ? (const float *) expert_scale->data : nullptr,
            (const float *) weights->data, (float *) dst->data,
            n_embd, n_tokens, (int) n_expert_used, stream)) {
        return;
    }
"""

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def patch_one(name, inc_anchor, inc, body_anchor, body):
    src = os.path.join(NATIVE, name)
    text = open(src, "r", encoding="utf-8").read()
    if inc_anchor not in text:
        raise SystemExit("missing include anchor in " + name)
    if body_anchor not in text:
        raise SystemExit("missing body anchor in " + name)
    if text.count(inc_anchor) != 1 or text.count(body_anchor) != 1:
        raise SystemExit("ambiguous anchor in " + name)
    text = text.replace(inc_anchor, inc, 1)
    text = text.replace(body_anchor, body, 1)
    return text

def emit():
    os.makedirs(OUT, exist_ok=True)
    patched = {
        ARGSORT: patch_one(ARGSORT, INC_ARGSORT_A, INC_ARGSORT, BODY_ARGSORT_A, BODY_ARGSORT),
        TOPK: patch_one(TOPK, INC_TOPK_A, INC_TOPK, BODY_TOPK_A, BODY_TOPK),
        RED: patch_one(RED, INC_RED_A, INC_RED, BODY_RED_A, BODY_RED),
    }
    for name, text in patched.items():
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(text)
    with open(HEADER, "r", encoding="utf-8") as f:
        header = f.read()
    with open(os.path.join(OUT, os.path.basename(HEADER)), "w", encoding="utf-8") as f:
        f.write(header)
    lock = {
        "inputs": {n: sha(os.path.join(NATIVE, n)) for n in LOCKED},
        "header": sha(HEADER),
        "outputs": {n: sha(os.path.join(OUT, n)) for n in list(patched) + [os.path.basename(HEADER)]},
        "anchors": {
            "argsort_include": INC_ARGSORT_A,
            "argsort_body": BODY_ARGSORT_A.strip(),
            "topk_include": INC_TOPK_A,
            "topk_body": BODY_TOPK_A.strip(),
            "reduce_include": INC_RED_A,
            "reduce_body": BODY_RED_A.strip(),
        },
        "cuh_locked_unchanged": ["argsort.cuh", "top-k.cuh", "moe-weighted-reduction.cuh"],
        "rollback": {
            "GGML_CUDA_GOOSE_ARGSORT": "0",
            "GGML_CUDA_GOOSE_TOP_K": "0",
            "GGML_CUDA_GOOSE_MOE_REDUCTION": "0",
        },
        "default": "on",
    }
    with open(LOCK, "w", encoding="utf-8") as f:
        json.dump(lock, f, indent=2, sort_keys=True)
        f.write("\n")
    for name, text in patched.items():
        old = open(os.path.join(NATIVE, name), "r", encoding="utf-8").read().splitlines(True)
        diff = list(difflib.unified_diff(old, text.splitlines(True), fromfile="a/" + name, tofile="b/" + name))
        with open(os.path.join(OUT, name + ".diff"), "w", encoding="utf-8") as f:
            f.writelines(diff)
    print("FG153_PATCH_EMIT argsort=%s topk=%s reduce=%s" %
          (lock["outputs"][ARGSORT][:16], lock["outputs"][TOPK][:16], lock["outputs"][RED][:16]))

def verify():
    lock = json.load(open(LOCK, "r", encoding="utf-8"))
    for n, want in lock["inputs"].items():
        if sha(os.path.join(NATIVE, n)) != want:
            raise SystemExit("input hash changed: " + n)
    for n, want in lock["outputs"].items():
        if sha(os.path.join(OUT, n)) != want:
            raise SystemExit("output hash changed: " + n)
    if lock.get("cuh_locked_unchanged") != ["argsort.cuh", "top-k.cuh", "moe-weighted-reduction.cuh"]:
        raise SystemExit("cuh lock list changed")
    for n in (ARGSORT, TOPK, RED):
        text = open(os.path.join(OUT, n), "r", encoding="utf-8").read()
        if "FG153 Goose" not in text or "0 rolls back" not in text:
            raise SystemExit("patch marker missing in " + n)
    print("FG153_PATCH_VERIFY inputs=%d outputs=%d cuh_locked=3 PASS" %
          (len(lock["inputs"]), len(lock["outputs"])))

def main():
    action = sys.argv[1] if len(sys.argv) > 1 else "verify"
    if action == "emit":
        emit()
    elif action == "verify":
        verify()
    else:
        raise SystemExit("usage: replay.py emit|verify")
if __name__ == "__main__":
    main()
