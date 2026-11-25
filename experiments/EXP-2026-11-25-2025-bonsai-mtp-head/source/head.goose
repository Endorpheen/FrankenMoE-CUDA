// Q4 draft projections use the original activation basis.
extern "bonsai_mtp_mat" fn mtp_mat(context: u64, weight: i64, input: i64,
    output: i64, width: i64, rows: i64) -> i64;

export fn bonsai_mtp_head(c: u64, w: const i64[:], s: const i64[:],
    t: const i64[:], g: const i64[:], b: i64[:], token: i64, position: i64) -> i64 {
    if w.len != 15 || w[0] != 0 || s.len != 2 || t.len != 4 ||
       g.len != 3 || b.len != 24 || position < 0 { return -1; }
    if bonsai_mtp_stem(c, t, b, g[0], token) != 0 ||
       b_norm(c, b[12], w[1], 0, b[1], 5120, 1, 0) != 0 ||
       mtp_mat(c, w[2], b[1], b[3], 5120, 12288) != 0 ||
       mtp_mat(c, w[3], b[1], b[4], 5120, 1024) != 0 ||
       mtp_mat(c, w[4], b[1], b[5], 5120, 1024) != 0 ||
       b_launch(c, 7, b[3], b[6], b[7], 0, 0, 0, 0, 0) != 0 ||
       b_norm(c, b[6], w[6], 0, b[8], 256, 24, 0) != 0 ||
       b_norm(c, b[4], w[7], 0, b[9], 256, 4, 0) != 0 ||
       b_launch(c, 8, b[8], b[10], 24, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 8, b[9], b[11], 4, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[11], s[0] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[5], s[1] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 10, b[10], s[0], s[1], b[8], position + 1, 0, 0, 0) != 0 ||
       b_launch(c, 11, b[8], b[7], b[6], 0, 0, 0, 0, 0) != 0 ||
       mtp_mat(c, w[5], b[6], b[18], 6144, 5120) != 0 ||
       b_add(c, b[12], b[18], b[13]) != 0 { return -2; }
    if b_norm(c, b[13], w[8], 0, b[1], 5120, 1, 0) != 0 ||
       mtp_mat(c, w[12], b[1], b[14], 5120, 17408) != 0 ||
       mtp_mat(c, w[13], b[1], b[15], 5120, 17408) != 0 ||
       b_launch(c, 13, b[14], b[15], b[16], 0, 0, 0, 0, 0) != 0 ||
       mtp_mat(c, w[14], b[16], b[18], 17408, 5120) != 0 ||
       b_add(c, b[13], b[18], b[17]) != 0 ||
       b_norm(c, b[17], t[3], 0, b[1], 5120, 1, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[2], 5120, 0, 0) != 0 ||
       b_mat(c, g[2], b[2], b[19], 5120, 248320) != 0 { return -3; }
    return 0;
}
