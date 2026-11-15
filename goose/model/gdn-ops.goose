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
extern "fg158_ne" fn ne(t: i64, i: i64) -> i64;
extern "fg158_transpose" fn transpose(c: i64, a: i64) -> i64;
extern "fg158_concat" fn concat(c: i64, a: i64, b: i64, dim: i64) -> i64;
extern "fg158_cpy" fn cpy(c: i64, a: i64, b: i64) -> i64;
extern "fg158_view2" fn view2(c: i64, a: i64, n0: i64, n1: i64, nb1: i64, offset: i64) -> i64;
extern "fg158_reshape3" fn reshape3(c: i64, a: i64, n0: i64, n1: i64, n2: i64) -> i64;
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

// ---- the recurrent state update (delta-net-base.cpp:449-496) ----
//
// The state is not a separate subsystem: it is a concat at the front and a cpy of the
// tail at the back, on the same graph, every token.
//
//   conv_states = reshape(build_rs(...), kernel-1, channels, n_seqs)
//   qkv_mixed   = transpose(qkv_mixed)
//   conv_input  = concat(conv_states, qkv_mixed, 0)
//   ... ssm_conv(conv_input, kernel) ...
//   cpy(view_3d(conv_input, tail), view_2d(conv_states_all, slot offset))
//
// The reshaped state is passed in already reshaped by the caller's slot view; the
// decisions that live here are the transpose, the concat axis, which rows are the
// tail, and the slot arithmetic.

// conv_input = concat(state, transpose(mixed), dim 0)
export fn gdn_conv_input(c: i64, state: i64, mixed: i64, elem: i64) -> i64 {
    if state == 0 || mixed == 0 || elem <= 0 {
        fail(80);
    }
    let t: i64 = transpose(c, mixed);
    if t == 0 {
        fail(81);
    }
    let r: i64 = concat(c, state, t, 0);
    if r == 0 {
        fail(82);
    }
    return r;
}

// The tail of conv_input becomes the next state: the last kernel-1 kernel rows.
//
// conv_input is [kernel-1+T, channels], so axis 0 is the kernel position and axis 1
// is the channel. nb[1] is therefore elem * ne0, NOT elem * channels - getting this
// backwards produced a view that read the wrong 24576 elements while every other
// node in the chain still matched, which is exactly what a tail comparison catches
// and a conv-output comparison does not.
export fn gdn_conv_tail(c: i64, conv_input: i64, kernel: i64, channels: i64,
                        n_seqs: i64, elem: i64) -> i64 {
    if kernel < 2 || channels <= 0 || n_seqs <= 0 {
        fail(83);
    }
    let keep: i64 = kernel - 1;
    let total_rows: i64 = ne(conv_input, 0);
    if total_rows < kernel {
        fail(88);
    }
    let nb1: i64 = elem * total_rows;
    let nb2: i64 = nb1 * channels;
    let off_rows: i64 = total_rows - keep;
    return view4(c, conv_input, keep, channels, n_seqs, 1, nb1, nb2, nb2, elem * off_rows);
}

// The destination slot inside the flat per-layer state tensor.
export fn gdn_conv_slot(c: i64, states_all: i64, kernel: i64, channels: i64,
                        n_seqs: i64, slot: i64, mem_size: i64, kv_head: i64,
                        elem: i64) -> i64 {
    if kernel < 2 || channels <= 0 || n_seqs <= 0 || mem_size <= 0 {
        fail(84);
    }
    if slot < 0 || kv_head < 0 || kv_head >= mem_size {
        fail(85);
    }
    let row_count: i64 = (kernel - 1) * channels;
    let row_size: i64 = elem * row_count;
    let nb1: i64 = elem * row_count;
    let offset: i64 = (slot * mem_size + kv_head) * row_size;
    return view2(c, states_all, row_count, n_seqs, nb1, offset);
}

// Write the tail into the slot. Returns the cpy node, which must be part of the graph.
export fn gdn_conv_store(c: i64, tail: i64, dst: i64) -> i64 {
    if tail == 0 || dst == 0 {
        fail(86);
    }
    let r: i64 = cpy(c, tail, dst);
    if r == 0 {
        fail(87);
    }
    return r;
}
