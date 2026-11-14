// P6-05 architecture 16: gemma3 (dense GQA, qk-norm, optional SWA).
//
// Decision/shape half of the builder, not the graph emitter. The trunk plan
// comes from base/gemma.goose: all layers are attention layers; what varies
// per layer is the SWA flag, which is a graph input choice rather than a
// different tensor set.
//
// Native anchors (src/models/gemma3.cpp):
//   hparams: SWA detection, period, softcap, eps      :3-18
//   type switch by n_layer                            :20-28
//   attention scale: 27B uses n_embd/n_head           :30-34
//   global tensors, dense_2/dense_3 heads             :39-52
//   per-layer attention, post norms, qk-norm, ffn     :55-72
//   graph: iswa selection, input scale, rope per layer:75-121
//   graph: qk-norm before rope, GELU-PAR ffn          :123-186
//   graph: softcap decision                           :212-216
//   create_tensor_qkv                                 : src/llama-model.cpp:3860
//
// gemma3 differs from gemma3n (architecture 17) in what the shape table must
// keep:
//   - SWA is conditional: LLAMA_SWA_TYPE_STANDARD only when n_swa > 0;
//   - the period defaults to 6 and comes from the model file;
//   - 27B (62 layers) scales Q by 1/sqrt(n_embd/n_head), every other size by
//     1/sqrt(n_embd_head_k);
//   - optional dense_2_out and dense_3_out heads exist;
//   - final logit softcapping is conditional;
//   - there is no altup, no laurel and no per-layer embedding table.

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
const P_HAS_OUTPUT: i64 = 8;       // 0 -> output duplicates token_embd
const P_N_SWA: i64 = 9;            // model key; 0 -> no SWA layer
const P_SWA_PERIOD: i64 = 10;      // 0 -> every layer is SWA
const P_DENSE_2_OUT: i64 = 11;     // 0 -> no dense_2_out head
const P_DENSE_3_IN: i64 = 12;      // 0 -> no dense_3_out head
const P_SOFTCAP_BITS: i64 = 13;    // 0.0f -> softcap disabled
const P_FSCALE_BITS: i64 = 14;     // hparams.f_attention_scale bit pattern
const P_LEN: i64 = 15;

// model-type enum, as the switch at gemma3.cpp:20-28 labels it
const MT_UNKNOWN: i64 = 0;
const MT_270M: i64 = 1;
const MT_1B: i64 = 2;
const MT_8B: i64 = 3;
const MT_4B: i64 = 4;
const MT_12B: i64 = 5;
const MT_27B: i64 = 6;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_DENSE_2_OUT: i64 = 3;
const T_DENSE_3_OUT: i64 = 4;
const T_ATTN_NORM: i64 = 5;
const T_ATTN_QKV: i64 = 6;
const T_ATTN_WO: i64 = 7;
const T_ATTN_Q_NORM: i64 = 8;
const T_ATTN_K_NORM: i64 = 9;
const T_ATTN_POST_NORM: i64 = 10;
const T_FFN_NORM: i64 = 11;
const T_FFN_GATE: i64 = 12;
const T_FFN_UP: i64 = 13;
const T_FFN_DOWN: i64 = 14;
const T_FFN_POST_NORM: i64 = 15;
const T_LEN: i64 = 16;

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

// g3_swa_type: gemma3.cpp:4-14. STANDARD (1) only when the model declares a
// positive sliding window; otherwise NONE (0) and every layer is full
// attention. The enum values are llama-hparams.h:24-30.
export fn g3_swa_type(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if p[P_N_SWA] > 0 {
        return 1;
    }
    return 0;
}

// g3_graph_iswa: gemma3.cpp:76-80 chooses graph<true> from the same condition,
// so the graph template and the hparam can never disagree.
export fn g3_graph_iswa(p: i64[:]) -> i64 {
    return g3_swa_type(p);
}

// g3_swa_period: gemma3.cpp:7-9. The loader seeds the value with 6 and reads
// the key or array only inside the SWA branch, so the default is 6 and the
// value is a document property. 0 is legal and means "every layer SWA".
export fn g3_swa_period_default() -> i64 {
    return 6;
}

export fn g3_swa_period(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    return p[P_SWA_PERIOD];
}

// g3_is_swa: the per-layer mask, base/gemma.goose:g_swa_is. Layers outside
// the trunk are never SWA.
export fn g3_is_swa(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    e_ensure(il >= 0 && il < p[P_N_LAYER], 13);
    return g_swa_is(p[P_SWA_PERIOD], il);
}

// g3_rope_base_swa: gemma3.cpp:115-121. On an SWA layer the graph reads the
// per-layer rope base/scale (model.get_rope_freq_base(il)), which selects
// rope_freq_base_train_swa; on a full layer it uses the global pair. 1 means
// "take the SWA rope base", 0 means the global one.
export fn g3_rope_base_swa(is_swa: i64) -> i64 {
    if is_swa != 0 {
        return 1;
    }
    return 0;
}

// g3_model_type: gemma3.cpp:20-28, expressed as an enum instead of the
// LLM_TYPE_* constants.
export fn g3_model_type(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    let n = p[P_N_LAYER];
    if n == 18 {
        return MT_270M;
    }
    if n == 26 {
        return MT_1B;
    }
    if n == 32 {
        return MT_8B;
    }
    if n == 34 {
        return MT_4B;
    }
    if n == 48 {
        return MT_12B;
    }
    if n == 62 {
        return MT_27B;
    }
    return MT_UNKNOWN;
}

// g3_attn_scale_kind: gemma3.cpp:30-34.
//   1 -> 1/sqrt(n_embd / n_head)   (27B only)
//   0 -> 1/sqrt(n_embd_head_k)     (every other size)
export fn g3_attn_scale_kind(p: i64[:]) -> i64 {
    if g3_model_type(p) == MT_27B {
        return 1;
    }
    return 0;
}

// g3_kq_scale_auto: the builder always has a non-zero f_attention_scale from
// load_arch_hparams, so the shared auto branch (base/model_graph.goose) must
// never fire. Kept as a decision so the gate can hold that boundary: the
// caller passes the real scale bit pattern.
export fn g3_kq_scale_auto(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    return kq_scale_auto(p[P_FSCALE_BITS]);
}

// g3_qk_norm: Q and K are normalised per head before RoPE, gemma3.cpp:133-141;
// both norm tensors are always declared. Marker only: gemma3n also has it.
export fn g3_qk_norm() -> i64 {
    return 1;
}

// g3_act_gelu: the FFN is GELU-PAR, gemma3.cpp:184, not SILU.
export fn g3_act_gelu() -> i64 {
    return 1;
}

// g3_softcap_on: gemma3.cpp:212 applies the tanh softcap only when
// f_final_logit_softcapping is non-zero; gemma3n applies it unconditionally.
export fn g3_softcap_on(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if p[P_SOFTCAP_BITS] != 0 {
        return 1;
    }
    return 0;
}

// g3_embd_scale: gemma3.cpp:93, decision only (base/gemma.goose:g_embd_scale).
export fn g3_embd_scale(has_tokens: i64) -> i64 {
    return g_embd_scale(has_tokens);
}

// g3_q_width: gemma3.cpp:60, the query block is n_embd_head_k * n_head.
export fn g3_q_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    return g_gqa_width(p[P_HEAD_K], p[P_N_HEAD]);
}

// g3_kv_width: one value head times n_head_kv for both the key and value
// blocks (llama-model.h n_embd_k_gqa / n_embd_v_gqa).
export fn g3_kv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 18);
    return g_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);
}

// g3_qkv_width: the fused row create_tensor_qkv declares, gemma3.cpp:60.
export fn g3_qkv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 19);
    return g_qkv_width(g3_q_width(p), g3_kv_width(p), g3_kv_width(p));
}

// g3_output_dup: output is optional; absent means token_embd duplicated,
// gemma3.cpp:43-48.
export fn g3_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    return g_output_dup(p[P_HAS_OUTPUT]);
}

// g3_plan: all trunk layers are attention, base/gemma.goose:g_plan.
export fn g3_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 21);
    return g_plan(p[P_N_LAYER], kinds);
}

// Shape of one tensor. Returns 1 and fills out[0..2] when the architecture
// owns the tensor, 0 otherwise. Per-layer tensors use the same shape on every
// layer because the plan is uniform and the SWA flag changes no tensor.
export fn g3_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 30);
    e_ensure(out.len >= 3, 31);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let q_width = g3_q_width(p);
    let kv_width = g3_kv_width(p);
    let qkv = g3_qkv_width(p);

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // optional; absent means duplicate token_embd, same shape.
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM || tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM
        || tid == T_FFN_NORM || tid == T_FFN_POST_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_DENSE_2_OUT {
        if p[P_DENSE_2_OUT] <= 0 {
            return 0;
        }
        e_put3(out, n_embd, p[P_DENSE_2_OUT], 1);
        return 1;
    }
    if tid == T_DENSE_3_OUT {
        if p[P_DENSE_3_IN] <= 0 {
            return 0;
        }
        e_put3(out, p[P_DENSE_3_IN], n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_QKV {
        e_put3(out, n_embd, qkv, 1);
        return 1;
    }
    if tid == T_ATTN_WO {
        // gemma3.cpp:61: rows are the query block, not n_embd.
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        // gemma3.cpp:64-65: one width per head, not per hidden state.
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

    return 0;
}
