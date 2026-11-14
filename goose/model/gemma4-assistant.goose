// P6-05 architecture 19: gemma4-assistant builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted slice in
// base/model_graph.goose:
//   key_dim, value_dim, kq_scale_auto.
//
// Native anchors (src/models/gemma4-assistant.cpp):
//   hparams: n_embd_inp = target hidden size, SWA pattern, f_attention_scale
//     fixed at 1.0, n_layer_nextn must equal n_layer_all        :3-22
//   global tensors, duplicated output, optional masked-embd
//     metadata, nextn_proj_post                                :37-46
//   nextn_proj_pre on layer 0 only                             :57-59
//   per-layer attention, post norm, out_scale, rope_freqs      :61-73
//   per-layer dense FFN                                        :75-79
//   graph: concat(target embed, h) -> proj_pre -> N nextn layers
//     with the target KV cache, then proj_post                 :87-203
//
// The assistant is the drafter of the Gemma 4 pair, so its own hidden state is
// n_embd while everything it consumes from the target is n_embd_backbone:
//   - nextn_proj_pre is [2 * n_embd_backbone, n_embd], because the graph
//     concatenates the target token embedding with the target hidden state
//     along dim 0 before the projection (:118-121);
//   - nextn_proj_post is [n_embd, n_embd_backbone], the reverse direction for
//     the next draft step (:197);
//   - the loader throws when n_embd_backbone == n_embd, so a valid file always
//     has two distinct widths (:33-35).
//
// Three further differences from gemma4 that the shape table keeps:
//   - there are no wq/wk/wv key/value projections at all: attention reuses the
//     target KV cache, with Kcur and Vcur passed as null (:153-154);
//   - output is created under the token_embd name with TENSOR_DUPLICATED, so it
//     is never a separate tensor (:38);
//   - out_scale is a required per-layer tensor here, not optional (:68).

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

// One layer kind: every effective layer of the assistant is an MTP/nextn
// block (:50, :130). Kept as a named constant so the plan reads like the
// other families'.
const K_NEXTN: i64 = 0;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT: i64 = 1;
const T_OUTPUT_NORM: i64 = 2;
const T_MASKED_EMBD_CENTROIDS: i64 = 3;
const T_MASKED_EMBD_ORDERING: i64 = 4;
const T_NEXTN_PROJ_PRE: i64 = 5;
const T_NEXTN_PROJ_POST: i64 = 6;
const T_ATTN_NORM: i64 = 7;
const T_ATTN_Q: i64 = 8;
const T_ATTN_WO: i64 = 9;
const T_ATTN_Q_NORM: i64 = 10;
const T_ATTN_POST_NORM: i64 = 11;
const T_LAYER_OUT_SCALE: i64 = 12;
const T_ROPE_FREQS: i64 = 13;
const T_FFN_NORM: i64 = 14;
const T_FFN_GATE: i64 = 15;
const T_FFN_UP: i64 = 16;
const T_FFN_DOWN: i64 = 17;
const T_FFN_POST_NORM: i64 = 18;
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

// is_swa over the draft's own pattern array (llama-hparams.cpp:264). Same
// 4-byte element layout as the base module's is_recr reader.
export fn g4a_is_swa(p: i64[:], il: i64, swa_base: i64) -> i64 {
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

// Per-layer head width, llama-hparams.cpp:115-123.
export fn g4a_head_dim(p: i64[:], il: i64, swa_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if g4a_is_swa(p, il, swa_base) == 1 {
        return p[P_HEAD_K_SWA];
    }
    return p[P_HEAD_K];
}

// Layer kind and plan. The native loop bound is n_layer_nextn (:50, :130);
// there is no dense or MoE block in this architecture.
export fn g4a_layer_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    if il < 0 || il >= p[P_N_LAYER_NEXTN] {
        return -1;
    }
    return K_NEXTN;
}

export fn g4a_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    e_ensure(kinds.len >= p[P_N_LAYER_NEXTN], 14);
    var i: i64 = 0;
    while i < p[P_N_LAYER_NEXTN] {
        kinds[i] = K_NEXTN;
        i = i + 1;
    }
    return p[P_N_LAYER_NEXTN];
}

// The assertion at :15 -- n_layer_nextn must equal n_layer_all. 1 when the
// file satisfies it, 0 when the native loader would abort.
export fn g4a_nextn_matches_all(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    if p[P_N_LAYER_NEXTN] == p[P_N_LAYER_ALL] {
        return 1;
    }
    return 0;
}

// The two widths must differ or the loader throws (:33-35).
export fn g4a_hidden_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if p[P_N_EMBD_INP] != p[P_N_EMBD] {
        return 1;
    }
    return 0;
}

// No layer owns a KV projection: attention reads the target cache (:153-154).
export fn g4a_has_kv() -> i64 {
    return 0;
}

// out_scale is required on every draft layer (:68).
export fn g4a_out_scale_required() -> i64 {
    return 1;
}

// f_attention_scale is forced to 1.0 (:12).
export fn g4a_kq_scale_auto() -> i64 {
    return kq_scale_auto(1065353216);
}

// rope_freqs presence, :70-73 with the same duplicated-first rule as gemma4:
//   0 -> none on a SWA layer, 1 -> first full layer, 2 -> later full layers.
export fn g4a_rope_freqs_need(p: i64[:], il: i64, swa_base: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if g4a_is_swa(p, il, swa_base) == 1 {
        return 0;
    }
    var prev_full: i64 = 0;
    var j: i64 = 0;
    while j < il {
        if g4a_is_swa(p, j, swa_base) == 0 {
            prev_full = 1;
        }
        j = j + 1;
    }
    if prev_full != 0 {
        return 2;
    }
    return 1;
}

// Shape of one tensor. Global tensors ignore il; per-layer tensors need a
// valid il. `n_embd_backbone` is the target hidden width (:45).
export fn g4a_shape(p: i64[:], il: i64, tid: i64, swa_base: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    e_ensure(out.len >= 3, 21);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_ff = p[P_N_FF];
    let n_backbone = p[P_N_EMBD_INP];

    if tid == T_TOKEN_EMBD {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT {
        // duplicated from token_embd, same declared shape (:38)
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_MASKED_EMBD_CENTROIDS || tid == T_MASKED_EMBD_ORDERING {
        // native declares these with an empty shape ({}), a 0-dim tensor;
        // the gate's three-slot contract reports it as {1,1,1}
        e_put3(out, 1, 1, 1);
        return 1;
    }
    if tid == T_NEXTN_PROJ_POST {
        e_put3(out, n_embd, n_backbone, 1);
        return 1;
    }
    if tid == T_NEXTN_PROJ_PRE {
        // layer 0 only (:57-59)
        if il != 0 {
            return 0;
        }
        e_put3(out, 2 * n_backbone, n_embd, 1);
        return 1;
    }

    if il < 0 || il >= p[P_N_LAYER_NEXTN] {
        return 0;
    }

    let n_head = p[P_N_HEAD];
    let head = g4a_head_dim(p, il, swa_base);
    let q_width = head * n_head;

    if tid == T_ATTN_NORM || tid == T_ATTN_POST_NORM || tid == T_FFN_NORM || tid == T_FFN_POST_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_Q {
        e_put3(out, n_embd, q_width, 1);
        return 1;
    }
    if tid == T_ATTN_WO {
        e_put3(out, q_width, n_embd, 1);
        return 1;
    }
    if tid == T_ATTN_Q_NORM {
        e_put3(out, head, 1, 1);
        return 1;
    }
    if tid == T_LAYER_OUT_SCALE {
        e_put3(out, 1, 1, 1);
        return 1;
    }
    if tid == T_ROPE_FREQS {
        if g4a_is_swa(p, il, swa_base) != 0 {
            return 0;
        }
        e_put3(out, head / 2, 1, 1);
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
