#!/usr/bin/env python3
# replay.py - hash-bound exact patch of the native ggml-cuda GDN/ssm_conv
# dispatch to the Goose-emitted kernels.  Inputs are read-only snapshots.
import hashlib, json, os, sys

ROOT = "/work" if os.path.isdir("/work/inputs") or os.path.isdir("/inputs/native") else os.path.dirname(os.path.abspath(__file__))
NATIVE = "/inputs/native/ggml/src/ggml-cuda"
OUT = os.path.join(ROOT, "patched")
HEADER = "fg153-goose-cuda.h"
GDN = "gated_delta_net.cu"
CONV = "ssm-conv.cu"
LOCK = os.path.join(ROOT, "source-lock.json")

GDN_INC_ANCHOR = '#include "ggml-cuda/common.cuh"\n'
GDN_INC = GDN_INC_ANCHOR + '#include "fg153-goose-cuda.h"\n'
GDN_BODY_ANCHOR = """    if (cache != nullptr) {
        state_d           = cache->data;
        state_slot_stride = cache->slot_stride;
    }

"""
GDN_BODY = GDN_BODY_ANCHOR + """    // FG153 Goose GDN default-on path; GGML_CUDA_GOOSE_GDN=0 rolls back.
    if (fg153_goose_gdn_launch(q_d, k_d, v_d, g_d, b_d, s_d, dst_d, state_d,
                               S_v, H, n_tokens, n_seqs,
                               sq1, sq2, sq3, sv1, sv2, sv3, sb1, sb2, sb3,
                               neqk1, rq3, scale, state_slot_stride, K, kda, keep_rs,
                               stream)) {
        return;
    }

"""
CONV_INC_ANCHOR = '#include "unary.cuh"\n'
CONV_INC = CONV_INC_ANCHOR + '#include "fg153-goose-cuda.h"\n'
CONV_BODY_ANCHOR = """    if (fuse_bias) {
        GGML_ASSERT(bias->type == GGML_TYPE_F32);
        GGML_ASSERT(ggml_is_contiguous(bias));
        GGML_ASSERT(ggml_nelements(bias) == nr);
    }

"""
CONV_BODY = CONV_BODY_ANCHOR + """    // FG153 Goose ssm_conv default-on path; GGML_CUDA_GOOSE_SSM_CONV=0 rolls back.
    if (fg153_goose_ssm_conv_launch(src0_d, src1_d, bias_d, dst_d, nc, nr, n_t, n_s,
            (int64_t) (src0->nb[0] / sizeof(float)),
            (int64_t) (src0->nb[1] / sizeof(float)),
            (int64_t) (src0->nb[2] / sizeof(float)),
            (int64_t) (src1->nb[1] / sizeof(float)),
            (int64_t) (out->nb[0] / sizeof(float)),
            (int64_t) (out->nb[1] / sizeof(float)),
            (int64_t) (out->nb[2] / sizeof(float)),
            fuse_silu, stream)) {
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
    gdn = patch_one(GDN, GDN_INC_ANCHOR, GDN_INC, GDN_BODY_ANCHOR, GDN_BODY)
    conv = patch_one(CONV, CONV_INC_ANCHOR, CONV_INC, CONV_BODY_ANCHOR, CONV_BODY)
    for name, text in ((GDN, gdn), (CONV, conv)):
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write(text)
    hdr = os.path.join(ROOT, HEADER)
    with open(hdr, "r", encoding="utf-8") as f:
        header = f.read()
    with open(os.path.join(OUT, HEADER), "w", encoding="utf-8") as f:
        f.write(header)
    lock = {
        "inputs": {n: sha(os.path.join(NATIVE, n)) for n in (GDN, CONV)},
        "header": sha(hdr),
        "outputs": {n: sha(os.path.join(OUT, n)) for n in (GDN, CONV, HEADER)},
        "anchors": {
            "gdn_include": GDN_INC_ANCHOR,
            "gdn_body": "if (cache != nullptr) { ... state_slot_stride = cache->slot_stride; }",
            "conv_include": CONV_INC_ANCHOR,
            "conv_body": "if (fuse_bias) { ... ggml_nelements(bias) == nr; }"
        },
        "rollback": {"GGML_CUDA_GOOSE_GDN": "0", "GGML_CUDA_GOOSE_SSM_CONV": "0"},
        "default": "on"
    }
    with open(LOCK, "w", encoding="utf-8") as f:
        json.dump(lock, f, indent=2, sort_keys=True)
        f.write("\n")
    # review diff
    import difflib
    for name, patched in ((GDN, gdn), (CONV, conv)):
        old = open(os.path.join(NATIVE, name), "r", encoding="utf-8").read().splitlines(True)
        new = patched.splitlines(True)
        diff = list(difflib.unified_diff(old, new, fromfile="a/" + name, tofile="b/" + name))
        with open(os.path.join(OUT, name + ".diff"), "w", encoding="utf-8") as f:
            f.writelines(diff)
    print("FG153_PATCH_EMIT gdn_sha=%s conv_sha=%s" %
          (lock["outputs"][GDN][:16], lock["outputs"][CONV][:16]))

def verify():
    lock = json.load(open(LOCK, "r", encoding="utf-8"))
    for n, want in lock["inputs"].items():
        got = sha(os.path.join(NATIVE, n))
        if got != want:
            raise SystemExit("input hash changed: " + n)
    for n, want in lock["outputs"].items():
        got = sha(os.path.join(OUT, n))
        if got != want:
            raise SystemExit("output hash changed: " + n)
    for n in (GDN, CONV):
        text = open(os.path.join(OUT, n), "r", encoding="utf-8").read()
        if "fg153_goose_" not in text or "0 rolls back" not in text:
            raise SystemExit("patch marker missing in " + n)
    print("FG153_PATCH_VERIFY inputs=%d outputs=%d PASS" %
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
