// P6-05 architecture 3: qwen3 (dense GQA with qk-norm).
//
// Decision/shape half of the builder, not the graph emitter. The plan comes
// from base/dense.goose: all trunk layers are attention layers.
//
// Native anchors (src/models/qwen3.cpp):
//   type switch by n_layer                        :5-16
//   embeddings, output, optional heads            :22-33
//   per-layer attention + qk-norm + ffn           :37-50
//   graph: qk-norm before RoPE, GQA QKV           :80-127
//   graph: gated FFN, SILU-PAR                    :156-164
//   create_tensor_qkv                             : src/llama-model.cpp:3860
//
// qwen3 differs from qwen2 (architecture 6) in what the shape table must keep:
//   - the query block is n_embd_head_k * n_head, not n_embd; for the 0.6B
//     geometry (n_embd 1024, 16 heads x 128) the two differ by 2x;
//   - every layer carries attn_q_norm and attn_k_norm of width n_embd_head_k,
//     applied before RoPE;
//   - there is no output bias tensor at all;
//   - there are optional cls_out and dense_2_out heads (reranker/embedding).
//
// The fused and legacy attention biases still exist here because both
// architectures go through create_tensor_qkv; qwen3 just normally ships none.

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
const P_HAS_OUTPUT: i64 = 8;      // 0 -> output duplicates token_embd
const P_HAS_ATTN_BIAS: i64 = 9;   // fused qkv bias, else legacy q/k/v biases
const P_N_CLS_OUT: i64 = 10;      // 0 -> no rerank head
const P_N_EMBD_OUT: i64 = 11;     // 0 -> no dense_2_out head
const P_LEN: i64 = 12;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_CLS_OUT: i64 = 3;
const T_DENSE_2_OUT: i64 = 4;
const T_DENSE_2_OUT_B: i64 = 5;
const T_ATTN_NORM: i64 = 6;
const T_FFN_NORM: i64 = 7;
const T_WQKV: i64 = 8;
const T_WQKV_B: i64 = 9;
const T_WQ_B: i64 = 10;
const T_WK_B: i64 = 11;
const T_WV_B: i64 = 12;
const T_WO: i64 = 13;
const T_ATTN_Q_NORM: i64 = 14;
const T_ATTN_K_NORM: i64 = 15;
const T_FFN_GATE: i64 = 16;
const T_FFN_UP: i64 = 17;
const T_FFN_DOWN: i64 = 18;
const T_LEN: i64 = 19;

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

// q3_q_width: qwen3.cpp:41, the query block feeds from
// n_embd_head_k * n_head, unlike qwen2's n_embd.
export fn q3_q_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    return d_gqa_width(p[P_HEAD_K], p[P_N_HEAD]);
}

// q3_kv_width: qwen3.cpp:41 and llama-model.h:990-992, one value head times
// n_head_kv heads for both the key and the value block.
export fn q3_kv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    return d_gqa_width(p[P_HEAD_V], p[P_N_HEAD_KV]);
}

// q3_qkv_width: the fused row create_tensor_qkv declares, qwen3.cpp:41.
export fn q3_qkv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    return d_qkv_width(q3_q_width(p), q3_kv_width(p), q3_kv_width(p));
}

// q3_qk_norm: qwen3 normalises Q and K per head before RoPE, qwen3.cpp:90-104.
// Always 1 here; qwen2 (architecture 6) returns 0.
export fn q3_qk_norm() -> i64 {
    return 1;
}

// q3_output_dup: output is optional; when absent it is token_embd duplicated
// (qwen3.cpp:23-26). 1 means the duplicated source.
export fn q3_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    return d_output_dup(p[P_HAS_OUTPUT]);
}

// q3_plan: all trunk layers are attention, dense.goose:d_plan.
export fn q3_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return d_plan(p[P_N_LAYER], kinds);
}

// Shape of one tensor. Returns 1 and fills out[0..2] when the architecture
// owns the tensor, 0 otherwise. Per-layer tensors use the same shape on every
// layer because the plan is uniform.
export fn q3_shape(p: i64[:], tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let q_width = q3_q_width(p);
    let kv_width = q3_kv_width(p);
    let qkv = q3_qkv_width(p);

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
    if tid == T_CLS_OUT {
        if p[P_N_CLS_OUT] <= 0 {
            return 0;
        }
        e_put3(out, n_embd, p[P_N_CLS_OUT], 1);
        return 1;
    }
    if tid == T_DENSE_2_OUT {
        if p[P_N_EMBD_OUT] <= 0 {
            return 0;
        }
        e_put3(out, n_embd, p[P_N_EMBD_OUT], 1);
        return 1;
    }
    if tid == T_DENSE_2_OUT_B {
        if p[P_N_EMBD_OUT] <= 0 {
            return 0;
        }
        e_put3(out, p[P_N_EMBD_OUT], 1, 1);
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
        e_put3(out, qkv, 1, 1);
        return 1;
    }
    if tid == T_WQ_B {
        if p[P_HAS_ATTN_BIAS] != 0 {
            return 0;
        }
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
        // qwen3.cpp:42: rows are the query block, not n_embd.
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        // qwen3.cpp:44-45: one width per head, not per hidden state.
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
