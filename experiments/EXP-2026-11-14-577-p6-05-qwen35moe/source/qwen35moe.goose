// P6-05 architecture 1: qwen35moe (Apex) builder preparation.
//
// This is the decision/shape half of the builder, not the graph emitter. It
// reuses the accepted qwen35moe slice in base/model_graph.goose (a verbatim
// copy of goose/model/goose-model-graph.goose with only its `fn main()` removed):
//   is_recr, n_ff_exp, n_ff_shexp, key_dim, value_dim, conv_dim, wqkv_width,
//   head_v_dim, conv_channels, qkv_dim, q_full_width, gate_elem_offset.
//
// Native anchors (src/models/qwen35moe.cpp):
//   trunk tensors and shapes        :54-100
//   MTP tensors and shapes          :102-146
//   layer loop and kind choice      :166-232
//   full-attention QKV/norm/rope    :276-355
//   gated delta net geometry        :357-484
//   MoE + gated shared expert       :486-548
//   MTP block                       :551-735
//
// The parameter vector p is indexed by the P_* constants. The shape function
// answers exactly what load_arch_tensors declares; the plan function answers
// the per-layer kind (0 attention, 1 recurrent, 2 MTP).

import std;
import base.model_graph;

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_HEAD: i64 = 4;
const P_N_HEAD_KV: i64 = 5;
const P_N_FF: i64 = 6;
const P_N_EXPERT: i64 = 7;
const P_N_EXPERT_USED: i64 = 8;
const P_N_FF_EXP: i64 = 9;      // declared, 0 -> n_ff / n_expert_used
const P_N_FF_SHEXP: i64 = 10;   // declared, 0 -> n_ff
const P_D_CONV: i64 = 11;
const P_D_INNER: i64 = 12;
const P_D_STATE: i64 = 13;
const P_DT_RANK: i64 = 14;
const P_N_GROUP: i64 = 15;
const P_HEAD_K: i64 = 16;
const P_INTERVAL: i64 = 17;
const P_HAS_OUTPUT: i64 = 18;
const P_LEN: i64 = 19;

// layer kinds
const K_ATTN: i64 = 0;
const K_RECR: i64 = 1;
const K_MTP: i64 = 2;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_POST_NORM: i64 = 4;
const T_ATTN_QKV: i64 = 5;
const T_ATTN_OUT: i64 = 6;
const T_ATTN_Q_NORM: i64 = 7;
const T_ATTN_K_NORM: i64 = 8;
const T_WQKV: i64 = 9;
const T_WQKV_GATE: i64 = 10;
const T_SSM_CONV1D: i64 = 11;
const T_SSM_DT: i64 = 12;
const T_SSM_A: i64 = 13;
const T_SSM_BETA: i64 = 14;
const T_SSM_ALPHA: i64 = 15;
const T_SSM_NORM: i64 = 16;
const T_SSM_OUT: i64 = 17;
const T_FFN_GATE_INP: i64 = 18;
const T_FFN_DOWN_EXPS: i64 = 19;
const T_FFN_GATE_UP_EXPS: i64 = 20;
const T_FFN_GATE_EXPS: i64 = 21;
const T_FFN_UP_EXPS: i64 = 22;
const T_FFN_GATE_INP_SHEXP: i64 = 23;
const T_FFN_GATE_SHEXP: i64 = 24;
const T_FFN_UP_SHEXP: i64 = 25;
const T_FFN_DOWN_SHEXP: i64 = 26;
const T_NEXTN_EH_PROJ: i64 = 27;
const T_NEXTN_ENORM: i64 = 28;
const T_NEXTN_HNORM: i64 = 29;
const T_NEXTN_EMBED: i64 = 30;
const T_NEXTN_HEAD: i64 = 31;
const T_NEXTN_HEAD_NORM: i64 = 32;
const T_LEN: i64 = 33;

fn q_ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn q_put3(out: i64[:], a: i64, b: i64, c: i64) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
}

// 0 attention, 1 recurrent, 2 MTP. flags_base is the address of the native
// per-layer u32 array; 0 selects the interval formula the loader uses when
// LLM_KV_ATTENTION_RECURRENT_LAYERS is absent.
export fn q35_kind(p: i64[:], il: i64, flags_base: i64) -> i64 {
    q_ensure(p.len >= P_LEN, 10);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return if il < p[P_N_LAYER_ALL] { K_MTP } else { -1 };
    }
    if flags_base != 0 {
        let r = is_recr(il, p[P_N_LAYER_ALL], flags_base as! u64);
        if r == 1 { return K_RECR; }
        if r == 0 { return K_ATTN; }
        return -1;
    }
    let interval = p[P_INTERVAL];
    if interval <= 0 {
        return K_ATTN;
    }
    return if (il + 1) % interval != 0 { K_RECR } else { K_ATTN };
}

export fn q35_plan(p: i64[:], flags_base: i64, kinds: i64[:]) -> i64 {
    q_ensure(p.len >= P_LEN, 11);
    q_ensure(kinds.len >= p[P_N_LAYER_ALL], 12);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        kinds[i] = q35_kind(p, i, flags_base);
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// Shape of one tensor for a layer of the given kind. Returns 1 when the tensor
// exists for that kind and fills out[0..2], 0 when the kind does not own it.
// The values are exactly the loader's declarations; n_ff_exp/n_ff_shexp follow
// the declared-else-derived rule at qwen35moe.cpp:57-58.
export fn q35_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    q_ensure(p.len >= P_LEN, 20);
    q_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);
    let n_ff_shexp_v = n_ff_shexp(p[P_N_FF_SHEXP], n_ff);

    let d_state = p[P_D_STATE];
    let d_inner = p[P_D_INNER];
    let dt_rank = p[P_DT_RANK];
    let n_group = p[P_N_GROUP];
    let head_v = head_v_dim(d_inner, dt_rank);
    let key_d = key_dim(d_state, n_group);
    let value_d = value_dim(head_v, dt_rank);
    let conv_d = conv_dim(key_d, value_d);
    let qkv_w = wqkv_width(key_d, value_d);

    let head_k = p[P_HEAD_K];
    let n_head = p[P_N_HEAD];
    let n_head_kv = p[P_N_HEAD_KV];
    let n_embd_k_gqa = head_k * n_head_kv;
    let n_embd_v_gqa = head_v * n_head_kv;
    let q_width = q_full_width(head_k) * n_head;
    let qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa;
    let wo_in = head_k * n_head;

    if tid == T_TOKEN_EMBD {
        q_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // output is optional; when absent the loader duplicates token_embd and
        // the declared shape is the same either way.
        q_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM || tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM {
        q_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind == K_ATTN || kind == K_MTP {
        if tid == T_ATTN_QKV { q_put3(out, n_embd, qkv_fused, 1); return 1; }
        if tid == T_ATTN_OUT { q_put3(out, wo_in, n_embd, 1); return 1; }
        if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM { q_put3(out, head_k, 1, 1); return 1; }
    }

    if kind == K_RECR {
        if tid == T_WQKV { q_put3(out, n_embd, qkv_w, 1); return 1; }
        if tid == T_WQKV_GATE { q_put3(out, n_embd, value_d, 1); return 1; }
        if tid == T_SSM_CONV1D { q_put3(out, p[P_D_CONV], conv_d, 1); return 1; }
        if tid == T_SSM_DT || tid == T_SSM_A { q_put3(out, dt_rank, 1, 1); return 1; }
        if tid == T_SSM_BETA || tid == T_SSM_ALPHA { q_put3(out, n_embd, dt_rank, 1); return 1; }
        if tid == T_SSM_NORM { q_put3(out, head_v, 1, 1); return 1; }
        if tid == T_SSM_OUT { q_put3(out, value_d, n_embd, 1); return 1; }
    }

    if kind == K_ATTN || kind == K_RECR || kind == K_MTP {
        if tid == T_FFN_GATE_INP { q_put3(out, n_embd, n_expert, 1); return 1; }
        if tid == T_FFN_DOWN_EXPS { q_put3(out, n_ff_exp_v, n_embd, n_expert); return 1; }
        if tid == T_FFN_GATE_UP_EXPS { q_put3(out, n_embd, n_ff_exp_v * 2, n_expert); return 1; }
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS { q_put3(out, n_embd, n_ff_exp_v, n_expert); return 1; }
        if tid == T_FFN_GATE_INP_SHEXP { q_put3(out, n_embd, 1, 1); return 1; }
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP { q_put3(out, n_embd, n_ff_shexp_v, 1); return 1; }
        if tid == T_FFN_DOWN_SHEXP { q_put3(out, n_ff_shexp_v, n_embd, 1); return 1; }
    }

    if kind == K_MTP {
        if tid == T_NEXTN_EH_PROJ { q_put3(out, n_embd * 2, n_embd, 1); return 1; }
        if tid == T_NEXTN_ENORM || tid == T_NEXTN_HNORM || tid == T_NEXTN_HEAD_NORM { q_put3(out, n_embd, 1, 1); return 1; }
        if tid == T_NEXTN_EMBED || tid == T_NEXTN_HEAD { q_put3(out, n_embd, n_vocab, 1); return 1; }
    }

    return 0;
}
