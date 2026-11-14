// P6-05 architecture 12: glm5-next (GLM-5.3-Flash) builder preparation.
//
// Decision/shape half, not the graph emitter. It reuses the accepted shared
// slice in base/model_graph.goose (goose/model/base/model_graph.goose minus
// its `fn main`).
//
// Native anchors (src/models/glm5-next.cpp):
//   hparams: MLA/KDA keys, is_recr, dense lead, indexer, mHC   :7-57
//   global tensors                                             :71-74
//   per-layer common + HC modules                              :82-90
//   KDA (gated delta net) tensors                              :98-119
//   nope MLA + DSA indexer tensors                             :120-151
//   dense-lead FFN                                             :155-159
//   routed + shared experts                                    :160-173
//   MTP nextn tensors                                          :175-183
//   hyper-connection pre/post (mHC, Sinkhorn)                  :441-520
//   MTP graph (doubled input, flat output)                     :548-672
//   trunk loop (kind, HC, dense/MoE, nextn crop)               :677-790
//   KDA graph (decay gate, conv, output gate)                  :811-889
//   k-pool indexer select (top-k, tail, mask)                  :891-1032
//   DSA graph (nope MLA, absorbed wk_b, sparse attention)      :1034-1093
//
// glm5-next differs from deepseek4 and from the plain MoE families in the
// ways the plan and shape table must keep:
//   - attention is a hybrid: a layer whose KV head count is zero is a KDA
//     (gated delta net) layer, every other trunk layer is nope-MLA with a
//     k-pool DSA indexer (glm5-next.cpp:22),
//   - MLA is nope-only: the graph asserts n_rot == 0 (:1046), so the no-rope
//     head width equals the full K head width,
//   - the indexer pools kpool consecutive keys; the selection width is
//     kpool * min(n_pool, top_k/kpool) plus an optional kpool-1 tail (:322,
//     :976-1000) and the sparse mask is n_kv + n_sel wide,
//   - hyper-connections (mHC) run on trunk layers only, twice per layer, and
//     the loader asserts hc == 4 (:53, :461),
//   - the FFN is dense on the leading n_layer_dense_lead layers and routed
//     experts plus shared experts afterwards; the MTP block is always MoE,
//   - the MTP hidden state is flat n_embd (the streams are averaged by
//     build_hc_mean, :777-784) so eh_proj reads 2*n_embd and the loader
//     asserts n_embd_out() == n_embd (:564),
//   - there are no attention sinks in this architecture.

import std;
import base.model_graph;

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_HEAD: i64 = 4;
const P_EMBD_HEAD_K: i64 = 5;
const P_EMBD_HEAD_V: i64 = 6;
const P_N_ROT: i64 = 7;
const P_Q_LORA: i64 = 8;
const P_KV_LORA: i64 = 9;
const P_KDA_HEAD: i64 = 10;
const P_D_CONV: i64 = 11;
const P_N_EXPERT: i64 = 12;
const P_N_EXPERT_USED: i64 = 13;
const P_N_SHEXP: i64 = 14;
const P_N_FF: i64 = 15;
const P_IDX_NHEAD: i64 = 16;
const P_IDX_HEAD: i64 = 17;
const P_IDX_TOP_K: i64 = 18;
const P_KPOOL: i64 = 19;
const P_KPOOL_TAIL: i64 = 20;
const P_HC: i64 = 21;
const P_DENSE_LEAD: i64 = 22;
const P_GATING: i64 = 23;
const P_NEXTN_DECLARED: i64 = 24;
const P_MTP_PROBE: i64 = 25;
const P_LEN: i64 = 26;

// attention kinds; MTP is a separate kind, not a trunk attention kind
const K_RECR: i64 = 0;
const K_MLA: i64 = 1;
const K_MTP: i64 = 2;

// FFN kinds
const F_DENSE: i64 = 0;
const F_MOE: i64 = 1;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_FFN_NORM: i64 = 4;
const T_HC_ATTN_FN: i64 = 5;
const T_HC_ATTN_BASE: i64 = 6;
const T_HC_ATTN_SCALE: i64 = 7;
const T_HC_FFN_FN: i64 = 8;
const T_HC_FFN_BASE: i64 = 9;
const T_HC_FFN_SCALE: i64 = 10;
const T_WQ: i64 = 11;
const T_WK: i64 = 12;
const T_WV: i64 = 13;
const T_SSM_Q_CONV: i64 = 14;
const T_SSM_K_CONV: i64 = 15;
const T_SSM_V_CONV: i64 = 16;
const T_SSM_F_A: i64 = 17;
const T_SSM_F_B: i64 = 18;
const T_SSM_BETA: i64 = 19;
const T_SSM_A: i64 = 20;
const T_SSM_DT_B: i64 = 21;
const T_SSM_G_A: i64 = 22;
const T_SSM_G_B: i64 = 23;
const T_SSM_NORM: i64 = 24;
const T_WO: i64 = 25;
const T_Q_A_NORM: i64 = 26;
const T_KV_A_NORM: i64 = 27;
const T_WQ_A: i64 = 28;
const T_WQ_B: i64 = 29;
const T_WKV_A_MQA: i64 = 30;
const T_WK_B: i64 = 31;
const T_WV_B: i64 = 32;
const T_ATTN_OUT: i64 = 33;
const T_INDEXER_K_NORM: i64 = 34;
const T_INDEXER_K_NORM_B: i64 = 35;
const T_INDEXER_PROJ: i64 = 36;
const T_INDEXER_ATTN_K: i64 = 37;
const T_INDEXER_ATTN_Q_B: i64 = 38;
const T_INDEXER_KPOOL_GATE: i64 = 39;
const T_INDEXER_KPOOL_APE: i64 = 40;
const T_FFN_GATE: i64 = 41;
const T_FFN_DOWN: i64 = 42;
const T_FFN_UP: i64 = 43;
const T_FFN_GATE_INP: i64 = 44;
const T_FFN_EXP_PROBS_B: i64 = 45;
const T_FFN_GATE_EXPS: i64 = 46;
const T_FFN_DOWN_EXPS: i64 = 47;
const T_FFN_UP_EXPS: i64 = 48;
const T_FFN_GATE_SHEXP: i64 = 49;
const T_FFN_DOWN_SHEXP: i64 = 50;
const T_FFN_UP_SHEXP: i64 = 51;
const T_NEXTN_EH_PROJ: i64 = 52;
const T_NEXTN_ENORM: i64 = 53;
const T_NEXTN_HNORM: i64 = 54;
const T_NEXTN_EMBED: i64 = 55;
const T_NEXTN_SHARED_HEAD: i64 = 56;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 57;
const T_LEN: i64 = 58;

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

// g5_attn_kind: glm5-next.cpp:22,
// `is_recr_impl[i] = hparams.n_head_kv(i) == 0`. A layer with zero KV heads is
// a KDA (gated delta net) layer; a positive KV head count selects the nope-MLA
// plus DSA branch. The KV head count is passed as an array because the native
// loader fills is_recr for every layer from the per-layer attention shape.
export fn g5_attn_kind(kv_heads: i64[:], il: i64) -> i64 {
    e_ensure(il >= 0 && il < kv_heads.len, 10);
    if kv_heads[il] == 0 {
        return K_RECR;
    }
    return K_MLA;
}

// g5_layer_kind: the plan decision for one layer. Trunk layers take their kind
// from the KV head count; layers at and past n_layer are the MTP block, which
// always runs the DSA branch with an MTP input (glm5-next.cpp:548-616),
// regardless of their declared KV head count.
export fn g5_layer_kind(p: i64[:], kv_heads: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return K_MTP;
    }
    e_ensure(kv_heads.len >= p[P_N_LAYER_ALL], 12);
    return g5_attn_kind(kv_heads, il);
}

// g5_plan: fills one kind per layer. Mirrors the loader's is_recr array
// (:20-23) and marks the MTP block explicitly so the shape table cannot hand a
// KDA layer the DSA family or the other way round.
export fn g5_plan(p: i64[:], kv_heads: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    e_ensure(kv_heads.len >= p[P_N_LAYER_ALL], 14);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 15);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        let k = g5_layer_kind(p, kv_heads, i);
        e_ensure(k >= 0, 16);
        kinds[i] = k;
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// g5_ffn_kind: glm5-next.cpp:155-173. The leading n_layer_dense_lead layers
// build one dense SwiGLU FFN; every later trunk layer builds routed experts
// plus shared experts. The MTP block is built by graph_mtp with build_moe_ffn
// unconditionally (:630-649), so it is MoE even if dense_lead were larger than
// n_layer. Returns -1 outside the model.
export fn g5_ffn_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return F_MOE;
    }
    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }
    return F_MOE;
}

// g5_indexer_full: glm5-next.cpp:141-151 and :1069-1073. The loader defaults
// every trunk layer to a full indexer and may clear the flag from the model
// file (n_layer entries). A non-full MLA layer reuses the previous full
// layer's selection and only carries optional copies of the indexer tensors.
// The MTP block is always full (`il >= n_layer()`).
export fn g5_indexer_full(p: i64[:], full_flags: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 18);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return 1;
    }
    e_ensure(full_flags.len >= p[P_N_LAYER], 19);
    if full_flags[il] != 0 {
        return 1;
    }
    return 0;
}

// g5_has_indexer: the DSA indexer lives on the MLA branch only; KDA layers have
// no indexer at all (glm5-next.cpp:139-151 is inside the MLA else-branch).
export fn g5_has_indexer(kind: i64) -> i64 {
    if kind == K_MLA || kind == K_MTP {
        return 1;
    }
    return 0;
}

// g5_kpool_ok: glm5-next.cpp:45,
// `indexer_kpool > 1 && indexer_top_k % indexer_kpool == 0`.
export fn g5_kpool_ok(kpool: i64, top_k: i64) -> i64 {
    if kpool <= 1 {
        return 0;
    }
    if top_k <= 0 {
        return 0;
    }
    if top_k % kpool == 0 {
        return 1;
    }
    return 0;
}

// g5_n_top_pool: glm5-next.cpp:322 / :973,
// `n_top_pool = min(n_pool, indexer_top_k / kpool)`.
export fn g5_n_top_pool(n_pool: i64, top_k: i64, kpool: i64) -> i64 {
    e_ensure(n_pool >= 0 && top_k >= 0 && kpool > 1, 20);
    let want = top_k / kpool;
    if n_pool < want {
        return n_pool;
    }
    return want;
}

// g5_n_sel: glm5-next.cpp:325-329, the sparse selection width is the whole
// pools selected by top-k plus the incomplete tail when the model asks for it.
export fn g5_n_sel(n_pool: i64, top_k: i64, kpool: i64, tail: i64) -> i64 {
    let n_top = g5_n_top_pool(n_pool, top_k, kpool);
    var n: i64 = kpool * n_top;
    if tail != 0 {
        n = n + (kpool - 1);
    }
    return n;
}

// g5_dsa_mask_len: glm5-next.cpp:1002,
// `mask_all` is `n_kv + n_sel` wide before the causal mask is folded in.
export fn g5_dsa_mask_len(n_kv: i64, n_sel: i64) -> i64 {
    e_ensure(n_kv >= 0 && n_sel >= 0, 21);
    return n_kv + n_sel;
}

// g5_mla_nope_ok: glm5-next.cpp:1046, `GGML_ASSERT(n_rot() == 0)`. The DSA
// builder only implements a no-rope MLA head.
export fn g5_mla_nope_ok(n_rot: i64) -> i64 {
    if n_rot == 0 {
        return 1;
    }
    return 0;
}

// g5_qk_nope: glm5-next.cpp:1042,
// `n_embd_head_qk_nope = n_embd_head_k_mla - n_rot`.
export fn g5_qk_nope(head_k: i64, n_rot: i64) -> i64 {
    e_ensure(head_k >= n_rot && n_rot >= 0, 22);
    return head_k - n_rot;
}

// g5_mla_cache_width: glm5-next.cpp:19,
// `n_embd_head_v_full = n_lora_kv`: the MLA cache stores the compressed latent,
// not a per-head value vector.
export fn g5_mla_cache_width(kv_lora: i64) -> i64 {
    e_ensure(kv_lora > 0, 23);
    return kv_lora;
}

// g5_wkv_a_width: glm5-next.cpp:134, `{n_embd, kv_lora_rank + qk_rope_head_dim}`.
// With nope-only MLA n_rot is zero, but the loader formula stays generic.
export fn g5_wkv_a_width(kv_lora: i64, n_rot: i64) -> i64 {
    e_ensure(kv_lora >= 0 && n_rot >= 0, 24);
    return kv_lora + n_rot;
}

// g5_d_inner: glm5-next.cpp:96, `d_inner = head_dim * n_head` with
// head_dim = n_embd_head_kda. Every KDA projection is d_inner wide.
export fn g5_d_inner(head_dim: i64, n_head: i64) -> i64 {
    e_ensure(head_dim > 0 && n_head > 0, 25);
    return head_dim * n_head;
}

// g5_kda_conv_state: glm5-next.cpp:213-215,
// `conv_state_size = (d_conv - 1) * d_inner` per Q/K/V stream.
export fn g5_kda_conv_state(d_conv: i64, d_inner: i64) -> i64 {
    e_ensure(d_conv > 1 && d_inner > 0, 26);
    return (d_conv - 1) * d_inner;
}

// g5_kda_gate_kind: glm5-next.cpp:834-846. A finite kda_gate_lower_bound
// selects the sigmoid-bounded decay path; an absent key keeps the softplus
// path. 1 means bounded, 0 means softplus.
export fn g5_kda_gate_kind(lower_bound_set: i64) -> i64 {
    if lower_bound_set != 0 {
        return 1;
    }
    return 0;
}

// g5_hc_mix_dim: glm5-next.cpp:65, `hc_mix_dim = (2 + hc)*hc` -- two hc-wide
// affine rows (pre, post) plus the hc*hc mixing matrix.
export fn g5_hc_mix_dim(hc: i64) -> i64 {
    e_ensure(hc > 0, 27);
    return (2 + hc) * hc;
}

// g5_hc_supported: glm5-next.cpp:53, `GGML_ASSERT(dsv4_hc_mult == 4)`, repeated
// at :461 before the Sinkhorn mixer is built.
export fn g5_hc_supported(hc: i64) -> i64 {
    if hc == 4 {
        return 1;
    }
    return 0;
}

// g5_hc_fn_width: glm5-next.cpp:86, `{hc*n_embd, hc_mix_dim}`. The mixer reads
// the hc streams flattened.
export fn g5_hc_fn_width(n_embd: i64, hc: i64) -> i64 {
    e_ensure(n_embd > 0 && hc > 0, 28);
    return hc * n_embd;
}

// g5_out_width_effective: llama-hparams.cpp:136-138, an unset impl falls back
// to n_embd.
export fn g5_out_width_effective(impl: i64, n_embd: i64) -> i64 {
    if impl > 0 {
        return impl;
    }
    return n_embd;
}

// g5_out_matches: the MTP assert at glm5-next.cpp:564,
// `n_embd_out() == n_embd`. Unlike deepseek4 the MTP block is flat: the trunk
// averages the hc streams before the head, so the width is not hc*n_embd.
export fn g5_out_matches(out_width: i64, n_embd: i64) -> i64 {
    if out_width == n_embd {
        return 1;
    }
    return 0;
}

// g5_ff_exp: llama-hparams.cpp:74-80, `n_ff_exp_arr[il]` directly; there is no
// fallback to n_ff/n_expert_used in this architecture.
export fn g5_ff_exp(p: i64[:], ff_exp: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 29);
    e_ensure(il >= 0 && il < p[P_N_LAYER_ALL] && il < ff_exp.len, 30);
    return ff_exp[il];
}

// g5_shexp_width: glm5-next.cpp:170-172, one shared expert of
// `n_ff_exp * n_expert_shared` rows, added to the routed output.
export fn g5_shexp_width(ff_exp: i64, n_shexp: i64) -> i64 {
    e_ensure(ff_exp >= 0 && n_shexp >= 0, 31);
    return ff_exp * n_shexp;
}

// g5_gating_effective: glm5-next.cpp:31-33, an absent gating function defaults
// to SIGMOID (1) instead of aborting as deepseek4 does.
export fn g5_gating_effective(func: i64) -> i64 {
    if func == 0 {
        return 1;
    }
    return func;
}

// g5_clamp_shexp: glm5-next.cpp:36-38, `swiglu_clamp_shexp` defaults to
// `swiglu_clamp_exp` when the key is absent.
export fn g5_clamp_shexp(found: i64, declared: i64, exp: i64) -> i64 {
    if found == 0 {
        return exp;
    }
    return declared;
}

// g5_nextn_count: glm5-next.cpp:548-555 and llama-model.cpp nextn_flags. A
// declared MTP block survives only if it exists in the file; otherwise the
// loader zeroes n_layer_nextn.
export fn g5_nextn_count(declared: i64, probe_found: i64) -> i64 {
    if declared <= 0 {
        return 0;
    }
    if probe_found == 0 {
        return 0;
    }
    return declared;
}

// g5_nextn_single: glm5-next.cpp:552,
// `GGML_ASSERT(n_layer_nextn == 1)`. Only a single NextN block is supported.
export fn g5_nextn_single(declared: i64) -> i64 {
    if declared == 1 {
        return 1;
    }
    return 0;
}

// g5_eh_proj_width: glm5-next.cpp:176, `{2 * n_embd, n_embd}`: the projection
// concatenates the normalized embedding and the flat MTP hidden state.
export fn g5_eh_proj_width(n_embd: i64) -> i64 {
    e_ensure(n_embd > 0, 32);
    return 2 * n_embd;
}

// g5_shape: shape of one tensor for a layer kind. Returns 1 and fills
// out[0..2] when the plan owns the tensor, 0 otherwise. full_flags carries the
// per-trunk-layer indexer flag (n_layer entries, MTP excluded).
export fn g5_shape(p: i64[:], ff_exp: i64[:], full_flags: i64[:],
                   kind: i64, il: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 40);
    e_ensure(out.len >= 3, 41);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let head_k = p[P_EMBD_HEAD_K];
    let head_v = p[P_EMBD_HEAD_V];
    let n_rot = p[P_N_ROT];
    let q_lora = p[P_Q_LORA];
    let kv_lora = p[P_KV_LORA];
    let kda_head = p[P_KDA_HEAD];
    let d_conv = p[P_D_CONV];
    let hc = p[P_HC];

    let d_inner = g5_d_inner(kda_head, n_head);
    let hc_dim = g5_hc_fn_width(n_embd, hc);
    let hc_mix = g5_hc_mix_dim(hc);

    if tid == T_TOKEN_EMBD || tid == T_OUTPUT {
        // an absent output reuses token_embd, same shape
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind != K_RECR && kind != K_MLA && kind != K_MTP {
        return 0;
    }

    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    // hyper-connections: trunk layers only (glm5-next.cpp:85-90, `i < n_layer`)
    if kind != K_MTP {
        if tid == T_HC_ATTN_FN || tid == T_HC_FFN_FN {
            e_put3(out, hc_dim, hc_mix, 1);
            return 1;
        }
        if tid == T_HC_ATTN_BASE || tid == T_HC_FFN_BASE {
            e_put3(out, hc_mix, 1, 1);
            return 1;
        }
        if tid == T_HC_ATTN_SCALE || tid == T_HC_FFN_SCALE {
            e_put3(out, 3, 1, 1);
            return 1;
        }
    }

    // attention family
    if kind == K_RECR {
        // KDA: Q/K/V all d_inner wide, then the decay and output gates
        if tid == T_WQ || tid == T_WK || tid == T_WV {
            e_put3(out, n_embd, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_Q_CONV || tid == T_SSM_K_CONV || tid == T_SSM_V_CONV {
            // native prefers the 4D {d_conv, 1, d_inner, 1} form; the 3D
            // fallback has the same elements
            e_put3(out, d_conv, 1, d_inner);
            return 1;
        }
        if tid == T_SSM_F_A || tid == T_SSM_G_A {
            e_put3(out, n_embd, kda_head, 1);
            return 1;
        }
        if tid == T_SSM_F_B || tid == T_SSM_G_B {
            e_put3(out, kda_head, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_BETA {
            e_put3(out, n_embd, n_head, 1);
            return 1;
        }
        if tid == T_SSM_A {
            e_put3(out, n_head, 1, 1);
            return 1;
        }
        if tid == T_SSM_DT_B {
            e_put3(out, d_inner, 1, 1);
            return 1;
        }
        if tid == T_SSM_NORM {
            e_put3(out, kda_head, 1, 1);
            return 1;
        }
        if tid == T_WO {
            e_put3(out, d_inner, n_embd, 1);
            return 1;
        }
    } else {
        // nope MLA: low-rank query, compressed KV, absorbed K and V output
        let qk_nope = g5_qk_nope(head_k, n_rot);
        if tid == T_Q_A_NORM {
            e_put3(out, q_lora, 1, 1);
            return 1;
        }
        if tid == T_KV_A_NORM {
            e_put3(out, kv_lora, 1, 1);
            return 1;
        }
        if tid == T_WQ_A {
            e_put3(out, n_embd, q_lora, 1);
            return 1;
        }
        if tid == T_WQ_B {
            e_put3(out, q_lora, n_head * head_k, 1);
            return 1;
        }
        if tid == T_WKV_A_MQA {
            e_put3(out, n_embd, g5_wkv_a_width(kv_lora, n_rot), 1);
            return 1;
        }
        if tid == T_WK_B {
            e_put3(out, qk_nope, kv_lora, n_head);
            return 1;
        }
        if tid == T_WV_B {
            e_put3(out, kv_lora, head_v, n_head);
            return 1;
        }
        if tid == T_ATTN_OUT {
            e_put3(out, n_head * head_v, n_embd, 1);
            return 1;
        }

        // k-pool DSA indexer: full layers own the tensors, shared layers reuse
        // the previous selection (:141-151, :1069-1073)
        if g5_indexer_full(p, full_flags, il) == 1 {
            let idx_nhead = p[P_IDX_NHEAD];
            let idx_head = p[P_IDX_HEAD];
            let kpool = p[P_KPOOL];
            if tid == T_INDEXER_K_NORM || tid == T_INDEXER_K_NORM_B {
                e_put3(out, idx_head, 1, 1);
                return 1;
            }
            if tid == T_INDEXER_PROJ {
                e_put3(out, n_embd, idx_nhead, 1);
                return 1;
            }
            if tid == T_INDEXER_ATTN_K || tid == T_INDEXER_KPOOL_GATE {
                e_put3(out, n_embd, idx_head, 1);
                return 1;
            }
            if tid == T_INDEXER_ATTN_Q_B {
                e_put3(out, q_lora, idx_nhead * idx_head, 1);
                return 1;
            }
            if tid == T_INDEXER_KPOOL_APE {
                e_put3(out, idx_head, kpool, 1);
                return 1;
            }
        }
    }

    // FFN: dense lead, then routed plus shared experts; MTP is always MoE
    let ffn = g5_ffn_kind(p, il);
    if ffn == F_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, p[P_N_FF], 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            e_put3(out, p[P_N_FF], n_embd, 1);
            return 1;
        }
        return 0;
    }
    if ffn == F_MOE {
        let n_ff_exp = g5_ff_exp(p, ff_exp, il);
        let shexp_w = g5_shexp_width(n_ff_exp, p[P_N_SHEXP]);
        if tid == T_FFN_GATE_INP {
            e_put3(out, n_embd, p[P_N_EXPERT], 1);
            return 1;
        }
        if tid == T_FFN_EXP_PROBS_B {
            e_put3(out, p[P_N_EXPERT], 1, 1);
            return 1;
        }
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
            e_put3(out, n_embd, n_ff_exp, p[P_N_EXPERT]);
            return 1;
        }
        if tid == T_FFN_DOWN_EXPS {
            e_put3(out, n_ff_exp, n_embd, p[P_N_EXPERT]);
            return 1;
        }
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            e_put3(out, n_embd, shexp_w, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            e_put3(out, shexp_w, n_embd, 1);
            return 1;
        }
    }

    // MTP block tensors (glm5-next.cpp:175-183)
    if kind == K_MTP {
        if tid == T_NEXTN_EH_PROJ {
            e_put3(out, g5_eh_proj_width(n_embd), n_embd, 1);
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
    }

    return 0;
}
