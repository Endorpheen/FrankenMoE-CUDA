// Shared Gemma-family skeleton for P6-05 architectures 16 (gemma3) and
// 17 (gemma3n).
//
// Both native builders declare the same trunk geometry: every layer is a
// full-attention layer with GQA, a fused QKV row, per-head q/k norms, post
// norms after attention and after the FFN, and a GELU-PAR gated FFN. There is
// no MoE, no hybrid interval, no recurrent layer and no MTP layer.
//
// What is genuinely shared and therefore lives here:
//   - the per-layer plan is "all attention", with bounds handled once;
//   - the SWA mask that llama_hparams::set_swa_pattern builds from a period
//     (period 0 means every layer is sliding-window);
//   - the fused QKV width and the GQA key/value width;
//   - the optional lm_head duplicate decision;
//   - the input-embedding scale decision (sqrt(n_embd) for token inputs).
//
// What stays in the architecture file, because the families differ:
//   - gemma3: the conditional SWA type, the type switch by n_layer, the 27B
//     attention-scale rule, dense_2/dense_3 heads, softcap presence;
//   - gemma3n: unconditional SWA, the fixed attention scale, the KV prefix,
//     altup, laurel, per-layer embeddings and activation sparsity.
//
// Native anchors: src/models/gemma3.cpp, src/models/gemma3n.cpp,
// src/llama-hparams.cpp:8-25 (set_swa_pattern), src/llama-model.cpp:3860
// (create_tensor_qkv).

extern "fg200g_fail" fn g_fail(code: i64);

const G_ATTN: i64 = 0;
const G_NONE: i64 = -1;

// g_kind: every trunk layer of a Gemma model is attention. Out-of-range
// layers stay -1, matching the "no such layer" answer callers expect.
fn g_kind(n_layer: i64, il: i64) -> i64 {
    if n_layer < 0 {
        g_fail(40);
    }

    if il < 0 {
        return G_NONE;
    }

    if il >= n_layer {
        return G_NONE;
    }
    return G_ATTN;
}

// g_plan: fill kinds[0..n_layer) and return the count. A short destination is
// a caller bug, not a model property, so it aborts instead of truncating.
fn g_plan(n_layer: i64, kinds: i64[:]) -> i64 {
    if n_layer < 0 {
        g_fail(41);
    }

    if kinds.len < n_layer {
        g_fail(42);
    }

    var i: i64 = 0;
    while i < n_layer {
        kinds[i] = g_kind(n_layer, i);
        i = i + 1;
    }
    return n_layer;
}

// g_swa_is: llama-hparams.cpp:8-25, dense_first = false.
//   is_swa = period == 0 || il % period < period - 1
// A period of 0 means the model declares no period and every layer is SWA;
// otherwise the last layer of each group of `period` is full attention.
fn g_swa_is(period: i64, il: i64) -> i64 {
    if period < 0 {
        g_fail(43);
    }

    if il < 0 {
        g_fail(44);
    }

    if period == 0 {
        return 1;
    }

    if il % period < period - 1 {
        return 1;
    }
    return 0;
}

// g_gqa_width: n_embd_k_gqa / n_embd_v_gqa in llama-model.h, i.e. one head
// width times the number of key/value heads.
fn g_gqa_width(head: i64, n_heads: i64) -> i64 {
    if head <= 0 {
        g_fail(45);
    }

    if n_heads <= 0 {
        g_fail(46);
    }
    return head * n_heads;
}

// g_qkv_width: create_tensor_qkv packs the query, key and value blocks into
// one row, llama-model.cpp:3863.
fn g_qkv_width(q_w: i64, k_w: i64, v_w: i64) -> i64 {
    if q_w < 0 || k_w < 0 || v_w < 0 {
        g_fail(47);
    }
    return q_w + k_w + v_w;
}

// g_output_dup: the lm_head weight is optional. When it is absent the loader
// duplicates token_embd, so the shape is the same but the source tensor is
// not (gemma3.cpp:43-48, gemma3n.cpp:30-34).
fn g_output_dup(has_output: i64) -> i64 {
    if has_output == 0 {
        return 1;
    }
    return 0;
}

// g_embd_scale: gemma3.cpp:93 and gemma3n.cpp:104 scale the input embedding
// by sqrt(n_embd) for token inputs only. Raw embeddings (encoded image
// vectors) must not be rescaled, so the flag decides whether the scale node
// is inserted at all. 1 means "insert the sqrt(n_embd) scale".
fn g_embd_scale(has_tokens: i64) -> i64 {
    if has_tokens != 0 {
        return 1;
    }
    return 0;
}
