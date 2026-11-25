extern "bonsai_mtp_concat_launch" fn mtp_concat(c: u64, embeddings: i64, hidden: i64,
    joined: i64) -> i64;
extern "bonsai_mtp_mat_batch" fn mtp_mat_batch(c: u64, weight: i64, input: i64,
    output: i64, width: i64, rows: i64) -> i64;

export fn bonsai_mtp_kv_batch(c: u64, w: const i64[:], s: const i64[:], t: const i64[:],
    g: const i64[:], b: i64[:], token: i64, position: i64) -> i64 {
    if w.len != 15 || w[0] != 0 || s.len != 2 || t.len != 4 ||
       g.len != 3 || b.len != 24 || position < 0 { return -1; }
    if b_launch(c, 12, g[0], b[1], token, 0, 0, 0, 0, 0) != 0 ||
       b_rotate(c, b[1], b[20], b[2], 5120, 1, 0) != 0 ||
       b_norm(c, b[2], t[1], 0, b[12], 5120, 1, 0) != 0 ||
       b_norm(c, b[13], t[2], 0, b[3], 5120, 1, 0) != 0 ||
       mtp_concat(c, b[12], b[3], b[7]) != 0 ||
       mtp_mat_batch(c, t[0], b[7], b[18], 10240, 5120) != 0 ||
       b_norm(c, b[18], w[1], 0, b[1], 5120, 1, 0) != 0 ||
       mtp_mat_batch(c, w[3], b[1], b[4], 5120, 1024) != 0 ||
       mtp_mat_batch(c, w[4], b[1], b[5], 5120, 1024) != 0 ||
       b_norm(c, b[4], w[7], 0, b[9], 256, 4, 0) != 0 ||
       b_launch(c, 8, b[9], b[11], 4, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[11], s[0] + position * 576, 0, 0, 0, 0, 0, 0) != 0 ||
       b_launch(c, 9, b[5], s[1] + position * 576, 0, 0, 0, 0, 0, 0) != 0 { return -2; }
    return 0;
}
