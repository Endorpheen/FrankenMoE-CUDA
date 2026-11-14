// P6-05 architecture 17: gemma3n (dense GQA, qk-norm, altup, laurel, PLE).
//
// Decision/shape half of the builder, not the graph emitter. The trunk plan
// comes from base/gemma.goose: every layer is an attention layer, and the SWA
// flag is per layer but changes no tensor.
//
// Native anchors (src/models/gemma3n.cpp):
//   hparams: unconditional SWA, period 5, KV prefix 20  :3-14
//   hparams: type switch by n_layer                     :16-21
//   global tensors + altup/laurel/PLE tables            :23-45
//   per-layer attention, post norms, qk-norm, ffn       :47-63
//   per-layer altup and laurel tensors                  :65-76
//   graph: input scale, per-layer input projection      :101-131
//   graph: laurel, KV prefix branch, V norm             :150-193
//   graph: activation sparsity, GELU-PAR ffn            :207-221
//   graph: altup predict/correct, PLE merge             :228-256
//   graph: altup un-embedding, unconditional softcap    :263-306
//   altup and laurel helpers                            :313-459
//
// gemma3n differs from gemma3 (architecture 16) in what the shape table must
// keep:
//   - SWA is unconditional (LLAMA_SWA_TYPE_STANDARD), period defaults to 5;
//   - the attention scale is fixed at 1.0f, not derived from n_embd_head_k;
//   - only the first n_layer_kv_from_start (20) layers own a KV cache; later
//     layers build the Q-only branch and reuse the earlier cache;
//   - V gets a weightless RMS norm, so there is no V norm tensor;
//   - altup: n_altup states, one active slice, a router, predict and correct
//     coefficient sets; laurel: a low-rank adapter around attention;
//   - per-layer embeddings: token table, model projection and proj norm, plus
//     per-layer inp gate / proj / post norm;
//   - the first 10 layers apply activation sparsity to the FFN gate;
//   - the final logit softcap is unconditional.

import base.gemma;
import base.model_graph;

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_HEAD: i64 = 3;
const P_N_HEAD_KV: i64 = 4;
const P_N_FF: i64 = 5;
const P_HEAD_K: i64 = 6;
const P_HEAD_V: i64 = 7;
const P_HAS_OUTPUT: i64 = 8;      // 0 -> output duplicates token_embd
const P_SWA_PERIOD: i64 = 9;      // 0 -> every layer is SWA
const P_N_ALTUP: i64 = 10;        // altup_num_inputs, default 4
const P_I_ALTUP: i64 = 11;        // altup_active_idx, default 0
const P_LAUREL: i64 = 12;         // laurel_rank, default 64
const P_EMBD_ALTUP: i64 = 13;     // n_embd_altup, default 256
const P_LEN: i64 = 14;

// graph.hpp constants
const G3N_KV_FROM_START: i64 = 20;        // gemma3n.cpp:9
const G3N_SPARSITY_LAYERS: i64 = 10;      // models.h n_layer_sparsity
const G3N_ATTN_SCALE_BITS: i64 = 1065353216; // 1.0f, gemma3n.cpp:10

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ALTUP_PROJ: i64 = 3;
const T_ALTUP_UNEMBD_PROJ: i64 = 4;
const T_PER_LAYER_TOK_EMBD: i64 = 5;
const T_PER_LAYER_MODEL_PROJ: i64 = 6;
const T_PER_LAYER_PROJ_NORM: i64 = 7;
const T_ATTN_NORM: i64 = 8;
const T_ATTN_QKV: i64 = 9;
const T_ATTN_WO: i64 = 10;
const T_ATTN_Q_NORM: i64 = 11;
const T_ATTN_K_NORM: i64 = 12;
const T_ATTN_POST_NORM: i64 = 13;
const T_FFN_NORM: i64 = 14;
const T_FFN_GATE: i64 = 15;
const T_FFN_UP: i64 = 16;
const T_FFN_DOWN: i64 = 17;
const T_FFN_POST_NORM: i64 = 18;
const T_PER_LAYER_INP_GATE: i64 = 19;
const T_PER_LAYER_PROJ: i64 = 20;
const T_PER_LAYER_POST_NORM: i64 = 21;
const T_ALTUP_CORRECT_COEF: i64 = 22;
const T_ALTUP_CORRECT_SCALE: i64 = 23;
const T_ALTUP_PREDICT_COEF: i64 = 24;
const T_ALTUP_ROUTER: i64 = 25;
const T_ALTUP_ROUTER_NORM: i64 = 26;
const T_LAUREL_L: i64 = 27;
const T_LAUREL_R: i64 = 28;
const T_LAUREL_POST_NORM: i64 = 29;
const T_LEN: i64 = 30;

fn e_ensure(v: bool, c: i64) {
    if !v {
        g_fail(c);
    }
}

fn e_put3(out: i64[:], a: i64, b: i64, c: i64) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
}

// g3n_swa_type: gemma3n.cpp:4-7 sets STANDARD unconditionally; there is no
// n_swa > 0 gate as in gemma3. Returns the llama_swa_type value.
export fn g3n_swa_type() -> i64 {
    return 1;
}

// g3n_swa_period_default: gemma3n.cpp:4, the seed value before the key read.
export fn g3n_swa_period_default() -> i64 {
    return 5;
}

export fn g3n_swa_period(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    return p[P_SWA_PERIOD];
}

export fn g3n_is_swa(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    e_ensure(il >= 0 && il < p[P_N_LAYER], 12);
    return g_swa_is(p[P_SWA_PERIOD], il);
}

// g3n_kv_from_start: gemma3n.cpp:9, the value the builder stores in
// hparams.n_layer_kv_from_start.
export fn g3n_kv_from_start() -> i64 {
    return G3N_KV_FROM_START;
}

// g3n_has_kv: llama-hparams.cpp:295-304. A negative value means "all layers";
// otherwise only the prefix owns a KV cache. The value is passed in so both
// branches are testable; g3n_kv_from_start supplies the real one.
export fn g3n_has_kv(n_kv_from_start: i64, il: i64) -> i64 {
    e_ensure(il >= 0, 13);
    if n_kv_from_start >= 0 {
        if il < n_kv_from_start {
            return 1;
        }
        return 0;
    }
    return 1;
}

// g3n_attn_scale_bits: gemma3n.cpp:10, f_attention_scale = 1.0f.
export fn g3n_attn_scale_bits() -> i64 {
    return G3N_ATTN_SCALE_BITS;
}

// g3n_attn_scale_auto: the shared auto branch must not fire because the scale
// is explicitly 1.0f; base/model_graph.goose:kq_scale_auto returns 0 here.
export fn g3n_attn_scale_auto() -> i64 {
    return kq_scale_auto(G3N_ATTN_SCALE_BITS);
}

// g3n_qk_norm: Q and K are normalised per head with weights, gemma3n.cpp:157-158.
export fn g3n_qk_norm() -> i64 {
    return 1;
}

// g3n_v_norm: V is normalised with ggml_rms_norm and no weight tensor,
// gemma3n.cpp:159, so the shape table must declare no V norm tensor.
export fn g3n_v_norm() -> i64 {
    return 1;
}

// g3n_sparsity_layers: models.h:836, the number of leading layers that apply
// gaussian-topk activation sparsity.
export fn g3n_sparsity_layers() -> i64 {
    return G3N_SPARSITY_LAYERS;
}

// g3n_uses_sparsity: gemma3n.cpp:212, `il < n_layer_sparsity`.
export fn g3n_uses_sparsity(il: i64) -> i64 {
    e_ensure(il >= 0, 14);
    if il < G3N_SPARSITY_LAYERS {
        return 1;
    }
    return 0;
}

// g3n_softcap_always: gemma3n.cpp:301-306 applies the tanh softcap with no
// condition, unlike gemma3.
export fn g3n_softcap_always() -> i64 {
    return 1;
}

// g3n_altup_extra: altup.cpp stacking, gemma3n.cpp:38-39 and :123-128 -- the
// altup_proj and altup_unembd_proj tensors hold n_altup - 1 projections.
export fn g3n_altup_extra(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    e_ensure(p[P_N_ALTUP] >= 2, 16);
    return p[P_N_ALTUP] - 1;
}

// g3n_altup_merge_div: gemma3n.cpp:283, the merged altup state is divided by
// n_altup (torch.mean over the state axis).
export fn g3n_altup_merge_div(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    e_ensure(p[P_N_ALTUP] >= 1, 18);
    return p[P_N_ALTUP];
}

// g3n_altup_active: gemma3n.cpp:142,/232,/267 slice the active prediction at
// index i_altup_act.
export fn g3n_altup_active(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 19);
    e_ensure(p[P_I_ALTUP] >= 0 && p[P_I_ALTUP] < p[P_N_ALTUP], 20);
    return p[P_I_ALTUP];
}

// g3n_altup_correct_bias: gemma3n.cpp:448 adds 1.0 to every correct
// coefficient before use (ggml_scale_bias with 1, 1).
export fn g3n_altup_correct_bias() -> i64 {
    return 1;
}

// g3n_q_width: gemma3n.cpp:52, the query block is n_embd_head_k * n_head.
export fn g3n_q_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 21);
    return g_gqa_width(p[P_HEAD_K], p[P_N_HEAD]);
}

// g3n_kv_width: one value head times n_head_kv for both blocks.
export fn g3n_kv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 22);
    return g_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);
}

// g3n_qkv_width: fused row of create_tensor_qkv, gemma3n.cpp:52. Declared for
// every layer even when the layer has no KV cache.
export fn g3n_qkv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 23);
    return g_qkv_width(g3n_q_width(p), g3n_kv_width(p), g3n_kv_width(p));
}

// g3n_output_dup: output is optional; absent means token_embd duplicated,
// gemma3n.cpp:30-34.
export fn g3n_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 24);
    return g_output_dup(p[P_HAS_OUTPUT]);
}

// g3n_plan: all trunk layers are attention, base/gemma.goose:g_plan.
export fn g3n_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 25);
    return g_plan(p[P_N_LAYER], kinds);
}

// g3n_embd_scale: gemma3n.cpp:104, decision only.
export fn g3n_embd_scale(has_tokens: i64) -> i64 {
    return g_embd_scale(has_tokens);
}

// Shape of one tensor. Returns 1 and fills out[0..2] when the architecture
// owns the tensor, 0 otherwise.
export fn g3n_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 30);
    e_ensure(out.len >= 3, 31);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let n_layer = p[P_N_LAYER];
    let n_altup = p[P_N_ALTUP];
    let laurel = p[P_LAUREL];
    let embd_altup = p[P_EMBD_ALTUP];

    let q_width = g3n_q_width(p);
    let kv_width = g3n_kv_width(p);
    let qkv = g3n_qkv_width(p);

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM || tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM
        || tid == T_FFN_NORM || tid == T_FFN_POST_NORM || tid == T_PER_LAYER_POST_NORM
        || tid == T_ALTUP_CORRECT_SCALE || tid == T_ALTUP_ROUTER_NORM
        || tid == T_LAUREL_POST_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ALTUP_PROJ || tid == T_ALTUP_UNEMBD_PROJ {
        // gemma3n.cpp:38-39: [n_embd, n_embd, n_altup - 1].
        e_put3(out, n_embd, n_embd, g3n_altup_extra(p));
        return 1;
    }
    if tid == T_PER_LAYER_TOK_EMBD {
        // gemma3n.cpp:41: one row of n_embd_altup per layer per token.
        e_put3(out, embd_altup * n_layer, n_vocab, 1);
        return 1;
    }
    if tid == T_PER_LAYER_MODEL_PROJ {
        // gemma3n.cpp:42: [n_embd, n_embd_altup * n_layer].
        e_put3(out, n_embd, embd_altup * n_layer, 1);
        return 1;
    }
    if tid == T_PER_LAYER_PROJ_NORM {
        e_put3(out, embd_altup, 1, 1);
        return 1;
    }
    if tid == T_ATTN_QKV {
        e_put3(out, n_embd, qkv, 1);
        return 1;
    }
    if tid == T_ATTN_WO {
        // gemma3n.cpp:53: rows are the query block.
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        e_put3(out, p[P_HEAD_K], 1, 1);
        return 1;
    }
    if tid == T_FFN_GATE || tid == T_FFN_UP {
        e_put3(out, n_embd, n_ff, 1);
        return 1;
    }
    if tid == T_FFN_DOWN {
        e_put3(out, n_ff, n_embd, 1);
        return 1;
    }
    if tid == T_PER_LAYER_INP_GATE {
        // gemma3n.cpp:66: [n_embd, n_embd_altup].
        e_put3(out, n_embd, embd_altup, 1);
        return 1;
    }
    if tid == T_PER_LAYER_PROJ {
        // gemma3n.cpp:67: [n_embd_altup, n_embd].
        e_put3(out, embd_altup, n_embd, 1);
        return 1;
    }
    if tid == T_ALTUP_CORRECT_COEF {
        // gemma3n.cpp:69: [n_altup, n_altup].
        e_put3(out, n_altup, n_altup, 1);
        return 1;
    }
    if tid == T_ALTUP_PREDICT_COEF {
        // gemma3n.cpp:71: [n_altup, n_altup * n_altup].
        e_put3(out, n_altup, n_altup * n_altup, 1);
        return 1;
    }
    if tid == T_ALTUP_ROUTER {
        // gemma3n.cpp:72: [n_embd, n_altup].
        e_put3(out, n_embd, n_altup, 1);
        return 1;
    }
    if tid == T_LAUREL_L {
        e_put3(out, n_embd, laurel, 1);
        return 1;
    }
    if tid == T_LAUREL_R {
        e_put3(out, laurel, n_embd, 1);
        return 1;
    }

    return 0;
}
