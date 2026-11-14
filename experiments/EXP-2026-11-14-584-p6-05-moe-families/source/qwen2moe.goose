// P6-05 architecture 7: qwen2moe builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   n_ff_exp, n_ff_shexp, key_dim, value_dim.
//
// Native anchors (src/models/qwen2moe.cpp):
//   hparams (expert ffn length, shared expert ffn length, rms eps) :3-14
//   global tensors (token_embd, output_norm, output) :19-23
//   per-layer attention tensors :28-31
//   routed experts :35-49
//   shared expert (gate_inp, gate, down, up) :51-57
//   graph: dense attention, MoE ffn, sigmoid shared expert :65-192
//
// qwen2moe is the shared-expert MoE sibling of qwen3moe. Every layer is one
// dense attention layer with a routed-expert FFN plus one shared expert. The
// differences the shape table must keep:
//   - no attn_q_norm/attn_k_norm (qwen2moe.cpp:92-117 has no norm before rope),
//   - the query block is n_embd wide, not n_embd_head_k * n_head (:30),
//   - wo is square {n_embd, n_embd} (:31),
//   - a shared expert exists on every layer, with its own gate input vector
//     {n_embd}, gate/up {n_embd, n_ff_shexp} and down {n_ff_shexp, n_embd},
//   - the shared-expert width is declared separately and falls back to n_ff,
//     not to the routed width (:52),
//   - output is mandatory (:23).

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

const K_ATTN: i64 = 0;

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

// No per-layer kind switch: all n_layer layers are attention + routed experts +
// one shared expert (qwen2moe.cpp:25-58, :83-177).
export fn q2m_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    e_ensure(kinds.len >= p[P_N_LAYER], 11);
    var i: i64 = 0;
    while i < p[P_N_LAYER] {
        kinds[i] = K_ATTN;
        i = i + 1;
    }
    return p[P_N_LAYER];
}

// qwen2moe.cpp:23 creates output unconditionally; it cannot be duplicated from
// token_embd.
export fn q2m_output_optional() -> i64 {
    return 0;
}

export fn q2m_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let head_k = p[P_HEAD_K];
    let head_v = p[P_HEAD_V];
    let n_head_kv = p[P_N_HEAD_KV];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];

    // qwen2moe.cpp:45, `n_ff_exp ? n_ff_exp : n_ff / n_expert_used`
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);
    // qwen2moe.cpp:52, `n_ff_shexp ? n_ff_shexp : n_ff`
    let n_ff_shexp_v = n_ff_shexp(p[P_N_FF_SHEXP], n_ff);

    let n_embd_k_gqa = key_dim(head_k, n_head_kv);
    let n_embd_v_gqa = value_dim(head_v, n_head_kv);
    // qwen2moe.cpp:30 passes n_embd as the query width.
    let q_width = n_embd;
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
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
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
        e_put3(out, n_embd, n_embd, 1);
        return 1;
    }
    if tid == T_FFN_GATE_INP {
        e_put3(out, n_embd, n_expert, 1);
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
    if tid == T_FFN_GATE_INP_SHEXP {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
        e_put3(out, n_embd, n_ff_shexp_v, 1);
        return 1;
    }
    if tid == T_FFN_DOWN_SHEXP {
        e_put3(out, n_ff_shexp_v, n_embd, 1);
        return 1;
    }
    return 0;
}
