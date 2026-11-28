extern fn pred_load(address: u64, index: i64) -> f32;
extern fn pred_load_logit(address: u64, index: i32) -> f32;
extern fn pred_store_logit(address: u64, index: i32, value: f32);
extern fn pred_shared_load(address: u64, index: i32) -> f32;
extern fn pred_shared_store(address: u64, index: i32, value: f32);
extern fn pred_shuffle(value: f32, offset: i32) -> f32;
extern fn pred_sync();
extern fn pred_fence_gpu();
extern fn pred_fence_system();
extern fn pred_mark_done(address: u64) -> i32;
extern fn pred_reset(address: u64);
extern fn pred_publish(address: u64, index: i32, value: i32);
extern fn pred_save_last(address: u64, value: i32);
extern fn pred_load_last(address: u64) -> i32;

export fn apex_predict_route(weights: u64, input: u64, logits: u64, completed: u64, mailbox: u64,
    partials: u64, ranked: u64, last: u64, width: i32, limit: i32, sequence: i32, row: i32, thread: i32) {
    let base: i64 = (row as i64) * width;
    var sum: f32 = 0.0;
    var index: i64 = (thread as i64) * 4;
    while index < width {
        sum += pred_load(weights, base + index) * pred_load(input, index)
            + pred_load(weights, base + index + 1) * pred_load(input, index + 1)
            + pred_load(weights, base + index + 2) * pred_load(input, index + 2)
            + pred_load(weights, base + index + 3) * pred_load(input, index + 3);
        index += 1024;
    }
    var offset: i32 = 16;
    while offset > 0 {
        sum += pred_shuffle(sum, offset);
        offset >>= 1;
    }
    if (thread & 31) == 0 { pred_shared_store(partials, thread >> 5, sum); }
    pred_sync();
    if thread == 0 {
        var total: f32 = 0.0;
        for warp in 8 { total += pred_shared_load(partials, warp as i32); }
        pred_store_logit(logits, row, total);
        pred_fence_gpu();
        pred_save_last(last, if pred_mark_done(completed) == 255 { 1 } else { 0 });
    }
    pred_sync();
    if pred_load_last(last) == 0 { return; }
    pred_fence_gpu();
    pred_shared_store(ranked, thread, pred_load_logit(logits, thread));
    pred_sync();
    let score = pred_shared_load(ranked, thread);
    var rank: i32 = 0;
    for expert in 256 {
        let other = pred_shared_load(ranked, expert as i32);
        if other > score || (other == score && expert < thread) { rank += 1; }
    }
    if rank < limit { pred_publish(mailbox, 1 + rank, thread); }
    pred_fence_system();
    pred_sync();
    if thread == 0 {
        pred_reset(completed);
        pred_publish(mailbox, 0, sequence);
        pred_fence_system();
    }
}
