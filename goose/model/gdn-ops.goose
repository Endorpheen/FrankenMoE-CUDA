// The two graph operations the recurrent (GDN) path needs and the graph builder has
// no mode for: L2 normalisation and repeat-to-extents.
//
// From llama.cpp/src/models/qwen35moe.cpp:459-470:
//
//   q_conv = ggml_l2_norm(ctx0, q_conv, eps_norm);
//   k_conv = ggml_l2_norm(ctx0, k_conv, eps_norm);
//   if (num_k_heads != num_v_heads) {
//       q_conv = ggml_repeat_4d(ctx0, q_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
//       k_conv = ggml_repeat_4d(ctx0, k_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
//   }
//
// The module owns the epsilon policy and the target extents; the host owns the two
// ggml calls.
extern "fg158_l2norm" fn l2norm(c: i64, a: i64, eps_bits: i64) -> i64;
extern "fg158_repeat4" fn repeat4(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64;
extern "fg158_fail" fn fail(code: i64);
extern "fg158_view4" fn view4(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64,
                              nb1: i64, nb2: i64, nb3: i64, offset: i64) -> i64;

// L2-normalise one tensor. The epsilon is passed as f32 bits so the host does not
// reinterpret a float through an integer parameter.
export fn gdn_l2norm(c: i64, a: i64, eps_bits: i64) -> i64 {
    if a == 0 {
        fail(70);
    }
    let r: i64 = l2norm(c, a, eps_bits);
    if r == 0 {
        fail(71);
    }
    return r;
}

// Repeat q or k to the value head count. The guard is the native one: the value
// count must be a multiple of the key count, otherwise the repeat is not a repeat.
export fn gdn_repeat_to_v(c: i64, a: i64, head_dim: i64, n_k_heads: i64,
                         n_v_heads: i64, n_tokens: i64, n_seqs: i64) -> i64 {
    if n_k_heads <= 0 {
        fail(72);
    }
    if n_v_heads % n_k_heads != 0 {
        fail(73);
    }
    if n_v_heads == n_k_heads {
        return a;   // nothing to repeat; the native builder skips the op too
    }
    let r: i64 = repeat4(c, a, head_dim, n_v_heads, n_tokens, n_seqs);
    if r == 0 {
        fail(74);
    }
    return r;
}

// The qkv views over the convolved output (qwen35moe.cpp:432-452).
//
// The mixed output carries q, k and v at the stride 2*head_k*n_k + head_v*n_v, so
// each view needs its own offset and the same outer strides. These numbers were
// carried in the gate before this export; they are a decision, so they live here.
//
// `which` selects q (0), k (1) or v (2). One symbol with a selector rather than an
// array out-parameter: the C side then calls it with plain integers, and the
// decision - which extents, which offset - never leaves this function.
export fn gdn_qkv_view(c: i64, mixed: i64, which: i64, head_k: i64, n_k: i64,
                       head_v: i64, n_v: i64, n_tokens: i64, n_seqs: i64,
                       elem: i64) -> i64 {
    if head_k <= 0 || n_k <= 0 || head_v <= 0 || n_v <= 0 {
        fail(75);
    }
    if n_tokens <= 0 || n_seqs <= 0 || elem <= 0 {
        fail(76);
    }

    let nb1 = elem * (2 * head_k * n_k + head_v * n_v);
    let nb2 = nb1 * n_tokens;
    let nb3 = nb2 * n_seqs;

    if which == 0 {
        return view4(c, mixed, head_k, n_k, n_tokens, n_seqs,
                     elem * head_k, nb1, nb2, 0);
    }
    if which == 1 {
        return view4(c, mixed, head_k, n_k, n_tokens, n_seqs,
                     elem * head_k, nb1, nb2, elem * head_k * n_k);
    }
    if which == 2 {
        return view4(c, mixed, head_v, n_v, n_tokens, n_seqs,
                     elem * head_v, nb1, nb2, elem * 2 * head_k * n_k);
    }
    fail(78);
    return 0;
}
