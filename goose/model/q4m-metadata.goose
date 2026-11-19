// C1-02 slice M: qwen4exp metadata adapter (hparams keys, presence rules,
// derived values and the validation order).
//
// Native anchors (src/models/qwen4exp.cpp of the pinned llama.cpp):
//   load_arch_hparams :5-107
//   llama-arch.cpp key table :191-337
//   llama-hparams.cpp ple_conv_state :243-250
//   llama-hparams.cpp set_recr_pattern :17-40
//   llama-hparams.cpp n_embd_out :111-113
//
// This is the half of the builder that decides what the GGUF must carry and
// which combinations are refused, not the shapes (canon/qwen4exp.goose holds
// those) and not the tensors (q4t_ / q4e_ hold those).
//
// Three rules that the native code expresses only through control flow and are
// therefore easy to get wrong in a port:
//   - a key's requirement is three-valued: always required, always optional, or
//     required only inside the PLE group (which itself is optional);
//   - the PLE validation order is load-bearing: the conv/embedding assertion
//     fires before the n-gram range check, so a file with both defects reports
//     the conv one;
//   - `ple_n_heads` is computed before the n-gram range check runs, so the port
//     has to reproduce the pre-check arithmetic in i64 rather than uint32.
//
// Native errors are std::runtime_error / GGML_ASSERT. The port returns a code
// so a gate can assert the refusal without killing the host:
//   0 ok, 1 ple layer out of range, 2 more than one PLE layer,
//   3 conv kernel or per-layer embedding not positive,
//   4 n-gram size outside [2, LLAMA_MAX_PLE_NGRAM],
//   5 head count outside [1, LLAMA_MAX_PLE_HEADS].

// The unit has one abort primitive. Binding a second symbol for the same signature would make
// q4m composable with its own gate and not with the cumulative unit (EXP-2026-11-19-1498);
// fg200g_fail is the binding goose-model-graph.goose uses, so this reuses it.
extern "fg200g_fail" fn fail(code: i64);

fn q4m_ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn put_str(s: const u8[:], out: u8[:]) -> i64 {
    if out.len < s.len {
        return -1;
    }
    var i: i64 = 0;
    while i < s.len {
        out[i] = s[i];
        i += 1;
    }
    return s.len;
}

// ---- metadata keys ---------------------------------------------------------
//
// Key ids are the load order of load_arch_hparams, so a fixture can walk them
// and the integration step keeps the same sequence as the native reader.

const K_EXPERT_FF: i64 = 0;
const K_EXPERT_FF_SHEXP: i64 = 1;
const K_NORM_RMS_EPS: i64 = 2;
const K_ROPE_SECTIONS: i64 = 3;
const K_SSM_CONV_KERNEL: i64 = 4;
const K_SSM_INNER: i64 = 5;
const K_SSM_STATE: i64 = 6;
const K_SSM_DT_RANK: i64 = 7;
const K_SSM_GROUP: i64 = 8;
const K_HC_COUNT: i64 = 9;
const K_HC_LOW_RANK: i64 = 10;
const K_INDEXER_HEADS: i64 = 11;
const K_INDEXER_KEY_LEN: i64 = 12;
const K_INDEXER_TOP_K: i64 = 13;
const K_COMPRESS_RATIOS: i64 = 14;
const K_PLE_LAYERS: i64 = 15;
const K_PLE_NGRAM: i64 = 16;
const K_PLE_HEADS_PER_NGRAM: i64 = 17;
const K_PLE_CONV_KERNEL: i64 = 18;
const K_PLE_LAYER_MULT: i64 = 19;
const K_PLE_HEAD_OFFSETS: i64 = 20;
const K_PLE_HEAD_VOCAB: i64 = 21;
const K_PLE_EOS: i64 = 22;
const K_PLE_IMAGE: i64 = 23;
const K_EMBD_PER_LAYER: i64 = 24;
const K_RECURRENT_LAYERS: i64 = 25;
const K_FULL_ATTN_INTERVAL: i64 = 26;
const K_LEN: i64 = 27;

// Requirement classes.
const R_ALWAYS: i64 = 0;
const R_OPTIONAL: i64 = 1;
const R_PLE_GROUP: i64 = 2;

// Arity classes.
const A_SCALAR: i64 = 0;
const A_FOUR: i64 = 1;       // rope.dimension_sections: get_key_or_arr(..., 4, true)
const A_PER_LAYER: i64 = 2;  // array sized n_layer_all
const A_LIST: i64 = 3;       // variable-length array

const LLAMA_MAX_PLE_NGRAM: i64 = 8;
const LLAMA_MAX_PLE_HEADS: i64 = 64;
const INT32_MAX: i64 = 2147483647;

export fn q4m_key_count() -> i64 {
    return K_LEN;
}

// The suffix of "<arch>.<suffix>"; llama-arch.cpp:191-337.
fn key_suffix(k: i64) -> const u8[:] {
    if k == 0 { return "expert_feed_forward_length"; }
    if k == 1 { return "expert_shared_feed_forward_length"; }
    if k == 2 { return "attention.layer_norm_rms_epsilon"; }
    if k == 3 { return "rope.dimension_sections"; }
    if k == 4 { return "ssm.conv_kernel"; }
    if k == 5 { return "ssm.inner_size"; }
    if k == 6 { return "ssm.state_size"; }
    if k == 7 { return "ssm.time_step_rank"; }
    if k == 8 { return "ssm.group_count"; }
    if k == 9 { return "hyper_connection.count"; }
    if k == 10 { return "hyper_connection.low_rank"; }
    if k == 11 { return "attention.indexer.head_count"; }
    if k == 12 { return "attention.indexer.key_length"; }
    if k == 13 { return "attention.indexer.top_k"; }
    if k == 14 { return "attention.compress_ratios"; }
    if k == 15 { return "ple.layers"; }
    if k == 16 { return "ple.ngram_size"; }
    if k == 17 { return "ple.heads_per_ngram"; }
    if k == 18 { return "ple.conv_kernel"; }
    if k == 19 { return "ple.layer_multipliers"; }
    if k == 20 { return "ple.head_offsets"; }
    if k == 21 { return "ple.head_vocab_sizes"; }
    if k == 22 { return "ple.eos_token_id"; }
    if k == 23 { return "ple.image_token_id"; }
    if k == 24 { return "embedding_length_per_layer_input"; }
    if k == 25 { return "attention.recurrent_layers"; }
    if k == 26 { return "full_attention_interval"; }
    return "";
}

export fn q4m_key_name(k: i64, out: u8[:]) -> i64 {
    if k < 0 || k >= K_LEN {
        return -1;
    }
    return put_str(key_suffix(k), out);
}

// The full GGUF key, "<arch>.<suffix>".
export fn q4m_key_full(k: i64, out: u8[:]) -> i64 {
    if k < 0 || k >= K_LEN {
        return -1;
    }
    let pre: const u8[:] = "qwen4exp.";
    let suf: const u8[:] = key_suffix(k);
    if out.len < pre.len + suf.len {
        return -1;
    }
    var i: i64 = 0;
    while i < pre.len {
        out[i] = pre[i];
        i += 1;
    }
    var j: i64 = 0;
    while j < suf.len {
        out[i + j] = suf[j];
        j += 1;
    }
    return pre.len + suf.len;
}

export fn q4m_key_required(k: i64) -> i64 {
    if k < 0 || k >= K_LEN {
        return -1;
    }
    // ml.get_key(..., false) and get_key_or_arr(..., false) are optional.
    if k == K_EXPERT_FF || k == K_EXPERT_FF_SHEXP || k == K_COMPRESS_RATIOS {
        return R_OPTIONAL;
    }
    if k == K_PLE_LAYERS || k == K_PLE_IMAGE || k == K_RECURRENT_LAYERS || k == K_FULL_ATTN_INTERVAL {
        return R_OPTIONAL;
    }
    // The rest of the PLE group is read without the optional flag, but only
    // after ple.layers proved non-empty, so it is conditionally required.
    if k == K_PLE_NGRAM || k == K_PLE_HEADS_PER_NGRAM || k == K_PLE_CONV_KERNEL ||
       k == K_PLE_LAYER_MULT || k == K_PLE_HEAD_OFFSETS || k == K_PLE_HEAD_VOCAB ||
       k == K_PLE_EOS || k == K_EMBD_PER_LAYER {
        return R_PLE_GROUP;
    }
    return R_ALWAYS;
}

export fn q4m_key_arity(k: i64) -> i64 {
    if k < 0 || k >= K_LEN {
        return -1;
    }
    if k == K_ROPE_SECTIONS {
        return A_FOUR;
    }
    if k == K_COMPRESS_RATIOS || k == K_RECURRENT_LAYERS {
        return A_PER_LAYER;
    }
    if k == K_PLE_LAYERS || k == K_PLE_LAYER_MULT || k == K_PLE_HEAD_OFFSETS || k == K_PLE_HEAD_VOCAB {
        return A_LIST;
    }
    return A_SCALAR;
}

// ---- PLE ------------------------------------------------------------------

// The PLE key group is read only when ple.layers is present and non-empty; an
// absent group leaves every field zero (qwen4exp.cpp:40-42).
export fn q4m_ple_group_present(n_ple: i64) -> i64 {
    if n_ple > 0 {
        return 1;
    }
    return 0;
}

// ple_n_heads = (ngram_size - 1) * heads_per_ngram, computed before the range
// checks (qwen4exp.cpp:64). i64 so a zero n-gram cannot wrap.
export fn q4m_ple_heads(ngram: i64, per_ngram: i64) -> i64 {
    return (ngram - 1) * per_ngram;
}

// ple_head_dim = n_embd_per_layer (qwen4exp.cpp:65).
export fn q4m_ple_head_dim(embd_per_layer: i64) -> i64 {
    return embd_per_layer;
}

// Validation order of qwen4exp.cpp:44-72, in one code.
export fn q4m_ple_error(n_ple: i64, ple_layer: i64, n_layer_all: i64,
                        ngram: i64, heads: i64, conv_kernel: i64, embd_per_layer: i64) -> i64 {
    if n_ple < 0 {
        return 2;
    }
    // GGML_ASSERT(n_ple == 1): the architecture supports exactly one PLE layer.
    if n_ple > 1 {
        return 2;
    }
    if n_ple == 1 {
        if ple_layer < 0 || ple_layer >= n_layer_all {
            return 1;
        }
        // GGML_ASSERT(ple_conv_kernel > 0 && n_embd_per_layer > 0)
        if conv_kernel <= 0 || embd_per_layer <= 0 {
            return 3;
        }
        if ngram < 2 || ngram > LLAMA_MAX_PLE_NGRAM {
            return 4;
        }
        if heads <= 0 || heads > LLAMA_MAX_PLE_HEADS {
            return 5;
        }
    }
    return 0;
}

// qwen4exp.cpp:82-90 -- each head's [offset, offset+vocab) must fit the int32
// row index. Returns the first offending head, or -1.
export fn q4m_ple_head_index_errors(off: const i64[:], vocab: const i64[:], n: i64) -> i64 {
    if n < 0 || off.len < n || vocab.len < n {
        return -1;
    }
    var h: i64 = 0;
    while h < n {
        let o: i64 = off[h];
        let v: i64 = vocab[h];
        if v == 0 || o > INT32_MAX || v > INT32_MAX || o + v > INT32_MAX {
            return h;
        }
        h += 1;
    }
    return -1;
}

// qwen4exp.cpp:132-137 -- the same ranges must stay inside the padded table
// rows read back from the file. Returns the first offending head, or -1.
export fn q4m_ple_row_errors(off: const i64[:], vocab: const i64[:], n: i64, rows: i64) -> i64 {
    if n < 0 || off.len < n || vocab.len < n {
        return -1;
    }
    var h: i64 = 0;
    while h < n {
        if off[h] + vocab[h] > rows {
            return h;
        }
        h += 1;
    }
    return -1;
}

// llama-hparams.cpp:243-250 -- conv history rows, 0 without a PLE module.
export fn q4m_ple_conv_state(n_heads: i64, conv_kernel: i64, ngram: i64, hc: i64, n_embd: i64) -> i64 {
    if n_heads == 0 || conv_kernel == 0 {
        return 0;
    }
    return (conv_kernel - 1) * ngram * hc * n_embd;
}

// ---- hyper-connections, indexer, SSM ---------------------------------------

// qwen4exp.cpp:27-31. Both are GGML_ASSERT(x > 0).
export fn q4m_hc_error(hc: i64, hc_low_rank: i64) -> i64 {
    if hc <= 0 || hc_low_rank <= 0 {
        return 1;
    }
    return 0;
}

// qwen4exp.cpp:33-36.
export fn q4m_indexer_error(n_head: i64, head_size: i64, top_k: i64) -> i64 {
    if n_head <= 0 || head_size <= 0 || top_k <= 0 {
        return 1;
    }
    return 0;
}

// qwen4exp.cpp:14-18.
export fn q4m_ssm_error(d_conv: i64, d_inner: i64, d_state: i64, dt_rank: i64, n_group: i64) -> i64 {
    if d_conv > 0 && d_inner > 0 && d_state > 0 && dt_rank > 0 && n_group > 0 {
        return 0;
    }
    return 1;
}

// llama-hparams.cpp:111-113 -- n_embd_out() = impl or n_embd, and qwen4exp
// forces impl = hc * n_embd.
export fn q4m_embd_out(hc: i64, n_embd: i64) -> i64 {
    q4m_ensure(hc > 0, 110);
    q4m_ensure(n_embd > 0, 111);
    return hc * n_embd;
}

// ---- recurrent pattern ----------------------------------------------------

// qwen4exp.cpp:93-101. Without an explicit recurrent_layers array the pattern
// is derived from full_attention_interval, defaulting to 4, and the nextn
// layers past n_layer() are never recurrent. Returns -1 when the interval is
// not positive (the native GGML_ASSERT).
export fn q4m_recr_flag(il: i64, n_layer: i64, interval: i64) -> i64 {
    q4m_ensure(il >= 0, 120);
    if interval <= 0 {
        return -1;
    }
    if il < n_layer {
        if (il + 1) % interval != 0 {
            return 1;
        }
    }
    return 0;
}

// The default used when full_attention_interval is absent.
export fn q4m_interval_default() -> i64 {
    return 4;
}

// ---- model type -----------------------------------------------------------

// qwen4exp.cpp:103-106 -- only 48 layers names the A3B type, everything else
// stays LLM_TYPE_UNKNOWN.
export fn q4m_type(n_layer: i64) -> i64 {
    if n_layer == 48 {
        return 1;
    }
    return 0;
}
