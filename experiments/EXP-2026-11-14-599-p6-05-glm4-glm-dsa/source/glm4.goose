// P6-05 architecture 14: glm4 (GLM-4 dense trunk with post-norms and NextN).
//
// Decision/shape half of the builder, not the graph emitter. The plan and the
// trunk/NextN split come from base/trunk.goose.
//
// Native anchors (src/models/glm4.cpp):
//   hparams: rms eps, rope sections, NextN assert      :3-9
//   type switch by n_layer (17 / 40 / 61)              :11-16
//   embeddings and the duplicated output               :22-30
//   per-layer attention, post-norms, FFN               :32-52
//   NextN tensors                                      :54-64
//   graph: post-attention norm, then the residual      :143-149
//   graph: SWIGLU FFN in sequence form, one up tensor  :156-164
//   graph: post-MLP norm, then the residual            :165-169
//   create_tensor_qkv                                  : src/llama-model.cpp:3146
//
// What the shape table keeps, and what makes glm4 different from the qwen
// families already accepted:
//   - the FFN is LLM_FFN_SWIGLU with LLM_FFN_SEQ: gate and up share one
//     tensor, so ffn_up is {n_embd, 2*n_ff} and there is no ffn_gate at all;
//   - every layer carries attn_post_norm and ffn_post_norm of width n_embd on
//     top of attn_norm and ffn_norm, and the graph applies them inside the
//     residual branch;
//   - the query block is n_embd_head_k * n_head and attn_out reads it back;
//   - the graph asserts n_embd_head_k == n_embd_head_v (:75), so the key and
//     value blocks have the same width but are still computed separately;
//   - NextN layers (il >= n_layer) declare the same attention/FFN tensors with
//     TENSOR_SKIP plus the nextn block, and the forward pass never runs them
//     (:32-37, :98-100). The plan therefore marks them, it does not remove
//     their trunk rows.

import base.trunk;

// The abort hook. base/trunk.goose binds the same C symbol under a name local
// to that module, so this file declares the one its own helpers call.
extern "fg200g_fail" fn fail(code: i64);

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;      // trunk layers (n_layer)
const P_N_NEXTN: i64 = 3;      // declared NextN blocks (n_layer_nextn)
const P_N_HEAD: i64 = 4;
const P_N_HEAD_KV: i64 = 5;
const P_N_FF: i64 = 6;
const P_HEAD_K: i64 = 7;       // n_embd_head_k
const P_HEAD_V: i64 = 8;       // n_embd_head_v
const P_HAS_OUTPUT: i64 = 9;   // 0 -> output duplicates token_embd
const P_HAS_ATTN_BIAS: i64 = 10; // 1 -> fused qkv bias, 0 -> legacy q/k/v
const P_LEN: i64 = 11;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_WQKV: i64 = 4;
const T_WQKV_B: i64 = 5;
const T_WQ_B: i64 = 6;
const T_WK_B: i64 = 7;
const T_WV_B: i64 = 8;
const T_WO: i64 = 9;
const T_ATTN_POST_NORM: i64 = 10;
const T_FFN_NORM: i64 = 11;
const T_FFN_UP: i64 = 12;
const T_FFN_DOWN: i64 = 13;
const T_FFN_POST_NORM: i64 = 14;
const T_NEXTN_EH_PROJ: i64 = 15;
const T_NEXTN_ENORM: i64 = 16;
const T_NEXTN_HNORM: i64 = 17;
const T_NEXTN_EMBED: i64 = 18;
const T_NEXTN_SHARED_HEAD: i64 = 19;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 20;
const T_LEN: i64 = 21;

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

// g4_n_layer_all: block_count = trunk + NextN (glm4.cpp:32).
export fn g4_n_layer_all(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    return t_n_layer_all(p[P_N_LAYER], p[P_N_NEXTN]);
}

// g4_plan: one kind per block; trunk below n_layer, NextN above.
export fn g4_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return t_plan(p[P_N_LAYER], g4_n_layer_all(p), kinds);
}

// g4_kind: the plan decision for one block, -1 outside the model.
export fn g4_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    return t_layer_kind(p[P_N_LAYER], g4_n_layer_all(p), il);
}

// g4_nextn_ok: glm4.cpp:9, `n_layer_nextn < n_layer_all`. 1 means the file is
// usable, 0 means the loader assert fires and no plan may be built.
export fn g4_nextn_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    return t_nextn_ok(p[P_N_NEXTN], g4_n_layer_all(p));
}

// g4_q_width: glm4.cpp:42 hands create_tensor_qkv `n_embd_head_k * n_head`.
export fn g4_q_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    e_ensure(p[P_N_HEAD] > 0, 18);
    e_ensure(p[P_HEAD_K] > 0, 19);
    return p[P_HEAD_K] * p[P_N_HEAD];
}

// g4_k_width / g4_v_width: n_embd_k_gqa and n_embd_v_gqa in llama-model.h,
// one head width times the key/value head count.
export fn g4_k_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 22);
    e_ensure(p[P_N_HEAD_KV] > 0, 23);
    e_ensure(p[P_HEAD_K] > 0, 24);
    return p[P_HEAD_K] * p[P_N_HEAD_KV];
}

export fn g4_v_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 25);
    e_ensure(p[P_N_HEAD_KV] > 0, 26);
    e_ensure(p[P_HEAD_V] > 0, 27);
    return p[P_HEAD_V] * p[P_N_HEAD_KV];
}

// g4_qkv_width: the fused row create_tensor_qkv declares, llama-model.cpp:3149.
export fn g4_qkv_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 28);
    return g4_q_width(p) + g4_k_width(p) + g4_v_width(p);
}

// g4_ffn_up_width: glm4.cpp:50, `{n_embd, n_ff * 2}`. LLM_FFN_SEQ with
// LLM_FFN_SWIGLU reads gate and up from this one tensor, so the width is
// doubled and no separate ffn_gate row exists.
export fn g4_ffn_up_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 29);
    e_ensure(p[P_N_FF] > 0, 30);
    return 2 * p[P_N_FF];
}

// g4_output_dup: glm4.cpp:27-30.
export fn g4_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 31);
    return t_output_dup(p[P_HAS_OUTPUT]);
}

// g4_nextn_eh_proj: glm4.cpp:56, `{2*n_embd, n_embd}`.
export fn g4_nextn_eh_proj(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 32);
    return t_nextn_eh_proj(p[P_N_EMBD]);
}

// g4_head_split_ok: glm4.cpp:75, `n_embd_head_v == n_embd_head_k`.
export fn g4_head_split_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 33);
    if p[P_HEAD_K] == p[P_HEAD_V] {
        return 1;
    }
    return 0;
}

// Shape of one tensor. Returns 1 and fills out[0..2] when the architecture
// owns the tensor for this block kind, 0 otherwise. Per-layer rows are the
// same on trunk and NextN blocks: glm4 declares them with TENSOR_SKIP, not
// with fewer names.
export fn g4_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 40);
    e_ensure(out.len >= 3, 41);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let q_width = g4_q_width(p);
    let k_width = g4_k_width(p);
    let v_width = g4_v_width(p);
    let qkv = g4_qkv_width(p);

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // output is optional; absent means duplicate token_embd, same shape.
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind != K_TRUNK && kind != K_NEXTN {
        return 0;
    }

    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_POST_NORM || tid == T_FFN_POST_NORM {
        // glm4.cpp:46,52: post-norms are per hidden state, not per head.
        e_put3(out, n_embd, 1, 1);
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
    if tid == T_WK_B {
        if p[P_HAS_ATTN_BIAS] != 0 {
            return 0;
        }
        e_put3(out, k_width, 1, 1);
        return 1;
    }
    if tid == T_WV_B {
        if p[P_HAS_ATTN_BIAS] != 0 {
            return 0;
        }
        e_put3(out, v_width, 1, 1);
        return 1;
    }
    if tid == T_WO {
        // glm4.cpp:44: rows are the query block, not n_embd.
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_FFN_UP {
        // glm4.cpp:50: one row for gate and up together.
        e_put3(out, n_embd, g4_ffn_up_width(p), 1);
        return 1;
    }
    if tid == T_FFN_DOWN {
        e_put3(out, p[P_N_FF], n_embd, 1);
        return 1;
    }

    if kind != K_NEXTN {
        return 0;
    }
    if tid == T_NEXTN_EH_PROJ {
        e_put3(out, g4_nextn_eh_proj(p), n_embd, 1);
        return 1;
    }
    if tid == T_NEXTN_ENORM || tid == T_NEXTN_HNORM || tid == T_NEXTN_SHARED_HEAD_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_NEXTN_EMBED || tid == T_NEXTN_SHARED_HEAD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }

    return 0;
}
