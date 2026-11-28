extern "i2o_load" fn load(address: u64, index: i64) -> f32;
extern "i2o_position" fn position(address: u64, index: i64) -> i64;
extern "i2o_shared_read" fn shared_read(address: u64, index: i64) -> f32;
extern "i2o_shared_write" fn shared_write(address: u64, index: i64, value: f32);
extern "inkling_write_probability_g" fn probability_write(address: u64, index: i64, value: f32);
extern "__nv_expf" fn exp(value: f32) -> f32;
extern "inkling_negative_infinity_g" fn neg_inf() -> f32;

fn adjusted_score(score: f32, query_position: i64, key_position: i64,
    window: i64, relative: u64, bias: u64, sink: f32, relative_index: i64, scale: f32) -> f32 {
    if key_position < 0 || key_position > query_position { return neg_inf(); }
    let distance = query_position - key_position;
    if window != 0 && distance >= window { return neg_inf(); }
    var relative_score: f64 = 0.0;
    if distance < 1024 {
        for i in 16 {
            let product: f32 = load(relative, relative_index + i) * load(bias, distance * 16 + i);
            relative_score += product as f64;
        }
    }
    return scale * score + ((relative_score as f32) - sink);
}

export fn i2_online_row(logits: u64, probabilities: u64, maxima: u64, totals: u64, rescale: u64,
    key_positions: u64, relative: u64, bias: u64, sinks: u64, tail: i64, key_start: i64,
    query_position: i64, query_head: i64, row: i64, window: i64, scale: f32) {
    let before = shared_read(maxima, row);
    let sink = load(sinks, query_head);
    var maximum = before;
    let base = row * 32;
    for i in 32 {
        let score = if i < tail {
            adjusted_score(shared_read(logits, base + i), query_position, position(key_positions, key_start + i),
                window, relative, bias, sink, query_head * 16, scale)
        } else { neg_inf() };
        shared_write(logits, base + i, score);
        if score > maximum { maximum = score; }
    }
    let alpha = exp(before - maximum);
    var total = shared_read(totals, row) * alpha;
    for i in 32 {
        let w = exp(shared_read(logits, base + i) - maximum);
        probability_write(probabilities, base + i, w);
        total += w;
    }
    shared_write(maxima, row, maximum);
    shared_write(totals, row, total);
    shared_write(rescale, row, alpha);
}

export fn i2_online_adjust_key(logits: u64, key_positions: u64, relative: u64, bias: u64, sinks: u64,
    tail: i64, key_start: i64, query_position: i64, query_head: i64, key: i64, window: i64, scale: f32) {
    let score = if key < tail {
        adjusted_score(shared_read(logits, key), query_position, position(key_positions, key_start + key),
            window, relative, bias, load(sinks, query_head), query_head * 16, scale)
    } else { neg_inf() };
    shared_write(logits, key, score);
}

export fn i2_online_prepared_row(logits: u64, probabilities: u64, maxima: u64, totals: u64, rescale: u64) {
    let row: i64 = 0;
    let base: i64 = 0;
    let before = shared_read(maxima, row);
    var maximum = before;
    for i in 32 {
        let score = shared_read(logits, i);
        if score > maximum { maximum = score; }
    }
    let alpha = exp(before - maximum);
    var total = shared_read(totals, row) * alpha;
    for i in 32 {
        let w = exp(shared_read(logits, base + i) - maximum);
        probability_write(probabilities, base + i, w);
        total += w;
    }
    shared_write(maxima, row, maximum);
    shared_write(totals, row, total);
    shared_write(rescale, row, alpha);
}

export fn i2_online_prepared_row_at(logits: u64, probabilities: u64, maxima: u64, totals: u64, rescale: u64, row: i64) {
    let base = row * 32;
    let before = shared_read(maxima, row);
    var maximum = before;
    for i in 32 {
        let score = shared_read(logits, base + i);
        if score > maximum { maximum = score; }
    }
    let alpha = exp(before - maximum);
    var total = shared_read(totals, row) * alpha;
    for i in 32 {
        let w = exp(shared_read(logits, base + i) - maximum);
        probability_write(probabilities, base + i, w);
        total += w;
    }
    shared_write(maxima, row, maximum);
    shared_write(totals, row, total);
    shared_write(rescale, row, alpha);
}
