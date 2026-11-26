extern "ggml_set_rows" fn giga_set_rows(ctx: u64, dest: u64, src: u64, rows: u64) -> u64;
extern "ggml_flash_attn_ext" fn giga_flash(ctx: u64, q: u64, k: u64, v: u64, mask: u64,
                                          scale: f32, bias: f32, softcap: f32) -> u64;
extern "ggml_flash_attn_ext_set_n_kv_max" fn giga_flash_limit(x: u64, count: i32);
extern "ggml_prec_set_acc" fn giga_set_acc(x: u64, precision: i32) -> bool;

extern "ggml_mul_mat_set_hint" fn giga_mat_hint(x: u64, hint: i32);

fn giga_rotate_index(ctx: u64, x: u64, rotation: u64, heads: i64, tokens: i64) -> u64 {
    let matrix = giga_reshape2(ctx, x, 128, heads * tokens);
    let rotated = giga_mat(ctx, rotation, matrix);
    giga_mat_hint(rotated, 1);
    return giga_reshape3(ctx, rotated, 128, heads, tokens);
}

// Cache storage is flattened across streams. Rows are global physical slot indices.
export fn giga_mla_layer(ctx: u64, graph: u64, projected: u64[:], index: u64[:],
                         kv_store: u64, idx_store: u64, idx_rotation: u64, rows: u64, mask: u64,
                         tokens: i64, cells: i64, capacity: i64, streams: i64,
                         kv_stride: u64, idx_stride: u64, scale: f32,
                         wv: u64, wo: u64, cuda_dsa: bool) -> u64 {
    if ctx == 0 || graph == 0 || kv_store == 0 || idx_store == 0 || rows == 0 || mask == 0 ||
       wv == 0 || wo == 0 || projected.len < 4 || index.len < 3 ||
       tokens < 1 || tokens > 1048576 || streams < 1 || tokens % streams != 0 ||
       cells < 1 || capacity < cells || capacity > 1048576 ||
       kv_stride == 0 || idx_stride == 0 || !(scale > 0.0) { return 0; }
    for i in 4 { if projected[i] == 0 { return 0; } }
    for i in 3 { if index[i] == 0 { return 0; } }

    let k_rows = giga_view2(ctx, projected[1], 576, tokens, 2304, 0);
    var index_q = index[0];
    var index_k = index[1];
    if idx_rotation != 0 {
        index_q = giga_rotate_index(ctx, index_q, idx_rotation, 64, tokens);
        index_k = giga_rotate_index(ctx, index_k, idx_rotation, 1, tokens);
    }
    let idx_rows = giga_view2(ctx, index_k, 128, tokens, 512, 0);
    let written_k = giga_set_rows(ctx, kv_store, k_rows, rows);
    let written_idx = giga_set_rows(ctx, idx_store, idx_rows, rows);
    let kv_plane = kv_stride * (capacity as u64);
    var selected = giga_cast(ctx, mask, 1);
    if cells <= 2048 {
        giga_expand(graph, written_idx);
    } else {
        let idx_plane = idx_stride * (capacity as u64);
        let idx_keys = giga_view4(ctx, written_idx, 128, 1, cells, streams,
                                  idx_stride, idx_stride, idx_plane, 0);
        selected = giga_dsa_index_mask(ctx, index_q, idx_keys, index[2], mask,
                                       tokens, streams, cells, cuda_dsa);
    }

    let batch = tokens / streams;
    let q = giga_view4(ctx, projected[0], 576, 32, batch, streams, 2304, 73728,
                       (73728 * batch) as u64, 0);
    let k = giga_view4(ctx, written_k, 576, cells, 1, streams, kv_stride, kv_plane, kv_plane, 0);
    let v = giga_view4(ctx, written_k, 512, cells, 1, streams, kv_stride, kv_plane, kv_plane, 0);
    let values = giga_flash(ctx, giga_permute(ctx, q, 0, 2, 1, 3), k, v,
                            selected, scale, 0.0, 0.0);
    var count = cells;
    if count > 2048 { count = 2048; }
    giga_flash_limit(values, count as i32);
    giga_set_acc(values, 10);

    let decoded = giga_mat(ctx, wv, giga_permute(ctx, values, 0, 2, 1, 3));
    let flattened = giga_reshape2(ctx,
        giga_cont(ctx, giga_permute(ctx, decoded, 0, 2, 1, 3)), 4096, tokens);
    return giga_mat(ctx, wo, giga_mul(ctx, flattened, giga_sigmoid(ctx, projected[3])));
}
