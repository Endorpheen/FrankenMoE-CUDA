// P6-05 architecture 18: gemma4 builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose (goose/model/base/model_graph.goose minus `fn main`):
//   key_dim, value_dim, kq_scale_auto.
//
// Native anchors (src/models/gemma4.cpp):
//   hparams: swa_type, swa pattern, shared-KV layer count, f_attention_scale
//     fixed at 1.0, n_ff_exp, n_swa, n_embd_per_layer, head sizes per SWA
//     kind, final logit softcapping                         :3-29
//   global tensors, optional output, PLE globals            :44-58
//   per-layer attention, q/k norm, post norm, out_scale     :62-88
//   per-layer dense FFN, shared-expert FFN                  :90-98
//   MoE router and expert tensors                           :100-123
//   per-layer PLE tensors                                   :125-130
//   graph: attention, then dense or MoE+sherxp FFN          :175-375
//
// Gemma 4 is one attention shape with two orthogonal per-layer axes:
//   - sliding-window vs full attention. The layer head width is not constant:
//     n_embd_head_k(i) = n_embd_head_k_swa on SWA layers and n_embd_head_k
//     on full layers (llama-hparams.cpp:115-123), so both the query and the
//     GQA key/value widths change per layer.
//   - dense FFN vs MoE. The router tensor is optional per layer, so the loader
//     decides the kind from tensor presence; the plan takes that flag array.
// A layer without a router still keeps the full dense FFN, and on a MoE layer
// that same dense FFN becomes the shared expert (native comment at :93). The
// two FFN branches are therefore not exclusive in the file layout: the dense
// tensors exist on every layer, the expert tensors only where the router is.
//
// Two more details the shape table keeps:
//   - output is TENSOR_NOT_REQUIRED and is duplicated from token_embd when
//     absent (:44-48);
//   - rope_freqs exists only on full-attention layers: the first one is
//     required, later ones are duplicated (:84-88).
//
// Per-layer embeddings (PLE) appear when n_embd_per_layer > 0: one lazy table
// [n_embd_per_layer * n_layer, n_vocab], one projection [n_embd,
// n_embd_per_layer * n_layer], one norm [n_embd_per_layer], and per layer an
// input gate, a projection and a post norm (:52-56, :126-130).

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

// Layer kinds: two independent axes packed as attention kind + 2 * moe.
const K_ATTN_FULL: i64 = 0;
const K_ATTN_SWA: i64 = 1;
const K_ATTN_MOE_FULL: i64 = 2;
const K_ATTN_MOE_SWA: i64 = 3;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT: i64 = 1;
const T_OUTPUT_NORM: i64 = 2;
const T_PER_LAYER_TOKEN_EMBD: i64 = 3;
const T_PER_LAYER_MODEL_PROJ: i64 = 4;
const T_PER_LAYER_PROJ_NORM: i64 = 5;
const T_ATTN_NORM: i64 = 6;
const T_ATTN_Q: i64 = 7;
const T_ATTN_K: i64 = 8;
const T_ATTN_V: i64 = 9;
const T_ATTN_WO: i64 = 10;
const T_ATTN_Q_NORM: i64 = 11;
const T_ATTN_K_NORM: i64 = 12;
const T_ATTN_POST_NORM: i64 = 13;
const T_LAYER_OUT_SCALE: i64 = 14;
const T_ROPE_FREQS: i64 = 15;
const T_FFN_NORM: i64 = 16;
const T_FFN_GATE: i64 = 17;
const T_FFN_UP: i64 = 18;
const T_FFN_DOWN: i64 = 19;
const T_FFN_POST_NORM: i64 = 20;
const T_FFN_GATE_INP: i64 = 21;
const T_FFN_GATE_INP_S: i64 = 22;
const T_FFN_PRE_NORM_2: i64 = 23;
const T_FFN_POST_NORM_1: i64 = 24;
const T_FFN_POST_NORM_2: i64 = 25;
const T_FFN_GATE_UP_EXPS: i64 = 26;
const T_FFN_GATE_EXPS: i64 = 27;
const T_FFN_UP_EXPS: i64 = 28;
const T_FFN_DOWN_EXPS: i64 = 29;
const T_PER_LAYER_INP_GATE: i64 = 30;
const T_PER_LAYER_PROJ: i64 = 31;
const T_PER_LAYER_POST_NORM: i64 = 32;
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

// is_swa, llama-hparams.cpp:264. `is_swa_impl` is one 4-byte element per layer,
// the same layout as is_recr_impl, so the shared reader shape applies. Returns
// -1 past n_layer_all, where the native code aborts.
export fn g4_is_swa(p: i64[:], il: i64, swa_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    let v = ld_u32(swa_base as! u64, il);
    if v != 0 {
        return 1;
    }
    return 0;
}

// has_expert on layer il, gemma4.cpp:101-102. The native test is tensor
// presence (`ffn_gate_inp != nullptr`); the loader reports that presence as a
// per-layer flag, which is what this reads. -1 past n_layer.
export fn g4_has_expert(p: i64[:], il: i64, moe_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return -1;
    }
    let v = ld_u32(moe_base as! u64, il);
    if v != 0 {
        return 1;
    }
    return 0;
}

// has_kv, gemma4.cpp:7-10 and llama-hparams.cpp:295-303.
//   n_layer_kv_from_start = n_layer_all - n_kv_shared_layers;
//   has_kv(il) = il < n_layer_kv_from_start
// The trailing shared layers read the KV of earlier layers and own only a
// query projection.
export fn g4_has_kv(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    let n_from_start = p[P_N_LAYER_ALL] - p[P_N_KV_SHARED];
    e_ensure(n_from_start >= 0, 13);
    if il < n_from_start {
        return 1;
    }
    return 0;
}

// Per-layer head width, llama-hparams.cpp:115-123.
//   n_embd_head_k(il) = is_swa(il) ? n_embd_head_k_swa : n_embd_head_k
// Gemma 4 requires k and v to agree (:37-42), so one value serves both.
export fn g4_head_dim(p: i64[:], il: i64, swa_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if g4_is_swa(p, il, swa_base) == 1 {
        return p[P_HEAD_K_SWA];
    }
    return p[P_HEAD_K];
}

// rope_freqs presence, gemma4.cpp:84-88 and :60-61.
//   0 -> no tensor on a SWA layer
//   1 -> first full-attention layer, tensor required
//   2 -> later full-attention layers, TENSOR_DUPLICATED
export fn g4_rope_freqs_need(p: i64[:], il: i64, swa_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if g4_is_swa(p, il, swa_base) == 1 {
        return 0;
    }
    var prev_full: i64 = 0;
    var j: i64 = 0;
    while j < il {
        if g4_is_swa(p, j, swa_base) == 0 {
            prev_full = 1;
        }
        j = j + 1;
    }
    if prev_full != 0 {
        return 2;
    }
    return 1;
}

// Per-layer kind, from the two axes above.
export fn g4_layer_kind(p: i64[:], il: i64, swa_base: i64, moe_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if il < 0 || il >= p[P_N_LAYER] {
        return -1;
    }
    let base: i64 = if g4_is_swa(p, il, swa_base) == 1 { K_ATTN_SWA } else { K_ATTN_FULL };
    if g4_has_expert(p, il, moe_base) == 1 {
        return base + 2;
    }
    return base;
}

export fn g4_plan(p: i64[:], swa_base: i64, moe_base: i64, kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    e_ensure(kinds.len >= p[P_N_LAYER], 18);
    var i: i64 = 0;
    while i < p[P_N_LAYER] {
        kinds[i] = g4_layer_kind(p, i, swa_base, moe_base);
        i = i + 1;
    }
    return p[P_N_LAYER];
}

// Per-layer embeddings exist when the file carries n_embd_per_layer (:52-56).
export fn g4_ple_active(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 19);
    if p[P_N_EMBD_PER_LAYER] > 0 {
        return 1;
    }
    return 0;
}

// output is TENSOR_NOT_REQUIRED and is duplicated from token_embd (:44-48).
export fn g4_output_optional() -> i64 {
    return 1;
}

// f_attention_scale is forced to 1.0 (:11), so the caller never computes
// 1/sqrt(n_embd_head); kq_scale_auto with the 1.0f pattern reports that.
export fn g4_kq_scale_auto() -> i64 {
    return kq_scale_auto(1065353216);
}

// Shape of one tensor. Returns 1 and fills out[0..2], 0 when the architecture
// does not own the tensor for that layer. Global tensors ignore il; per-layer
// tensors need a valid il, and the expert/PLE groups need their flag on.
export fn g4_shape(p: i64[:], il: i64, tid: i64, swa_base: i64, moe_base: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let n_ppl = p[P_N_EMBD_PER_LAYER];
    // gemma4.cpp:35 -- the expert width is the declared hparams.n_ff_exp, with
    // no n_ff / n_expert_used fallback, unlike the qwen/deepseek MoE builders.
    let n_ff_exp_v = p[P_N_FF_EXP];

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
    if tid == T_PER_LAYER_TOKEN_EMBD {
        if n_ppl <= 0 {
            return 0;
        }
        e_put3(out, n_ppl * p[P_N_LAYER], n_vocab, 1);
        return 1;
    }
    if tid == T_PER_LAYER_MODEL_PROJ {
        if n_ppl <= 0 {
            return 0;
        }
        e_put3(out, n_embd, n_ppl * p[P_N_LAYER], 1);
        return 1;
    }
    if tid == T_PER_LAYER_PROJ_NORM {
        if n_ppl <= 0 {
            return 0;
        }
        e_put3(out, n_ppl, 1, 1);
        return 1;
    }

    if il < 0 || il >= p[P_N_LAYER] {
        return 0;
    }

    let n_head = p[P_N_HEAD];
    let n_head_kv = p[P_N_HEAD_KV];
    let head = g4_head_dim(p, il, swa_base);
    let n_embd_k_gqa = key_dim(head, n_head_kv);
    let n_embd_v_gqa = value_dim(head, n_head_kv);
    let q_width = head * n_head;

    if tid == T_ATTN_NORM || tid == T_FFN_NORM || tid == T_ATTN_POST_NORM || tid == T_FFN_POST_NORM {
        e_put3(out, n_embd, 1, 1);
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
    if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM {
        e_put3(out, head, 1, 1);
        return 1;
    }
    if tid == T_LAYER_OUT_SCALE {
        e_put3(out, 1, 1, 1);
        return 1;
    }
    if tid == T_ROPE_FREQS {
        if g4_is_swa(p, il, swa_base) != 0 {
            return 0;
        }
        e_put3(out, head / 2, 1, 1);
        return 1;
    }

    // Dense FFN on every layer (:94-98). On a MoE layer this is the shared
    // expert, so the tensors keep their dense names and widths.
    if tid == T_FFN_GATE || tid == T_FFN_UP {
        e_put3(out, n_embd, n_ff, 1);
        return 1;
    }
    if tid == T_FFN_DOWN {
        e_put3(out, n_ff, n_embd, 1);
        return 1;
    }

    // MoE group, only where the router exists (:105-123).
    if g4_has_expert(p, il, moe_base) == 1 {
        if tid == T_FFN_GATE_INP {
            e_put3(out, n_embd, n_expert, 1);
            return 1;
        }
        if tid == T_FFN_GATE_INP_S || tid == T_FFN_PRE_NORM_2 || tid == T_FFN_POST_NORM_1 || tid == T_FFN_POST_NORM_2 {
            e_put3(out, n_embd, 1, 1);
            return 1;
        }
        if tid == T_FFN_GATE_UP_EXPS {
            e_put3(out, n_embd, n_ff_exp_v * 2, n_expert);
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
    }

    // PLE per layer (:126-130).
    if n_ppl > 0 {
        if tid == T_PER_LAYER_INP_GATE {
            e_put3(out, n_embd, n_ppl, 1);
            return 1;
        }
        if tid == T_PER_LAYER_PROJ {
            e_put3(out, n_ppl, n_embd, 1);
            return 1;
        }
        if tid == T_PER_LAYER_POST_NORM {
            e_put3(out, n_embd, 1, 1);
            return 1;
        }
    }
    return 0;
}

// Convenience for the gate: single-value answers that are decisions, not
// shapes. Kept exported so the fixture does not have to know the layout.
export fn g4_softcap_active(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 22);
    if p[P_SOFT_CAP] != 0 {
        return 1;
    }
    return 0;
}
