#!/usr/bin/env python3
"""Generate the default-on Goose-routed context TU from the pinned snapshot.

Input : context-original.cpp (copy of inputs/active-llama-context.cpp)
Output: patched/llama-context.cpp, patch-manifest.json

The script refuses to run if the copied source no longer matches
/inputs/active-llama-context.cpp, so the patch is hash-bound to the snapshot.
Every anchor is exact; drift is a hard error.
"""
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "context-original.cpp"
INPUTS_CPP = Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/helpers/omo5/inputs/active-llama-context.cpp')
INPUTS_H = Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/helpers/omo5/inputs/active-llama-context.h')
OUT = HERE / "patched" / "llama-context.cpp"
MANIFEST = HERE / "patch-manifest.json"


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


def span(src, start, end, new, name):
    if src.count(start) != 1 or src.count(end) != 1:
        raise SystemExit(f"span '{name}' anchors missing/ambiguous")
    i = src.index(start)
    j = src.index(end, i) + len(end)
    return src[:i] + new + src[j:]


ROUTE = """
#ifndef FG107_NATIVE_ROLLBACK
    {
        FG107ExecFrame frame(*this);
        const int64_t rc = fg107_goose_decode((int64_t)(intptr_t)&frame, (int64_t)(intptr_t)&batch_inp);
        if (rc != -100) {
            return (int) rc;
        }
        // rc == -100: no memory module; fall through to the native encode path
    }
#endif
"""

SCHED_RESERVE = """
#ifndef FG107_NATIVE_ROLLBACK
    {
        FG107ExecFrame frame(*this);
        if (fg107_goose_sched_reserve((int64_t)(intptr_t)&frame) != 0) {
            throw std::runtime_error("Goose sched_reserve failed");
        }
        return;
    }
#endif
"""

SCHED_NEW = """
#ifndef FG107_NATIVE_ROLLBACK
    FG107ExecFrame frame(*this);
    fg107_goose_sched_new((int64_t)(intptr_t)&frame, (int64_t) max_nodes, parallel ? 1 : 0);
    return;
#endif
"""

RESOLVE_FUSED = """
#ifndef FG107_NATIVE_ROLLBACK
    {
        FG107ExecFrame frame(*this);
        if (fg107_goose_resolve_fused((int64_t)(intptr_t)&frame, (int64_t)(intptr_t) mctx, (int64_t) n_seqs) != 0) {
            throw std::runtime_error("Goose resolve_fused_ops failed");
        }
        return;
    }
#endif
"""


def state_route(fn, sig, ret):
    return f"""
#ifndef FG107_NATIVE_ROLLBACK
    {{
        FG107ExecFrame frame(*this);
        const int64_t rc = {fn}((int64_t)(intptr_t)&frame{sig});
        if (rc >= 0) return ({ret}) rc;
    }}
#endif
"""


GRAPH_CB_OLD = '''llm_graph_cb llama_context::graph_get_cb() const {
    return [&](const llama_ubatch & ubatch, ggml_tensor * cur, const char * name, int il) {
        if (il >= 0) {
            ggml_format_name(cur, "%s-%d", name, il);
        } else {
            ggml_set_name(cur, name);
        }

        // - norm may be automatically assigned to the backend of the previous layer, increasing data transfer between backends
        // - force the last op of the layer on the specified backend to avoid running it on the backend of the next layer due to scheduling
        // FIXME: fix in ggml_backend_sched
        const bool full_offload = model.n_gpu_layers() > model.hparams.n_layer_all;
        if (ubatch.n_tokens < 32 || full_offload) {
            if (il != -1 && (strcmp(name, "norm") == 0 || strcmp(name, "l_last") == 0)) {
                const auto & dev_layer = model.dev_layer(il);
                for (const auto & backend : backends) {
                    if (ggml_backend_get_device(backend.get()) == dev_layer) {
                        if (ggml_backend_supports_op(backend.get(), cur)) {
                            ggml_backend_sched_set_tensor_backend(sched.get(), cur, backend.get());
                        }
                    }
                }
            }
        }
    };
}'''

GRAPH_CB_NEW = '''llm_graph_cb llama_context::graph_get_cb() const {
    return [this](const llama_ubatch & ubatch, ggml_tensor * cur, const char * name, int il) {
        if (il >= 0) {
            ggml_format_name(cur, "%s-%d", name, il);
        } else {
            ggml_set_name(cur, name);
        }
#ifndef FG107_NATIVE_ROLLBACK
        const bool is_norm = name && (strcmp(name, "norm") == 0 || strcmp(name, "l_last") == 0);
        const bool full_offload = model.n_gpu_layers() > model.hparams.n_layer_all;
        FG107ExecFrame frame(const_cast<llama_context &>(*this));
        const bool pin = fg107_goose_graph_cb_policy((int64_t)(intptr_t)&frame, (int64_t)ubatch.n_tokens,
                il, is_norm ? 1 : 0, (name && strcmp(name, "l_last") == 0) ? 1 : 0,
                full_offload ? 1 : 0) != 0;
        if (pin) {
            const auto & dev_layer = model.dev_layer(il);
            for (const auto & backend : backends) {
                if (ggml_backend_get_device(backend.get()) == dev_layer &&
                    ggml_backend_supports_op(backend.get(), cur)) {
                    ggml_backend_sched_set_tensor_backend(sched.get(), cur, backend.get());
                }
            }
        }
#endif
    };
}'''

REALLOC_OLD = '''void llama_context::sched_graph_realloc(ggml_backend_sched_t sched, ggml_cgraph * gf, bool backend_ids_changed, bool graph_size_changed, void * user_data) {
    // reallocating after the backend assignments changed is expected, trace only the unexpected ones
    if (backend_ids_changed) {
        return;
    }

    auto * lctx = static_cast<llama_context *>(user_data);

    if (graph_size_changed) {
        lctx->n_reallocs_diff++;
    } else {
        lctx->n_reallocs_same++;
    }

    LLAMA_LOG_INFO("%s: graph reallocation: nodes = %d (changed = %d), splits = %d, n_same = %d, n_diff = %d\\n",
            __func__, ggml_graph_n_nodes(gf), graph_size_changed, ggml_backend_sched_get_n_splits(sched),
            lctx->n_reallocs_same, lctx->n_reallocs_diff);
}'''

REALLOC_NEW = '''void llama_context::sched_graph_realloc(ggml_backend_sched_t sched, ggml_cgraph * gf, bool backend_ids_changed, bool graph_size_changed, void * user_data) {
#ifndef FG107_NATIVE_ROLLBACK
    auto * lctx = static_cast<llama_context *>(user_data);
    FG107ExecFrame frame(*lctx);
    fg107_goose_realloc_policy((int64_t)(intptr_t)&frame, backend_ids_changed ? 1 : 0,
            graph_size_changed ? 1 : 0, ggml_graph_n_nodes(gf), ggml_backend_sched_get_n_splits(sched));
#else
    if (backend_ids_changed) return;
    auto * lctx = static_cast<llama_context *>(user_data);
    if (graph_size_changed) lctx->n_reallocs_diff++; else lctx->n_reallocs_same++;
    LLAMA_LOG_INFO("%s: graph reallocation: nodes = %d (changed = %d), splits = %d, n_same = %d, n_diff = %d\\n",
            __func__, ggml_graph_n_nodes(gf), graph_size_changed, ggml_backend_sched_get_n_splits(sched),
            lctx->n_reallocs_same, lctx->n_reallocs_diff);
#endif
}'''

CPARAMS_START = "    if (cparams.rope_scaling_type == LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED) {"
CPARAMS_END = "            cparams.n_outputs_max : std::min(params.n_outputs_max_per_seq, cparams.n_outputs_max);"
CPARAMS_NEW = """    {
        FG107ExecFrame frame(*this);
        frame.params = &params;
        if (fg107_goose_cparams_policy((int64_t)(intptr_t)&frame) != 0) {
            throw std::runtime_error("Goose cparams policy failed");
        }
    }"""


def wrap_block(src, start, end, goose_body, name):
    if src.count(start) != 1 or src.count(end) != 1:
        raise SystemExit(f"block '{name}' anchors missing/ambiguous")
    i = src.index(start)
    j = src.index(end, i) + len(end)
    orig = src[i:j]
    new = "#ifndef FG107_NATIVE_ROLLBACK\n" + goose_body + "#else\n" + orig + "\n#endif"
    return src[:i] + new + src[j:]


CTOR_START = "    if (!hparams.vocab_only) {\n        // GPU backends"
CTOR_END = "        sampling.token_ids_full_vocab[i] = i;\n        }\n    }\n}"
CTOR_GOOSE = """    {
        FG107ExecFrame frame(*this);
        frame.abort_cb = (void *) params.abort_callback;
        frame.abort_data = params.abort_callback_data;
        frame.type_k = (int) params.type_k;
        frame.type_v = (int) params.type_v;
        frame.swa_full = params.swa_full;
        if (fg107_goose_resource_init((int64_t)(intptr_t)&frame) != 0) {
            throw std::runtime_error("Goose resource_init failed");
        }
    }
}
"""

DTOR_START = "llama_context::~llama_context() {"
DTOR_END = "    ggml_opt_free(opt_ctx);\n}"
DTOR_GOOSE = """llama_context::~llama_context() {
    {
        FG107ExecFrame frame(*this);
        fg107_goose_resource_release((int64_t)(intptr_t)&frame);
    }
}
"""


def main():
    if not SRC.is_file():
        raise SystemExit("context-original.cpp missing (copy step first)")
    base_cpp = sha256(SRC)
    if base_cpp != sha256(INPUTS_CPP):
        raise SystemExit("context-original.cpp does not match inputs/active-llama-context.cpp")
    src = SRC.read_text(encoding="utf-8")
    edits = []

    src = once(src, '#include "llama.h"',
               '#include "llama.h"\n#include "fg107-route.h"\n#include "context-execution-frame.hpp"', "includes")
    edits.append("includes")

    src = once(src, "int llama_context::decode(const llama_batch_ext & batch_inp) {",
               "int llama_context::decode(const llama_batch_ext & batch_inp) {" + ROUTE, "decode")
    edits.append("decode")

    src = once(src, "void llama_context::sched_reserve() {",
               "void llama_context::sched_reserve() {" + SCHED_RESERVE, "sched_reserve")
    edits.append("sched_reserve")

    src = once(src, "void llama_context::sched_new(size_t max_nodes, bool parallel) {",
               "void llama_context::sched_new(size_t max_nodes, bool parallel) {" + SCHED_NEW, "sched_new")
    edits.append("sched_new")

    src = once(src, "void llama_context::resolve_fused_ops(const llama_memory_context_i * mctx, uint32_t n_seqs) {",
               "void llama_context::resolve_fused_ops(const llama_memory_context_i * mctx, uint32_t n_seqs) {" + RESOLVE_FUSED,
               "resolve_fused_ops")
    edits.append("resolve_fused_ops")

    src = once(src, GRAPH_CB_OLD, GRAPH_CB_NEW, "graph_get_cb")
    edits.append("graph_get_cb")

    src = once(src, REALLOC_OLD, REALLOC_NEW, "sched_graph_realloc")
    edits.append("sched_graph_realloc")

    src = span(src, CPARAMS_START, CPARAMS_END, CPARAMS_NEW, "cparams_policy")
    edits.append("cparams_policy")

    src = wrap_block(src, CTOR_START, CTOR_END, CTOR_GOOSE, "constructor_resources")
    edits.append("constructor_resources")

    src = wrap_block(src, DTOR_START, DTOR_END, DTOR_GOOSE, "destructor_release")
    edits.append("destructor_release")

    src = once(src, "            model.pin_mmap_experts(ggml_backend_dev_backend_reg(device));",
               "            fg107_compat_pin_experts(&model, ggml_backend_dev_backend_reg(device));",
               "pin_experts_optional")
    edits.append("pin_experts_optional")

    for fn, sig, ret in [
        ("fg107_goose_state_get_size", "", "size_t"),
        ("fg107_goose_state_get_data", ", (int64_t)(intptr_t) dst, (int64_t) size", "size_t"),
        ("fg107_goose_state_set_data", ", (int64_t)(intptr_t) src, (int64_t) size", "size_t"),
        ("fg107_goose_state_seq_get_size", ", (int64_t) seq_id, (int64_t) flags", "size_t"),
        ("fg107_goose_state_seq_get_data", ", (int64_t) seq_id, (int64_t)(intptr_t) dst, (int64_t) size, (int64_t) flags", "size_t"),
        ("fg107_goose_state_seq_set_data", ", (int64_t) seq_id, (int64_t)(intptr_t) src, (int64_t) size, (int64_t) flags", "size_t"),
    ]:
        anchor = {
            "fg107_goose_state_get_size": "size_t llama_context::state_get_size() {",
            "fg107_goose_state_get_data": "size_t llama_context::state_get_data(uint8_t * dst, size_t size) {",
            "fg107_goose_state_set_data": "size_t llama_context::state_set_data(const uint8_t * src, size_t size) {",
            "fg107_goose_state_seq_get_size": "size_t llama_context::state_seq_get_size(llama_seq_id seq_id, llama_state_seq_flags flags) {",
            "fg107_goose_state_seq_get_data": "size_t llama_context::state_seq_get_data(llama_seq_id seq_id, uint8_t * dst, size_t size, llama_state_seq_flags flags) {",
            "fg107_goose_state_seq_set_data": "size_t llama_context::state_seq_set_data(llama_seq_id seq_id, const uint8_t * src, size_t size, llama_state_seq_flags flags) {",
        }[fn]
        src = once(src, anchor, anchor + state_route(fn, sig, ret), fn)
        edits.append(fn)

    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(src, encoding="utf-8")
    manifest = {
        "schema": "fg107.context-patch/1",
        "base_cpp": "inputs/active-llama-context.cpp",
        "base_cpp_sha256": base_cpp,
        "base_h": "inputs/active-llama-context.h",
        "base_h_sha256": sha256(INPUTS_H),
        "patched": "worker-4/patched/llama-context.cpp",
        "patched_sha256": sha256(OUT),
        "default": "goose",
        "rollback": "FG107_NATIVE_ROLLBACK",
        "edits": edits,
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "PATCHED", "edits": len(edits), "patched_sha256": manifest["patched_sha256"]}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
