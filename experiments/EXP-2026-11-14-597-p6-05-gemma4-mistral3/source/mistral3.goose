// P6-05 architecture 20: mistral3 builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose:
//   key_dim, value_dim, kq_scale_auto.
//
// Native anchors (src/models/mistral3.cpp):
//   hparams: rms eps, attention temperature scale and the n_ctx_orig_yarn
//     floor check, yarn beta fast/slow, rope yarn log multiplier   :3-27
//   globals: token_embd, output_norm, optional output             :32-41
//   attention norms, fused-or-split QKV, wo and wo bias           :46-52
//   rope tensors, longrope pair vs single freq factors             :56-62
//   dense FFN with optional biases, or MoE + optional shared
//     expert                                                     :64-85
//   graph: attention, dense or MoE FFN, cvec                      :119-218
//
// Mistral 3 is one attention shape with two per-file FFN kinds, selected by
// n_expert: with 0 experts the loader declares the dense gate/up/down triplet,
// otherwise the router and the expert triplet. The graph then branches on
// ffn_gate_inp presence, which is the same condition one level later.
//
// Details the shape table keeps:
//   - the expert width is plain n_ff. Unlike the qwen and deepseek MoE
//     builders there is no n_ff_exp and no n_ff / n_expert_used fallback, so a
//     routed expert has the same inner width as the dense FFN (:75-77);
//   - the shared expert is optional and carries its own declared width
//     n_ff_shexp, only inside the MoE branch (:80-84);
//   - the fused attn_qkv tensor is optional: when it is absent the loader
//     falls back to separate q/k/v, and its width is q + k + v
//     (llama-model.cpp:3146-3174);
//   - output is optional and duplicated from token_embd when absent (:36-41);
//   - rope factors are optional and duplicated for every layer after the
//     first, and longrope files carry a long/short pair instead of the single
//     freq tensor (:56-62);
//   - attention temperature scaling adds an input and scales Q before
//     attention, but no tensor shape (:108-111, :153-157).

import std;
import base.model_graph;

// p indices -- shared fixture contract of this experiment, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_LAYER_NEXTN: i64 = 4;
const P_N_HEAD: i64 = 5;
const P_N_HEAD_KV: i64 = 6;
const P_HEAD_K: i64 = 7;
const P_HEAD_V: i64 = 8;
const P_HEAD_K_SWA: i64 = 9;
const P_HEAD_V_SWA: i64 = 10;
const P_N_ROT: i64 = 11;
const P_N_FF: i64 = 12;
const P_N_EXPERT: i64 = 13;
const P_N_EXPERT_USED: i64 = 14;
const P_N_FF_EXP: i64 = 15;
const P_N_FF_SHEXP: i64 = 16;
const P_N_EMBD_PER_LAYER: i64 = 17;
const P_N_KV_SHARED: i64 = 18;
const P_N_EMBD_INP: i64 = 19;
const P_HAS_OUTPUT: i64 = 20;
const P_SCALE_BITS: i64 = 21;
const P_ATTN_TEMPERATURE: i64 = 22;
const P_LONGROPE: i64 = 23;
const P_SOFT_CAP: i64 = 24;
const P_LEN: i64 = 25;

const K_DENSE: i64 = 0;
const K_MOE: i64 = 1;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_QKV: i64 = 4;
const T_ATTN_Q: i64 = 5;
const T_ATTN_K: i64 = 6;
const T_ATTN_V: i64 = 7;
const T_ATTN_WO: i64 = 8;
const T_ATTN_WO_B: i64 = 9;
const T_FFN_NORM: i64 = 10;
const T_ROPE_FREQS: i64 = 11;
const T_ROPE_FACTORS_LONG: i64 = 12;
const T_ROPE_FACTORS_SHORT: i64 = 13;
const T_FFN_GATE: i64 = 14;
const T_FFN_DOWN: i64 = 15;
const T_FFN_UP: i64 = 16;
const T_FFN_GATE_B: i64 = 17;
const T_FFN_DOWN_B: i64 = 18;
const T_FFN_UP_B: i64 = 19;
const T_FFN_GATE_INP: i64 = 20;
const T_FFN_GATE_EXPS: i64 = 21;
const T_FFN_DOWN_EXPS: i64 = 22;
const T_FFN_UP_EXPS: i64 = 23;
const T_FFN_GATE_SHEXP: i64 = 24;
const T_FFN_UP_SHEXP: i64 = 25;
const T_FFN_DOWN_SHEXP: i64 = 26;
const T_LEN: i64 = 27;

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

// Layer kind, mistral3.cpp:64 and :172. The loader switches on n_expert, the
// graph on the router tensor; both are the same per-file condition, so one
// n_expert value decides every layer.
export fn m3_layer_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if il < 0 || il >= p[P_N_LAYER] {
        return -1;
    }
    if p[P_N_EXPERT] == 0 {
        return K_DENSE;
    }
    return K_MOE;
}

export fn m3_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    e_ensure(kinds.len >= p[P_N_LAYER], 12);
    var i: i64 = 0;
    while i < p[P_N_LAYER] {
        kinds[i] = m3_layer_kind(p, i);
        i = i + 1;
    }
    return p[P_N_LAYER];
}

// kq_scale, mistral3.cpp:115 -- 1/sqrt(n_embd_head) when f_attention_scale is
// 0.0f, else the configured value. The caller passes the float bits.
export fn m3_kq_scale_auto(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return kq_scale_auto(p[P_SCALE_BITS]);
}

// fused attn_qkv is TENSOR_NOT_REQUIRED (llama-model.cpp:3162).
export fn m3_fused_qkv_optional() -> i64 {
    return 1;
}

// output is TENSOR_NOT_REQUIRED (:36-41).
export fn m3_output_optional() -> i64 {
    return 1;
}

// rope tensor group, mistral3.cpp:56-62.
//   0 -> single rope_freqs tensor
//   1 -> long/short pair (rope_scaling_type == LONGROPE)
export fn m3_rope_tensor_kind(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    if p[P_LONGROPE] != 0 {
        return 1;
    }
    return 0;
}

// shared expert exists only in the MoE branch and only with a declared width
// (:80-84).
export fn m3_has_shexp(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    if m3_layer_kind(p, 0) == K_MOE && p[P_N_FF_SHEXP] > 0 {
        return 1;
    }
    return 0;
}

// attention temperature scaling adds an input and a Q scaling node (:108-111).
export fn m3_attn_temperature(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if p[P_ATTN_TEMPERATURE] != 0 {
        return 1;
    }
    return 0;
}

// Shape of one tensor. Returns 1 and fills out[0..2], 0 when this layer kind
// does not own the tensor.
export fn m3_shape(p: i64[:], il: i64, tid: i64, out: i64[:]) -> i64 {
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
    let n_rot = p[P_N_ROT];
    let n_shexp = p[P_N_FF_SHEXP];

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

    if il < 0 || il >= p[P_N_LAYER] {
        return 0;
    }

    let kind = m3_layer_kind(p, il);

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
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_WO_B {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ROPE_FREQS || tid == T_ROPE_FACTORS_LONG || tid == T_ROPE_FACTORS_SHORT {
        if m3_rope_tensor_kind(p) == 0 {
            if tid != T_ROPE_FREQS {
                return 0;
            }
        } else {
            if tid == T_ROPE_FREQS {
                return 0;
            }
        }
        e_put3(out, n_rot / 2, 1, 1);
        return 1;
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
        if tid == T_FFN_GATE_B || tid == T_FFN_UP_B {
            e_put3(out, n_ff, 1, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_B {
            e_put3(out, n_embd, 1, 1);
            return 1;
        }
        return 0;
    }

    // MoE branch: the expert width is n_ff itself (:75-77).
    if tid == T_FFN_GATE_INP {
        e_put3(out, n_embd, n_expert, 1);
        return 1;
    }
    if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
        e_put3(out, n_embd, n_ff, n_expert);
        return 1;
    }
    if tid == T_FFN_DOWN_EXPS {
        e_put3(out, n_ff, n_embd, n_expert);
        return 1;
    }
    if n_shexp > 0 {
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            e_put3(out, n_embd, n_shexp, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            e_put3(out, n_shexp, n_embd, 1);
            return 1;
        }
    }
    return 0;
}
