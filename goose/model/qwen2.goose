// P6-05 architecture 6: qwen2 (dense GQA with attention and output biases).
//
// Decision/shape half of the builder, not the graph emitter. The plan comes
// from base/dense.goose: all trunk layers are attention layers.
//
// Native anchors (src/models/qwen2.cpp):
//   type switch by n_layer / n_embd                 :5-20
//   embeddings, output, output bias                 :23-33
//   per-layer attention, no qk-norm                 :38-50
//   graph: plain RoPE, GQA QKV                      :68-108
//   graph: gated FFN, SILU-PAR                      :128-136
//   graph: lm_head + optional output bias           :160-167
//   create_tensor_qkv                               : src/llama-model.cpp:3860
//
// qwen2 differs from qwen3 (architecture 3) in what the shape table must keep:
//   - the query block is n_embd directly (qwen2.cpp:41), not
//     n_embd_head_k * n_head;
//   - there is no qk-norm: Q and K go straight to RoPE (qwen2.cpp:81-98), and
//     no attn_q_norm / attn_k_norm tensor is declared;
//   - the bias set is real: fused attn_qkv.bias or legacy q/k/v biases from
//     create_tensor_qkv, and output.bias (qwen2.cpp:26, 164-166);
//   - there is no wo bias: the loader never declares one, so build_attn is
//     passed a null wo_b (qwen2.cpp:104-106);
//   - there are no cls_out / dense_2_out heads.

import base.dense;

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
const P_HAS_ATTN_BIAS: i64 = 9;    // fused qkv bias, else legacy q/k/v biases
const P_HAS_OUTPUT_BIAS: i64 = 10;
const P_LEN: i64 = 11;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_OUTPUT_B: i64 = 3;
const T_ATTN_NORM: i64 = 4;
const T_WQKV: i64 = 5;
const T_WQKV_B: i64 = 6;
const T_WQ_B: i64 = 7;
const T_WK_B: i64 = 8;
const T_WV_B: i64 = 9;
const T_WO: i64 = 10;
const T_WO_B: i64 = 11;   // declared to prove absence
const T_FFN_NORM: i64 = 12;
const T_FFN_GATE: i64 = 13;
const T_FFN_UP: i64 = 14;
const T_FFN_DOWN: i64 = 15;
const T_ATTN_Q_NORM: i64 = 16;   // declared to prove absence
const T_ATTN_K_NORM: i64 = 17;   // declared to prove absence
const T_LEN: i64 = 18;

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

// q2_q_width: qwen2.cpp:41 passes n_embd as the query width. For every
// released qwen2 size n_embd == n_embd_head_k * n_head, so the two expressions
// agree numerically; the difference is documented, not observable in the gate.
export fn q2_q_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    return p[P_N_EMBD];
}

// q2_kv_width: qwen2.cpp:41 with n_embd_gqa = n_embd_head_v * n_head_kv.
export fn q2_kv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    return d_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);
}

// q2_qkv_width: the fused row, qwen2.cpp:41.
export fn q2_qkv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    return d_qkv_width(q2_q_width(p), q2_kv_width(p), q2_kv_width(p));
}

// q2_has_qk_norm: qwen2 has no qk-norm pass and no such tensors, qwen2.cpp:81.
export fn q2_has_qk_norm() -> i64 {
    return 0;
}

// q2_output_dup: output is optional; when absent it is token_embd duplicated
// (qwen2.cpp:28-31). 1 means the duplicated source.
export fn q2_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    return d_output_dup(p[P_HAS_OUTPUT]);
}

// q2_plan: all trunk layers are attention, dense.goose:d_plan.
export fn q2_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return d_plan(p[P_N_LAYER], kinds);
}

// Shape of one tensor. Returns 1 and fills out[0..2] when the architecture
// owns the tensor, 0 otherwise. Per-layer tensors use the same shape on every
// layer because the plan is uniform.
export fn q2_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let q_width = q2_q_width(p);
    let kv_width = q2_kv_width(p);
    let qkv = q2_qkv_width(p);

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // output is optional; absent means duplicate token_embd, same shape.
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM || tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_OUTPUT_B {
        if p[P_HAS_OUTPUT_BIAS] == 0 {
            return 0;
        }
        e_put3(out, n_vocab, 1, 1);
        return 1;
    }
    if tid == T_WQKV {
        e_put3(out, n_embd, qkv, 1);
        return 1;
    }
    if tid == T_WQKV_B {
        if p[P_HAS_ATTN_BIAS] == 0 {
            return 0;
        }
        // one fused row, qwen2.cpp:41 through llama-model.cpp:3881
        e_put3(out, qkv, 1, 1);
        return 1;
    }
    if tid == T_WQ_B {
        if p[P_HAS_ATTN_BIAS] != 0 {
            return 0;
        }
        // legacy split bias, llama-model.cpp:3892, query width is q_width
        e_put3(out, q_width, 1, 1);
        return 1;
    }
    if tid == T_WK_B || tid == T_WV_B {
        if p[P_HAS_ATTN_BIAS] != 0 {
            return 0;
        }
        e_put3(out, kv_width, 1, 1);
        return 1;
    }
    if tid == T_WO {
        // qwen2.cpp:42: square n_embd x n_embd, not the query-block width.
        e_put3(out, n_embd, n_embd, 1);
        return 1;
    }
    if tid == T_WO_B {
        // qwen2 declares no attn_out.bias at all, qwen2.cpp:38-50.
        return 0;
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        // no qk-norm tensors exist for this architecture.
        return 0;
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
