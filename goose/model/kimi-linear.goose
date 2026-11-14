// P6-05 architecture 11: kimi-linear builder preparation.
//
// Decision/shape half, not the graph emitter. Reuses the accepted shared slice
// in base/model_graph.goose (goose/model/goose-model-graph.goose minus its
// `fn main`): key_dim, n_ff_exp, gating_kind, kq_scale_auto, weights_need_scale.
//
// Native anchors (src/models/kimi-linear.cpp):
//   hparams: MLA head widths, KV lora rank, KDA head dim, conv kernel :4-10
//   per-layer recurrent marking from n_head_kv == 0                    :15-19
//   expert ffn length array, shared count, dense lead, score scale     :21-26
//   type table, 27 layers -> 48B-A3B                                   :28-31
//   global tensors, output is required (flags 0)                       :37-41
//   per-layer attn_norm                                                :46
//   KDA tensor set (conv1d, q/k/v, f_a/f_b, beta, A_log, dt, g gate)   :53-103
//   MLA tensor set (optional q lora, kv_a_mqa, wkv_b or wk_b/wv_b)     :105-134
//   ffn_norm                                                           :137
//   dense FFN for il < n_layer_dense_lead                              :144-148
//   routed + shared experts, exp_probs_b bias                          :149-164
//   causal_conv1d geometry, three conv states                          :175-178
//   MLA head split, kq scale from n_embd_head_k_mla                    :272-276
//   KDA graph: conv, softplus decay, sigmoid beta, sigmoid out gate     :303-386
//   MLA graph: combined wkv_b fallback vs wk_b/wv_b absorb             :387-483
//   dense/MoE FFN branch in the graph                                  :501-543
//   final RMS norm and output                                          :551-558
//
// kimi-linear is the hybrid MLA/KDA member (Kimi Delta Attention) with a
// leading dense block. The shape table must keep these decisions:
//   - per-layer kind: KDA where the file reports n_head_kv == 0, MLA
//     everywhere else; the loader writes that into is_recr_impl and the graph
//     only reads the resulting flag, so there is no hybrid interval and no MTP
//     block,
//   - KDA is not a GQA head split: q, k and v are all n_head * n_embd_head_kda
//     wide, each with its own causal conv1d (three conv states, not one fused),
//     and the value head width equals the key head width by construction,
//   - MLA is latent, not GQA: the KV side is one latent vector of width
//     kv_lora_rank plus a rotary-width slice that is never rotated, so a
//     non-zero n_head_kv on an MLA layer does not widen the KV projection,
//   - the query side is optional low rank (attn_q_a_norm decides wq_a/wq_b
//     versus one wq),
//   - the KV side has two load-time paths: the legacy combined wkv_b (MHA
//     fallback) or the split wk_b/wv_b used by the MLA KV cache,
//   - every layer at or past n_layer_dense_lead carries routed experts plus one
//     shared expert whose width is n_ff_exp * n_expert_shared, and a per-layer
//     router score bias.

import std;
import base.model_graph;

// p indices -- the shared fixture contract of this experiment, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_HEAD: i64 = 3;
const P_N_FF: i64 = 4;
const P_N_EXPERT: i64 = 5;
const P_N_EXPERT_USED: i64 = 6;
const P_N_FF_EXP: i64 = 7;
const P_N_EXPERT_SHARED: i64 = 8;
const P_D_CONV: i64 = 9;
const P_HEAD_KDA: i64 = 10;
const P_EMBD_HEAD_K: i64 = 11;
const P_EMBD_HEAD_V: i64 = 12;
const P_KV_LORA: i64 = 13;
const P_Q_LORA: i64 = 14;
const P_N_ROT: i64 = 15;
const P_DENSE_LEAD: i64 = 16;
const P_HAS_Q_LORA: i64 = 17;
const P_HAS_WKV_B: i64 = 18;
const P_GATING: i64 = 19;
const P_W_SCALE_BITS: i64 = 20;
const P_LEN: i64 = 21;

// attention kinds
const K_MLA: i64 = 0;
const K_KDA: i64 = 1;

// ffn kinds
const F_DENSE: i64 = 0;
const F_MOE: i64 = 1;

// gating op kinds as the recurrent branch applies them
const GATE_SILU: i64 = 1;
const GATE_SIGMOID: i64 = 2;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_FFN_NORM: i64 = 4;
const T_WQ: i64 = 5;
const T_WQ_A: i64 = 6;
const T_ATTN_Q_A_NORM: i64 = 7;
const T_WQ_B: i64 = 8;
const T_WKV_A_MQA: i64 = 9;
const T_ATTN_KV_A_NORM: i64 = 10;
const T_WKV_B: i64 = 11;
const T_WK_B: i64 = 12;
const T_WV_B: i64 = 13;
const T_ATTN_OUT: i64 = 14;
const T_SSM_Q_CONV: i64 = 15;
const T_SSM_K_CONV: i64 = 16;
const T_SSM_V_CONV: i64 = 17;
const T_WQ_KDA: i64 = 18;
const T_WK_KDA: i64 = 19;
const T_WV_KDA: i64 = 20;
const T_SSM_F_A: i64 = 21;
const T_SSM_F_B: i64 = 22;
const T_SSM_BETA: i64 = 23;
const T_SSM_A: i64 = 24;
const T_SSM_DT: i64 = 25;
const T_SSM_G_A: i64 = 26;
const T_SSM_G_B: i64 = 27;
const T_SSM_O_NORM: i64 = 28;
const T_FFN_GATE: i64 = 29;
const T_FFN_DOWN: i64 = 30;
const T_FFN_UP: i64 = 31;
const T_FFN_GATE_INP: i64 = 32;
const T_FFN_GATE_EXPS: i64 = 33;
const T_FFN_DOWN_EXPS: i64 = 34;
const T_FFN_UP_EXPS: i64 = 35;
const T_FFN_GATE_SHEXP: i64 = 36;
const T_FFN_DOWN_SHEXP: i64 = 37;
const T_FFN_UP_SHEXP: i64 = 38;
const T_FFN_EXP_PROBS_B: i64 = 39;
const T_LEN: i64 = 40;

fn k_ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn k_put3(out: i64[:], a: i64, b: i64, c: i64) {
    out[0] = a;
    out[1] = b;
    out[2] = c;
}

// kl_recr_flags: kimi-linear.cpp:15-19. The loader does not read a hybrid
// interval: it marks a layer recurrent exactly when the file reports
// n_head_kv == 0 for it, and every KDA layer of the 48B-A3B checkpoint is
// declared that way. flags[i] = 1 means "recurrent" (KDA). Returns n_layer.
export fn kl_recr_flags(kv: i64[:], flags: i64[:]) -> i64 {
    k_ensure(flags.len >= kv.len, 10);
    var i: i64 = 0;
    while i < kv.len {
        if kv[i] == 0 {
            flags[i] = 1;
        } else {
            flags[i] = 0;
        }
        i = i + 1;
    }
    return kv.len;
}

// kl_kind: the plan decision of the graph, which only sees the flag that
// llama-hparams.cpp:260-265 exposes as is_recr(il). Out of range is fatal in
// native (GGML_ABORT), reported here as -1 so the caller can stop cleanly.
export fn kl_kind(flags: i64[:], il: i64) -> i64 {
    if il < 0 {
        return -1;
    }
    if il >= flags.len {
        return -1;
    }
    if flags[il] != 0 {
        return K_KDA;
    }
    return K_MLA;
}

export fn kl_plan(flags: i64[:], kinds: i64[:]) -> i64 {
    k_ensure(kinds.len >= flags.len, 11);
    var i: i64 = 0;
    while i < flags.len {
        kinds[i] = kl_kind(flags, i);
        i = i + 1;
    }
    return flags.len;
}

// kl_ffn_kind: kimi-linear.cpp:144, `i < n_layer_dense_lead` picks the dense
// FFN, the graph repeats the comparison at :501. The default lead comes from
// LLM_KV_LEADING_DENSE_BLOCK_COUNT (:24), so it is a per-file value.
export fn kl_ffn_kind(p: i64[:], il: i64) -> i64 {
    k_ensure(p.len >= P_LEN, 12);
    if il < 0 {
        return -1;
    }
    if il >= p[P_N_LAYER] {
        return -1;
    }
    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }
    return F_MOE;
}

// kl_type_known: kimi-linear.cpp:28-31. The loader recognises exactly one
// checkpoint size (27 layers -> LLM_TYPE_48B_A3B) and falls back to
// LLM_TYPE_UNKNOWN otherwise; an unknown type is not an abort, it only means
// the metadata table has no name for the model.
export fn kl_type_known(n_layer: i64) -> i64 {
    if n_layer == 27 {
        return 1;
    }
    return 0;
}

// Loader policy exports. They are named so the gate pins them without copying
// native line numbers into the fixture.
//   output is a required tensor (flags 0 at kimi-linear.cpp:41), unlike the
//   MoE families where an absent output reuses token_embd.
export fn kl_output_required() -> i64 {
    return 1;
}
//   no attn_post_norm exists in this architecture (:46 creates attn_norm only).
export fn kl_has_post_norm() -> i64 {
    return 0;
}
//   MLA carries no q/k RMS norm: the query side is unnormalised and the only
//   norm is attn_kv_a_norm on the latent (:113).
export fn kl_qk_norm_required() -> i64 {
    return 0;
}
//   Kimi MLA does not apply rotary at all: n_rot() is used as the width of the
//   k_pe slice (:125, :273) and the comment at :396 notes rotary_emb is None.
export fn kl_rope_applied() -> i64 {
    return 0;
}
//   build_moe_ffn is called with norm_w = true (:520) and the score scale from
//   the file (expert_weights_scale, :25).
export fn kl_moe_norm_w() -> i64 {
    return 1;
}
//   KDA beta is ggml_sigmoid (:334) and the output gate is ggml_sigmoid too
//   (:378), not the SiLU gate of the GDN families.
export fn kl_kda_beta_gate() -> i64 {
    return GATE_SIGMOID;
}
export fn kl_kda_out_gate() -> i64 {
    return GATE_SIGMOID;
}
//   the output gate multiplies an RMS-normalised value (:376-379), so the norm
//   is applied before the gate, not after it.
export fn kl_kda_norm_before_gate() -> i64 {
    return 1;
}
export fn kl_gating_kind(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 13);
    return gating_kind(p[P_GATING]);
}
export fn kl_weights_scale_used(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 14);
    return weights_need_scale(p[P_W_SCALE_BITS]);
}
//   the MLA scale is computed as 1/sqrt(n_embd_head_k_mla) (:276), it does not
//   come from a file key.
export fn kl_kq_scale_auto_kind() -> i64 {
    return kq_scale_auto(0);
}
//   a query lora exists only when attn_q_a_norm was created (:112-122).
export fn kl_q_lora_used(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 15);
    if p[P_HAS_Q_LORA] != 0 {
        return 1;
    }
    return 0;
}
// kl_mla_cache_path: kimi-linear.cpp:127-134. The legacy layout stores one
// combined wkv_b and runs the MHA fallback; the modern layout stores wk_b and
// wv_b and absorbs the latent into the query. 1 means the split (absorb) path.
export fn kl_mla_cache_path(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 16);
    if p[P_HAS_WKV_B] != 0 {
        return 0;
    }
    return 1;
}

// kl_nope: kimi-linear.cpp:274, `n_embd_head_k_mla - n_rot`. The rotary width
// is a prefix of the MLA key head even though no rotary is applied.
export fn kl_nope(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 17);
    if p[P_EMBD_HEAD_K] < p[P_N_ROT] {
        fail(18);
    }
    return p[P_EMBD_HEAD_K] - p[P_N_ROT];
}

// kl_mla_wkv_b_width: kimi-linear.cpp:129, the combined tensor emits
// n_embd_head_qk_nope + n_embd_head_v_mla values per head.
export fn kl_mla_wkv_b_width(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 19);
    return p[P_N_HEAD] * (kl_nope(p) + p[P_EMBD_HEAD_V]);
}

// kl_d_inner: kimi-linear.cpp:265, `d_inner = n_head * n_embd_head_kda`
// (32 * 128 = 4096). KDA has one shared width for q, k and v.
export fn kl_d_inner(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 20);
    return key_dim(p[P_HEAD_KDA], p[P_N_HEAD]);
}

// kl_kda_conv_state: llama-hparams.cpp:219-224, the Kimi KDA branch of
// n_embd_r(): `3 * (ssm_d_conv > 0 ? ssm_d_conv - 1 : 3) * d_inner`.
// One state per Q/K/V stream, each d_conv-1 taps wide.
export fn kl_kda_conv_state(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 21);
    let taps = if p[P_D_CONV] > 0 { p[P_D_CONV] - 1 } else { 3 };
    return taps * kl_d_inner(p);
}

// kl_kda_conv_total: kimi-linear.cpp:177-178, `n_embd_r_total = 3 * conv_state_size`
// because Q, K and V are packed into one recurrent row.
export fn kl_kda_conv_total(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 22);
    return 3 * kl_kda_conv_state(p);
}

// kl_kda_state: llama-hparams.cpp:242-247, n_embd_s() for KDA is
// `n_embd_head_kda * n_embd_head_kda * n_head` -- the delta-net matrix state.
export fn kl_kda_state(p: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 23);
    return p[P_HEAD_KDA] * p[P_HEAD_KDA] * p[P_N_HEAD];
}

// kl_shexp_width: kimi-linear.cpp:159, `n_ff_exp * (n_expert_shared > 0 ?
// n_expert_shared : 1)`. Unlike the GDN families the shared width is a
// multiple of the expert width, not `n_ff`.
export fn kl_shexp_width(n_ff_exp: i64, n_shexp: i64) -> i64 {
    k_ensure(n_ff_exp >= 0, 24);
    if n_shexp > 0 {
        return n_ff_exp * n_shexp;
    }
    return n_ff_exp;
}

// kl_shape: shape of one tensor for a layer. kind selects the attention family,
// il selects the FFN branch (dense lead); a tensor the layer does not own
// returns 0. Three-slot report: a 1-D vector is {n, 1, 1}, a 2-D matrix is
// {rows, cols, 1}, and a 4-D declaration is reported by its leading three
// dimensions with the trailing 1 dropped (the conv weight below).
export fn kl_shape(p: i64[:], kind: i64, il: i64, tid: i64, out: i64[:]) -> i64 {
    k_ensure(p.len >= P_LEN, 30);
    k_ensure(out.len >= 3, 31);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let n_ff = p[P_N_FF];
    let n_expert = p[P_N_EXPERT];
    let head_kda = p[P_HEAD_KDA];
    let head_k = p[P_EMBD_HEAD_K];
    let head_v = p[P_EMBD_HEAD_V];
    let kv_lora = p[P_KV_LORA];
    let q_lora = p[P_Q_LORA];
    let d_inner = kl_d_inner(p);
    let nope = kl_nope(p);

    // kimi-linear.cpp:140, declared expert width (per-layer array in the file,
    // a scalar here) with the shared fallback n_ff / n_expert_used.
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], n_ff, p[P_N_EXPERT_USED]);
    let n_ff_shexp_v = kl_shexp_width(n_ff_exp_v, p[P_N_EXPERT_SHARED]);

    if tid == T_TOKEN_EMBD || tid == T_OUTPUT {
        // output is required here; both are [n_embd, n_vocab] (:37-41)
        k_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        k_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind != K_MLA && kind != K_KDA {
        return 0;
    }

    // every layer carries attn_norm (:46) and ffn_norm (:137)
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        k_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind == K_MLA {
        // query side: low rank only when attn_q_a_norm exists (:112-121)
        if tid == T_ATTN_Q_A_NORM {
            if kl_q_lora_used(p) == 0 {
                return 0;
            }
            k_put3(out, q_lora, 1, 1);
            return 1;
        }
        if tid == T_WQ_A {
            if kl_q_lora_used(p) == 0 {
                return 0;
            }
            k_put3(out, n_embd, q_lora, 1);
            return 1;
        }
        if tid == T_WQ_B {
            if kl_q_lora_used(p) == 0 {
                return 0;
            }
            k_put3(out, q_lora, n_head * head_k, 1);
            return 1;
        }
        if tid == T_WQ {
            if kl_q_lora_used(p) != 0 {
                return 0;
            }
            k_put3(out, n_embd, n_head * head_k, 1);
            return 1;
        }
        // KV side: one latent of kv_lora_rank plus the unrotated k_pe slice (:126)
        if tid == T_WKV_A_MQA {
            k_put3(out, n_embd, kv_lora + p[P_N_ROT], 1);
            return 1;
        }
        if tid == T_ATTN_KV_A_NORM {
            k_put3(out, kv_lora, 1, 1);
            return 1;
        }
        if tid == T_WKV_B {
            // legacy combined layout only; absent on the split path
            if kl_mla_cache_path(p) != 0 {
                return 0;
            }
            k_put3(out, kv_lora, kl_mla_wkv_b_width(p), 1);
            return 1;
        }
        if tid == T_WK_B {
            if kl_mla_cache_path(p) == 0 {
                return 0;
            }
            k_put3(out, nope, kv_lora, n_head);
            return 1;
        }
        if tid == T_WV_B {
            if kl_mla_cache_path(p) == 0 {
                return 0;
            }
            k_put3(out, kv_lora, head_v, n_head);
            return 1;
        }
        if tid == T_ATTN_OUT {
            k_put3(out, n_head * head_v, n_embd, 1);
            return 1;
        }
    }

    if kind == K_KDA {
        // three causal conv1d weights, 4-D in the file (:59-73)
        if tid == T_SSM_Q_CONV || tid == T_SSM_K_CONV || tid == T_SSM_V_CONV {
            k_put3(out, p[P_D_CONV], 1, d_inner);
            return 1;
        }
        // create_tensor_qkv with d_inner on all three blocks (:76); the value
        // head width equals the key head width for KDA (:54-55)
        if tid == T_WQ_KDA || tid == T_WK_KDA || tid == T_WV_KDA {
            k_put3(out, n_embd, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_F_A || tid == T_SSM_G_A {
            k_put3(out, n_embd, head_kda, 1);
            return 1;
        }
        if tid == T_SSM_F_B || tid == T_SSM_G_B {
            k_put3(out, head_kda, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_BETA {
            // one mixing coefficient per head, not per channel (:84)
            k_put3(out, n_embd, n_head, 1);
            return 1;
        }
        if tid == T_SSM_A {
            // A_log is [1, n_head, 1, 1] or [1, n_head] after quantisation (:87-89)
            k_put3(out, 1, n_head, 1);
            return 1;
        }
        if tid == T_SSM_DT {
            // dt_bias covers the whole inner width (:93)
            k_put3(out, d_inner, 1, 1);
            return 1;
        }
        if tid == T_SSM_O_NORM {
            // FusedRMSNormGated over one head width (:100)
            k_put3(out, head_kda, 1, 1);
            return 1;
        }
        if tid == T_ATTN_OUT {
            k_put3(out, d_inner, n_embd, 1);
            return 1;
        }
    }

    let fk = kl_ffn_kind(p, il);
    if fk == F_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            k_put3(out, n_embd, n_ff, 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            k_put3(out, n_ff, n_embd, 1);
            return 1;
        }
        return 0;
    }
    if fk == F_MOE {
        if tid == T_FFN_GATE_INP {
            k_put3(out, n_embd, n_expert, 1);
            return 1;
        }
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
            k_put3(out, n_embd, n_ff_exp_v, n_expert);
            return 1;
        }
        if tid == T_FFN_DOWN_EXPS {
            k_put3(out, n_ff_exp_v, n_embd, n_expert);
            return 1;
        }
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            k_put3(out, n_embd, n_ff_shexp_v, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            k_put3(out, n_ff_shexp_v, n_embd, 1);
            return 1;
        }
        if tid == T_FFN_EXP_PROBS_B {
            // required per-layer router score bias (:164)
            k_put3(out, n_expert, 1, 1);
            return 1;
        }
    }

    return 0;
}
