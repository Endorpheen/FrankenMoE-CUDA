// Maxwell-V1-Flash tensor shapes and per-layer plan, in Goose.
//
// Same role as goose/model/qwen35moe.goose: the decision/shape half of the
// builder. The graph emitter stays host-side (ggml calls), the shapes and the
// layer plan are owned here.
//
// Anchors: modeling_maxwell_moe.py and src/models/maxwell.cpp load_arch_tensors.
//
// ABI:
//   mx_n_tensors()                     -> tensors per layer in the trunk
//   mx_layer_kind(il, lead)            -> 0 dense FFN, 1 routed MoE
//   mx_shape(tid, p, out4)             -> ndims written to out4, or -1
//   mx_plan(p, kinds)                  -> number of layers written
//
// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_HEAD: i64 = 2;
const P_N_HEAD_KV: i64 = 3;
const P_HEAD_DIM: i64 = 4;
const P_N_FF: i64 = 5;
const P_N_FF_EXP: i64 = 6;
const P_N_FF_SHEXP: i64 = 7;
const P_N_EXPERT: i64 = 8;
const P_N_LAYER: i64 = 9;
const P_LEAD_DENSE: i64 = 10;
const P_HC: i64 = 11;
const P_IDX_HEADS: i64 = 12;
const P_IDX_HEAD_DIM: i64 = 13;

// tensor ids (one layer)
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_FFN_NORM: i64 = 4;
const T_ATTN_Q: i64 = 5;
const T_ATTN_K: i64 = 6;
const T_ATTN_V: i64 = 7;
const T_ATTN_O: i64 = 8;
const T_ATTN_Q_NORM: i64 = 9;
const T_ATTN_K_NORM: i64 = 10;
const T_HC_ATTN_FN: i64 = 11;
const T_HC_ATTN_BASE: i64 = 12;
const T_HC_ATTN_SCALE: i64 = 13;
const T_HC_FFN_FN: i64 = 14;
const T_HC_FFN_BASE: i64 = 15;
const T_HC_FFN_SCALE: i64 = 16;
const T_FFN_GATE: i64 = 17;
const T_FFN_UP: i64 = 18;
const T_FFN_DOWN: i64 = 19;
const T_FFN_GATE_INP: i64 = 20;
const T_FFN_EXP_PROBS_B: i64 = 21;
const T_FFN_GATE_EXPS: i64 = 22;
const T_FFN_UP_EXPS: i64 = 23;
const T_FFN_DOWN_EXPS: i64 = 24;
const T_FFN_GATE_SHEXP: i64 = 25;
const T_FFN_UP_SHEXP: i64 = 26;
const T_FFN_DOWN_SHEXP: i64 = 27;
const T_HC_HEAD_FN: i64 = 28;
const T_HC_HEAD_BASE: i64 = 29;
const T_HC_HEAD_SCALE: i64 = 30;
const T_IDX_Q_PROJ: i64 = 31;
const T_IDX_K_PROJ: i64 = 32;
const T_IDX_Q_NORM: i64 = 33;
const T_IDX_K_NORM: i64 = 34;
const T_MTP_ENORM: i64 = 35;
const T_MTP_HNORM: i64 = 36;
const T_MTP_E_PROJ: i64 = 37;
const T_MTP_H_PROJ: i64 = 38;
const T_MTP_HC_HEAD_FN: i64 = 39;
const T_MTP_FINAL_NORM: i64 = 40;
const T_COUNT: i64 = 41;

export fn mx_n_tensors() -> i64 {
    return T_COUNT;
}

// The leading blocks are dense; Maxwell's mlp_only_layers is a prefix.
// 0 dense FFN, 1 routed MoE, 2 MTP head layer (after the trunk)
export fn mx_layer_kind(il: i64, lead: i64, n_trunk: i64) -> i64 {
    if il >= n_trunk {
        return 2;
    }
    if il < lead {
        return 0;
    }
    return 1;
}

fn put(out4: i64[:], a: i64, b: i64, c: i64, d: i64, nd: i64) -> i64 {
    if out4.len < 4 {
        return -2;
    }
    out4[0] = a;
    out4[1] = b;
    out4[2] = c;
    out4[3] = d;
    return nd;
}

// Shapes are ggml order: ne[0] first. Dense FFN tensors exist only on the lead
// layers; routed tensors only after them.
export fn mx_shape(tid: i64, p: i64[:], out4: i64[:]) -> i64 {
    if p.len < 14 {
        return -1;
    }
    let n_embd: i64 = p[P_N_EMBD];
    let n_vocab: i64 = p[P_N_VOCAB];
    let n_head: i64 = p[P_N_HEAD];
    let n_head_kv: i64 = p[P_N_HEAD_KV];
    let hd: i64 = p[P_HEAD_DIM];
    let n_ff: i64 = p[P_N_FF];
    let n_ff_exp: i64 = p[P_N_FF_EXP];
    let n_ff_shexp: i64 = p[P_N_FF_SHEXP];
    let n_expert: i64 = p[P_N_EXPERT];
    let hc: i64 = p[P_HC];
    let idx_heads: i64 = p[P_IDX_HEADS];
    let idx_hd: i64 = p[P_IDX_HEAD_DIM];
    let mix: i64 = (2 + hc) * hc;

    if tid == T_TOKEN_EMBD {
        return put(out4, n_embd, n_vocab, 1, 1, 2);
    }
    if tid == T_OUTPUT_NORM {
        return put(out4, n_embd, 1, 1, 1, 1);
    }
    if tid == T_OUTPUT {
        return put(out4, n_embd, n_vocab, 1, 1, 2);
    }
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        return put(out4, n_embd, 1, 1, 1, 1);
    }
    if tid == T_ATTN_Q {
        return put(out4, n_embd, hd * n_head, 1, 1, 2);
    }
    if tid == T_ATTN_K || tid == T_ATTN_V {
        return put(out4, n_embd, hd * n_head_kv, 1, 1, 2);
    }
    if tid == T_ATTN_O {
        return put(out4, hd * n_head, n_embd, 1, 1, 2);
    }
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        return put(out4, hd, 1, 1, 1, 1);
    }
    if tid == T_HC_ATTN_FN || tid == T_HC_FFN_FN {
        return put(out4, hc * n_embd, mix, 1, 1, 2);
    }
    if tid == T_HC_ATTN_BASE || tid == T_HC_FFN_BASE {
        return put(out4, mix, 1, 1, 1, 1);
    }
    if tid == T_HC_ATTN_SCALE || tid == T_HC_FFN_SCALE {
        return put(out4, 3, 1, 1, 1, 1);
    }
    if tid == T_FFN_GATE || tid == T_FFN_UP {
        return put(out4, n_embd, n_ff, 1, 1, 2);
    }
    if tid == T_FFN_DOWN {
        return put(out4, n_ff, n_embd, 1, 1, 2);
    }
    if tid == T_FFN_GATE_INP {
        return put(out4, n_embd, n_expert, 1, 1, 2);
    }
    if tid == T_FFN_EXP_PROBS_B {
        return put(out4, n_expert, 1, 1, 1, 1);
    }
    if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
        return put(out4, n_embd, n_ff_exp, n_expert, 1, 3);
    }
    if tid == T_FFN_DOWN_EXPS {
        return put(out4, n_ff_exp, n_embd, n_expert, 1, 3);
    }
    if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
        return put(out4, n_embd, n_ff_shexp, 1, 1, 2);
    }
    if tid == T_FFN_DOWN_SHEXP {
        return put(out4, n_ff_shexp, n_embd, 1, 1, 2);
    }
    if tid == T_HC_HEAD_FN {
        return put(out4, hc * n_embd, hc, 1, 1, 2);
    }
    if tid == T_HC_HEAD_BASE {
        return put(out4, hc, 1, 1, 1, 1);
    }
    if tid == T_HC_HEAD_SCALE {
        return put(out4, 1, 1, 1, 1, 1);
    }
    if tid == T_IDX_Q_PROJ {
        return put(out4, n_embd, idx_heads * idx_hd, 1, 1, 2);
    }
    if tid == T_IDX_K_PROJ {
        return put(out4, n_embd, idx_hd, 1, 1, 2);
    }
    if tid == T_IDX_Q_NORM || tid == T_IDX_K_NORM {
        return put(out4, idx_hd, 1, 1, 1, 1);
    }
    if tid == T_MTP_E_PROJ || tid == T_MTP_H_PROJ {
        return put(out4, n_embd, n_embd, 1, 1, 2);
    }
    if tid == T_MTP_ENORM || tid == T_MTP_HNORM || tid == T_MTP_FINAL_NORM {
        return put(out4, n_embd, 1, 1, 1, 1);
    }
    if tid == T_MTP_HC_HEAD_FN {
        return put(out4, hc * n_embd, hc, 1, 1, 2);
    }
    return -3;
}

export fn mx_plan(p: i64[:], kinds: i64[:]) -> i64 {
    if p.len < 12 {
        return -1;
    }
    let n_layer: i64 = p[P_N_LAYER];
    let lead: i64 = p[P_LEAD_DENSE];
    if kinds.len < n_layer {
        return -2;
    }
    var i: i64 = 0;
    while i < n_layer {
        kinds[i] = mx_layer_kind(i, lead, n_layer);
        i += 1;
    }
    return n_layer;
}
