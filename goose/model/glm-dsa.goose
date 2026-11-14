// P6-05 architecture 15: glm-dsa (GLM-5.2: MLA attention, sigmoid MoE with a
// shared expert, dense lead layers, DSA lightning indexer and a NextN block).
//
// Decision/shape half of the builder, not the graph emitter. The trunk/NextN
// split comes from base/trunk.goose; the expert width fallback comes from the
// accepted base/model_graph.goose slice (n_ff_exp).
//
// Native anchors (src/models/glm-dsa.cpp):
//   hparams: MoE, MLA, DSA keys, gating default       :29-78
//   indexer types: pre-5.2 all full, 5.2 pattern      :63-70
//   MTP-only / trunk-only probes                      :84-96
//   MLA requirement (throws when not MLA)             :98-101
//   head split rope/nope                              :104-108
//   global tensors, tied output                       :115-123
//   per-layer MLA tensors                             :132-147
//   per-layer DSA indexer tensors                     :149-154
//   dense lead vs MoE + shared experts                :155-179
//   NextN tensors                                     :181-190
//   graph: indexer, top-k reuse, MLA absorption       :267-451
//   graph: dense SILU-PAR vs MoE + shexp              :465-505
//   MTP graph asserts (single block, MoE FFN)         :545-560
//
// What the shape table keeps, and what makes glm-dsa different from deepseek4
// (architecture 9), the closest accepted family:
//   - attention is MLA, but the output side is a plain projection: there is no
//     o-group / o-lora split, so wo is {n_head*n_embd_head_v_mla, n_embd} and
//     the low-rank part lives entirely in wq_a/wq_b and wkv_a_mqa/wk_b/wv_b;
//   - the first n_layer_dense_lead layers load a dense SILU-PAR FFN with three
//     separate rows; every later layer loads routed experts plus a shared
//     expert whose width is n_ff_exp * n_expert_shared (not n_ff_exp);
//   - the DSA indexer tensors are declared for every layer, dense lead layers
//     included, and are optional in the file; which layers run the indexer is
//     a separate per-layer flag (indexer types), not a shape decision;
//   - there is no compressor, no hash routing and no hyper-connection here;
//   - the NextN block is a full decoder block: it takes the dense/MoE decision
//     from its own index, exactly like a trunk layer.

import base.trunk;
import base.model_graph;

// p indices
const P_N_EMBD: i64 = 0;
const P_N_VOCAB: i64 = 1;
const P_N_LAYER: i64 = 2;      // trunk layers (n_layer)
const P_N_NEXTN: i64 = 3;      // declared NextN blocks (n_layer_nextn)
const P_N_HEAD: i64 = 4;
const P_N_ROT: i64 = 5;        // n_rot (rope dimension of the qk head)
const P_Q_LORA: i64 = 6;       // attention_q_lora_rank
const P_KV_LORA: i64 = 7;      // attention_kv_lora_rank
const P_K_MLA: i64 = 8;        // attention_key_length_mla
const P_V_MLA: i64 = 9;        // attention_value_length_mla
const P_N_FF: i64 = 10;        // dense FFN length
const P_N_FF_EXP: i64 = 11;    // expert_feed_forward_length
const P_N_EXPERT: i64 = 12;
const P_N_EXPERT_USED: i64 = 13;
const P_N_SHEXP: i64 = 14;     // expert_shared_count
const P_DENSE_LEAD: i64 = 15;  // leading_dense_block_count
const P_IDX_NHEAD: i64 = 16;
const P_IDX_HEAD: i64 = 17;
const P_IDX_TOP_K: i64 = 18;
const P_GATING: i64 = 19;      // expert_gating_func as stored in the file
const P_HAS_OUTPUT: i64 = 20;  // 0 -> output duplicates token_embd
const P_CTX_TRAIN: i64 = 21;   // used only by the indexer-type back-compat rule
const P_LEN: i64 = 22;

// FFN kinds
const F_DENSE: i64 = 0;
const F_MOE: i64 = 1;

// indexer kinds
const I_SHARED: i64 = 0;
const I_FULL: i64 = 1;

// expert gating: NONE in the file means SIGMOID for this architecture (:53-57)
const G_NONE: i64 = 0;
const G_SIGMOID: i64 = 2;

// tensor ids
const T_TOKEN_EMBD: i64 = 0;
const T_OUTPUT_NORM: i64 = 1;
const T_OUTPUT: i64 = 2;
const T_ATTN_NORM: i64 = 3;
const T_ATTN_Q_A_NORM: i64 = 4;
const T_ATTN_KV_A_NORM: i64 = 5;
const T_WQ_A: i64 = 6;
const T_WQ_B: i64 = 7;
const T_WKV_A_MQA: i64 = 8;
const T_WK_B: i64 = 9;
const T_WV_B: i64 = 10;
const T_WO: i64 = 11;
const T_FFN_NORM: i64 = 12;
const T_INDEXER_K_NORM: i64 = 13;
const T_INDEXER_K_NORM_B: i64 = 14;
const T_INDEXER_PROJ: i64 = 15;
const T_INDEXER_ATTN_K: i64 = 16;
const T_INDEXER_ATTN_Q_B: i64 = 17;
const T_FFN_GATE: i64 = 18;
const T_FFN_DOWN: i64 = 19;
const T_FFN_UP: i64 = 20;
const T_FFN_GATE_INP: i64 = 21;
const T_FFN_EXP_PROBS_B: i64 = 22;
const T_FFN_GATE_EXPS: i64 = 23;
const T_FFN_DOWN_EXPS: i64 = 24;
const T_FFN_UP_EXPS: i64 = 25;
const T_FFN_GATE_SHEXP: i64 = 26;
const T_FFN_DOWN_SHEXP: i64 = 27;
const T_FFN_UP_SHEXP: i64 = 28;
const T_NEXTN_EH_PROJ: i64 = 29;
const T_NEXTN_ENORM: i64 = 30;
const T_NEXTN_HNORM: i64 = 31;
const T_NEXTN_EMBED: i64 = 32;
const T_NEXTN_SHARED_HEAD: i64 = 33;
const T_NEXTN_SHARED_HEAD_NORM: i64 = 34;
const T_LEN: i64 = 35;

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

// dsa_n_layer_all: block_count = trunk + NextN (glm-dsa.cpp:125).
export fn dsa_n_layer_all(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 10);
    return t_n_layer_all(p[P_N_LAYER], p[P_N_NEXTN]);
}

// dsa_plan: one kind per block; trunk below n_layer, NextN above.
export fn dsa_plan(p: i64[:], kinds: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 11);
    return t_plan(p[P_N_LAYER], dsa_n_layer_all(p), kinds);
}

// dsa_kind: the plan decision for one block, -1 outside the model.
export fn dsa_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 12);
    return t_layer_kind(p[P_N_LAYER], dsa_n_layer_all(p), il);
}

// dsa_nextn_ok: glm-dsa.cpp:61, `n_layer_nextn < n_layer_all`.
export fn dsa_nextn_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 13);
    return t_nextn_ok(p[P_N_NEXTN], dsa_n_layer_all(p));
}

// dsa_nextn_single: glm-dsa.cpp:548, the MTP graph asserts
// `n_layer_nextn == 1`: exactly one draft block is supported.
export fn dsa_nextn_single(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 14);
    if p[P_N_NEXTN] == 1 {
        return 1;
    }
    return 0;
}

// dsa_ffn_kind: glm-dsa.cpp:155 compares the block index against
// n_layer_dense_lead, so the leading dense layers keep three dense rows and
// every later block, the NextN block included, loads routed experts plus the
// shared expert. -1 outside the model.
export fn dsa_ffn_kind(p: i64[:], il: i64) -> i64 {
    e_ensure(p.len >= P_LEN, 15);
    if il < 0 || il >= dsa_n_layer_all(p) {
        return -1;
    }
    if il < p[P_DENSE_LEAD] {
        return F_DENSE;
    }
    return F_MOE;
}

// dsa_mtp_ffn_ok: the loader gives the NextN block whatever the index rule
// says (:155), but the MTP graph always builds a MoE FFN and asserts
// `ffn_gate_inp` (:560). A file whose dense lead reaches the NextN block would
// load dense rows and then abort in the graph, so 1 means the two agree.
export fn dsa_mtp_ffn_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 16);
    if p[P_N_LAYER] >= p[P_DENSE_LEAD] {
        return 1;
    }
    return 0;
}

// dsa_is_mla: llama-hparams.cpp:272-277 plus the loader throw at :98-101.
// Both MLA head sizes have to be present for the architecture to load.
export fn dsa_is_mla(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 17);
    if p[P_K_MLA] > 0 && p[P_V_MLA] > 0 {
        return 1;
    }
    return 0;
}

// dsa_mla_pair_ok: the assert inside llama-hparams.cpp:273-274, either both
// sizes are unset or both are set. A half-declared pair aborts before is_mla
// is even consulted.
export fn dsa_mla_pair_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 18);
    if p[P_K_MLA] == 0 && p[P_V_MLA] == 0 {
        return 1;
    }
    if p[P_K_MLA] != 0 && p[P_V_MLA] != 0 {
        return 1;
    }
    return 0;
}

// dsa_nope_width: glm-dsa.cpp:108, n_embd_head_qk_nope = k_mla - n_rot.
export fn dsa_nope_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 19);
    e_ensure(p[P_K_MLA] >= p[P_N_ROT], 20);
    return p[P_K_MLA] - p[P_N_ROT];
}

// dsa_rope_in_indexer_ok: glm-dsa.cpp:220, `n_rot <= n_embd_indexer_head`.
export fn dsa_rope_in_indexer_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 21);
    if p[P_N_ROT] <= p[P_IDX_HEAD] {
        return 1;
    }
    return 0;
}

// dsa_indexer_default: glm-dsa.cpp:63-69. Below a 1M training context the
// loader fills is_indexer_full_impl with ones, i.e. every layer runs the full
// indexer; the 5.2 default pattern is only used for larger contexts.
export fn dsa_indexer_default(n_ctx_train: i64) -> i64 {
    if n_ctx_train < 1048576 {
        return 1;
    }
    return 0;
}

// dsa_indexer_5_2: the GLM_5_2_DEFAULT_INDEXER_TYPES table (:6-27) written as
// a rule: layers 0 and 1 are full, and from layer 2 on every fourth layer
// (il % 4 == 2) is full while the three after it reuse its top-k.
export fn dsa_indexer_5_2(il: i64) -> i64 {
    e_ensure(il >= 0, 22);
    if il < 2 {
        return I_FULL;
    }
    if il % 4 == 2 {
        return I_FULL;
    }
    return I_SHARED;
}

// dsa_indexer_kind: llama-hparams.cpp:279-285, is_indexer_full reads the
// per-layer table and aborts for il >= n_layer (the NextN block has no type).
// -1 marks that abort; 1 is a full indexer layer, 0 a shared one.
export fn dsa_indexer_kind(types: i64[:], n_layer: i64, il: i64) -> i64 {
    e_ensure(n_layer >= 0, 23);
    if il < 0 || il >= n_layer {
        return -1;
    }
    e_ensure(types.len >= n_layer, 24);
    if types[il] != 0 {
        return I_FULL;
    }
    return I_SHARED;
}

// dsa_indexer_ok: glm-dsa.cpp:370, a shared indexer layer asserts
// `prev_top_k != nullptr`, so the first layer is never allowed to be shared.
// 1 means the layer can run.
export fn dsa_indexer_ok(types: i64[:], n_layer: i64, il: i64) -> i64 {
    let kind = dsa_indexer_kind(types, n_layer, il);
    if kind == I_FULL {
        return 1;
    }
    if kind < 0 {
        return 0;
    }
    e_ensure(types.len >= n_layer, 25);
    var j: i64 = 0;
    while j < il {
        if types[j] != 0 {
            return 1;
        }
        j = j + 1;
    }
    return 0;
}

// dsa_indexer_top_k: glm-dsa.cpp:364,
// `n_top_k = min(indexer_score ne0, indexer_top_k)`.
export fn dsa_indexer_top_k(score_ne0: i64, top_k: i64) -> i64 {
    e_ensure(score_ne0 >= 0 && top_k >= 0, 26);
    if score_ne0 < top_k {
        return score_ne0;
    }
    return top_k;
}

// dsa_gating_func: glm-dsa.cpp:54-57, an absent or NONE gating function
// becomes SIGMOID; any declared value is used as stored.
export fn dsa_gating_func(declared: i64) -> i64 {
    if declared == G_NONE {
        return G_SIGMOID;
    }
    return declared;
}

// dsa_shexp_width: glm-dsa.cpp:176-178, the shared expert is
// n_ff_exp * n_expert_shared wide, not n_ff_exp.
export fn dsa_shexp_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 27);
    e_ensure(p[P_N_SHEXP] > 0, 28);
    return p[P_N_FF_EXP] * p[P_N_SHEXP];
}

// dsa_moe_dims_ok: glm-dsa.cpp:163-168, the MoE branch throws when either
// expert count is zero. Only reached for layers at and past the dense lead.
export fn dsa_moe_dims_ok(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 29);
    if p[P_N_EXPERT] > 0 && p[P_N_EXPERT_USED] > 0 {
        return 1;
    }
    return 0;
}

// dsa_output_dup: glm-dsa.cpp:121-123.
export fn dsa_output_dup(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 30);
    return t_output_dup(p[P_HAS_OUTPUT]);
}

// dsa_nextn_eh_proj: glm-dsa.cpp:183, `{2*n_embd, n_embd}`.
export fn dsa_nextn_eh_proj(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 31);
    return t_nextn_eh_proj(p[P_N_EMBD]);
}

// dsa_wo_width: glm-dsa.cpp:145, `n_head * n_embd_head_v_mla` rows.
export fn dsa_wo_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 32);
    e_ensure(p[P_N_HEAD] > 0, 33);
    return p[P_N_HEAD] * p[P_V_MLA];
}

// dsa_wq_b_width: glm-dsa.cpp:137, `n_lora_q -> n_head * n_embd_head_k_mla`.
export fn dsa_wq_b_width(p: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 34);
    e_ensure(p[P_N_HEAD] > 0, 35);
    return p[P_N_HEAD] * p[P_K_MLA];
}

// dsa_shape: shape of one tensor. kind is the plan kind of the block, il its
// index (the FFN branch is an index decision, not a kind decision). Returns 1
// and fills out[0..2] when the block owns the tensor, 0 otherwise.
export fn dsa_shape(p: i64[:], kind: i64, il: i64, tid: i64, out: i64[:]) -> i64 {
    e_ensure(p.len >= P_LEN, 40);
    e_ensure(out.len >= 3, 41);
    e_ensure(il >= 0 && il < dsa_n_layer_all(p), 42);
    e_ensure(kind == K_TRUNK || kind == K_NEXTN, 43);

    let n_embd = p[P_N_EMBD];
    let n_vocab = p[P_N_VOCAB];
    let n_head = p[P_N_HEAD];
    let n_ff = p[P_N_FF];
    let q_lora = p[P_Q_LORA];
    let kv_lora = p[P_KV_LORA];
    let nope = dsa_nope_width(p);
    let n_ff_exp_v = n_ff_exp(p[P_N_FF_EXP], p[P_N_FF], p[P_N_EXPERT_USED]);
    let n_ff_shexp = n_ff_exp_v * p[P_N_SHEXP];

    if tid == T_TOKEN_EMBD || tid == T_OUTPUT {
        // output may be absent; an absent output reuses token_embd, same shape
        e_put3(out, n_embd, n_vocab, 1);
        return 1;
    }
    if tid == T_OUTPUT_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }

    // MLA path: every block, trunk and NextN, is a full decoder block
    if tid == T_ATTN_NORM || tid == T_FFN_NORM {
        e_put3(out, n_embd, 1, 1);
        return 1;
    }
    if tid == T_ATTN_Q_A_NORM {
        e_put3(out, q_lora, 1, 1);
        return 1;
    }
    if tid == T_ATTN_KV_A_NORM {
        e_put3(out, kv_lora, 1, 1);
        return 1;
    }
    if tid == T_WQ_A {
        e_put3(out, n_embd, q_lora, 1);
        return 1;
    }
    if tid == T_WQ_B {
        e_put3(out, q_lora, dsa_wq_b_width(p), 1);
        return 1;
    }
    if tid == T_WKV_A_MQA {
        // glm-dsa.cpp:139: the compressed KV and the rope key share one row
        e_put3(out, n_embd, kv_lora + p[P_N_ROT], 1);
        return 1;
    }
    if tid == T_WK_B {
        // glm-dsa.cpp:142: {n_embd_head_qk_nope, kv_lora_rank, n_head}
        e_put3(out, nope, kv_lora, n_head);
        return 1;
    }
    if tid == T_WV_B {
        // glm-dsa.cpp:143: {kv_lora_rank, n_embd_head_v_mla, n_head}
        e_put3(out, kv_lora, p[P_V_MLA], n_head);
        return 1;
    }
    if tid == T_WO {
        e_put3(out, dsa_wo_width(p), n_embd, 1);
        return 1;
    }

    // DSA indexer: declared for every block, optional in the file
    if tid == T_INDEXER_K_NORM || tid == T_INDEXER_K_NORM_B {
        e_put3(out, p[P_IDX_HEAD], 1, 1);
        return 1;
    }
    if tid == T_INDEXER_PROJ {
        e_put3(out, n_embd, p[P_IDX_NHEAD], 1);
        return 1;
    }
    if tid == T_INDEXER_ATTN_K {
        e_put3(out, n_embd, p[P_IDX_HEAD], 1);
        return 1;
    }
    if tid == T_INDEXER_ATTN_Q_B {
        e_put3(out, q_lora, p[P_IDX_NHEAD] * p[P_IDX_HEAD], 1);
        return 1;
    }

    // FFN branch: index decision, glm-dsa.cpp:155
    if dsa_ffn_kind(p, il) == F_DENSE {
        if tid == T_FFN_GATE || tid == T_FFN_UP {
            e_put3(out, n_embd, n_ff, 1);
            return 1;
        }
        if tid == T_FFN_DOWN {
            e_put3(out, n_ff, n_embd, 1);
            return 1;
        }
    } else {
        if tid == T_FFN_GATE_INP {
            e_put3(out, n_embd, p[P_N_EXPERT], 1);
            return 1;
        }
        if tid == T_FFN_EXP_PROBS_B {
            // optional routing bias, declared for every MoE layer (:161)
            e_put3(out, p[P_N_EXPERT], 1, 1);
            return 1;
        }
        if tid == T_FFN_GATE_EXPS || tid == T_FFN_UP_EXPS {
            e_put3(out, n_embd, n_ff_exp_v, p[P_N_EXPERT]);
            return 1;
        }
        if tid == T_FFN_DOWN_EXPS {
            e_put3(out, n_ff_exp_v, n_embd, p[P_N_EXPERT]);
            return 1;
        }
        if tid == T_FFN_GATE_SHEXP || tid == T_FFN_UP_SHEXP {
            e_put3(out, n_embd, n_ff_shexp, 1);
            return 1;
        }
        if tid == T_FFN_DOWN_SHEXP {
            e_put3(out, n_ff_shexp, n_embd, 1);
            return 1;
        }
    }

    // NextN wiring, glm-dsa.cpp:181-190
    if kind == K_NEXTN {
        if tid == T_NEXTN_EH_PROJ {
            e_put3(out, dsa_nextn_eh_proj(p), n_embd, 1);
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
