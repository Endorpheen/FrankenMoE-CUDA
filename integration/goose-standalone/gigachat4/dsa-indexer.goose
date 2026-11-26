extern "ggml_rope_ext" fn giga_rope(ctx: u64, x: u64, pos: u64, factors: u64,
                                   dims: i32, mode: i32, original_ctx: i32,
                                   base: f32, scale: f32, ext: f32, attn: f32,
                                   beta_fast: f32, beta_slow: f32) -> u64;
extern "ggml_cast" fn giga_cast(ctx: u64, x: u64, kind: i32) -> u64;
extern "ggml_lightning_indexer" fn giga_lightning(ctx: u64, q: u64, k: u64,
                                                  weights: u64, mask: u64) -> u64;
extern "ggml_relu" fn giga_relu(ctx: u64, x: u64) -> u64;
extern "ggml_top_k" fn giga_top_k(ctx: u64, x: u64, k: i32) -> u64;
extern "ggml_cont" fn giga_cont(ctx: u64, x: u64) -> u64;
extern "ggml_fill" fn giga_fill(ctx: u64, x: u64, value: f32) -> u64;
extern "ggml_new_tensor_4d" fn giga_tensor4(ctx: u64, kind: i32, a: i64, b: i64, c: i64, d: i64) -> u64;

fn giga_mask_selected(ctx: u64, mask: u64, picks: u64, cells: i64,
                       count: i64, batch: i64, streams: i64) -> u64 {
    let mask_stride = (cells * 4) as u64;
    let mask_plane = (cells * batch * 4) as u64;
    let pick_stride = (count * 4) as u64;
    let pick_plane = (count * batch * 4) as u64;
    let empty = giga_fill(ctx, mask, -1.0 / 0.0);
    let rows = giga_view4(ctx, empty, 1, cells, batch, streams, 4, mask_stride, mask_plane, 0);
    let indices = giga_view4(ctx, picks, count, batch, streams, 1,
                             pick_stride, pick_plane, pick_plane * (streams as u64), 0);
    let zeros = giga_fill(ctx, giga_tensor4(ctx, 0, 1, count, batch, streams), 0.0);
    let written = giga_set_rows(ctx, rows, zeros, indices);
    let additive = giga_view4(ctx, written, cells, batch, 1, streams,
                              mask_stride, mask_plane, mask_plane, 0);
    return giga_add(ctx, additive, mask);
}

export fn giga_dsa_index_project(ctx: u64, input: u64, qr: u64, pos: u64,
                                 wq: u64, wk: u64, knorm: u64, whead: u64,
                                 tokens: i64, dims: i32, original_ctx: i32,
                                 rope: f32[:], out: u64[:]) -> i64 {
    if ctx == 0 || input == 0 || qr == 0 || pos == 0 || wq == 0 || wk == 0 ||
       knorm == 0 || whead == 0 || tokens < 1 || dims < 2 || dims > 128 ||
       dims % 2 != 0 || rope.len < 6 || out.len < 3 { return -1; }
    let q = giga_reshape3(ctx, giga_mat(ctx, wq, qr), 128, 64, tokens);
    let kn = giga_mul(ctx, giga_rms(ctx, giga_mat(ctx, wk, input), 0.000001), knorm);
    let k = giga_reshape3(ctx, kn, 128, 1, tokens);
    out[0] = giga_rope(ctx, q, pos, 0, dims, 2, original_ctx,
                       rope[0], rope[1], rope[2], rope[3], rope[4], rope[5]);
    out[1] = giga_rope(ctx, k, pos, 0, dims, 2, original_ctx,
                       rope[0], rope[1], rope[2], rope[3], rope[4], rope[5]);
    out[2] = giga_scale(ctx, giga_mat(ctx, whead, input), 1.0 / giga_sqrt(8192.0));
    return 0;
}

export fn giga_dsa_index_mask(ctx: u64, q: u64, cached_k: u64, weights: u64,
                              mask_f32: u64, tokens: i64, streams: i64, cells: i64, cuda_dsa: bool) -> u64 {
    if ctx == 0 || q == 0 || cached_k == 0 || weights == 0 || mask_f32 == 0 ||
       tokens < 1 || tokens > 1048576 || streams < 1 || tokens % streams != 0 ||
       cells < 1 || cells > 1048576 { return 0; }
    let batch = tokens / streams;
    let qs = giga_view4(ctx, q, 128, 64, batch, streams, 512, 32768,
                        (batch * 32768) as u64, 0);
    let ws = giga_view4(ctx, weights, 64, batch, 1, streams, 256,
                        (batch * 256) as u64, (batch * 256) as u64, 0);
    var score: u64 = 0;
    if cuda_dsa && streams == 1 && batch <= 4096 {
        score = giga_lightning(ctx, qs, cached_k, ws, giga_cast(ctx, mask_f32, 1));
    } else {
        let q_rows = giga_permute(ctx, qs, 0, 2, 1, 3);
        let k_rows = giga_permute(ctx, cached_k, 0, 2, 1, 3);
        let dot = giga_mat(ctx, k_rows, q_rows);
        let by_head = giga_cont(ctx, giga_permute(ctx, dot, 2, 1, 0, 3));
        let weighted = giga_mul(ctx, giga_relu(ctx, by_head), ws);
        let summed = giga_sum_rows(ctx, weighted);
        let scores = giga_cont(ctx, giga_permute(ctx, summed, 2, 1, 0, 3));
        score = giga_add(ctx, scores, mask_f32);
    }
    var count = cells;
    if count > 2048 { count = 2048; }
    let top = giga_top_k(ctx, score, count as i32);
    if cells > 4096 { giga_op_param(top as! i64, 13, 1); }
    let picks = giga_cont(ctx, top);
    return giga_mask_selected(ctx, mask_f32, picks, cells, count, batch, streams);
}
