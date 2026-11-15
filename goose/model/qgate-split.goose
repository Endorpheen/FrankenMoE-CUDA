// The joint QG split of the Qwen hybrid family (qwen35moe, qwen3next).
//
// From llama.cpp/src/models/qwen35moe.cpp:293-300:
//
//   ggml_tensor * Qcur = ggml_view_3d(ctx0, Qcur_full, n_embd_head, n_head, n_tokens,
//       ggml_element_size(Qcur_full) * n_embd_head * 2,
//       ggml_element_size(Qcur_full) * n_embd_head * 2 * n_head, 0);
//
// The Q projection emits query and gate interleaved per head, so the head stride is
// two head widths and the gate sits one head width inside each head. Splitting the
// tensor into halves would run and compute nonsense, which is why this is a module
// decision and not a host convenience.
//
// The module owns the numbers; the host owns the view primitive.
extern "fg158_view4" fn view4(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64,
                              nb1: i64, nb2: i64, nb3: i64, offset: i64) -> i64;
// Reuse the graph builder's abort so a refusal stops loudly instead of returning a
// plausible handle.
extern "fg158_fail" fn fail(code: i64);

export fn qg_split(c: i64, qfull: i64, head_dim: i64, n_head: i64, n_tokens: i64,
                   elem: i64, out: i64[:]) -> i64 {
    if head_dim <= 0 {
        fail(60);
    }
    if n_head <= 0 {
        fail(61);
    }
    if n_tokens <= 0 {
        fail(62);
    }
    if elem <= 0 {
        fail(63);
    }
    if out.len < 2 {
        fail(64);
    }

    let head_stride = elem * head_dim * 2;
    let tok_stride  = head_stride * n_head;
    let seq_stride  = tok_stride * n_tokens;

    out[0] = view4(c, qfull, head_dim, n_head, n_tokens, 1,
                   head_stride, tok_stride, seq_stride, 0);
    out[1] = view4(c, qfull, head_dim, n_head, n_tokens, 1,
                   head_stride, tok_stride, seq_stride, elem * head_dim);
    return 2;
}
