// P6-05 architecture 8: deepseek32 (DeepSeek V3.2 family) builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   n_ff_exp, gating_kind.
//
// Native anchors (src/models/deepseek32.cpp):
//   hparams (expert width array, dense lead, MLA ranks, indexer, gating) :6-44
//   MLA validation and head splits                                 :53-66
//   global tensors (token_embd, output_norm, tied output)          :71-79
//   per-layer MLA tensors (norms, wq_a/wq_b, wkv_a_mqa, wk_b, wv_b, wo) :81-101
//   DSA indexer tensors                                            :103-108
//   dense lead FFN                                                 :109-112
//   routed + shared experts                                        :113-133
//   nextn/MTP tensors                                              :135-145
//   trunk graph: MLA absorption, indexer top-k, dense/MoE switch   :203-457
//   MTP draft graph: no indexer at runtime                         :480-717
//
// deepseek32 is the MLA sibling of the deepseek2 MoE family. The details the
// shape table must keep apart from the qwen MoE slices:
//   - attention is MLA with two low-rank compressions: q through
//     wq_a {n_embd,q_lora_rank} and wq_b {q_lora_rank,n_head*192}, keys/values
//     through wkv_a_mqa {n_embd,kv_lora_rank+rope} and the split wk_b/wv_b,
//   - the q/k normalisation is per-rank: attn_q_a_norm {q_lora_rank} and
//     attn_kv_a_norm {kv_lora_rank}, not per-head q/k norms,
//   - the first n_layer_dense_lead layers are dense MLP, the rest routed MoE,
//   - the shared expert width is n_ff_exp * n_expert_shared (not n_ff),
//   - the DSA indexer exists on every layer in the loader but the MTP draft
//     graph does not execute it,
//   - the MTP layer is a full MLA + MoE decoder block plus eh_proj/enorm/hnorm
//     and optional shared head tensors; n_layer_nextn is one block.
//
// The plan has no recurrent/attention switch: every trunk layer is MLA
// attention. The per-layer decision is only the FFN kind and the MTP flag.

import std;
import base.model_graph;

// p indices -- the shared fixture contract of this experiment, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_LAYER_NEXTN: i64 = 4;
const P_N_HEAD: i64 = 5;
const P_N_FF: i64 = 6;
const P_N_EXPERT: i64 = 7;
const P_N_EXPERT_USED: i64 = 8;
const P_N_FF_EXP: i64 = 9;
const P_N_EXPERT_SHARED: i64 = 10;
const P_DENSE_LEAD: i64 = 11;
const P_Q_LORA_RANK: i64 = 12;
const P_KV_LORA_RANK: i64 = 13;
const P_HEAD_K_MLA: i64 = 14;
const P_HEAD_V_MLA: i64 = 15;
const P_N_ROT: i64 = 16;
const P_IDX_NHEAD: i64 = 17;
const P_IDX_HEAD_SIZE: i64 = 18;
const P_IDX_TOP_K: i64 = 19;
const P_GATING: i64 = 20;
const P_HAS_OUTPUT: i64 = 21;
const P_LEN: i64 = 22;

// layer kinds: the FFN decision crossed with the trunk/MTP decision
const K_DENSE: i64 = 0;
const K_MOE: i64 = 1;
const K_MTP_DENSE: i64 = 2;
const K_MTP_MOE: i64 = 3;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_Q_A_NORM: i64 = 4;
const T_ATTN_KV_A_NORM: i64 = 5;
const T_WQ_A: i64 = 6;
const T_WQ_B: i64 = 7;
const T_WKV_A_MQA: i64 = 8;
const T_WK_B: i64 = 9;
const T_WV_B: i64 = 10;
const T_WO: i64 = 11;
const T_FFN_NORM: i64 = 12;
const T_INDEXER_K_NORM: i64 = 13;
const T_INDEXER_K_NORM_B: i64 = 14;
const T_INDEXER_PROJ: i64 = 15;
const T_INDEXER_ATTN_K: i64 = 16;
const T_INDEXER_ATTN_Q_B: i64 = 17;
const T_FFN_GATE: i64 = 18;
const T_FFN_DOWN: i64 = 19;
const T_FFN_UP: i64 = 20;
const T_FFN_GATE_INP: i64 = 21;
const T_FFN_EXP_PROBS_B: i64 = 22;
const T_FFN_GATE_EXPS: i64 = 23;
const T_FFN_DOWN_EXPS: i64 = 24;
const T_FFN_UP_EXPS: i64 = 25;
const T_FFN_GATE_SHEXP: i64 = 26;
const T_FFN_DOWN_SHEXP: i64 = 27;
const T_FFN_UP_SHEXP: i64 = 28;
const T_NEXTN_EH_PROJ: i64 = 29;
const T_NEXTN_ENORM: i64 = 30;
const T_NEXTN_HNORM: i64 = 31;
const T_NEXTN_EMBED_TOKENS: i64 = 32;
const T_NEXTN_SHARED_HEAD_HEAD: i64 = 33;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 34;
const T_LEN: i64 = 35;

fn e_ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn e_put3(out: i64[:], a: i64, b: i64, c: i64) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
}

// Per-layer kind: dense-lead trunk layer, routed-MoE trunk layer, or one of
// the two MTP shapes. Native: deepseek32.cpp:81-83 (`i >= n_layer` selects the
// mtp flags) and :109 (`i < n_layer_dense_lead` selects the dense FFN).
export fn ds3_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if il < 0 {
        return -1;
    }

    var ffn_dense: i64 = 0;
    if il < p[P_DENSE_LEAD] {
        ffn_dense = 1;
    }

    if il < p[P_N_LAYER_ALL] {
        if il >= p[P_N_LAYER] {
            if ffn_dense != 0 { return K_MTP_DENSE; }
            return K_MTP_MOE;
        }
        if ffn_dense != 0 { return K_DENSE; }
        return K_MOE;
    }
    return -1;
}

export fn ds3_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 12);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        kinds[i] = ds3_kind(p, i);
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// hparams.is_mla(): both implementation head sizes are set, or neither. The
// native architecture throws when MLA is off (deepseek32.cpp:53-56); the
// assertion at llama-hparams.cpp:299-302 is the half-set abort.
export fn ds3_is_mla(head_k_mla: i64, head_v_mla: i64) -> i64 {
    if head_k_mla == 0 && head_v_mla == 0 {
        return 0;
    }
    if head_k_mla != 0 && head_v_mla != 0 {
        return 1;
    }
    e_ensure(false, 13);
    return 0;
}

export fn ds3_require_mla(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    return ds3_is_mla(p[P_HEAD_K_MLA], p[P_HEAD_V_MLA]);
}

// The DSA indexer is built for every loader layer but is not executed by the
// MTP draft graph (deepseek32.cpp:480-486, "The DSA indexer is not used at
// runtime"). Trunk layers execute it.
export fn ds3_indexer_used(kind: i64) -> i64 {
    if kind == K_MTP_DENSE || kind == K_MTP_MOE {
        return 0;
    }
    if kind == K_DENSE || kind == K_MOE {
        return 1;
    }
    return 0;
}

// Indexer top-k: `min(n_cached, indexer_top_k)` -- deepseek32.cpp:314-315.
export fn ds3_top_k(n_cached: i64, top_k: i64) -> i64 {
    e_ensure(n_cached >= 0, 15);
    e_ensure(top_k >= 0, 16);
    if n_cached < top_k {
        return n_cached;
    }
    return top_k;
}

// Expert gating function of the routed MoE, through the accepted base switch
// (llama-graph.cpp:2227-2249). 0 SOFTMAX, 1 SIGMOID, 2 SOFTMAX_WEIGHT,
// 3 SQRT_SOFTPLUS.
export fn ds3_gating(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    return gating_kind(p[P_GATING]);
}

// MLA query head split: {nope, rope, head_k}. deepseek32.cpp:62-63,
// :322-332. RoPE must be the trailing part of the head.
export fn ds3_qk_split(p: i64[:], out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 18);
    e_ensure(out.len >= 3, 19);
    let head_k = p[P_HEAD_K_MLA];
    let rope = p[P_N_ROT];
    e_ensure(rope >= 0, 20);
    e_ensure(rope <= head_k, 21);
    e_put3(out, head_k - rope, rope, head_k);
    return 1;
}

// output.weight is TENSOR_NOT_REQUIRED and falls back to the duplicated
// token_embd tensor (deepseek32.cpp:76-79).
export fn ds3_output_optional() -> i64 {
    return 1;
}

// Shape of one tensor for a layer kind. Returns 1 and fills out[0..2] when the
// kind owns it, 0 otherwise.
export fn ds3_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 22);
    e_ensure(out.len >= 3, 23);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];

    let q_lora = p[P_Q_LORA_RANK];
    let kv_lora = p[P_KV_LORA_RANK];
    let head_k = p[P_HEAD_K_MLA];
    let head_v = p[P_HEAD_V_MLA];
    let rope = p[P_N_ROT];
    e_ensure(rope >= 0, 24);
    e_ensure(rope <= head_k, 25);
    let nope = head_k - rope;

    let idx_nhead = p[P_IDX_NHEAD];
    let idx_head = p[P_IDX_HEAD_SIZE];

    // deepseek32.cpp:68, `n_ff_exp = hparams.n_ff_exp()`; the accepted base
    // helper keeps the declared-or-n_ff/n_expert_used fallback.
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);
    let shexp_w = n_ff_exp_v * p[P_N_EXPERT_SHARED];

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }

    if kind != K_DENSE && kind != K_MOE && kind != K_MTP_DENSE && kind != K_MTP_MOE {
        return 0;
    }

    // MLA tensors, every layer including MTP (deepseek32.cpp:86-101).
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_Q_A_NORM {
        e_put3(out, q_lora, 1, 1);
        return 1;
    }
    if tid == T_ATTN_KV_A_NORM {
        e_put3(out, kv_lora, 1, 1);
        return 1;
    }
    if tid == T_WQ_A {
        e_put3(out, n_embd, q_lora, 1);
        return 1;
    }
    if tid == T_WQ_B {
        e_put3(out, q_lora, n_head * head_k, 1);
        return 1;
    }
    if tid == T_WKV_A_MQA {
        e_put3(out, n_embd, kv_lora + rope, 1);
        return 1;
    }
    if tid == T_WK_B {
        e_put3(out, nope, kv_lora, n_head);
        return 1;
    }
    if tid == T_WV_B {
        e_put3(out, kv_lora, head_v, n_head);
        return 1;
    }
    if tid == T_WO {
        e_put3(out, n_head * head_v, n_embd, 1);
        return 1;
    }

    // DSA indexer, created for every loader layer (deepseek32.cpp:103-108).
    if tid == T_INDEXER_K_NORM || tid == T_INDEXER_K_NORM_B {
        e_put3(out, idx_head, 1, 1);
        return 1;
    }
    if tid == T_INDEXER_PROJ {
        e_put3(out, n_embd, idx_nhead, 1);
        return 1;
    }
    if tid == T_INDEXER_ATTN_K {
        e_put3(out, n_embd, idx_head, 1);
        return 1;
    }
    if tid == T_INDEXER_ATTN_Q_B {
        e_put3(out, q_lora, idx_nhead * idx_head, 1);
        return 1;
    }

    // Dense lead FFN (deepseek32.cpp:109-112).
    if kind == K_DENSE || kind == K_MTP_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, n_ff, 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            e_put3(out, n_ff, n_embd, 1);
            return 1;
        }
    }

    // Routed + shared experts (deepseek32.cpp:113-133). The native aborts on a
    // zero expert count or a zero used count (:117-122).
    if kind == K_MOE || kind == K_MTP_MOE {
        e_ensure(n_expert > 0, 30);
        e_ensure(used > 0, 31);
        if tid == T_FFN_GATE_INP {
            e_put3(out, n_embd, n_expert, 1);
            return 1;
        }
        if tid == T_FFN_EXP_PROBS_B {
            e_put3(out, n_expert, 1, 1);
            return 1;
        }
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
            e_put3(out, n_embd, n_ff_exp_v, n_expert);
            return 1;
        }
        if tid == T_FFN_DOWN_EXPS {
            e_put3(out, n_ff_exp_v, n_embd, n_expert);
            return 1;
        }
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            e_put3(out, n_embd, shexp_w, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            e_put3(out, shexp_w, n_embd, 1);
            return 1;
        }
    }

    // NextN/MTP tensors, only on layers at or past n_layer (:135-145).
    if kind == K_MTP_DENSE || kind == K_MTP_MOE {
        if tid == T_NEXTN_EH_PROJ {
            e_put3(out, n_embd * 2, n_embd, 1);
            return 1;
        }
        if tid == T_NEXTN_ENORM || tid == T_NEXTN_HNORM || tid == T_NEXTN_SHARED_HEAD_NORM {
            e_put3(out, n_embd, 1, 1);
            return 1;
        }
        if tid == T_NEXTN_EMBED_TOKENS || tid == T_NEXTN_SHARED_HEAD_HEAD {
            e_put3(out, n_embd, n_vocab, 1);
            return 1;
        }
    }

    return 0;
}
