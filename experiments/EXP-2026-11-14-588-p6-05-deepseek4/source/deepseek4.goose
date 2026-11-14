// P6-05 architecture 9: deepseek4 (DeepSeek-V4 family) builder preparation.
//
// Decision/shape half, not the graph emitter. It reuses the accepted shared
// slice in base/model_graph.goose (goose/model/goose-model-graph.goose minus
// its `fn main`): n_ff_exp, n_ff_shexp.
//
// Native anchors (src/models/deepseek4.cpp):
//   hparams: MTP probe, compress ratios, HC keys, indexer keys :15-113
//   global tensors, HC head                                  :127-134
//   per-layer attention tensors (MLA, o-groups)              :137-152
//   per-layer HC modules (attn and ffn)                      :154-159
//   compressor + indexer tensors                             :161-182
//   router, hash routing / score bias                        :184-190
//   routed + shared experts                                  :193-203
//   MTP nextn tensors                                        :205-212
//   attention variant choice (raw/CSA+HCA/sinks)             :1226-1243
//   graph trunk loop (all layers are MoE)                    :1320-1440
//   MTP graph, hash-routing boundary                         :1444-1560
//
// deepseek4 differs from the MoE families (qwen3moe/qwen2moe/glm4-moe) in the
// ways the plan and shape table must keep:
//   - there is no dense FFN layer at all: routed experts plus one shared
//     expert exist on every trunk and MTP layer,
//   - attention is MLA: a low-rank query (wq_a/wq_b), a single KV head with
//     its own norm, and a grouped low-rank output (wo_a/wo_b),
//   - the attention variant per layer is chosen by the compress ratio read
//     from the model file: 0 raw/local, 4 CSA plus a lightning indexer,
//     128 HCA; any other value aborts in the loader,
//   - every attention variant carries per-head sinks,
//   - hyper-connections run on every layer twice (attn and ffn), and the
//     builder asserts hc == 4,
//   - the compressor is a gated, overlapped convolution over n_embd_head
//     (coff = 2 for ratio 4, coff = 1 for ratio 128),
//   - layers below hash_layer_count route by token id (tid2eid) instead of a
//     score bias, and the MTP block refuses to be hash-routed,
//   - the MTP hidden state is hc*n_embd wide, so eh_proj reads 2*n_embd.

import std;
import base.model_graph;

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;
const P_N_LAYER_ALL: i64 = 3;
const P_N_HEAD: i64 = 4;
const P_EMBD_HEAD: i64 = 5;
const P_N_ROT: i64 = 6;
const P_Q_LORA: i64 = 7;
const P_O_GROUPS: i64 = 8;
const P_O_LORA: i64 = 9;
const P_N_EXPERT: i64 = 10;
const P_N_EXPERT_USED: i64 = 11;
const P_N_FF_EXP: i64 = 12;
const P_N_SHEXP: i64 = 13;
const P_IDX_NHEAD: i64 = 14;
const P_IDX_HEAD: i64 = 15;
const P_IDX_TOP_K: i64 = 16;
const P_HC: i64 = 17;
const P_HASH_LAYERS: i64 = 18;
const P_N_SWA: i64 = 19;
const P_GATING: i64 = 20;
const P_MTP_PROBE: i64 = 21;
const P_NEXTN_DECLARED: i64 = 22;
const P_LEN: i64 = 23;

// layer kinds (attention variant); MTP is a separate kind, not a ratio
const K_RAW: i64 = 0;
const K_CSA: i64 = 1;
const K_HCA: i64 = 2;
const K_MTP: i64 = 3;

// compressor ratio constants and the resulting overlap coefficient
const R_RAW: i64 = 0;
const R_CSA: i64 = 4;
const R_HCA: i64 = 128;

// routing kinds
const ROUTE_HASH: i64 = 0;
const ROUTE_SCORE: i64 = 1;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_HC_HEAD_FN: i64 = 3;
const T_HC_HEAD_BASE: i64 = 4;
const T_HC_HEAD_SCALE: i64 = 5;
const T_ATTN_NORM: i64 = 6;
const T_ATTN_SINKS: i64 = 7;
const T_WQ_A: i64 = 8;
const T_ATTN_Q_A_NORM: i64 = 9;
const T_WQ_B: i64 = 10;
const T_WKV: i64 = 11;
const T_ATTN_KV_NORM: i64 = 12;
const T_WO_A: i64 = 13;
const T_WO_B: i64 = 14;
const T_HC_ATTN_FN: i64 = 15;
const T_HC_ATTN_BASE: i64 = 16;
const T_HC_ATTN_SCALE: i64 = 17;
const T_HC_FFN_FN: i64 = 18;
const T_HC_FFN_BASE: i64 = 19;
const T_HC_FFN_SCALE: i64 = 20;
const T_FFN_GATE_INP: i64 = 21;
const T_FFN_GATE_TID2EID: i64 = 22;
const T_FFN_EXP_PROBS_B: i64 = 23;
const T_FFN_EXP_PROBS_B_VL: i64 = 24;
const T_FFN_NORM: i64 = 25;
const T_FFN_GATE_EXPS: i64 = 26;
const T_FFN_DOWN_EXPS: i64 = 27;
const T_FFN_UP_EXPS: i64 = 28;
const T_FFN_GATE_SHEXP: i64 = 29;
const T_FFN_DOWN_SHEXP: i64 = 30;
const T_FFN_UP_SHEXP: i64 = 31;
const T_COMP_WKV: i64 = 32;
const T_COMP_WGATE: i64 = 33;
const T_COMP_APE: i64 = 34;
const T_COMP_NORM: i64 = 35;
const T_INDEXER_PROJ: i64 = 36;
const T_INDEXER_ATTN_Q_B: i64 = 37;
const T_INDEXER_COMP_WKV: i64 = 38;
const T_INDEXER_COMP_WGATE: i64 = 39;
const T_INDEXER_COMP_APE: i64 = 40;
const T_INDEXER_COMP_NORM: i64 = 41;
const T_NEXTN_EH_PROJ: i64 = 42;
const T_NEXTN_ENORM: i64 = 43;
const T_NEXTN_HNORM: i64 = 44;
const T_NEXTN_EMBED: i64 = 45;
const T_NEXTN_SHARED_HEAD: i64 = 46;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 47;
const T_LEN: i64 = 48;

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

// ds4_ratio_kind: deepseek4.cpp:161-181. The loader accepts exactly 0, 4 and
// 128; a value in between (the 16/64 cases of other families) aborts with
// "only supports compression ratios 0, 4, and 128". -1 marks the abort.
export fn ds4_ratio_kind(ratio: i64) -> i64 {
    if ratio == R_RAW {
        return K_RAW;
    }
    if ratio == R_CSA {
        return K_CSA;
    }
    if ratio == R_HCA {
        return K_HCA;
    }
    return -1;
}

// ds4_coff: deepseek4.cpp:163, `const int64_t coff = ratio == 4 ? 2 : 1`.
// The CSA compressor overlaps two blocks so its projections are twice as wide
// as one head; HCA compresses one block at a time. Ratio 0 owns no compressor
// at all, which is reported as 0 rather than 1 so callers cannot build shapes
// for a layer that has none.
export fn ds4_coff(ratio: i64) -> i64 {
    if ratio == R_RAW {
        return 0;
    }
    if ratio == R_CSA {
        return 2;
    }
    if ratio == R_HCA {
        return 1;
    }
    return -1;
}

// ds4_has_indexer: deepseek4.cpp:171. The lightning indexer exists only on the
// CSA layers (ratio 4); HCA layers compress without an indexer.
export fn ds4_has_indexer(ratio: i64) -> i64 {
    if ratio == R_CSA {
        return 1;
    }
    return 0;
}

// ds4_layer_kind: the plan decision for one layer. Trunk layers take their
// kind from the model-file compress ratio, layers at and past n_layer are the
// MTP block. Native builds trunk MTP from the same tensors but a separate
// graph (deepseek4.cpp:205-212, :1444).
export fn ds4_layer_kind(p: i64[:], ratios: i64[:], il: i64) -> i64 {
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
    e_ensure(ratios.len >= p[P_N_LAYER_ALL], 11);
    return ds4_ratio_kind(ratios[il]);
}

// ds4_plan: fills one kind per layer. Mirrors two loader checks:
//   - compress_ratios shorter than block_count throws (deepseek4.cpp:77-79),
//   - an unknown ratio aborts (:179-180),
// and the MTP attention assert `inp_dsv4 || ratio == 0` (:940) is reported as
// ds4_mtp_ratio_ok instead of aborting, so the gate can hold both directions.
export fn ds4_plan(p: i64[:], ratios: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    e_ensure(ratios.len >= p[P_N_LAYER_ALL], 13);
    e_ensure(kinds.len >= p[P_N_LAYER_ALL], 14);
    var i: i64 = 0;
    while i < p[P_N_LAYER_ALL] {
        let k = ds4_layer_kind(p, ratios, i);
        e_ensure(k >= 0, 15);
        kinds[i] = k;
        i = i + 1;
    }
    return p[P_N_LAYER_ALL];
}

// ds4_ffn_kind: every layer, trunk or MTP, builds routed experts and one
// shared expert (deepseek4.cpp:1385-1402, :1520-1538); there is no dense FFN
// branch in this architecture, so a dense layer is impossible. Returns 1 (MoE)
// for a valid layer and -1 outside the model.
export fn ds4_ffn_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if il < 0 || il >= p[P_N_LAYER_ALL] {
        return -1;
    }
    return 1;
}

// ds4_route_kind: deepseek4.cpp:1371-1376. Layers below hash_layer_count take
// their expert ids from ffn_gate_tid2eid (token-id lookup, no score bias);
// every other layer routes on scores with ffn_exp_probs_b.
export fn ds4_route_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    if il >= 0 && il < p[P_HASH_LAYERS] {
        return ROUTE_HASH;
    }
    return ROUTE_SCORE;
}

// ds4_mtp_route_ok: deepseek4.cpp:1509, the MTP graph asserts that the block
// is not hash-routed, `il >= hash_layer_count`.
export fn ds4_mtp_route_ok(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 18);
    if il >= p[P_HASH_LAYERS] {
        return 1;
    }
    return 0;
}

// ds4_swa: llama-hparams.cpp:8-22 plus the deepseek4 override at :101-105.
// set_swa_pattern(0) marks every trunk layer SWA (n_pattern == 0 is true for
// all of them), the loop that follows clears the MTP layers, and deepseek4
// then sets them back to SWA explicitly. n_pattern is a parameter so the
// generic rule stays visible.
export fn ds4_swa(il: i64, n_layer: i64, n_pattern: i64) -> i64 {
    e_ensure(il >= 0 && n_layer >= 0, 19);
    if il < n_layer {
        if n_pattern == 0 {
            return 1;
        }
        if il % n_pattern != 0 {
            return 1;
        }
        return 0;
    }
    return 1;
}

// ds4_nextn_count: deepseek4.cpp:15-21. A declared MTP block survives only if
// `blk.<n_layer_main>.nextn.eh_proj.weight` exists in the file; otherwise the
// loader zeroes n_layer_nextn and the extra layers are never planned as MTP.
export fn ds4_nextn_count(declared: i64, probe_found: i64) -> i64 {
    if declared > 0 && probe_found == 0 {
        return 0;
    }
    if declared < 0 {
        return 0;
    }
    return declared;
}

// ds4_out_width: deepseek4.cpp:73, `n_embd_out_impl = hc_mult * n_embd`.
export fn ds4_out_width(n_embd: i64, hc: i64) -> i64 {
    e_ensure(n_embd > 0 && hc > 0, 20);
    return hc * n_embd;
}

// ds4_out_width_effective: llama-hparams.cpp:136-138, an unset impl falls back
// to n_embd.
export fn ds4_out_width_effective(impl: i64, n_embd: i64) -> i64 {
    if impl > 0 {
        return impl;
    }
    return n_embd;
}

// ds4_out_matches: the MTP assert at deepseek4.cpp:1451,
// `n_embd_out() == n_embd*hc`.
export fn ds4_out_matches(out_width: i64, n_embd: i64, hc: i64) -> i64 {
    if out_width == n_embd * hc {
        return 1;
    }
    return 0;
}

// ds4_hc_mix_dim: deepseek4.cpp:125, `hc_mix_dim = (2 + hc)*hc` -- two
// hc-wide affine rows (pre, post) plus the hc*hc mixing matrix.
export fn ds4_hc_mix_dim(hc: i64) -> i64 {
    e_ensure(hc > 0, 21);
    return (2 + hc) * hc;
}

// ds4_hc_supported: deepseek4.cpp:372, `GGML_ASSERT(hc == 4)`. The Sinkhorn
// mixer and the flat RMS norm are only written for four streams.
export fn ds4_hc_supported(hc: i64) -> i64 {
    if hc == 4 {
        return 1;
    }
    return 0;
}

// ds4_hc_head_fn: deepseek4.cpp:131, the head mixer reads hc*n_embd flattened
// and emits hc values, so its weight is [hc*n_embd, hc].
export fn ds4_hc_head_fn(n_embd: i64, hc: i64) -> i64 {
    e_ensure(n_embd > 0 && hc > 0, 22);
    return hc * n_embd;
}

// ds4_o_groups_ok / ds4_o_group_dim: deepseek4.cpp:884-888,
// `n_head % n_groups == 0` and `o_group_dim = (n_head/n_groups)*n_embd_head`.
export fn ds4_o_groups_ok(n_head: i64, n_groups: i64) -> i64 {
    e_ensure(n_head >= 0 && n_groups >= 0, 23);
    if n_groups == 0 {
        return 0;
    }
    if n_head % n_groups == 0 {
        return 1;
    }
    return 0;
}

export fn ds4_o_group_dim(n_head: i64, n_groups: i64, n_embd_head: i64) -> i64 {
    e_ensure(n_head >= 0 && n_groups > 0 && n_embd_head >= 0, 24);
    return (n_head / n_groups) * n_embd_head;
}

// ds4_head_split_ok: deepseek4.cpp:889, `n_embd_head >= n_embd_head_rope`; the
// rotary part is a prefix of the head and the rest (nope) is untouched.
export fn ds4_head_split_ok(n_embd_head: i64, n_rot: i64) -> i64 {
    if n_embd_head >= n_rot && n_rot >= 0 {
        return 1;
    }
    return 0;
}

// ds4_indexer_top_k: deepseek4.cpp:653,
// `n_top_k = min(indexer_score ne0, indexer_top_k)`.
export fn ds4_indexer_top_k(score_ne0: i64, top_k: i64) -> i64 {
    e_ensure(score_ne0 >= 0 && top_k >= 0, 25);
    if score_ne0 < top_k {
        return score_ne0;
    }
    return top_k;
}

// ds4_gating_ok: deepseek4.cpp:86-89, the loader refuses any MoE scoring other
// than sqrtsoftplus, so gating function 3 is the only legal value here.
export fn ds4_gating_ok(func: i64) -> i64 {
    if func == 3 {
        return 1;
    }
    return 0;
}

// ds4_clamp_shexp: deepseek4.cpp:38-40, `swiglu_clamp_shexp` defaults to
// `swiglu_clamp_exp` when the key is absent. Only the fallback is a builder
// decision; the magnitude itself is a float consumed by the fused op.
export fn ds4_clamp_shexp(found: i64, declared: i64, exp: i64) -> i64 {
    if found == 0 {
        return exp;
    }
    return declared;
}

// ds4_mtp_ratio_ok: deepseek4.cpp:940, the MTP pass has no dsv4 input, so a
// non-zero compress ratio there is an assert.
export fn ds4_mtp_ratio_ok(ratio: i64) -> i64 {
    if ratio == 0 {
        return 1;
    }
    return 0;
}

// ds4_sinks_width: deepseek4.cpp:139, one sink per query head, carried by
// every attention variant (raw, CSA+LID, HCA).
export fn ds4_sinks_width(n_head: i64) -> i64 {
    e_ensure(n_head > 0, 26);
    return n_head;
}

// ds4_shape: shape of one tensor for a layer kind. Returns 1 and fills
// out[0..2] when the kind owns the tensor, 0 otherwise.
export fn ds4_shape(p: i64[:], kind: i64, il: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 30);
    e_ensure(out.len >= 3, 31);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let n_embd_head = p[P_EMBD_HEAD];
    let q_lora = p[P_Q_LORA];
    let n_expert = p[P_N_EXPERT];
    let used = p[P_N_EXPERT_USED];
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], 0, used);
    let n_ff_shexp_v = n_ff_exp_v * p[P_N_SHEXP];

    let hc = p[P_HC];
    let hc_dim = hc * n_embd;
    let hc_mix = ds4_hc_mix_dim(hc);

    let n_groups = p[P_O_GROUPS];
    let o_lora = p[P_O_LORA];
    let o_group_dim = ds4_o_group_dim(n_head, n_groups, n_embd_head);

    if tid == T_TOKEN_EMBD || tid == T_OUTPUT {
        // output may be absent; an absent output reuses token_embd, same shape
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_HC_HEAD_FN {
        e_put3(out, hc_dim, hc, 1);
        return 1;
    }
    if tid == T_HC_HEAD_BASE {
        e_put3(out, hc, 1, 1);
        return 1;
    }
    if tid == T_HC_HEAD_SCALE {
        e_put3(out, 1, 1, 1);
        return 1;
    }

    if kind != K_RAW && kind != K_CSA && kind != K_HCA && kind != K_MTP {
        return 0;
    }

    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_SINKS {
        e_put3(out, ds4_sinks_width(n_head), 1, 1);
        return 1;
    }
    if tid == T_WQ_A {
        e_put3(out, n_embd, q_lora, 1);
        return 1;
    }
    if tid == T_ATTN_Q_A_NORM {
        e_put3(out, q_lora, 1, 1);
        return 1;
    }
    if tid == T_WQ_B {
        e_put3(out, q_lora, n_head * n_embd_head, 1);
        return 1;
    }
    if tid == T_WKV {
        // MLA keeps a single KV head; the latent itself is one head wide
        e_put3(out, n_embd, n_embd_head, 1);
        return 1;
    }
    if tid == T_ATTN_KV_NORM {
        e_put3(out, n_embd_head, 1, 1);
        return 1;
    }
    if tid == T_WO_A {
        // the file stores (n_head*n_embd_head/o_groups, o_lora_rank*o_groups)
        // and the loader reshapes it into three dimensions
        e_put3(out, o_group_dim, o_lora, n_groups);
        return 1;
    }
    if tid == T_WO_B {
        e_put3(out, n_groups * o_lora, n_embd, 1);
        return 1;
    }
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

    // compressor: CSA and HCA only, never raw and never MTP
    if kind == K_CSA || kind == K_HCA {
        let ratio = if kind == K_CSA { R_CSA } else { R_HCA };
        let coff = ds4_coff(ratio);
        if tid == T_COMP_WKV || tid == T_COMP_WGATE {
            e_put3(out, n_embd, coff * n_embd_head, 1);
            return 1;
        }
        if tid == T_COMP_APE {
            e_put3(out, coff * n_embd_head, ratio, 1);
            return 1;
        }
        if tid == T_COMP_NORM {
            e_put3(out, n_embd_head, 1, 1);
            return 1;
        }
    }

    // lightning indexer: CSA only
    if kind == K_CSA {
        let idx_head = p[P_IDX_HEAD];
        if tid == T_INDEXER_PROJ {
            e_put3(out, n_embd, p[P_IDX_NHEAD], 1);
            return 1;
        }
        if tid == T_INDEXER_ATTN_Q_B {
            e_put3(out, q_lora, p[P_IDX_NHEAD] * idx_head, 1);
            return 1;
        }
        if tid == T_INDEXER_COMP_WKV || tid == T_INDEXER_COMP_WGATE {
            e_put3(out, n_embd, 2 * idx_head, 1);
            return 1;
        }
        if tid == T_INDEXER_COMP_APE {
            e_put3(out, 2 * idx_head, R_CSA, 1);
            return 1;
        }
        if tid == T_INDEXER_COMP_NORM {
            e_put3(out, idx_head, 1, 1);
            return 1;
        }
    }

    // router and experts
    if tid == T_FFN_GATE_INP {
        e_put3(out, n_embd, n_expert, 1);
        return 1;
    }
    if tid == T_FFN_GATE_TID2EID {
        if ds4_route_kind(p, il) != ROUTE_HASH {
            return 0;
        }
        e_put3(out, used, n_vocab, 1);
        return 1;
    }
    if tid == T_FFN_EXP_PROBS_B {
        if ds4_route_kind(p, il) != ROUTE_SCORE {
            return 0;
        }
        e_put3(out, n_expert, 1, 1);
        return 1;
    }
    if tid == T_FFN_EXP_PROBS_B_VL {
        // optional vision bias, declared for every layer and not required
        e_put3(out, n_expert, 1, 1);
        return 1;
    }
    if tid == T_FFN_GATE_EXPS {
        e_put3(out, n_embd, n_ff_exp_v, n_expert);
        return 1;
    }
    if tid == T_FFN_DOWN_EXPS {
        e_put3(out, n_ff_exp_v, n_embd, n_expert);
        return 1;
    }
    if tid == T_FFN_UP_EXPS {
        e_put3(out, n_embd, n_ff_exp_v, n_expert);
        return 1;
    }
    if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
        e_put3(out, n_embd, n_ff_shexp_v, 1);
        return 1;
    }
    if tid == T_FFN_DOWN_SHEXP {
        e_put3(out, n_ff_shexp_v, n_embd, 1);
        return 1;
    }

    // MTP block tensors
    if kind == K_MTP {
        if tid == T_NEXTN_EH_PROJ {
            // concat(e_norm, h_norm) on dim 0 of an [n_embd, hc] state
            e_put3(out, n_embd * 2, n_embd, 1);
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
