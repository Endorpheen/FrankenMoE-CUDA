// Populate accepted draft history without attention, FFN, or vocabulary logits.
export fn bonsai_mtp_kv(c: u64, w: const i64[:], s: const i64[:],
    t: const i64[:], g: const i64[:], b: i64[:], token: i64, position: i64) -> i64 {
    if w.len != 15 || w[0] != 0 || s.len != 2 || t.len != 4 ||
       g.len != 3 || b.len != 24 || position < 0 { return -1; }
    if bonsai_mtp_stem(c, t, b, g[0], token) != 0 ||
       b_norm(c, b[12], w[1], 0, b[1], 5120, 1, 0) != 0 ||
       mtp_mat(c, w[3], b[1], b[4], 5120, 1024) != 0 ||
       mtp_mat(c, w[4], b[1], b[5], 5120, 1024) != 0 ||
       b_norm(c, b[4], w[7], 0, b[9], 256, 4, 0) != 0 ||
       b_launch(c, 8, b[9], b[11], 4, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[11], s[0] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[5], s[1] + position * 576, 0, 0, 0, 0, 0, 0) != 0 { return -2; }
    return 0;
}
