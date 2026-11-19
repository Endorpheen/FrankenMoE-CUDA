// P6-05 architecture 2: qwen4exp (Qwen3.8 family) builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   is_recr, n_ff_exp, n_ff_shexp, key_dim, value_dim, conv_dim, wqkv_width,
//   head_v_dim, conv_channels, qkv_dim, q_full_width.
//
// Native anchors (src/models/qwen4exp.cpp):
//   hparams, HC/PLE/indexer validation   :26-185
//   global tensors, HC head, PLE table   :188-232
//   per-layer HC modules                 :247-254
//   attention + indexer tensors          :256-270
//   recurrent (gated delta net) tensors  :271-282
//   PLE layer tensors                    :284-291
//   routed/shared experts                :293-301
//   MTP nextn tensors                    :312-322
//
// qwen4exp differs from qwen35moe in four ways that the shape table must keep:
//   - there is no output_norm; the final HC mixer normalises,
//   - two HC modules per layer (attn and ffn), with [n_embd,hc] gammas and
//     low-rank [hc*n_embd,hc_lr] / [hc_lr,hc*n_embd] projections,
//   - an indexer on full-attention layers (q_proj, k_proj, q/k norms),
//   - optional PLE on recurrent layers and one optional per-layer embedding
//     table, plus MTP hnorm/hc-head tensors carrying [n_embd,hc].

import std;
import base.model_graph;

// The graph module's abort is renamed to fgg_fail when the unit is composed, so an importer
// cannot call `fail` and still link. This module binds the same primitive under its own name.
extern "fg200g_fail" fn q4_abort(code: i64);
// p indices
const Q4P_N_EMBD: i64 = 0;
const Q4P_N_VOCAB: i64 = 1;
const Q4P_N_LAYER: i64 = 2;
const Q4P_N_LAYER_ALL: i64 = 3;
const Q4P_N_HEAD: i64 = 4;
const Q4P_N_HEAD_KV: i64 = 5;
const Q4P_N_FF: i64 = 6;
const Q4P_N_EXPERT: i64 = 7;
const Q4P_N_EXPERT_USED: i64 = 8;
const Q4P_N_FF_EXP: i64 = 9;
const Q4P_N_FF_SHEXP: i64 = 10;
const Q4P_D_CONV: i64 = 11;
const Q4P_D_INNER: i64 = 12;
const Q4P_D_STATE: i64 = 13;
const Q4P_DT_RANK: i64 = 14;
const Q4P_N_GROUP: i64 = 15;
const Q4P_HEAD_K: i64 = 16;
const Q4P_INTERVAL: i64 = 17;
const Q4P_HC: i64 = 18;
const Q4P_HC_LR: i64 = 19;
const Q4P_IDX_NHEAD: i64 = 20;
const Q4P_IDX_DIM: i64 = 21;
const Q4P_PLE_HEAD_DIM: i64 = 22;
const Q4P_PLE_ROWS: i64 = 23;
const Q4P_PLE_CONV: i64 = 24;
const Q4P_PLE_LAYER: i64 = 25;   // -1 = no PLE layer
const Q4P_HAS_OUTPUT: i64 = 26;
const Q4P_LEN: i64 = 27;

const Q4K_ATTN: i64 = 0;
const Q4K_RECR: i64 = 1;
const Q4K_MTP: i64 = 2;

// tensor ids
const Q4T_TOKEN_EMBD: i64 = 0;
const Q4T_HC_HEAD_NORM: i64 = 1;
const Q4T_HC_HEAD_DOWN: i64 = 2;
const Q4T_HC_HEAD_UP: i64 = 3;
const Q4T_OUTPUT: i64 = 4;
const Q4T_PER_LAYER_TOK_EMBD: i64 = 5;
const Q4T_HC_ATTN_NORM: i64 = 6;
const Q4T_HC_ATTN_DOWN: i64 = 7;
const Q4T_HC_ATTN_UP: i64 = 8;
const Q4T_HC_ATTN_INJECT: i64 = 9;
const Q4T_HC_FFN_NORM: i64 = 10;
const Q4T_HC_FFN_DOWN: i64 = 11;
const Q4T_HC_FFN_UP: i64 = 12;
const Q4T_HC_FFN_INJECT: i64 = 13;
const Q4T_ATTN_QKV: i64 = 14;
const Q4T_ATTN_OUT: i64 = 15;
const Q4T_ATTN_Q_NORM: i64 = 16;
const Q4T_ATTN_K_NORM: i64 = 17;
const Q4T_INDEXER_Q_PROJ: i64 = 18;
const Q4T_INDEXER_K_PROJ: i64 = 19;
const Q4T_INDEXER_Q_NORM: i64 = 20;
const Q4T_INDEXER_K_NORM: i64 = 21;
const Q4T_WQKV: i64 = 22;
const Q4T_WQKV_GATE: i64 = 23;
const Q4T_SSM_CONV1D: i64 = 24;
const Q4T_SSM_DT: i64 = 25;
const Q4T_SSM_A: i64 = 26;
const Q4T_SSM_BETA: i64 = 27;
const Q4T_SSM_ALPHA: i64 = 28;
const Q4T_SSM_NORM: i64 = 29;
const Q4T_SSM_OUT: i64 = 30;
const Q4T_PLE_KEY: i64 = 31;
const Q4T_PLE_VALUE: i64 = 32;
const Q4T_PLE_NORM_KEY: i64 = 33;
const Q4T_PLE_NORM_QUERY: i64 = 34;
const Q4T_PLE_NORM_CONV: i64 = 35;
const Q4T_PLE_CONV1D: i64 = 36;
const Q4T_FFN_GATE_INP: i64 = 37;
const Q4T_FFN_DOWN_EXPS: i64 = 38;
const Q4T_FFN_GATE_UP_EXPS: i64 = 39;
const Q4T_FFN_GATE_EXPS: i64 = 40;
const Q4T_FFN_UP_EXPS: i64 = 41;
const Q4T_FFN_GATE_INP_SHEXP: i64 = 42;
const Q4T_FFN_GATE_SHEXP: i64 = 43;
const Q4T_FFN_UP_SHEXP: i64 = 44;
const Q4T_FFN_DOWN_SHEXP: i64 = 45;
const Q4T_NEXTN_EH_PROJ: i64 = 46;
const Q4T_NEXTN_ENORM: i64 = 47;
const Q4T_NEXTN_HNORM: i64 = 48;
const Q4T_NEXTN_HC_HEAD_NORM: i64 = 49;
const Q4T_NEXTN_HC_HEAD_DOWN: i64 = 50;
const Q4T_NEXTN_HC_HEAD_UP: i64 = 51;
const Q4T_LEN: i64 = 52;

fn e_ensure(v: bool, c: i64) {
    if !v {
        q4_abort(c);
    }
}

fn e_put3(out: i64[:], a: i64, b: i64, c: i64) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
}

export fn q4_kind(p: i64[:], il: i64, flags_base: i64) -> i64 {
    e_ensure(p.len >= Q4P_LEN, 10);
    if il < 0 {
        return -1;
    }
    if il >= p[Q4P_N_LAYER] {
        return if il < p[Q4P_N_LAYER_ALL] { Q4K_MTP } else { -1 };
    }
    if flags_base != 0 {
        let r = is_recr(il, p[Q4P_N_LAYER_ALL], flags_base as! u64);
        if r == 1 { return Q4K_RECR; }
        if r == 0 { return Q4K_ATTN; }
        return -1;
    }
    let interval = p[Q4P_INTERVAL];
    if interval <= 0 {
        return Q4K_ATTN;
    }
    return if (il + 1) % interval != 0 { Q4K_RECR } else { Q4K_ATTN };
}

export fn q4_is_ple(p: i64[:], il: i64) -> i64 {
    if p[Q4P_PLE_LAYER] >= 0 && il == p[Q4P_PLE_LAYER] {
        return 1;
    }
    return 0;
}

export fn q4_plan(p: i64[:], flags_base: i64, kinds: i64[:]) -> i64 {
    e_ensure(p.len >= Q4P_LEN, 11);
    e_ensure(kinds.len >= p[Q4P_N_LAYER_ALL], 12);
    var i: i64 = 0;
    while i < p[Q4P_N_LAYER_ALL] {
        kinds[i] = q4_kind(p, i, flags_base);
        i = i + 1;
    }
    return p[Q4P_N_LAYER_ALL];
}

// Shape of one tensor for a layer kind. Returns 1 and fills out[0..2] when the
// kind owns it, 0 otherwise. No output_norm exists in this architecture.
export fn q4_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= Q4P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[Q4P_N_EMBD];
    let n_vocab = p[Q4P_N_VOCAB];
    let n_ff = p[Q4P_N_FF];
    let n_expert = p[Q4P_N_EXPERT];
    let used = p[Q4P_N_EXPERT_USED];
    let n_ff_exp_v = n_ff_exp(p[Q4P_N_FF_EXP], n_ff, used);
    let n_ff_shexp_v = n_ff_shexp(p[Q4P_N_FF_SHEXP], n_ff);

    let hc = p[Q4P_HC];
    let hc_lr = p[Q4P_HC_LR];
    let hc_dim = hc * n_embd;

    let d_state = p[Q4P_D_STATE];
    let d_inner = p[Q4P_D_INNER];
    let dt_rank = p[Q4P_DT_RANK];
    let n_group = p[Q4P_N_GROUP];
    let head_v = head_v_dim(d_inner, dt_rank);
    let key_d = key_dim(d_state, n_group);
    let value_d = value_dim(head_v, dt_rank);
    let conv_d = conv_dim(key_d, value_d);
    let qkv_w = wqkv_width(key_d, value_d);

    let head_k = p[Q4P_HEAD_K];
    let n_head = p[Q4P_N_HEAD];
    let n_head_kv = p[Q4P_N_HEAD_KV];
    let n_embd_k_gqa = head_k * n_head_kv;
    let n_embd_v_gqa = head_v * n_head_kv;
    let q_width = q_full_width(head_k) * n_head;
    let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;
    let wo_in = head_k * n_head;

    let idx_dim = p[Q4P_IDX_DIM];
    let idx_q_out = p[Q4P_IDX_NHEAD] * idx_dim;

    if tid == Q4T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == Q4T_OUTPUT {
        // output is optional; absent means duplicate token_embd, same shape.
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == Q4T_HC_HEAD_NORM {
        e_put3(out, n_embd, hc, 1);
        return 1;
    }
    if tid == Q4T_HC_HEAD_DOWN {
        e_put3(out, hc_dim, hc_lr, 1);
        return 1;
    }
    if tid == Q4T_HC_HEAD_UP {
        e_put3(out, hc_lr, hc_dim, 1);
        return 1;
    }
    if tid == Q4T_PER_LAYER_TOK_EMBD {
        // the table exists only when a PLE layer is declared.
        if p[Q4P_PLE_LAYER] < 0 || p[Q4P_PLE_HEAD_DIM] <= 0 || p[Q4P_PLE_ROWS] <= 0 {
            return 0;
        }
        e_put3(out, p[Q4P_PLE_HEAD_DIM], p[Q4P_PLE_ROWS], 1);
        return 1;
    }

    if kind == Q4K_ATTN || kind == Q4K_RECR || kind == Q4K_MTP {
        if tid == Q4T_HC_ATTN_NORM || tid == Q4T_HC_FFN_NORM {
            e_put3(out, n_embd, hc, 1);
            return 1;
        }
        if tid == Q4T_HC_ATTN_DOWN || tid == Q4T_HC_FFN_DOWN {
            e_put3(out, hc_dim, hc_lr, 1);
            return 1;
        }
        if tid == Q4T_HC_ATTN_UP || tid == Q4T_HC_FFN_UP {
            e_put3(out, hc_lr, hc_dim, 1);
            return 1;
        }
        if tid == Q4T_HC_ATTN_INJECT || tid == Q4T_HC_FFN_INJECT {
            e_put3(out, hc_dim, hc, 1);
            return 1;
        }
    }

    if kind == Q4K_ATTN || kind == Q4K_MTP {
        if tid == Q4T_ATTN_QKV { e_put3(out, n_embd, qkv_fused, 1); return 1; }
        if tid == Q4T_ATTN_OUT { e_put3(out, wo_in, n_embd, 1); return 1; }
        if tid == Q4T_ATTN_Q_NORM || tid == Q4T_ATTN_K_NORM { e_put3(out, head_k, 1, 1); return 1; }
        if tid == Q4T_INDEXER_Q_PROJ { e_put3(out, n_embd, idx_q_out, 1); return 1; }
        if tid == Q4T_INDEXER_K_PROJ { e_put3(out, n_embd, idx_dim, 1); return 1; }
        if tid == Q4T_INDEXER_Q_NORM || tid == Q4T_INDEXER_K_NORM { e_put3(out, idx_dim, 1, 1); return 1; }
    }

    if kind == Q4K_RECR {
        if tid == Q4T_WQKV { e_put3(out, n_embd, qkv_w, 1); return 1; }
        if tid == Q4T_WQKV_GATE { e_put3(out, n_embd, value_d, 1); return 1; }
        if tid == Q4T_SSM_CONV1D { e_put3(out, p[Q4P_D_CONV], conv_d, 1); return 1; }
        if tid == Q4T_SSM_DT || tid == Q4T_SSM_A { e_put3(out, dt_rank, 1, 1); return 1; }
        if tid == Q4T_SSM_BETA || tid == Q4T_SSM_ALPHA { e_put3(out, n_embd, dt_rank, 1); return 1; }
        if tid == Q4T_SSM_NORM { e_put3(out, head_v, 1, 1); return 1; }
        if tid == Q4T_SSM_OUT { e_put3(out, value_d, n_embd, 1); return 1; }
        if tid == Q4T_PLE_KEY { e_put3(out, n_embd, hc_dim, 1); return 1; }
        if tid == Q4T_PLE_VALUE { e_put3(out, n_embd, n_embd, 1); return 1; }
        if tid == Q4T_PLE_NORM_KEY || tid == Q4T_PLE_NORM_QUERY || tid == Q4T_PLE_NORM_CONV {
            e_put3(out, n_embd, hc, 1);
            return 1;
        }
        if tid == Q4T_PLE_CONV1D { e_put3(out, p[Q4P_PLE_CONV], hc_dim, 1); return 1; }
    }

    if kind == Q4K_ATTN || kind == Q4K_RECR || kind == Q4K_MTP {
        if tid == Q4T_FFN_GATE_INP { e_put3(out, n_embd, n_expert, 1); return 1; }
        if tid == Q4T_FFN_DOWN_EXPS { e_put3(out, n_ff_exp_v, n_embd, n_expert); return 1; }
        if tid == Q4T_FFN_GATE_UP_EXPS { e_put3(out, n_embd, n_ff_exp_v * 2, n_expert); return 1; }
        if tid == Q4T_FFN_GATE_EXPS || tid == Q4T_FFN_UP_EXPS { e_put3(out, n_embd, n_ff_exp_v, n_expert); return 1; }
        if tid == Q4T_FFN_GATE_INP_SHEXP { e_put3(out, n_embd, 1, 1); return 1; }
        if tid == Q4T_FFN_GATE_SHEXP || tid == Q4T_FFN_UP_SHEXP { e_put3(out, n_embd, n_ff_shexp_v, 1); return 1; }
        if tid == Q4T_FFN_DOWN_SHEXP { e_put3(out, n_ff_shexp_v, n_embd, 1); return 1; }
    }

    if kind == Q4K_MTP {
        if tid == Q4T_NEXTN_EH_PROJ { e_put3(out, n_embd * 2, n_embd, 1); return 1; }
        if tid == Q4T_NEXTN_ENORM { e_put3(out, n_embd, 1, 1); return 1; }
        if tid == Q4T_NEXTN_HNORM || tid == Q4T_NEXTN_HC_HEAD_NORM { e_put3(out, n_embd, hc, 1); return 1; }
        if tid == Q4T_NEXTN_HC_HEAD_DOWN { e_put3(out, hc_dim, hc_lr, 1); return 1; }
        if tid == Q4T_NEXTN_HC_HEAD_UP { e_put3(out, hc_lr, hc_dim, 1); return 1; }
    }

    return 0;
}
