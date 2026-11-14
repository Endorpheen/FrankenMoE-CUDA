// P6-05 architecture 13: glm4-moe builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   n_ff_exp, key_dim, value_dim.
//
// Native anchors (src/models/glm4-moe.cpp):
//   hparams (expert ffn length, shared-expert count, dense lead, gating,
//   rope sections) :3-26
//   global tensors (token_embd, output_norm, output) :39-47
//   per-layer attention incl. attn_post_norm :53-66
//   dense lead FFN vs routed experts + shared expert :68-103
//   nextn/MTP tensors :105-115
//   MTP graph :126-283
//   trunk graph with dense/MoE switch and crop rules :285-436
//
// glm4-moe is the hybrid MoE family: the first n_layer_dense_lead blocks use a
// dense gated FFN, the rest use routed experts plus one shared expert. The
// differences the shape table must keep:
//   - three layer kinds: dense lead, routed MoE, MTP (beyond n_layer,
//     loop bounds are n_layer_all at :49),
//   - attn_post_norm {n_embd} replaces ffn_norm: the FFN input is normalised
//     with the post-attention norm (:369), there is no ffn_norm tensor,
//   - q/k norm tensors are optional (TENSOR_NOT_REQUIRED, :60-64),
//   - routed experts carry an expert-probability bias {n_expert} (:76),
//   - the shared-expert width is n_ff_exp * n_expert_shared, not n_ff (:90),
//   - nextn: eh_proj {2*n_embd, n_embd}, enorm/hnorm {n_embd}, plus optional
//     embed_tokens/shared_head_head/shared_head_norm (:107-114),
//   - MTP blocks still own the full MoE and attention tensor set (:49-103).
//
// The rope decision (mrope when rope_sections are set, :298, :334-351) is
// exported as g4m_use_mrope; it selects the op, not a tensor shape.

import std;
import base.model_graph;

// p indices -- the shared fixture contract of this experiment, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_LAYER_NEXTN: i64 = 4;
const P_N_HEAD: i64 = 5;
const P_N_HEAD_KV: i64 = 6;
const P_HEAD_K: i64 = 7;
const P_HEAD_V: i64 = 8;
const P_N_FF: i64 = 9;
const P_N_EXPERT: i64 = 10;
const P_N_EXPERT_USED: i64 = 11;
const P_N_FF_EXP: i64 = 12;
const P_N_FF_SHEXP: i64 = 13;
const P_N_EXPERT_SHARED: i64 = 14;
const P_DENSE_LEAD: i64 = 15;
const P_HAS_OUTPUT: i64 = 16;
const P_QK_NORM: i64 = 17;
const P_GATING: i64 = 18;
const P_MROPE: i64 = 19;
const P_N_ROT: i64 = 20;
const P_LEN: i64 = 21;

const K_DENSE: i64 = 0;
const K_MOE: i64 = 1;
const K_MTP: i64 = 2;

// tensor ids -- shared across the three MoE families of this experiment
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT: i64 = 1;
const T_OUTPUT_NORM: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_QKV: i64 = 4;
const T_ATTN_Q: i64 = 5;
const T_ATTN_K: i64 = 6;
const T_ATTN_V: i64 = 7;
const T_ATTN_WO: i64 = 8;
const T_ATTN_Q_NORM: i64 = 9;
const T_ATTN_K_NORM: i64 = 10;
const T_ATTN_POST_NORM: i64 = 11;
const T_FFN_NORM: i64 = 12;
const T_FFN_GATE_INP: i64 = 13;
const T_FFN_EXP_PROBS_B: i64 = 14;
const T_FFN_GATE_EXPS: i64 = 15;
const T_FFN_DOWN_EXPS: i64 = 16;
const T_FFN_UP_EXPS: i64 = 17;
const T_FFN_GATE_INP_SHEXP: i64 = 18;
const T_FFN_GATE_SHEXP: i64 = 19;
const T_FFN_DOWN_SHEXP: i64 = 20;
const T_FFN_UP_SHEXP: i64 = 21;
const T_FFN_GATE: i64 = 22;
const T_FFN_DOWN: i64 = 23;
const T_FFN_UP: i64 = 24;
const T_NEXTN_EH_PROJ: i64 = 25;
const T_NEXTN_ENORM: i64 = 26;
const T_NEXTN_HNORM: i64 = 27;
const T_NEXTN_EMBED_TOKENS: i64 = 28;
const T_NEXTN_SHARED_HEAD_HEAD: i64 = 29;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 30;
const T_LEN: i64 = 31;

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

// Layer kind, glm4-moe.cpp:49-51 and :70. n_layer is the trunk; blocks
// n_layer..n_layer_all-1 are MTP blocks and are always routed MoE.
export fn g4m_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if il < p[P_DENSE_LEAD] {
        return K_DENSE;
    }
    if il < p[P_N_LAYER] {
        return K_MOE;
    }
    return K_MTP;
}

export fn g4m_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 12);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        kinds[i] = g4m_kind(p, i);
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// glm4-moe.cpp:6 and :298/:334 -- rope_sections present means multi-axis rope.
export fn g4m_use_mrope(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    if p[P_MROPE] != 0 {
        return 1;
    }
    return 0;
}

// Shape of one tensor. Returns 1 and fills out[0..2], 0 otherwise. There is no
// ffn_norm in this architecture; attn_post_norm plays that role.
export fn g4m_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let head_k = p[P_HEAD_K];
    let head_v = p[P_HEAD_V];
    let n_head = p[P_N_HEAD];
    let n_head_kv = p[P_N_HEAD_KV];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];
    let n_shared = p[P_N_EXPERT_SHARED];

    // glm4-moe.cpp:79, `n_ff_exp ? n_ff_exp : n_ff / n_expert_used`
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);
    // glm4-moe.cpp:90, `n_ff_shexp = n_ff_exp * n_expert_shared`
    let n_ff_shexp_v = n_ff_exp_v * n_shared;

    let n_embd_k_gqa = key_dim(head_k, n_head_kv);
    let n_embd_v_gqa = value_dim(head_v, n_head_kv);
    let q_width = head_k * n_head;
    let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind == K_DENSE || kind == K_MOE || kind == K_MTP {
        if tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM {
            e_put3(out, n_embd, 1, 1);
            return 1;
        }
        if tid == T_ATTN_QKV {
            e_put3(out, n_embd, qkv_fused, 1);
            return 1;
        }
        if tid == T_ATTN_Q {
            e_put3(out, n_embd, q_width, 1);
            return 1;
        }
        if tid == T_ATTN_K {
            e_put3(out, n_embd, n_embd_k_gqa, 1);
            return 1;
        }
        if tid == T_ATTN_V {
            e_put3(out, n_embd, n_embd_v_gqa, 1);
            return 1;
        }
        if tid == T_ATTN_WO {
            e_put3(out, head_k * n_head, n_embd, 1);
            return 1;
        }
        if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
            // optional tensors (glm4-moe.cpp:60-64)
            if p[P_QK_NORM] == 0 {
                return 0;
            }
            e_put3(out, head_k, 1, 1);
            return 1;
        }
    }

    if kind == K_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, n_ff, 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            e_put3(out, n_ff, n_embd, 1);
            return 1;
        }
    }

    if kind == K_MOE || kind == K_MTP {
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
        if n_shared > 0 {
            if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
                e_put3(out, n_embd, n_ff_shexp_v, 1);
                return 1;
            }
            if tid == T_FFN_DOWN_SHEXP {
                e_put3(out, n_ff_shexp_v, n_embd, 1);
                return 1;
            }
        }
    }

    if kind == K_MTP {
        if tid == T_NEXTN_EH_PROJ {
            e_put3(out, 2 * n_embd, n_embd, 1);
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
