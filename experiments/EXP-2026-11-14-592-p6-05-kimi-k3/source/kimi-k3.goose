// P6-05 architecture 10: kimi-k3 builder preparation.
//
// Decision/shape half, not the graph emitter. It reuses the accepted shared
// slice in base/model_graph.goose (goose/model/goose-model-graph.goose minus
// its `fn main`): n_ff_exp and gating_kind.
//
// Native anchors (src/models/kimi-k3.cpp):
//   hparams: MLA key/value lengths, lora ranks, KDA keys, expert
//   feed-forward array, shared-expert count, dense lead, gating, latent
//   length, residual attention block size, situ betas         :16-51
//   global tensors, output res score                          :53-65
//   per-layer norms and the two res-score vectors              :67-76
//   KDA tensor set (convs, f_a/f_b, beta, A_log, dt, full gate,
//   o_norm, wo)                                               :78-105
//   MLA tensor set (low-rank q, wkv_a_mqa, wkv_b or wk_b/wv_b,
//   sigmoid output gate, wo)                                  :106-137
//   dense lead FFN vs latent MoE + shared experts             :139-165
//   residual mix / push and the trunk loop                    :277-330
//   KDA layer, lower-bound gate                               :397-477
//   MLA layer, nope-only attention                            :483-561
//   latent MoE body                                           :568-616
//
// kimi-k3 differs from the MoE families (qwen3moe/qwen2moe/glm4-moe) in the
// ways the plan and the shape table must keep:
//   - attention is hybrid: a layer is KDA (recurrent linear attention) when
//     its n_head_kv is 0 and MLA otherwise; the marker is a boolean, not a
//     grouped-query width, because MLA keeps exactly one latent KV row,
//   - the MLA is nope-only: the rope part exists in wkv_a_mqa but no RoPE op
//     is applied, so the rotary width only splits the head,
//   - the KDA carries a single full-rank gate (ssm_g) instead of kimi-linear's
//     g_a/g_b pair, and its decay gate swaps softplus for a lower-bound
//     sigmoid when the bound key is present,
//   - the first n_layer_dense_lead blocks run a dense situ FFN, the rest run
//     a latent MoE: routed experts read the down-projected latent state while
//     the router and the shared experts read the full-width one,
//   - shared experts are optional and their width is
//     n_ff_exp * max(n_expert_shared, 1),
//   - cross-layer residual attention is optional (attn_res_block_size): when
//     it is on, every layer owns attn_res_score and ffn_res_score vectors and
//     the model owns output_res_score,
//   - there is no MTP/nextn block: the loader and the graph loop over
//     n_layer only (lines 31, 67, 277), so a file that declares
//     nextn.predict_layers would silently lose those blocks.

import std;
import base.model_graph;

// p indices -- the fixture contract of this slice, see tests/cases.md
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_HEAD: i64 = 4;
const P_EMBD_KDA: i64 = 5;
const P_SSM_CONV: i64 = 6;
const P_HEAD_K_MLA: i64 = 7;
const P_HEAD_V_MLA: i64 = 8;
const P_N_ROT: i64 = 9;
const P_Q_LORA: i64 = 10;
const P_KV_LORA: i64 = 11;
const P_N_FF: i64 = 12;
const P_N_EXPERT: i64 = 13;
const P_N_EXPERT_USED: i64 = 14;
const P_N_FF_EXP: i64 = 15;
const P_N_SHEXP: i64 = 16;
const P_DENSE_LEAD: i64 = 17;
const P_EXPERT_LATENT: i64 = 18;
const P_RES_BLOCK: i64 = 19;
const P_GATING: i64 = 20;
const P_LINEAR_BETA: i64 = 21;
const P_LEN: i64 = 22;

// plan kinds (FFN side); MTP is defined but not supported by this family
const K_DENSE: i64 = 0;
const K_MOE: i64 = 1;
const K_MTP: i64 = 2;

// attention kinds
const A_KDA: i64 = 0;
const A_MLA: i64 = 1;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_OUTPUT_RES_SCORE: i64 = 3;
const T_ATTN_NORM: i64 = 4;
const T_FFN_NORM: i64 = 5;
const T_ATTN_RES_SCORE: i64 = 6;
const T_FFN_RES_SCORE: i64 = 7;
const T_SSM_CONV_Q: i64 = 8;
const T_SSM_CONV_K: i64 = 9;
const T_SSM_CONV_V: i64 = 10;
const T_ATTN_QKV: i64 = 11;
const T_SSM_F_A: i64 = 12;
const T_SSM_F_B: i64 = 13;
const T_SSM_BETA: i64 = 14;
const T_SSM_A: i64 = 15;
const T_SSM_DT_B: i64 = 16;
const T_SSM_G: i64 = 17;
const T_SSM_O_NORM: i64 = 18;
const T_ATTN_OUT: i64 = 19;
const T_WQ_A: i64 = 20;
const T_ATTN_Q_A_NORM: i64 = 21;
const T_WQ_B: i64 = 22;
const T_ATTN_Q: i64 = 23;
const T_WKV_A_MQA: i64 = 24;
const T_ATTN_KV_A_NORM: i64 = 25;
const T_WKV_B: i64 = 26;
const T_WK_B: i64 = 27;
const T_WV_B: i64 = 28;
const T_ATTN_GATE: i64 = 29;
const T_FFN_GATE: i64 = 30;
const T_FFN_DOWN: i64 = 31;
const T_FFN_UP: i64 = 32;
const T_FFN_GATE_INP: i64 = 33;
const T_FFN_EXP_PROBS_B: i64 = 34;
const T_FFN_GATE_EXPS: i64 = 35;
const T_FFN_DOWN_EXPS: i64 = 36;
const T_FFN_UP_EXPS: i64 = 37;
const T_FFN_ROUTED_DOWN: i64 = 38;
const T_FFN_ROUTED_UP: i64 = 39;
const T_FFN_ROUTED_NORM: i64 = 40;
const T_FFN_GATE_SHEXP: i64 = 41;
const T_FFN_DOWN_SHEXP: i64 = 42;
const T_FFN_UP_SHEXP: i64 = 43;
const T_LEN: i64 = 44;

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

// mk_is_recr: kimi-k3.cpp:31-33, `is_recr_impl[i] = n_head_kv(i) == 0`.
// The KDA layers are the recurrent ones; a positive count marks MLA.
export fn mk_is_recr(n_head_kv: i64) -> i64 {
    if n_head_kv == 0 {
        return 1;
    }
    return 0;
}

// mk_attn_kind: the same marker as a kind. kimi-k3.cpp:297 branches on
// `hparams.is_recr(il)` and never reads a GQA width: MLA keeps one latent KV
// row per token, so n_head_kv is not a grouped-query divisor here.
export fn mk_attn_kind(n_head_kv: i64) -> i64 {
    if n_head_kv == 0 {
        return A_KDA;
    }
    return A_MLA;
}

// mk_gqa_ok: the marker still has to be sane for an MLA layer. A zero count
// would select KDA, and the only accepted positive value is one that divides
// the query head count if a future conversion reuses this field for groups.
export fn mk_gqa_ok(n_head: i64, n_head_kv: i64) -> i64 {
    e_ensure(n_head >= 0, 10);
    if n_head_kv <= 0 {
        return 0;
    }
    if n_head_kv > n_head {
        return 0;
    }
    if n_head % n_head_kv == 0 {
        return 1;
    }
    return 0;
}

// mk_kv_rows: the KV cache rows one token occupies. MLA stores the compressed
// latent (one row), KDA stores no KV cache at all because its state is the
// recurrent matrix; -1 reports "no KV rows" instead of a fake zero.
export fn mk_kv_rows(attn_kind: i64) -> i64 {
    if attn_kind == A_MLA {
        return 1;
    }
    if attn_kind == A_KDA {
        return -1;
    }
    return -1;
}

// mk_layer_kind: the FFN side of the plan. Blocks below n_layer_dense_lead run
// the dense situ FFN (kimi-k3.cpp:315), the rest run the latent MoE (:321).
// il >= n_layer is only reported as MTP for callers that re-enable it; this
// architecture never does, see mk_mtp_supported.
export fn mk_layer_kind(p: i64[:], il: i64) -> i64 {
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
    if il < p[P_DENSE_LEAD] {
        return K_DENSE;
    }
    return K_MOE;
}

// mk_mtp_supported: kimi-k3.cpp has no nextn tensors and no MTP pass. The
// generic loop at llama-model.cpp:1398 can still read nextn.predict_layers
// from a file, but this family's loader (:31, :67) and graph (:277) iterate
// n_layer only, so an extra block would be loaded by nobody.
export fn mk_mtp_supported() -> i64 {
    return 0;
}

// mk_plan_ok: the trunk must equal the declared block count. A declared nextn
// layer is rejected rather than silently dropped, which is the only honest
// answer while mk_mtp_supported is 0.
export fn mk_plan_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    if mk_mtp_supported() == 0 && p[P_N_LAYER_ALL] != p[P_N_LAYER] {
        return 0;
    }
    return 1;
}

// mk_plan: one kind per declared block. The caller checks mk_plan_ok first;
// this function refuses to plan an MTP block while the family has none.
export fn mk_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 14);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        let k = mk_layer_kind(p, i);
        if k == K_MTP && mk_mtp_supported() == 0 {
            fail(15);
        }
        kinds[i] = k;
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// mk_has_res: kimi-k3.cpp:63-76, the res-score tensors exist only when
// attn_res_block_size is set.
export fn mk_has_res(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if p[P_RES_BLOCK] > 0 {
        return 1;
    }
    return 0;
}

// mk_res_push: kimi-k3.cpp:288, `(uint32_t) il % res_bs == 0`. The raw layer
// input is banked at these layers, before the attention output is added.
export fn mk_res_push(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    if il < 0 || p[P_RES_BLOCK] == 0 {
        return 0;
    }
    if il % p[P_RES_BLOCK] == 0 {
        return 1;
    }
    return 0;
}

// mk_res_points: the number of banked checkpoints over the trunk. Layer 0
// banks because 0 % bs == 0, so the count is (n_layer-1)/bs + 1.
export fn mk_res_points(n_layer: i64, res_bs: i64) -> i64 {
    e_ensure(n_layer >= 0, 18);
    if res_bs <= 0 || n_layer == 0 {
        return 0;
    }
    return (n_layer - 1) / res_bs + 1;
}

// mk_res_scores_width: kimi-k3.cpp:221, the softmax runs over the banked
// checkpoints plus the current stream.
export fn mk_res_scores_width(n_ckpt: i64) -> i64 {
    e_ensure(n_ckpt >= 0, 19);
    return n_ckpt + 1;
}

// mk_res_mix_active: kimi-k3.cpp:200-202, res_mix returns the input untouched
// while no checkpoint exists, which is the first layer.
export fn mk_res_mix_active(p: i64[:], n_ckpt: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 20);
    if mk_has_res(p) == 1 && n_ckpt > 0 {
        return 1;
    }
    return 0;
}

// mk_embd_latent: kimi-k3.cpp:56 and :275, an unset latent length means the
// experts run at the full hidden width.
export fn mk_embd_latent(n_embd: i64, n_latent: i64) -> i64 {
    e_ensure(n_embd > 0 && n_latent >= 0, 21);
    if n_latent > 0 {
        return n_latent;
    }
    return n_embd;
}

// mk_shexp_width: kimi-k3.cpp:161, `n_ff_exp * (n_expert_shared > 0 ?
// n_expert_shared : 1)`. The tensors stay optional even at a zero count.
export fn mk_shexp_width(n_ff_exp_v: i64, n_shared: i64) -> i64 {
    e_ensure(n_ff_exp_v >= 0 && n_shared >= 0, 22);
    if n_shared > 0 {
        return n_ff_exp_v * n_shared;
    }
    return n_ff_exp_v;
}

// mk_d_inner: kimi-k3.cpp:80, `head_dim * n_head` over the KDA head size.
export fn mk_d_inner(n_head: i64, head_dim: i64) -> i64 {
    e_ensure(n_head >= 0 && head_dim >= 0, 23);
    return n_head * head_dim;
}

// mk_qkv_width: create_tensor_qkv sums the q, k and v widths. K3 passes
// d_inner for each of them (kimi-k3.cpp:92).
export fn mk_qkv_width(q: i64, k: i64, v: i64) -> i64 {
    e_ensure(q >= 0 && k >= 0 && v >= 0, 24);
    return q + k + v;
}

// mk_nope: kimi-k3.cpp:112, `n_embd_head_k - qk_rope_head_dim`. The MLA is
// nope-only, so this width is only the share that is not rotary-tagged.
export fn mk_nope(head_k: i64, n_rot: i64) -> i64 {
    e_ensure(head_k >= 0 && n_rot >= 0, 25);
    e_ensure(head_k >= n_rot, 26);
    return head_k - n_rot;
}

// mk_kv_per_head: kimi-k3.cpp:536, `n_embd_head_qk_nope + n_embd_head_v_mla`,
// the per-head row of the unabsorbed wkv_b projection.
export fn mk_kv_per_head(nope: i64, head_v: i64) -> i64 {
    e_ensure(nope >= 0 && head_v >= 0, 27);
    return nope + head_v;
}

// mk_wkv_a_width: kimi-k3.cpp:124, the joint latent plus rope projection.
export fn mk_wkv_a_width(kv_lora: i64, n_rot: i64) -> i64 {
    e_ensure(kv_lora >= 0 && n_rot >= 0, 28);
    return kv_lora + n_rot;
}

// mk_gating_ok: llama-graph.cpp:2227-2249, build_moe_ffn aborts on any gating
// function outside the four it implements. kimi-k3 does not restrict it
// further (unlike deepseek4's sqrtsoftplus-only rule).
export fn mk_gating_ok(func: i64) -> i64 {
    if func >= 0 && func <= 3 {
        return 1;
    }
    return 0;
}

// mk_situ_up: kimi-k3.cpp:180, `linear_beta > 0.0f` enables the tanh
// transform of the up branch. The comparison is on the float bit pattern:
// every positive float has a positive signed bit pattern and zero is zero.
export fn mk_situ_up(linear_beta_bits: i64) -> i64 {
    if linear_beta_bits > 0 {
        return 1;
    }
    return 0;
}

// mk_kda_gate_low: kimi-k3.cpp:428-437. The loader defaults the bound to
// -INFINITY; a present key swaps the softplus decay for the lower-bound
// sigmoid, so "found" is exactly the branch decision.
export fn mk_kda_gate_low(found: i64) -> i64 {
    if found != 0 {
        return 1;
    }
    return 0;
}

// mk_shape: shape of one tensor. Returns 1 and fills out[0..2] when the plan
// kind and the attention kind own the tensor, 0 otherwise.
export fn mk_shape(p: i64[:], ffexp: i64[:], kind: i64, attn: i64, il: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 30);
    e_ensure(out.len >= 3, 31);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let head_dim = p[P_EMBD_KDA];
    let d_conv = p[P_SSM_CONV];
    let d_inner = mk_d_inner(n_head, head_dim);
    let head_k = p[P_HEAD_K_MLA];
    let head_v = p[P_HEAD_V_MLA];
    let n_rot = p[P_N_ROT];
    let nope = mk_nope(head_k, n_rot);
    let kv_lora = p[P_KV_LORA];
    let q_lora = p[P_Q_LORA];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];
    let embd_latent = mk_embd_latent(n_embd, p[P_EXPERT_LATENT]);

    // the per-layer expert width: the file carries one value per block
    var n_ff_exp_v = p[P_N_FF_EXP];
    if il >= 0 && il < ffexp.len {
        n_ff_exp_v = ffexp[il];
    }
    n_ff_exp_v = n_ff_exp(n_ff_exp_v, p[P_N_FF], used);
    let n_ff_shexp_v = mk_shexp_width(n_ff_exp_v, p[P_N_SHEXP]);

    // globals
    if tid == T_TOKEN_EMBD || tid == T_OUTPUT {
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_OUTPUT_RES_SCORE {
        if mk_has_res(p) == 0 {
            return 0;
        }
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    if kind != K_DENSE && kind != K_MOE && kind != K_MTP {
        return 0;
    }
    if attn != A_KDA && attn != A_MLA {
        return 0;
    }

    // per-layer norms and the cross-layer residual score vectors
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_RES_SCORE || tid == T_FFN_RES_SCORE {
        if mk_has_res(p) == 0 {
            return 0;
        }
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    // dense lead FFN (kimi-k3.cpp:139-142), situ activation is an op choice
    if kind == K_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, p[P_N_FF], 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            e_put3(out, p[P_N_FF], n_embd, 1);
            return 1;
        }
    }

    // latent MoE (kimi-k3.cpp:143-165)
    if kind == K_MOE || kind == K_MTP {
        if tid == T_FFN_GATE_INP {
            e_put3(out, n_embd, n_expert, 1);
            return 1;
        }
        if tid == T_FFN_EXP_PROBS_B {
            e_put3(out, n_expert, 1, 1);
            return 1;
        }
        // routed experts read the latent state, the router the full one
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
            e_put3(out, embd_latent, n_ff_exp_v, n_expert);
            return 1;
        }
        if tid == T_FFN_DOWN_EXPS {
            e_put3(out, n_ff_exp_v, embd_latent, n_expert);
            return 1;
        }
        if p[P_EXPERT_LATENT] > 0 {
            if tid == T_FFN_ROUTED_DOWN {
                e_put3(out, n_embd, embd_latent, 1);
                return 1;
            }
            if tid == T_FFN_ROUTED_UP {
                e_put3(out, embd_latent, n_embd, 1);
                return 1;
            }
            if tid == T_FFN_ROUTED_NORM {
                e_put3(out, embd_latent, 1, 1);
                return 1;
            }
        }
        // shared experts stay at n_embd and are optional in the file
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            e_put3(out, n_embd, n_ff_shexp_v, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            e_put3(out, n_ff_shexp_v, n_embd, 1);
            return 1;
        }
    }

    // KDA layer (kimi-k3.cpp:82-105)
    if attn == A_KDA {
        if tid == T_SSM_CONV_Q || tid == T_SSM_CONV_K || tid == T_SSM_CONV_V {
            // 4D when stored whole, 3D after quantization drops the last 1
            e_put3(out, d_conv, 1, d_inner);
            return 1;
        }
        if tid == T_ATTN_QKV {
            e_put3(out, n_embd, mk_qkv_width(d_inner, d_inner, d_inner), 1);
            return 1;
        }
        if tid == T_SSM_F_A {
            e_put3(out, n_embd, head_dim, 1);
            return 1;
        }
        if tid == T_SSM_F_B {
            e_put3(out, head_dim, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_BETA {
            e_put3(out, n_embd, n_head, 1);
            return 1;
        }
        if tid == T_SSM_A {
            // plain 1-D [n_head], unlike kimi-linear's padded tensor
            e_put3(out, n_head, 1, 1);
            return 1;
        }
        if tid == T_SSM_DT_B {
            e_put3(out, d_inner, 1, 1);
            return 1;
        }
        if tid == T_SSM_G {
            // single full-rank gate, no g_a/g_b pair
            e_put3(out, n_embd, d_inner, 1);
            return 1;
        }
        if tid == T_SSM_O_NORM {
            e_put3(out, head_dim, 1, 1);
            return 1;
        }
        if tid == T_ATTN_OUT {
            e_put3(out, d_inner, n_embd, 1);
            return 1;
        }
    }

    // MLA layer (kimi-k3.cpp:106-137)
    if attn == A_MLA {
        if tid == T_WQ_A {
            if q_lora <= 0 {
                return 0;
            }
            e_put3(out, n_embd, q_lora, 1);
            return 1;
        }
        if tid == T_ATTN_Q_A_NORM {
            if q_lora <= 0 {
                return 0;
            }
            e_put3(out, q_lora, 1, 1);
            return 1;
        }
        if tid == T_WQ_B {
            if q_lora <= 0 {
                return 0;
            }
            e_put3(out, q_lora, n_head * head_k, 1);
            return 1;
        }
        if tid == T_ATTN_Q {
            if q_lora > 0 {
                return 0;
            }
            e_put3(out, n_embd, n_head * head_k, 1);
            return 1;
        }
        if tid == T_WKV_A_MQA {
            e_put3(out, n_embd, mk_wkv_a_width(kv_lora, n_rot), 1);
            return 1;
        }
        if tid == T_ATTN_KV_A_NORM {
            e_put3(out, kv_lora, 1, 1);
            return 1;
        }
        if tid == T_WKV_B {
            e_put3(out, kv_lora, n_head * mk_kv_per_head(nope, head_v), 1);
            return 1;
        }
        // the absorbed fallback pair the loader declares when wkv_b is absent
        if tid == T_WK_B {
            e_put3(out, nope, kv_lora, n_head);
            return 1;
        }
        if tid == T_WV_B {
            e_put3(out, kv_lora, head_v, n_head);
            return 1;
        }
        if tid == T_ATTN_GATE {
            e_put3(out, n_embd, n_head * head_v, 1);
            return 1;
        }
        if tid == T_ATTN_OUT {
            e_put3(out, n_head * head_v, n_embd, 1);
            return 1;
        }
    }

    return 0;
}
