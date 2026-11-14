// Shared dense-transformer skeleton for P6-05 architectures 3 (qwen3) and
// 6 (qwen2).
//
// Both native builders have the same trunk plan: every layer is a
// full-attention layer. There is no hybrid interval, no recurrent branch and
// no MTP layer, so the plan is a constant per layer and this module owns it.
//
// What is genuinely shared and therefore lives here:
//   - the per-layer plan is "all attention", with bounds handled once;
//   - the fused QKV width is q_width + k_width + v_width (create_tensor_qkv);
//   - the GQA key/value width is head_dim * n_head_kv.
//
// What stays in the architecture file, because the families differ:
//   - the query width: qwen3 uses n_embd_head_k * n_head, qwen2 uses n_embd;
//   - qk-norm presence (qwen3 only) and the optional heads (cls_out, dense_2);
//   - the bias set (qwen2 only: fused qkv bias, legacy q/k/v biases, output
//     bias; qwen2 declares no wo bias).
//
// Native anchors: src/models/qwen3.cpp and src/models/qwen2.cpp for the
// loaders, src/llama-model.cpp:3860-3896 for create_tensor_qkv.

extern "fg200g_fail" fn fail(code: i64);

const D_ATTN: i64 = 0;
const D_NONE: i64 = -1;

// d_kind: every trunk layer of a dense model is attention. Out-of-range layers
// stay -1, matching the "no such layer" answer the callers expect.
fn d_kind(n_layer: i64, il: i64) -> i64 {
    if n_layer < 0 {
        fail(40);
    }

    if il < 0 {
        return D_NONE;
    }

    if il >= n_layer {
        return D_NONE;
    }
    return D_ATTN;
}

// d_plan: fill kinds[0..n_layer) and return the count. A short destination is
// a caller bug, not a model property, so it aborts instead of truncating.
fn d_plan(n_layer: i64, kinds: i64[:]) -> i64 {
    if n_layer < 0 {
        fail(41);
    }

    if kinds.len < n_layer {
        fail(42);
    }

    var i: i64 = 0;
    while i < n_layer {
        kinds[i] = d_kind(n_layer, i);
        i = i + 1;
    }
    return n_layer;
}

// d_gqa_width: n_embd_k_gqa / n_embd_v_gqa in llama-model.h:990-992, i.e. one
// head width times the number of key/value heads.
fn d_gqa_width(head: i64, n_heads: i64) -> i64 {
    if head <= 0 {
        fail(43);
    }

    if n_heads <= 0 {
        fail(44);
    }
    return head * n_heads;
}

// d_qkv_width: create_tensor_qkv packs the query, key and value blocks into one
// row, llama-model.cpp:3863.
fn d_qkv_width(q_w: i64, k_w: i64, v_w: i64) -> i64 {
    if q_w < 0 || k_w < 0 || v_w < 0 {
        fail(45);
    }
    return q_w + k_w + v_w;
}

// d_output_dup: the lm_head weight is optional. When it is absent the loader
// duplicates token_embd (qwen3.cpp:23-26, qwen2.cpp:28-31), so the shape is the
// same but the source tensor is not.
fn d_output_dup(has_output: i64) -> i64 {
    if has_output == 0 {
        return 1;
    }
    return 0;
}
