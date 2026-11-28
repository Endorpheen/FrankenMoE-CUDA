extern "i2d_load_f32" fn load(address: u64, index: i64) -> f32;
extern "i2d_load_i64" fn position_at(address: u64, index: i64) -> i64;
extern "i2d_store_f32" fn store(address: u64, index: i64, value: f32);
extern "__nv_expf" fn weight_exponential(value: f32) -> f32;
extern "inkling_negative_infinity_g" fn negative_infinity() -> f32;

fn ordered_dot(a: u64, ai: i64, b: u64, bi: i64, width: i64) -> f32 {
    var total: f64 = 0.0;
    for channel in width {
        let product: f32 = load(a, ai + channel) * load(b, bi + channel);
        total += product as f64;
    }
    return total as f32;
}

export fn i2_device_score(query: u64, keys: u64, relative: u64, bias: u64, sink: u64,
                          positions: u64, scores: u64, rows: i64, absolute: i64, window: i64,
                          head: i64, group: i64, lane: i64, qk_scale: f32) {
    let row = group * 128 + lane;
    if row >= rows {
        return;
    }
    let position = position_at(positions, row);
    var score = negative_infinity();
    if position >= 0 && position <= absolute {
        let distance = absolute - position;
        if window == 0 || distance < window {
            let key_offset = row * 1024 + (head / 3) * 128;
            let qk = ordered_dot(query, head * 128, keys, key_offset, 128);
            let shift: f32 = if distance < 1024 {
                ordered_dot(relative, head * 16, bias, distance * 16, 16)
            } else {
                0.0
            };
            score = qk * qk_scale + (shift - load(sink, head));
        }
    }
    store(scores, head * rows + row, score);
}

extern "i2d_shared_f32" fn shared_float(base: u64, index: i64) -> f32;
extern "i2d_set_shared_f32" fn set_shared_float(base: u64, index: i64, value: f32);
extern "i2d_shared_f64" fn shared_double(base: u64, index: i64) -> f64;
extern "i2d_set_shared_f64" fn set_shared_double(base: u64, index: i64, value: f64);
extern "i2d_sync" fn sync_attention();

fn max_shared(address: u64, value: f32, lane: i64) -> f32 {
    set_shared_float(address, lane, value);
    sync_attention();
    var offset: i64 = 64;
    while offset > 0 {
        if lane < offset {
            let other = shared_float(address, lane + offset);
            if other > shared_float(address, lane) { set_shared_float(address, lane, other); }
        }
        sync_attention();
        offset /= 2;
    }
    return shared_float(address, 0);
}

fn sum_shared(address: u64, value: f64, lane: i64) -> f64 {
    set_shared_double(address, lane, value);
    sync_attention();
    var offset: i64 = 64;
    while offset > 0 {
        if lane < offset {
            set_shared_double(address, lane, shared_double(address, lane) + shared_double(address, lane + offset));
        }
        sync_attention();
        offset /= 2;
    }
    return shared_double(address, 0);
}
extern "i2d_store_f64" fn store_double(address: u64, index: i64, value: f64);
extern "i2d_load_f64" fn load_double(address: u64, index: i64) -> f64;

export fn i2_device_softmax(scores: u64, inverse: u64, head: i64, rows: i64,
                            lane: i64, shared_max: u64, shared_total: u64) {
    let at = head * rows;
    var maximum: f32 = 0.0;
    var row = lane;
    while row < rows {
        let score = load(scores, at + row);
        if score > maximum {
            maximum = score;
        }
        row += 128;
    }
    maximum = max_shared(shared_max, maximum, lane);
    var partial: f64 = if lane == 0 { weight_exponential(-maximum) as f64 } else { 0.0 };
    row = lane;
    while row < rows {
        let weight = weight_exponential(load(scores, at + row) - maximum);
        partial += weight as f64;
        store(scores, at + row, weight);
        row += 128;
    }
    let total = sum_shared(shared_total, partial, lane);
    if lane == 0 {
        store(inverse, head, 1.0 / (total as f32));
    }
}

export fn i2_device_value(values: u64, scores: u64, inverse: u64, partials: u64,
                          rows: i64, head: i64, group: i64, lane: i64, tile: i64) {
    let channel = group * 32 + lane;
    let weights = head * rows;
    let value_offset = (head / 3) * 128 + channel;
    let inverse_total = load(inverse, head);
    let begin = tile * 256;
    let end = if begin + 256 < rows { begin + 256 } else { rows };
    var result: f64 = 0.0;
    for row in begin .. end {
        let weight = load(scores, weights + row) * inverse_total;
        let product: f32 = weight * load(values, row * 1024 + value_offset);
        result += product as f64;
    }
    store_double(partials, tile * 3072 + head * 128 + channel, result);
}

export fn i2_device_merge(partials: u64, output: u64, tiles: i64, group: i64, lane: i64) {
    let index = group * 128 + lane;
    var total: f64 = 0.0;
    for tile in tiles {
        total += load_double(partials, tile * 3072 + index);
    }
    store(output, index, total as f32);
}
