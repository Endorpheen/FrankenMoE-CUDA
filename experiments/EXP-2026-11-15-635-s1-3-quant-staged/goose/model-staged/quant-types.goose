// P7-04 S1: quantization policy / type selection, in Goose.
//
// Ported from src/llama-quant.cpp:
//   - llama_ftype_get_default_type
//   - tensor_get_category / category_is_attn_v
//   - tensor_allows_quantization (the generic name rules)
//   - init_quantize_state_counters
//   - llama_tensor_get_type (outer wrapper)
//   - llama_tensor_get_type_impl (the selection branches)
//   - tensor_type_fallback
//   - the per-tensor imatrix policy tensor_requires_imatrix
//   - the type-only ggml policy ggml_quantize_requires_imatrix
//
// The two imatrix policies are deliberately separate. They do not agree:
// tensor_requires_imatrix is name-aware (token_embd/output are exempt) and
// includes IQ3_XXS, IQ2_XXS, IQ2_XS, IQ2_S, IQ1_M, IQ1_S and Q2_K only for the
// Q2_K_S file type; ggml_quantize_requires_imatrix is type-only and covers
// only IQ2_XXS, IQ2_XS and IQ1_S.
//
// Not in this slice (S2-S5, see REPORT-S1.md): regex tensor-type overrides,
// architecture-specific allow-list cuts (GLM5_NEXT, VLLM_DECISION2), the
// Falcon/Bailing-MoE/70B special cases, model-type dependent bumps, the
// quantize execution itself and the model loader integration.

import std;

// ggml types
const T_F32: i64 = 0;
const T_F16: i64 = 1;
const T_Q4_0: i64 = 2;
const T_Q4_1: i64 = 3;
const T_Q5_0: i64 = 6;
const T_Q5_1: i64 = 7;
const T_Q8_0: i64 = 8;
const T_Q2_K: i64 = 10;
const T_Q3_K: i64 = 11;
const T_Q4_K: i64 = 12;
const T_Q5_K: i64 = 13;
const T_Q6_K: i64 = 14;
const T_IQ2_XXS: i64 = 16;
const T_IQ2_XS: i64 = 17;
const T_IQ3_XXS: i64 = 18;
const T_IQ1_S: i64 = 19;
const T_IQ4_NL: i64 = 20;
const T_IQ3_S: i64 = 21;
const T_IQ2_S: i64 = 22;
const T_IQ4_XS: i64 = 23;
const T_IQ1_M: i64 = 29;
const T_BF16: i64 = 30;
const T_TQ1_0: i64 = 34;
const T_TQ2_0: i64 = 35;
const T_MXFP4: i64 = 39;
const T_NVFP4: i64 = 40;
const T_Q1_0: i64 = 41;
const T_Q2_0: i64 = 42;
const T_COUNT: i64 = 43;

// llama_ftype
const F_ALL_F32: i64 = 0;
const F_F16: i64 = 1;
const F_Q4_0: i64 = 2;
const F_Q4_1: i64 = 3;
const F_Q8_0: i64 = 7;
const F_Q5_0: i64 = 8;
const F_Q5_1: i64 = 9;
const F_Q2_K: i64 = 10;
const F_Q3_K_S: i64 = 11;
const F_Q3_K_M: i64 = 12;
const F_Q3_K_L: i64 = 13;
const F_Q4_K_S: i64 = 14;
const F_Q4_K_M: i64 = 15;
const F_Q5_K_S: i64 = 16;
const F_Q5_K_M: i64 = 17;
const F_Q6_K: i64 = 18;
const F_IQ2_XXS: i64 = 19;
const F_IQ2_XS: i64 = 20;
const F_Q2_K_S: i64 = 21;
const F_IQ3_XS: i64 = 22;
const F_IQ3_XXS: i64 = 23;
const F_IQ1_S: i64 = 24;
const F_IQ4_NL: i64 = 25;
const F_IQ3_S: i64 = 26;
const F_IQ3_M: i64 = 27;
const F_IQ2_S: i64 = 28;
const F_IQ2_M: i64 = 29;
const F_IQ4_XS: i64 = 30;
const F_IQ1_M: i64 = 31;
const F_BF16: i64 = 32;
const F_TQ1_0: i64 = 36;
const F_TQ2_0: i64 = 37;
const F_MXFP4_MOE: i64 = 38;
const F_NVFP4: i64 = 39;
const F_Q1_0: i64 = 40;
const F_Q2_0: i64 = 41;

// tensor categories
const C_TOKEN_EMBD: i64 = 0;
const C_ATTN_Q: i64 = 1;
const C_ATTN_V: i64 = 2;
const C_ATTN_K: i64 = 3;
const C_ATTN_QKV: i64 = 4;
const C_ATTN_KV_B: i64 = 5;
const C_ATTN_OUTPUT: i64 = 6;
const C_ATTN_GATE: i64 = 7;
const C_FFN_UP: i64 = 8;
const C_FFN_GATE: i64 = 9;
const C_FFN_DOWN: i64 = 10;
const C_FFN_LATENT: i64 = 11;
const C_OUTPUT: i64 = 12;
const C_OTHER: i64 = 13;

// state indices
const S_ATTN_TOTAL: i64 = 0;
const S_I_ATTN: i64 = 1;
const S_N_DOWN: i64 = 2;
const S_I_DOWN: i64 = 3;
const S_N_GATE: i64 = 4;
const S_I_GATE: i64 = 5;
const S_N_UP: i64 = 6;
const S_I_UP: i64 = 7;
const S_TIED: i64 = 8;
const S_FALLBACK: i64 = 9;
const S_LEN: i64 = 10;

fn qs_find(s: const u8[:], from: i64, t: const u8[:]) -> i64 {
    if t.len == 0 {
        return from;
    }
    var i = from;
    while i + t.len <= s.len {
        var j: i64 = 0;
        var ok: i64 = 1;
        while j < t.len {
            if s[i + j] != t[j] {
                ok = 0;
                j = t.len;
            } else {
                j = j + 1;
            }
        }
        if ok == 1 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

fn qs_has(s: const u8[:], t: const u8[:]) -> bool {
    return qs_find(s, 0, t) >= 0;
}

fn qs_eq(s: const u8[:], t: const u8[:]) -> bool {
    if s.len != t.len {
        return false;
    }
    var i: i64 = 0;
    while i < s.len {
        if s[i] != t[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn qs_prefix(s: const u8[:], t: const u8[:]) -> bool {
    if s.len < t.len {
        return false;
    }
    var i: i64 = 0;
    while i < t.len {
        if s[i] != t[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn qs_ends(s: const u8[:], t: const u8[:]) -> bool {
    if s.len < t.len {
        return false;
    }
    var i: i64 = 0;
    while i < t.len {
        if s[s.len - t.len + i] != t[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// ---- ftype default type ----------------------------------------------------

fn qt_default_type(ft: i64) -> i64 {
    if ft == F_ALL_F32 { return T_F32; }
    if ft == F_F16 { return T_F16; }
    if ft == F_BF16 { return T_BF16; }
    if ft == F_Q4_0 { return T_Q4_0; }
    if ft == F_Q4_1 { return T_Q4_1; }
    if ft == F_Q5_0 { return T_Q5_0; }
    if ft == F_Q5_1 { return T_Q5_1; }
    if ft == F_Q8_0 { return T_Q8_0; }
    if ft == F_Q1_0 { return T_Q1_0; }
    if ft == F_Q2_0 { return T_Q2_0; }
    if ft == F_MXFP4_MOE { return T_MXFP4; }
    if ft == F_NVFP4 { return T_NVFP4; }
    if ft == F_Q2_K_S || ft == F_Q2_K { return T_Q2_K; }
    if ft == F_IQ3_XS { return T_IQ3_S; }
    if ft == F_Q3_K_S || ft == F_Q3_K_M || ft == F_Q3_K_L { return T_Q3_K; }
    if ft == F_Q4_K_S || ft == F_Q4_K_M { return T_Q4_K; }
    if ft == F_Q5_K_S || ft == F_Q5_K_M { return T_Q5_K; }
    if ft == F_Q6_K { return T_Q6_K; }
    if ft == F_TQ1_0 { return T_TQ1_0; }
    if ft == F_TQ2_0 { return T_TQ2_0; }
    if ft == F_IQ2_XXS { return T_IQ2_XXS; }
    if ft == F_IQ2_XS || ft == F_IQ2_S { return T_IQ2_XS; }
    if ft == F_IQ2_M { return T_IQ2_S; }
    if ft == F_IQ3_XXS { return T_IQ3_XXS; }
    if ft == F_IQ1_S { return T_IQ1_S; }
    if ft == F_IQ1_M { return T_IQ1_M; }
    if ft == F_IQ4_NL { return T_IQ4_NL; }
    if ft == F_IQ3_S || ft == F_IQ3_M { return T_IQ3_S; }
    if ft == F_IQ4_XS { return T_IQ4_XS; }
    return T_COUNT;
}

fn qt_is_quant(t: i64) -> bool {
    return t != T_F32 && t != T_F16 && t != T_BF16 && t != T_COUNT;
}

// ---- category --------------------------------------------------------------

fn qt_category(name: const u8[:]) -> i64 {
    if qs_eq(name, "output.weight") {
        return C_OUTPUT;
    }
    if qs_eq(name, "token_embd.weight") || qs_eq(name, "per_layer_token_embd.weight") {
        return C_TOKEN_EMBD;
    }
    if qs_has(name, "attn_qkv.weight") { return C_ATTN_QKV; }
    if qs_has(name, "attn_kv_b.weight") { return C_ATTN_KV_B; }
    if qs_has(name, "attn_v.weight") { return C_ATTN_V; }
    if qs_has(name, "attn_k.weight") { return C_ATTN_K; }
    if qs_has(name, "attn_q.weight") { return C_ATTN_Q; }
    if qs_has(name, "attn_output.weight") { return C_ATTN_OUTPUT; }
    if qs_has(name, "ffn_up") { return C_FFN_UP; }
    if qs_has(name, "ffn_gate") { return C_FFN_GATE; }
    if qs_has(name, "ffn_latent") { return C_FFN_LATENT; }
    if qs_has(name, "attn_gate.weight") { return C_ATTN_GATE; }
    if qs_has(name, "ffn_down") { return C_FFN_DOWN; }
    return C_OTHER;
}

fn qt_is_attn_v(cat: i64) -> bool {
    return cat == C_ATTN_V || cat == C_ATTN_QKV || cat == C_ATTN_KV_B;
}

// ---- allows_quantization (generic rules only) ------------------------------

fn qt_allows(name: const u8[:], ndims: i64, quantize_output: i64, only_copy: i64) -> bool {
    if only_copy != 0 { return false; }
    if ndims < 2 { return false; }
    if !qs_ends(name, "weight") { return false; }
    if qs_has(name, "_norm.weight") { return false; }
    if quantize_output == 0 && qs_eq(name, "output.weight") { return false; }
    if qs_has(name, "ffn_gate_inp.weight") { return false; }
    if qs_has(name, "ffn_gate_tid2eid.weight") { return false; }
    if qs_has(name, "altup") { return false; }
    if qs_has(name, "laurel") { return false; }
    if qs_has(name, "per_layer_model_proj") { return false; }
    if qs_has(name, "ssm_conv1d") { return false; }
    if qs_has(name, "shortconv.conv.weight") { return false; }
    if qs_has(name, "indexer.k_proj.weight") { return false; }
    if qs_has(name, "indexer.q_proj.weight") { return false; }
    if qs_has(name, "time_mix_first.weight") { return false; }
    if qs_has(name, "time_mix_w0.weight") { return false; }
    if qs_has(name, "time_mix_w1.weight") { return false; }
    if qs_has(name, "time_mix_w2.weight") { return false; }
    if qs_has(name, "time_mix_v0.weight") { return false; }
    if qs_has(name, "time_mix_v1.weight") { return false; }
    if qs_has(name, "time_mix_v2.weight") { return false; }
    if qs_has(name, "time_mix_a0.weight") { return false; }
    if qs_has(name, "time_mix_a1.weight") { return false; }
    if qs_has(name, "time_mix_a2.weight") { return false; }
    if qs_has(name, "time_mix_g1.weight") { return false; }
    if qs_has(name, "time_mix_g2.weight") { return false; }
    if qs_has(name, "time_mix_decay_w1.weight") { return false; }
    if qs_has(name, "time_mix_decay_w2.weight") { return false; }
    if qs_has(name, "time_mix_lerp_fused.weight") { return false; }
    if qs_has(name, "attn_rel_b.weight") { return false; }
    if qs_has(name, ".position_embd") { return false; }
    if qs_has(name, "sam.pos_embd") { return false; }
    if qs_has(name, "sam.neck.") { return false; }
    if qs_has(name, "sam.net_") { return false; }
    if qs_has(name, ".rel_pos") { return false; }
    if qs_has(name, ".patch_embd") { return false; }
    if qs_has(name, ".patch_merger") { return false; }
    if qs_has(name, "a.rvq.codebook") { return false; }
    if qs_has(name, "mm.a.code_embd") { return false; }
    return true;
}

// ---- block size and fallback ----------------------------------------------

fn qt_block(t: i64) -> i64 {
    if t == T_Q4_0 || t == T_Q4_1 || t == T_Q5_0 || t == T_Q5_1 || t == T_Q8_0 { return 32; }
    if t == T_IQ4_NL || t == T_MXFP4 { return 32; }
    if t == T_Q1_0 { return 128; }
    if t == T_Q2_0 || t == T_NVFP4 { return 64; }
    if t == T_Q2_K || t == T_Q3_K || t == T_Q4_K || t == T_Q5_K || t == T_Q6_K { return 256; }
    if t == T_IQ2_XXS || t == T_IQ2_XS || t == T_IQ3_XXS || t == T_IQ1_S { return 256; }
    if t == T_IQ3_S || t == T_IQ2_S || t == T_IQ4_XS || t == T_IQ1_M { return 256; }
    if t == T_TQ1_0 || t == T_TQ2_0 { return 256; }
    return 1;
}

fn qt_fallback(new_type: i64, ne0: i64, st: i64[:]) -> i64 {
    let qk = qt_block(new_type);
    if qk <= 1 || ne0 % qk == 0 {
        return new_type;
    }
    st[S_FALLBACK] = st[S_FALLBACK] + 1;
    var r = new_type;
    if new_type == T_IQ1_S || new_type == T_IQ1_M || new_type == T_IQ2_XXS ||
       new_type == T_IQ2_XS || new_type == T_IQ2_S || new_type == T_IQ3_XXS ||
       new_type == T_IQ3_S || new_type == T_IQ4_XS {
        r = T_IQ4_NL;
    } else if new_type == T_Q2_0 || new_type == T_Q2_K || new_type == T_Q3_K ||
              new_type == T_TQ1_0 || new_type == T_TQ2_0 {
        r = T_Q4_0;
    } else if new_type == T_Q4_K {
        r = T_Q5_0;
    } else if new_type == T_Q5_K {
        r = T_Q5_1;
    } else if new_type == T_Q6_K {
        r = T_Q8_0;
    }
    if ne0 % qt_block(r) != 0 {
        r = T_F16;
    }
    return r;
}

// ---- layer helpers ---------------------------------------------------------

fn qt_layer(name: const u8[:], fallback: i64) -> i64 {
    if !qs_prefix(name, "blk.") {
        return fallback;
    }
    var i: i64 = 4;
    var v: i64 = 0;
    var seen: i64 = 0;
    while i < name.len {
        let c = name[i];
        if c >= 48 && c <= 57 {
            v = v * 10 + (c as i64 - 48);
            seen = 1;
            i = i + 1;
        } else {
            i = name.len;
        }
    }
    if seen == 0 {
        return fallback;
    }
    return v;
}

fn qt_use_more_bits(i: i64, n: i64) -> bool {
    if n <= 0 { return false; }
    if i < n / 8 { return true; }
    if i >= 7 * n / 8 { return true; }
    return (i - n / 8) % 3 == 2;
}

// ---- type selection --------------------------------------------------------

fn qt_get_type(
    name: const u8[:],
    ne0: i64,
    ne1: i64,
    ne2: i64,
    ndims: i64,
    orig_type: i64,
    ftype: i64,
    n_layer: i64,
    n_expert: i64,
    n_gqa: i64,
    token_type: i64,
    output_type: i64,
    quantize_output: i64,
    only_copy: i64,
    pure: i64,
    has_imatrix: i64,
    cat: i64,
    st: i64[:]
) -> i64 {
    if !qt_allows(name, ndims, quantize_output, only_copy) {
        return orig_type;
    }
    if token_type < T_COUNT && cat == C_TOKEN_EMBD {
        return token_type;
    }
    if output_type < T_COUNT && cat == C_OUTPUT {
        return output_type;
    }

    let default_type = qt_default_type(ftype);
    if !qt_is_quant(default_type) {
        return default_type;
    }

    var new_type = default_type;

    if pure == 0 {
        // OUTPUT, or a tied token embedding, follows the output type rules.
        if cat == C_OUTPUT || (st[S_TIED] != 0 && cat == C_TOKEN_EMBD) {
            if output_type < T_COUNT {
                new_type = output_type;
            } else {
                let qk = qt_block(new_type);
                if ftype == F_MXFP4_MOE {
                    new_type = T_Q8_0;
                } else if ne0 % qk != 0 {
                    new_type = T_Q8_0;
                } else if ftype == F_IQ2_XXS || ftype == F_IQ2_XS || ftype == F_IQ3_XXS ||
                          ftype == F_IQ1_S || ftype == F_IQ2_S || ftype == F_IQ2_M || ftype == F_IQ1_M {
                    new_type = T_Q5_K;
                } else if new_type != T_Q8_0 {
                    new_type = T_Q6_K;
                }
            }
        } else if ftype == F_MXFP4_MOE {
            if ne2 > 1 {
                new_type = T_MXFP4;
            } else {
                new_type = T_Q8_0;
            }
        } else if cat == C_TOKEN_EMBD {
            if ftype == F_IQ2_XXS || ftype == F_IQ2_XS || ftype == F_IQ1_S || ftype == F_IQ1_M {
                new_type = T_Q2_K;
            } else if ftype == F_IQ2_S || ftype == F_IQ2_M {
                new_type = T_IQ3_S;
            } else if ftype == F_IQ3_XXS {
                new_type = T_IQ3_S;
            } else if ftype == F_TQ1_0 || ftype == F_TQ2_0 || ftype == F_Q2_0 {
                new_type = T_Q4_K;
            }
        } else if ftype == F_IQ2_XXS || ftype == F_IQ2_XS || ftype == F_IQ1_S ||
                  ftype == F_IQ2_S || ftype == F_IQ2_M || ftype == F_IQ1_M {
            if qt_is_attn_v(cat) {
                if n_gqa >= 4 || n_expert >= 4 {
                    new_type = T_Q4_K;
                } else if ftype == F_IQ2_S || ftype == F_IQ2_M {
                    new_type = T_IQ3_S;
                } else {
                    new_type = T_Q2_K;
                }
                st[S_I_ATTN] = st[S_I_ATTN] + 1;
            } else if n_expert == 8 && cat == C_ATTN_K {
                new_type = T_Q4_K;
            } else if cat == C_FFN_DOWN {
                if st[S_I_DOWN] < st[S_N_DOWN] / 8 {
                    if ftype == F_IQ2_S || ftype == F_IQ2_M {
                        new_type = T_IQ3_S;
                    } else {
                        new_type = T_Q2_K;
                    }
                }
                st[S_I_DOWN] = st[S_I_DOWN] + 1;
            } else if cat == C_ATTN_OUTPUT {
                if n_expert == 8 {
                    new_type = T_Q5_K;
                } else if ftype == F_IQ1_S || ftype == F_IQ1_M {
                    new_type = T_IQ2_XXS;
                } else if ftype == F_IQ2_S || ftype == F_IQ2_M {
                    new_type = T_IQ3_S;
                }
            }
        } else if qt_is_attn_v(cat) {
            if ftype == F_Q2_K {
                new_type = if n_gqa >= 4 { T_Q4_K } else { T_Q3_K };
            } else if ftype == F_Q2_K_S && n_gqa >= 4 {
                new_type = T_Q4_K;
            } else if ftype == F_IQ3_XXS {
                if n_gqa >= 4 {
                    new_type = T_Q4_K;
                } else if has_imatrix == 0 {
                    new_type = T_IQ3_S;
                } else {
                    new_type = T_IQ3_XXS;
                }
            } else if (ftype == F_IQ3_XS || ftype == F_IQ3_S) && n_gqa >= 4 {
                new_type = T_Q4_K;
            } else if ftype == F_IQ3_M {
                new_type = T_Q4_K;
            } else if ftype == F_Q3_K_M {
                new_type = if st[S_I_ATTN] < 2 { T_Q5_K } else { T_Q4_K };
            } else if ftype == F_Q3_K_L {
                new_type = T_Q5_K;
            } else if (ftype == F_IQ4_NL || ftype == F_IQ4_XS) && n_gqa >= 4 {
                new_type = T_Q5_K;
            } else if (ftype == F_Q4_K_M || ftype == F_Q5_K_M) &&
                      qt_use_more_bits(st[S_I_ATTN], st[S_ATTN_TOTAL]) {
                new_type = T_Q6_K;
            } else if ftype == F_Q4_K_S && st[S_I_ATTN] < 4 {
                new_type = T_Q5_K;
            }
            if n_expert == 8 {
                new_type = T_Q8_0;
            }
            st[S_I_ATTN] = st[S_I_ATTN] + 1;
        } else if cat == C_ATTN_K {
            if n_expert == 8 {
                new_type = T_Q8_0;
            } else if ftype == F_IQ3_XS {
                new_type = T_IQ3_XXS;
            } else if ftype == F_IQ3_XXS {
                new_type = T_IQ2_S;
            }
        } else if cat == C_ATTN_Q {
            if ftype == F_IQ3_XS {
                new_type = T_IQ3_XXS;
            } else if ftype == F_IQ3_XXS {
                new_type = T_IQ2_S;
            }
        } else if cat == C_FFN_DOWN {
            var i_layer = st[S_I_DOWN];
            if n_expert > 1 {
                i_layer = qt_layer(name, st[S_I_DOWN]);
            }
            if ftype == F_Q2_K {
                new_type = T_Q3_K;
            } else if ftype == F_Q2_K_S {
                if i_layer < n_layer / 8 {
                    new_type = T_Q4_K;
                }
            } else if ftype == F_IQ3_XXS && has_imatrix == 0 {
                new_type = if i_layer < n_layer / 8 { T_Q4_K } else { T_Q3_K };
            } else if ftype == F_Q3_K_M {
                // Non-Falcon architectures take the second branch unconditionally;
                // the Falcon split is an S2 architecture case.
                new_type = if i_layer < n_layer / 16 { T_Q5_K } else { T_Q4_K };
            } else if ftype == F_IQ3_M && (i_layer < n_layer / 8 ||
                      (n_expert == 8 && qt_use_more_bits(i_layer, n_layer))) {
                new_type = T_Q4_K;
            } else if ftype == F_Q3_K_L {
                new_type = T_Q5_K;
            } else if ftype == F_Q4_K_M {
                if qt_use_more_bits(i_layer, n_layer) {
                    new_type = T_Q6_K;
                }
            } else if i_layer < n_layer / 8 && (ftype == F_IQ4_NL || ftype == F_IQ4_XS) && has_imatrix == 0 {
                new_type = T_Q5_K;
            } else if ftype == F_Q5_K_M && qt_use_more_bits(i_layer, n_layer) {
                new_type = T_Q6_K;
            } else if ftype == F_Q4_K_S && i_layer < n_layer / 8 {
                new_type = T_Q5_K;
            } else if (ftype == F_Q4_0 || ftype == F_Q5_0) && has_imatrix != 0 && i_layer < n_layer / 8 {
                new_type = if ftype == F_Q4_0 { T_Q4_1 } else { T_Q5_1 };
            }
            st[S_I_DOWN] = st[S_I_DOWN] + 1;
        } else if cat == C_ATTN_OUTPUT {
            if n_expert == 8 {
                if ftype == F_Q2_K || ftype == F_IQ3_XS || ftype == F_IQ3_XXS ||
                   ftype == F_Q3_K_S || ftype == F_Q3_K_M || ftype == F_IQ4_NL ||
                   ftype == F_Q4_K_S || ftype == F_Q4_K_M || ftype == F_IQ3_S ||
                   ftype == F_IQ3_M || ftype == F_IQ4_XS {
                    new_type = T_Q5_K;
                }
            } else {
                if ftype == F_Q2_K { new_type = T_Q3_K; }
                else if ftype == F_IQ3_XXS { new_type = T_IQ3_S; }
                else if ftype == F_Q3_K_M { new_type = T_Q4_K; }
                else if ftype == F_Q3_K_L { new_type = T_Q5_K; }
                else if ftype == F_IQ3_M { new_type = T_Q4_K; }
            }
        } else if cat == C_ATTN_QKV {
            if ftype == F_Q3_K_M || ftype == F_Q3_K_L || ftype == F_IQ3_M {
                new_type = T_Q4_K;
            } else if ftype == F_Q4_K_M {
                new_type = T_Q5_K;
            } else if ftype == F_Q5_K_M {
                new_type = T_Q6_K;
            }
        } else if cat == C_FFN_GATE {
            var i_layer = st[S_I_GATE];
            if n_expert > 1 {
                i_layer = qt_layer(name, st[S_I_GATE]);
            }
            if ftype == F_IQ3_XS && (i_layer >= n_layer / 8 && i_layer < 7 * n_layer / 8) {
                new_type = T_IQ3_XXS;
            }
            st[S_I_GATE] = st[S_I_GATE] + 1;
        } else if cat == C_FFN_UP {
            var i_layer = st[S_I_UP];
            if n_expert > 1 {
                i_layer = qt_layer(name, st[S_I_UP]);
            }
            if ftype == F_IQ3_XS && (i_layer >= n_layer / 8 && i_layer < 7 * n_layer / 8) {
                new_type = T_IQ3_XXS;
            }
            st[S_I_UP] = st[S_I_UP] + 1;
        }
    }

    return qt_fallback(new_type, ne0, st);
}

// ---- entry: llama_quant_compute_types -------------------------------------

export fn qt_compute(
    names: const u8[:],
    noff: const i64[:],
    nlen: const i64[:],
    ne0: const i64[:],
    ne1: const i64[:],
    ne2: const i64[:],
    ndims: const i64[:],
    otypes: const i64[:],
    n: i64,
    ftype: i64,
    n_layer: i64,
    n_expert: i64,
    n_gqa: i64,
    token_type: i64,
    output_type: i64,
    quantize_output: i64,
    only_copy: i64,
    pure: i64,
    has_imatrix: i64,
    out: i64[:]
) -> i64 {
    var st: i64[S_LEN] = [0; S_LEN];
    st[S_TIED] = 1;
    st[S_N_DOWN] = n_layer;
    st[S_N_GATE] = n_layer;
    st[S_N_UP] = n_layer;

    var i: i64 = 0;
    while i < n {
        let name = names[noff[i] .. noff[i] + nlen[i]];
        let cat = qt_category(name);
        if cat == C_OUTPUT {
            st[S_TIED] = 0;
        }
        if qt_is_attn_v(cat) {
            st[S_ATTN_TOTAL] = st[S_ATTN_TOTAL] + 1;
        }
        i = i + 1;
    }

    i = 0;
    while i < n {
        let name = names[noff[i] .. noff[i] + nlen[i]];
        let cat = qt_category(name);
        out[i] = qt_get_type(
            name, ne0[i], ne1[i], ne2[i], ndims[i], otypes[i],
            ftype, n_layer, n_expert, n_gqa, token_type, output_type,
            quantize_output, only_copy, pure, has_imatrix, cat, st);
        i = i + 1;
    }

    return st[S_FALLBACK];
}

// ---- policy A: per-tensor imatrix requirement ------------------------------

export fn qt_tensor_requires_imatrix(name: const u8[:], dst_type: i64, ftype: i64) -> i64 {
    if qs_eq(name, "token_embd.weight") || qs_eq(name, "per_layer_token_embd.weight") ||
       qs_eq(name, "output.weight") {
        return 0;
    }
    if dst_type == T_IQ3_XXS || dst_type == T_IQ2_XXS || dst_type == T_IQ2_XS ||
       dst_type == T_IQ2_S || dst_type == T_IQ1_M || dst_type == T_IQ1_S {
        return 1;
    }
    if dst_type == T_Q2_K {
        return if ftype == F_Q2_K_S { 1 } else { 0 };
    }
    return 0;
}

// ---- policy B: type-only ggml imatrix requirement -------------------------

export fn qt_ggml_quantize_requires_imatrix(t: i64) -> i64 {
    if t == T_IQ2_XXS || t == T_IQ2_XS || t == T_IQ1_S {
        return 1;
    }
    return 0;
}

