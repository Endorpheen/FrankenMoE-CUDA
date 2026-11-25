// The validated weight slots follow bonsai_bind_layout. Activations and state
// are CUDA addresses, and only the scalar device driver ABI crosses this unit.
extern "bonsai_launch" fn b_launch(context: u64, op: i64, a: i64, b: i64, c: i64,
    d: i64, e: i64, f: i64, g: i64, h: i64) -> i64;

fn b_norm(c: u64, x: i64, w: i64, gate: i64, y: i64, n: i64, rows: i64, mode: i64) -> i64 {
    return b_launch(c, 0, x, w, gate, y, n, rows, mode, 0);
}
fn b_rotate(c: u64, x: i64, sign: i64, y: i64, n: i64, inverse: i64, grouped: i64) -> i64 {
    return b_launch(c, 1, x, sign, y, n, inverse, grouped, 0, 0);
}
fn b_mat(c: u64, w: i64, x: i64, y: i64, n: i64, rows: i64) -> i64 {
    return b_launch(c, 2, w, x, y, n, rows, 0, 0, 0);
}
fn b_add(c: u64, x: i64, y: i64, out: i64) -> i64 {
    return b_launch(c, 3, x, y, out, 0, 0, 0, 0, 0);
}

fn b_gdn(c: u64, w: const i64[:], b: i64[:], s: i64[:]) -> i64 {
    // Scratch 1 is primal normalized input and 2 is its Hadamard transform.
    // Slots 3..12 are shared by the two branch types. Slot 23 owns the
    // reusable next-state buffer, which exchanges ownership after launch.
    if b_mat(c, w[2], b[2], b[3], 5120, 10240) != 0 ||
       b_mat(c, w[3], b[2], b[4], 5120, 6144) != 0 ||
       b_launch(c, 4, b[1], w[5], w[4], w[6], w[7], b[5], b[6], 0) != 0 ||
       b_launch(c, 5, b[3], w[8], s[0], b[7], 0, 0, 0, 0) != 0 ||
       b_norm(c, b[7], 0, 0, b[8], 128, 32, 1) != 0 ||
       b_launch(c, 6, b[8], b[8] + 8192, b[7] + 16384, b[5], b[6], s[1], b[9], b[23]) != 0 {
        return -1;
    }
    let old = s[1];
    s[1] = b[23]; b[23] = old;
    if b_norm(c, b[9], w[9], b[4], b[10], 128, 48, 2) != 0 ||
       b_rotate(c, b[10], b[21], b[11], 6144, 0, 1) != 0 ||
       b_mat(c, w[10], b[11], b[12], 6144, 5120) != 0 { return -1; }
    return 0;
}

fn b_attention(c: u64, w: const i64[:], b: i64[:], s: const i64[:], position: i64) -> i64 {
    if b_mat(c, w[2], b[2], b[3], 5120, 12288) != 0 ||
       b_mat(c, w[3], b[2], b[4], 5120, 1024) != 0 ||
       b_mat(c, w[4], b[2], b[5], 5120, 1024) != 0 ||
       b_launch(c, 7, b[3], b[6], b[7], 0, 0, 0, 0, 0) != 0 ||
       b_norm(c, b[6], w[6], 0, b[8], 256, 24, 0) != 0 ||
       b_norm(c, b[4], w[7], 0, b[9], 256, 4, 0) != 0 ||
       b_launch(c, 8, b[8], b[10], 24, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 8, b[9], b[11], 4, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[11], s[0] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[5], s[1] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 10, b[10], s[0], s[1], b[8], position + 1, 0, 0, 0) != 0 ||
       b_launch(c, 11, b[8], b[7], b[6], 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[6], b[21], b[11], 6144, 0, 0) != 0 ||
       b_mat(c, w[5], b[11], b[12], 6144, 5120) != 0 { return -1; }
    return 0;
}

export fn bonsai_graph(c: u64, weights: const i64[:], states: i64[:], globals: const i64[:],
    b: i64[:], token: i64, position: i64, want_logits: i64) -> i64 {
    if weights.len != 64 * 15 || states.len != 64 * 2 || globals.len != 3 || b.len != 24 ||
       token < 0 || token >= 248320 || position < 0 { return -1; }
    if b_launch(c, 12, globals[0], b[1], token, 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[0], 5120, 1, 0) != 0 { return -1; }
    for layer in 64 {
        let w = weights[layer * 15 .. layer * 15 + 15];
        let s = states[layer * 2 .. layer * 2 + 2];
        if b_norm(c, b[0], w[1], 0, b[1], 5120, 1, 0) != 0 ||
           b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 { return -2 - layer; }
        let branch = if w[0] == 1 { b_gdn(c, w, b, s) } else { b_attention(c, w, b, s, position) };
        let post = if w[0] == 1 { w[11] } else { w[8] };
        if branch != 0 ||
           b_add(c, b[0], b[12], b[13]) != 0 ||
           b_norm(c, b[13], post, 0, b[1], 5120, 1, 0) != 0 ||
           b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||
           b_mat(c, w[12], b[2], b[14], 5120, 17408) != 0 ||
           b_mat(c, w[13], b[2], b[15], 5120, 17408) != 0 ||
           b_launch(c, 13, b[14], b[15], b[16], 0, 0, 0, 0, 0) != 0 ||
           b_rotate(c, b[16], b[22], b[17], 17408, 0, 0) != 0 ||
           b_mat(c, w[14], b[17], b[18], 17408, 5120) != 0 ||
           b_add(c, b[13], b[18], b[0]) != 0 { return -2 - layer; }
    }
    if want_logits != 0 &&
       (b_norm(c, b[0], globals[1], 0, b[1], 5120, 1, 0) != 0 ||
        b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||
        b_mat(c, globals[2], b[2], b[19], 5120, 248320) != 0) { return -66; }
    return 0;
}
