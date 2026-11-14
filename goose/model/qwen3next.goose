// P6-05 architecture 5: qwen3next builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/goose-model-graph.goose minus `fn main`):
//   is_recr, n_ff_exp, n_ff_shexp, key_dim, value_dim, conv_dim, wqkv_width,
//   head_v_dim, conv_channels, qkv_dim, q_full_width.
//
// Native anchors (src/models/qwen3next.cpp):
//   hparams, expert ffn lengths, GDN sizes         :4-29
//   global tensors, optional output               :42-51
//   GDN loader geometry (key/value/conv/qkvz)     :53-66
//   per-layer trunk block, attn vs GDN            :68-106
//   per-layer MTP (nextn) block                   :108-121
//   trunk loop over n_layer, MTP loop to n_layer_all :123-128
//   attention graph: joint QG split, q/k norm, gate :234-309
//   GDN graph: qkvz, conv, l2 norm, repeat, output :311-554
//   MoE + gated shared expert, dense fallback      :556-616
//   MTP head (h_norm, e_norm, eh_proj, attn, MoE)  :618-812
//
// qwen3next is the hybrid recurrent/attention MoE member of the family. The
// shape table must keep these six decisions:
//   - per-layer kind: GDN on every layer that is not a full-attention layer,
//     full attention when (il + 1) % full_attention_interval == 0, and MTP for
//     il in [n_layer, n_layer_all),
//   - attention layers carry a joint query+gate projection, GQA key/value
//     widths, and RMS q/k norm of width n_embd_head_k,
//   - attention layers carry attn_post_norm (used before the FFN residual),
//   - GDN layers carry the legacy ssm_in (qkvz), wqkv, wqkv_gate, conv1d,
//     beta/alpha, ssm_norm and ssm_out, with loader widths taken from
//     ssm_d_state (head_v_dim at load time, not d_inner / n_v_heads),
//   - every layer, MTP included, carries routed experts plus one gated shared
//     expert whose gate is a single vector,
//   - the MTP block doubles the hidden state in nextn.eh_proj and may override
//     the LM head with nextn.shared_head_head / shared_head_norm.

import std;
import base.model_graph;

// p indices -- the shared fixture contract of this experiment, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_HEAD: i64 = 4;
const P_N_HEAD_KV: i64 = 5;
const P_HEAD_K: i64 = 6;
const P_N_FF: i64 = 7;
const P_N_EXPERT: i64 = 8;
const P_N_EXPERT_USED: i64 = 9;
const P_N_FF_EXP: i64 = 10;
const P_N_FF_SHEXP: i64 = 11;
const P_D_CONV: i64 = 12;
const P_D_INNER: i64 = 13;
const P_D_STATE: i64 = 14;
const P_DT_RANK: i64 = 15;
const P_N_GROUP: i64 = 16;
const P_INTERVAL: i64 = 17;
const P_HAS_OUTPUT: i64 = 18;
const P_GATING: i64 = 19;
const P_LEN: i64 = 20;

const K_ATTN: i64 = 0;
const K_RECR: i64 = 1;
const K_MTP: i64 = 2;

// tensor ids -- one table for all three layer kinds
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_POST_NORM: i64 = 4;
const T_ATTN_QKV: i64 = 5;
const T_ATTN_OUT: i64 = 6;
const T_ATTN_Q_NORM: i64 = 7;
const T_ATTN_K_NORM: i64 = 8;
const T_SSM_IN: i64 = 9;
const T_WQKV: i64 = 10;
const T_WQKV_GATE: i64 = 11;
const T_SSM_CONV1D: i64 = 12;
const T_SSM_DT: i64 = 13;
const T_SSM_A: i64 = 14;
const T_SSM_BETA_ALPHA: i64 = 15;
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
const T_NEXTN_EMBED_TOKENS: i64 = 30;
const T_NEXTN_SHARED_HEAD_HEAD: i64 = 31;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 32;
const T_LEN: i64 = 33;

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

// Per-layer kind. qwen3next.cpp:17-23 fills is_recr_impl with
//   (i < n_layer) && ((i + 1) % full_attn_interval != 0)
// unless the file carries LLM_KV_ATTENTION_RECURRENT_LAYERS, in which case the
// per-layer array wins (qwen3next.cpp:163 uses hparams.is_recr(il)).
// Layers in [n_layer, n_layer_all) are the MTP block: the loader runs the trunk
// body for them (load_block_trunk, qwen3next.cpp:110) and the graph is the
// attention-form MTP head (graph_mtp, qwen3next.cpp:618).
export fn q3n_kind(p: i64[:], il: i64, flags_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return K_MTP;
    }
    if flags_base != 0 {
        let r = is_recr(il, p[P_N_LAYER_ALL], flags_base as! u64);
        if r == 1 { return K_RECR; }
        if r == 0 { return K_ATTN; }
        return -1;
    }
    let interval = p[P_INTERVAL];
    // The native modulo would divide by zero; the caller treats a missing or
    // zero interval as "no full-attention layer" instead of aborting.
    if interval <= 0 {
        return K_ATTN;
    }
    return if (il + 1) % interval != 0 { K_RECR } else { K_ATTN };
}

export fn q3n_plan(p: i64[:], flags_base: i64, kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 12);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        kinds[i] = q3n_kind(p, i, flags_base);
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// Policy decisions of the loader, kept as named exports so the gate can pin
// them without duplicating native line numbers in the fixture.
//   output is TENSOR_NOT_REQUIRED and falls back to token_embd (qwen3next.cpp:46-51)
export fn q3n_output_optional() -> i64 {
    return 1;
}
//   q/k RMS norm is created on every attention layer (qwen3next.cpp:80-81)
export fn q3n_qk_norm_required() -> i64 {
    return 1;
}
//   attn_post_norm exists on every layer, MTP included (qwen3next.cpp:73, :749)
export fn q3n_has_post_norm() -> i64 {
    return 1;
}
//   MTP block count: the loader loop at qwen3next.cpp:126-128
export fn q3n_mtp_blocks(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return p[P_N_LAYER_ALL] - p[P_N_LAYER];
}
//   routed MoE gating is LLAMA_EXPERT_GATING_FUNC_TYPE_SOFTMAX (qwen3next.cpp:570)
export fn q3n_gating_kind(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    return gating_kind(p[P_GATING]);
}

// GDN geometry as the graph builds it, not as the loader declares it.
// The loader (qwen3next.cpp:56-62) uses head_v_dim = ssm_d_state and therefore
// value_dim = ssm_d_state * ssm_dt_rank. The graph (qwen3next.cpp:398-404)
// recomputes head_v_dim = ssm_d_inner / ssm_dt_rank and derives the conv view
// channels from ssm_d_inner. out[0..2] = head_v_dim, conv_channels, qkv_dim.
export fn q3n_gdn_graph_dims(p: i64[:], out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    e_ensure(out.len >= 3, 16);
    let hv = head_v_dim(p[P_D_INNER], p[P_DT_RANK]);
    let cc = conv_channels(p[P_D_INNER], p[P_N_GROUP], p[P_D_STATE]);
    let qd = qkv_dim(p[P_D_STATE], p[P_N_GROUP], hv, p[P_DT_RANK]);
    e_put3(out, hv, cc, qd);
    return 1;
}

// Shape of one tensor for a layer kind. Returns 1 and fills out[0..2] when the
// kind owns it, 0 otherwise. Three-dimensional slices: a 1-D vector is reported
// as {n, 1, 1} and a 2-D matrix as {rows, cols, 1}.
export fn q3n_shape(p: i64[:], kind: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let head_k = p[P_HEAD_K];
    let n_head = p[P_N_HEAD];
    let n_head_kv = p[P_N_HEAD_KV];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];

    // qwen3next.cpp:53, `n_ff_exp() ? n_ff_exp() : n_ff / n_expert_used`
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, used);
    // qwen3next.cpp:70, `n_ff_shexp > 0 ? n_ff_shexp : n_ff(il)`
    let n_ff_shexp_v = n_ff_shexp(p[P_N_FF_SHEXP], n_ff);

    // attention widths: joint Q+gate projection, GQA key/value (qwen3next.cpp:77-78)
    let n_embd_k_gqa = key_dim(head_k, n_head_kv);
    let n_embd_v_gqa = value_dim(head_k, n_head_kv);
    let q_full = q_full_width(head_k) * n_head;
    let qkv_fused = q_full + n_embd_k_gqa + n_embd_v_gqa;

    // GDN loader geometry, qwen3next.cpp:56-66
    let gdn_key = key_dim(p[P_D_STATE], p[P_N_GROUP]);
    let gdn_value = value_dim(p[P_D_STATE], p[P_DT_RANK]);
    let gdn_conv = conv_dim(gdn_key, gdn_value);
    let gdn_qkvz = gdn_key * 2 + gdn_value * 2;
    let gdn_ba = p[P_DT_RANK] * 2;

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // optional; absent means duplicate token_embd, same shape (qwen3next.cpp:46-51)
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind != K_ATTN && kind != K_RECR && kind != K_MTP {
        return 0;
    }

    // every layer, MTP included (qwen3next.cpp:72-73)
    if tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind == K_ATTN || kind == K_MTP {
        if tid == T_ATTN_QKV { e_put3(out, n_embd, qkv_fused, 1); return 1; }
        if tid == T_ATTN_OUT { e_put3(out, head_k * n_head, n_embd, 1); return 1; }
        if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
            e_put3(out, head_k, 1, 1);
            return 1;
        }
    }

    if kind == K_RECR {
        if tid == T_SSM_IN { e_put3(out, n_embd, gdn_qkvz, 1); return 1; }
        if tid == T_WQKV { e_put3(out, n_embd, wqkv_width(gdn_key, gdn_value), 1); return 1; }
        if tid == T_WQKV_GATE { e_put3(out, n_embd, gdn_value, 1); return 1; }
        if tid == T_SSM_CONV1D { e_put3(out, p[P_D_CONV], gdn_conv, 1); return 1; }
        if tid == T_SSM_DT || tid == T_SSM_A { e_put3(out, p[P_DT_RANK], 1, 1); return 1; }
        if tid == T_SSM_BETA_ALPHA { e_put3(out, n_embd, gdn_ba, 1); return 1; }
        if tid == T_SSM_NORM { e_put3(out, p[P_D_STATE], 1, 1); return 1; }
        if tid == T_SSM_OUT { e_put3(out, gdn_value, n_embd, 1); return 1; }
    }

    // routed experts + gated shared expert on every kind (qwen3next.cpp:97-105)
    if tid == T_FFN_GATE_INP { e_put3(out, n_embd, n_expert, 1); return 1; }
    if tid == T_FFN_DOWN_EXPS { e_put3(out, n_ff_exp_v, n_embd, n_expert); return 1; }
    if tid == T_FFN_GATE_UP_EXPS { e_put3(out, n_embd, n_ff_exp_v * 2, n_expert); return 1; }
    if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS { e_put3(out, n_embd, n_ff_exp_v, n_expert); return 1; }
    if tid == T_FFN_GATE_INP_SHEXP { e_put3(out, n_embd, 1, 1); return 1; }
    if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP { e_put3(out, n_embd, n_ff_shexp_v, 1); return 1; }
    if tid == T_FFN_DOWN_SHEXP { e_put3(out, n_ff_shexp_v, n_embd, 1); return 1; }

    if kind == K_MTP {
        if tid == T_NEXTN_EH_PROJ { e_put3(out, n_embd * 2, n_embd, 1); return 1; }
        if tid == T_NEXTN_ENORM || tid == T_NEXTN_HNORM { e_put3(out, n_embd, 1, 1); return 1; }
        if tid == T_NEXTN_EMBED_TOKENS { e_put3(out, n_embd, n_vocab, 1); return 1; }
        if tid == T_NEXTN_SHARED_HEAD_HEAD { e_put3(out, n_embd, n_vocab, 1); return 1; }
        if tid == T_NEXTN_SHARED_HEAD_NORM { e_put3(out, n_embd, 1, 1); return 1; }
    }

    return 0;
}
