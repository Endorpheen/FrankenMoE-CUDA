#!/usr/bin/env python3
"""Hash-bound patch: connect the accepted Goose metadata module to the real
llm_graph_context constructor.

Inputs (read-only):
  /inputs/active-llama-graph.cpp
  /inputs/native/src/llama-graph.h
Outputs (worker-4/graph-metadata):
  patched/llama-graph.cpp, patched/llama-graph.h, patch-manifest.json

The patch keeps the class layout, const references and initializer semantics:
the public constructor delegates to a private constructor that projects a
Goose-derived fg153_graph_meta frame. The original constructor is retained
verbatim under FG153_NATIVE_ROLLBACK.
"""
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CPP_IN = Path("/inputs/active-llama-graph.cpp")
H_IN = Path("/inputs/native/src/llama-graph.h")
CPP_OUT = HERE / "patched" / "llama-graph.cpp"
H_OUT = HERE / "native" / "src" / "llama-graph.h"
MANIFEST = HERE / "patch-manifest.json"

CTOR_START = "llm_graph_context::llm_graph_context(const llm_graph_params & params) :"
CTOR_END = "        res->set_params(params);\n    }"

H_FWD_ANCHOR = "struct llm_graph_context {"
H_CTOR_ANCHOR = "    llm_graph_context(const llm_graph_params & params);"

H_FWD_NEW = """struct fg153_graph_meta;

struct llm_graph_context {"""
H_CTOR_NEW = """    llm_graph_context(const llm_graph_params & params);
#ifndef FG153_NATIVE_ROLLBACK
    // Layout-preserving delegating constructor: every const member is projected
    // from the Goose-derived metadata frame (graph-context.goose). The original
    // initializer list is compiled only under FG153_NATIVE_ROLLBACK.
    llm_graph_context(const llm_graph_params & params, const fg153_graph_meta & meta);
#endif"""

GOOSE_BRANCH = """#ifndef FG153_NATIVE_ROLLBACK
static fg153_graph_meta fg153_meta_for(const llm_graph_params & params) {
    fg153_graph_meta meta = {};
    if (fg153_graph_meta_fill(params, meta) != 0) {
        throw std::runtime_error("Goose graph metadata derivation failed");
    }
    return meta;
}

llm_graph_context::llm_graph_context(const llm_graph_params & params)
    : llm_graph_context(params, fg153_meta_for(params)) {}

llm_graph_context::llm_graph_context(const llm_graph_params & params, const fg153_graph_meta & meta) :
    arch             ((enum llm_arch) meta.arch),
    hparams          (params.hparams),
    cparams          (params.cparams),
    ubatch           (params.ubatch),
    n_embd           (meta.n_embd),
    n_layer          (meta.n_layer),
    n_layer_nextn    (meta.n_layer_nextn),
    n_rot            (meta.n_rot),
    n_ctx            (meta.n_ctx),
    n_head           (meta.n_head),
    n_head_kv        (meta.n_head_kv),
    n_embd_head_k    (meta.n_embd_head_k),
    n_embd_k_gqa     (meta.n_embd_k_gqa),
    n_embd_head_v    (meta.n_embd_head_v),
    n_embd_v_gqa     (meta.n_embd_v_gqa),
    n_expert         (meta.n_expert),
    n_expert_used    (meta.n_expert_used),
    freq_base        (meta.freq_base),
    freq_scale       (meta.freq_scale),
    ext_factor       (meta.ext_factor),
    attn_factor      (meta.attn_factor),
    beta_fast        (meta.beta_fast),
    beta_slow        (meta.beta_slow),
    norm_eps         (meta.norm_eps),
    norm_rms_eps     (meta.norm_rms_eps),
    n_tokens         (meta.n_tokens),
    n_outputs        (meta.n_outputs),
    n_ctx_orig       ((int32_t) meta.n_ctx_orig),
    pooling_type     ((enum llama_pooling_type) meta.pooling_type),
    rope_type        ((enum llama_rope_type) meta.rope_type),
    sched            ((ggml_backend_sched_t) (intptr_t) meta.sched),
    backend_cpu      ((ggml_backend_t) (intptr_t) meta.backend_cpu),
    cvec             ((const llama_adapter_cvec *) (intptr_t) meta.cvec),
    loras            ((const llama_adapter_loras *) (intptr_t) meta.loras),
    mctx             ((const llama_memory_context_i *) (intptr_t) meta.mctx),
    cross            ((const llama_cross *) (intptr_t) meta.cross),
    moe_cache        ((const llama_moe_cache *) (intptr_t) meta.moe_cache),
    prec_policy      ((const llama_prec_policy *) (intptr_t) meta.prec_policy),
    hdmd             ((const llama_hadamard *) (intptr_t) meta.hdmd),
    samplers         (params.samplers),
    cb_func          (params.cb),
    res              ((llm_graph_result *) (intptr_t) meta.res),
    ctx0             (res->get_ctx()),
    gf               (res->get_gf()) {
        res->set_params(params);
    }
#else
"""


def sha256(p):
    h = hashlib.sha256()
    with p.open("rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def once(src, old, new, name):
    if src.count(old) != 1:
        raise SystemExit(f"anchor '{name}' count={src.count(old)}")
    return src.replace(old, new, 1)


def main():
    cpp = CPP_IN.read_text(encoding="utf-8")
    hdr = H_IN.read_text(encoding="utf-8")
    base_cpp_sha = sha256(CPP_IN)
    base_h_sha = sha256(H_IN)

    if cpp.count(CTOR_START) != 1 or cpp.count(CTOR_END) != 1:
        raise SystemExit("constructor anchors missing/ambiguous")
    i = cpp.index(CTOR_START)
    j = cpp.index(CTOR_END, i) + len(CTOR_END)
    orig_ctor = cpp[i:j]
    cpp = cpp[:i] + GOOSE_BRANCH + orig_ctor + "\n#endif" + cpp[j:]

    cpp = once(cpp, '#include "llama-graph.h"',
               '#include "llama-graph.h"\n#include "graph-meta.h"', "include")
    hdr = once(hdr, H_FWD_ANCHOR, H_FWD_NEW, "header_forward")
    hdr = once(hdr, H_CTOR_ANCHOR, H_CTOR_NEW, "header_ctor")

    CPP_OUT.parent.mkdir(parents=True, exist_ok=True)
    CPP_OUT.write_text(cpp, encoding="utf-8")
    H_OUT.write_text(hdr, encoding="utf-8")
    accepted_dir = Path("/inputs/accepted-graph-result")
    accepted = {}
    for name in ["graph-context.goose", "graph-meta.h", "graph-bridge.h",
                 "graph-services.h", "graph-services.cpp",
                 "graph-context-adapter.cpp", "goose-entry-graph-context.c"]:
        p = accepted_dir / name
        if not p.is_file():
            raise SystemExit(f"accepted module file missing: {p}")
        accepted[name] = sha256(p)
    manifest = {
        "schema": "fg153.graph-metadata-patch/1",
        "base_cpp": "inputs/active-llama-graph.cpp",
        "base_cpp_sha256": base_cpp_sha,
        "base_h": "inputs/native/src/llama-graph.h",
        "base_h_sha256": base_h_sha,
        "patched_cpp": "worker-4/graph-metadata/patched/llama-graph.cpp",
        "patched_cpp_sha256": sha256(CPP_OUT),
        "patched_h": "worker-4/graph-metadata/native/src/llama-graph.h",
        "patched_h_sha256": sha256(H_OUT),
        "accepted_module": accepted,
        "constructor_anchor": CTOR_START,
        "default": "goose-metadata",
        "rollback": "FG153_NATIVE_ROLLBACK",
        "delegating_ctor": True,
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "PATCHED", "patched_cpp_sha256": manifest["patched_cpp_sha256"]}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
