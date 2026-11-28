extern "input_load" fn load(base: u64, index: i64) -> f32;
extern "input_store" fn store(base: u64, index: i64, value: f32);
extern "input_half" fn half(base: u64, index: i64, value: f32);
extern "input_sum" fn sum_at(base: u64, index: i64) -> f64;
extern "input_set_sum" fn sum_set(base: u64, index: i64, value: f64);
extern "input_sync" fn sync();
extern "__nv_sqrtf" fn sqrt(value: f32) -> f32;
extern "__nv_logf" fn log(value: f32) -> f32;

fn read_history(input: u64, past: u64, token: i64, channel: i64) -> f32 {
    return if token >= 0 { load(input, 1024 * token + channel) } else { load(past, 3 * channel + 3 + token) };
}

fn convolve(input: u64, past: u64, filter: u64, taps: u64, token: i64, channel: i64, sliding: bool) -> f32 {
    let h0 = read_history(input, past, token - 3, channel);
    let h1 = read_history(input, past, token - 2, channel);
    let h2 = read_history(input, past, token - 1, channel);
    let current = load(input, 1024 * token + channel);
    let at = 4 * channel;
    var mixed: f32 = 0.0;
    mixed += h0 * load(filter, at);
    mixed += h1 * load(filter, at + 1);
    mixed += h2 * load(filter, at + 2);
    mixed += current * load(filter, at + 3);
    if !sliding {
        let tap = 32 * token + 4 * (channel / 128);
        mixed += current * load(taps, tap);
        mixed += h2 * load(taps, tap + 1);
        mixed += h1 * load(taps, tap + 2);
        mixed += h0 * load(taps, tap + 3);
    }
    let scale: f32 = if sliding { 1.0 / sqrt(5.0) } else { 1.0 / sqrt(9.0) };
    return (current + mixed) * scale;
}

export fn attention_input(raw: u64, coeff: u64, history: u64, q: u64, r: u64, k: u64, v: u64,
    count: i64, first: i64, sliding_mode: i32, shared: u64, head: i64, row: i64, lane: i64) {
    let sliding = sliding_mode != 0;
    let relative = head >= 24 && head < 48;
    let width = if relative { 16 } else { 128 };
    let offset = if head < 24 { head * 128 } else { if relative { (head - 24) * 16 } else { ((head - 48) & 7) * 128 } };
    let channel = offset + lane;
    var value: f32 = 0.0;
    if head < 24 {
        value = load(raw, 3072 * row + channel) * load(coeff, channel);
    } else if relative {
        if lane < width { value = load(raw, 5120 * count + 384 * row + channel) * load(coeff, 3072 + channel); }
    } else {
        let is_value = head >= 56;
        let input: u64 = if is_value { 4096 } else { 3072 };
        let taps: u64 = if is_value { 8632 } else { 8600 };
        value = convolve(raw + input * (count as! u64) * 4, history + (if is_value { 12288 } else { 0 }),
            coeff + (if is_value { 30208 } else { 13824 }), raw + taps * (count as! u64) * 4, row, channel, sliding);
    }
    let square = value * value;
    sum_set(shared, lane, square as! f64);
    sync();
    var stride: i64 = 64;
    while stride != 0 {
        if lane < stride { sum_set(shared, lane, sum_at(shared, lane) + sum_at(shared, lane + stride)); }
        sync();
        stride /= 2;
    }
    if lane >= width { return; }
    let total = sum_at(shared, 0) as! f32;
    if head >= 56 {
        value *= 1.0 / sqrt(total / 128.0 + 0.000001);
    } else {
        let cap: f32 = if relative { 8.0 } else { 22.627416610717773 };
        let clip = cap / (sqrt(total) + 0.000001);
        value *= if clip < 1.0 { clip } else { 1.0 };
    }
    if head < 48 && !sliding && first + row + 1 > 8192 {
        let tau: f32 = 1.0 + 0.1 * log(((first + row + 1) as! f32) / 8192.0);
        value *= tau;
    }
    if head < 24 { half(q, 3072 * row + channel, value); }
    else if relative { store(r, 384 * row + channel, value); }
    else if head < 56 { store(k, 1024 * row + channel, value); }
    else { store(v, 1024 * row + channel, value); }
}

export fn attention_update_history(raw: u64, history: u64, count: i64, group: i64, lane: i64) {
    let channel = (group * 128 + lane) % 1024;
    let is_value = group >= 8;
    let width: u64 = if is_value { 4096 } else { 3072 };
    let input = raw + width * (count as! u64) * 4;
    let past = history + (if is_value { 12288 } else { 0 });
    let h0 = read_history(input, past, count - 3, channel);
    let h1 = read_history(input, past, count - 2, channel);
    let h2 = read_history(input, past, count - 1, channel);
    store(past, 3 * channel, h0);
    store(past, 3 * channel + 1, h1);
    store(past, 3 * channel + 2, h2);
}
