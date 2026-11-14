// P6-05 architecture 4: qwen3moe builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   n_ff_exp, key_dim, value_dim.
//
// Native anchors (src/models/qwen3moe.cpp):
//   hparams (expert ffn length, rms eps, type) :3-12
//   global tensors (token_embd, output_norm, output) :17-25
//   per-layer attention tensors :30-36
//   per-layer MoE tensors :38-54
//   graph: dense attention, q/k norm, rope, MoE ffn :62-177
//
// qwen3moe is the plain MoE sibling of qwen3: every layer is a dense attention
// layer followed by a routed-expert FFN. There is no shared expert, no dense
// lead block, no nextn/MTP block and no per-layer kind switch. The two details
// that separate it from qwen2moe are the mandatory q/k RMS norm tensors and the
// fused QKV width, where the query block is n_embd_head_k * n_head while the
// key/value blocks are GQA-sized.
//
// qwen2moe differs in three ways the shape table keeps:
//   - no attn_q_norm/attn_k_norm at all,
//   - a shared expert (ffn_gate_inp_shexp/ffn_*_shexp) on every layer,
//   - the query width is n_embd, not n_embd_head_k * n_head.

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

// qwen3moe has no per-layer kind switch: all n_layer layers are routed-expert
// attention layers (qwen3moe.cpp:27-55, :80-162).
export fn q3m_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    e_ensure(kinds.len >= p[P_N_LAYER], 11);
    var i: i64 = 0;
    while i < p[P_N_LAYER] {
        kinds[i] = K_ATTN;
        i = i + 1;
    }
    return p[P_N_LAYER];
}

// output is TENSOR_NOT_REQUIRED here: a missing tensor is duplicated from
// token_embd (qwen3moe.cpp:21-25). qwen2moe requires it.
export fn q3m_output_optional() -> i64 {
    return 1;
}

// Shape of one tensor. Returns 1 and fills out[0..2], 0 when the architecture
// does not own the tensor.
export fn q3m_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
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

    // qwen3moe.cpp:50, `n_ff_exp ? n_ff_exp : n_ff / n_expert_used`
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);

    // GQA key/value widths: n_embd_head_k * n_head_kv (llama-hparams.h:467)
    let n_embd_k_gqa = key_dim(head_k, n_head_kv);
    let n_embd_v_gqa = value_dim(head_v, n_head_kv);
    // query width is head-sized, not n_embd (qwen3moe.cpp:32)
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
        e_put3(out, head_k * n_head, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        e_put3(out, head_k, 1, 1);
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
    return 0;
}
