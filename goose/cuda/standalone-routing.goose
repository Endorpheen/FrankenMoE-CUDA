extern "fg_attn_word" fn rt_word(cfg: u64, index: i64) -> i64;
extern "fg_rope_float" fn rt_float(cfg: u64, index: i64) -> f32;
extern "fg_device_global_load" fn rt_load(address: u64) -> f32;
extern "fg_device_global_store" fn rt_store(address: u64, value: f32);
extern "fg_rope_position" fn rt_read_i32(address: u64, index: i64) -> i32;
extern "fg283d_st_i32" fn rt_store_index(address: u64, offset: u64, value: i32);
extern "fg_attn_neg_inf" fn rt_neg_inf() -> f32;
extern "fg_attn_exp" fn rt_exp(value: f32) -> f32;
extern "fg248d_fail" fn rt_fail(code: i64);
extern "fg_device_shfl" fn shfl_xor(value: f32, delta: i32) -> f32;
extern "fg_vector_max" fn fmaxf(a: f32, b: f32) -> f32;
extern "fg_vector_shuffle_f64" fn cv_shuffle_f64(value: f64, delta: i32) -> f64;

fn rt_row(x: u64, cfg: u64, row: i64) -> u64 {
    let dim1 = rt_word(cfg, 5);
    let dim2 = rt_word(cfg, 6);
    return x + (((row % dim1) * rt_word(cfg, 9) + ((row / dim1) % dim2) * rt_word(cfg, 10) +
        (row / (dim1 * dim2)) * rt_word(cfg, 11)) as u64);
}

export fn fg_route_reduce(x: u64, out: u64, cfg: u64, row: i64, lane: i64) {
    let width = rt_word(cfg, 4);
    let step = rt_word(cfg, 8);
    let src = rt_row(x, cfg, row);
    var sum: f64 = 0.0;
    var i = lane;
    if rt_word(cfg, 0) == 3 {
        while i < width {
            sum += rt_load(src + ((i * step) as u64)) as f64;
            i += 32;
        }
        sum = cv_warp_sum(sum);
        if lane == 0 { rt_store(out + ((4 * row) as u64), sum as f32); }
        return;
    }
    let scale = rt_float(cfg, 21);
    var maximum = rt_neg_inf();
    while i < width {
        maximum = fmaxf(maximum, rt_load(src + ((i * step) as u64)) * scale);
        i += 32;
    }
    maximum = warp_max(maximum);
    let dst = out + ((4 * row * width) as u64);
    i = lane;
    while i < width {
        let value = rt_exp(rt_load(src + ((i * step) as u64)) * scale - maximum);
        rt_store(dst + ((4 * i) as u64), value);
        sum += value as f64;
        i += 32;
    }
    sum = cv_warp_sum(sum);
    if !(sum > 0.0) { rt_fail(1511); return; }
    let inv = (1.0 / sum) as f32;
    i = lane;
    while i < width {
        let at = dst + ((4 * i) as u64);
        rt_store(at, rt_load(at) * inv);
        i += 32;
    }
}

export fn fg_route_sort(x: u64, out: u64, cfg: u64, row: i64, lane: i64, lanes: i64,
                        scratch: u64, flags: u64) {
    let width = rt_word(cfg, 4);
    let results = rt_word(cfg, 12);
    argsort_row(rt_row(x, cfg, row), out + ((4 * row * results) as u64),
        width, rt_word(cfg, 20), results, rt_word(cfg, 21), lanes, lane, scratch, flags);
}

// Select from normalized probabilities. One warp owns one 256-expert row.
export fn fg_route_topk(x: u64, out: u64, cfg: u64, row: i64, lane: i32) {
    if row >= rt_word(cfg, 5) { return; }
    let src = rt_row(x, cfg, row);
    var scores: f32[8] = default <f32[8]>();
    for i in 8 { scores[i] = rt_load(src + ((4 * (i * 32 + (lane as i64))) as u64)); }
    for slot in 8 {
        var key = scores[0];
        var expert = lane;
        for i in 1 .. 8 {
            if scores[i] > key {
                key = scores[i];
                expert = lane + 32 * (i as i32);
            }
        }
        let winner = topk_winner(key, key, expert);
        if winner.expert % 32 == lane { scores[winner.expert / 32] = rt_neg_inf(); }
        if lane == 0 { rt_store_index(out, (4 * (row * 8 + slot)) as u64, winner.expert); }
    }
}

export fn fg_route_gather(x: u64, y: u64, out: u64, cfg: u64, index: i64) {
    if index >= rt_word(cfg, 20) { return; }
    let width = rt_word(cfg, 12);
    let selected = rt_word(cfg, 13);
    let dim2 = rt_word(cfg, 14);
    let row = index / width;
    let expert = row % selected;
    let token = (row / selected) % dim2;
    let sequence = row / (selected * dim2);
    let offset = expert * rt_word(cfg, 16) + token * rt_word(cfg, 17) + sequence * rt_word(cfg, 18);
    let id = rt_read_i32(y, offset / 4) as i64;
    if id < 0 || id >= rt_word(cfg, 5) { rt_fail(1511); return; }
    let at = (index % width) * rt_word(cfg, 8) + id * rt_word(cfg, 9) +
        token * rt_word(cfg, 10) + sequence * rt_word(cfg, 11);
    rt_store(out + ((4 * index) as u64), rt_load(x + (at as u64)));
}
