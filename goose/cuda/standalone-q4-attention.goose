// One warp consumes a segment of the packed KV for one query and head.
// The second kernel merges the online softmax states of all segments.
extern "fg_device_global_load" fn attn_load(address: u64) -> f32;
extern "fg_device_global_store" fn attn_store(address: u64, value: f32);
extern "fg_device_fma" fn attn_fma(a: f32, b: f32, sum: f32) -> f32;
extern "fg175q_h2f" fn attn_half(bits: u32) -> f32;
extern "fg175q_ld_u16" fn attn_ld_u16(address: u64, offset: u64) -> u32;
extern "fg175q_ld_u8" fn ld_u8(address: u64, offset: u64) -> u32;
extern "fg175q_ld_u16" fn ld_u16(address: u64, offset: u64) -> u32;
extern "fg175q_h2f" fn h2f(bits: u32) -> f32;
extern "fg_attn_word" fn attn_word(config: u64, index: i64) -> i64;
extern "fg_attn_shfl" fn attn_shfl(value: f32, delta: i32) -> f32;
extern "fg_attn_exp" fn attn_exp(value: f32) -> f32;
extern "fg_attn_neg_inf" fn attn_neg_inf() -> f32;

fn attn_q4(row: u64, block_index: i64, lane: i64) -> f32 {
    let pair = dequant_q4_0(row + ((block_index * 18) as u64), lane % 16);
    return if lane < 16 { pair.x } else { pair.y };
}

fn attn_sum(value: f32) -> f32 {
    var partial = value;
    partial += attn_shfl(partial, 16);
    partial += attn_shfl(partial, 8);
    partial += attn_shfl(partial, 4);
    partial += attn_shfl(partial, 2);
    partial += attn_shfl(partial, 1);
    return partial;
}

fn attn_active_keys(cfg: u64, pos: i64) -> i64 {
    let first = attn_word(cfg, 21);
    return if first != 0 { first + pos } else { attn_word(cfg, 6) };
}

export fn fg_attn_tiles(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
                        scale: f32, task: i64, query: i64, tile: i64, lane: i64) {
    let col = query % attn_word(cfg, 2);
    let keys = attn_active_keys(cfg, col);
    let segment = attn_word(cfg, 16);
    let first = tile * segment;
    if first >= keys { return; }
    let last = if first + segment < keys { first + segment } else { keys };
    let heads = attn_word(cfg, 3);
    let head = query / attn_word(cfg, 2);
    let qrow = q + ((col * attn_word(cfg, 7) + head * attn_word(cfg, 8)) as u64);
    let kh = k + (((head / (heads / attn_word(cfg, 4))) * attn_word(cfg, 10)) as u64);
    let vh = v + (((head / (heads / attn_word(cfg, 5))) * attn_word(cfg, 12)) as u64);
    let mr = mask + ((col * attn_word(cfg, 13)) as u64);
    let qblocks = attn_word(cfg, 0) / 32;
    let vblocks = attn_word(cfg, 1) / 32;
    let neg_inf = attn_neg_inf();
    var maximum = neg_inf;
    var mass: f32 = 0.0;
    var query_values: f32[8] = [0.0; 8];
    var sum: f32[8] = [0.0; 8];
    for b in qblocks {
        query_values[b] = attn_load(qrow + ((4 * (32 * b + lane)) as u64));
    }
    for row in first .. last {
        let bias = if mask != 0 { attn_half(attn_ld_u16(mr, (2 * row) as u64)) } else { 0.0 };
        if bias == neg_inf { continue; }
        let kp = kh + ((row * attn_word(cfg, 9)) as u64);
        let vp = vh + ((row * attn_word(cfg, 11)) as u64);
        var dot: f32 = 0.0;
        for b in qblocks { dot = attn_fma(query_values[b], attn_q4(kp, b, lane), dot); }
        let score = attn_sum(dot) * scale + bias;
        let next = if score > maximum { score } else { maximum };
        let prev_weight = attn_exp(maximum - next);
        let weight = attn_exp(score - next);
        mass = mass * prev_weight + weight;
        maximum = next;
        for b in vblocks {
            sum[b] = sum[b] * prev_weight + weight * attn_q4(vp, b, lane);
        }
    }
    let stride = attn_word(cfg, 1) + 2;
    let result = scratch + ((4 * ((task * attn_word(cfg, 17) + tile) * stride)) as u64);
    if lane == 0 {
        attn_store(result, maximum);
        attn_store(result + 4, mass);
    }
    for b in vblocks {
        attn_store(result + ((4 * (2 + b * 32 + lane)) as u64), sum[b]);
    }
}

export fn fg_attn_merge(scratch: u64, out: u64, cfg: u64, task: i64, query: i64, lane: i64) {
    let stride = attn_word(cfg, 1) + 2;
    let tiles = attn_word(cfg, 17);
    let pos = query % attn_word(cfg, 2);
    let active_tiles = 1 + (attn_active_keys(cfg, pos) - 1) / attn_word(cfg, 16);
    let vblocks = attn_word(cfg, 1) / 32;
    var maximum = attn_neg_inf();
    var mass: f32 = 0.0;
    var sum: f32[8] = [0.0; 8];
    for i in active_tiles {
        let row = scratch + ((4 * ((task * tiles + i) * stride)) as u64);
        let partial = attn_load(row + 4);
        if partial == 0.0 { continue; }
        let peak = attn_load(row);
        let next = if peak > maximum { peak } else { maximum };
        let scale = attn_exp(maximum - next);
        let weight = attn_exp(peak - next);
        mass = mass * scale + partial * weight;
        maximum = next;
        for b in vblocks {
            let value = attn_load(row + ((4 * (2 + 32 * b + lane)) as u64));
            sum[b] = sum[b] * scale + value * weight;
        }
    }
    let head = query / attn_word(cfg, 2);
    let dst = out + ((head * attn_word(cfg, 14) + pos * attn_word(cfg, 15)) as u64);
    for b in vblocks {
        let value = if mass > 0.0 { sum[b] / mass } else { 0.0 };
        attn_store(dst + ((4 * (32 * b + lane)) as u64), value);
    }
}

fn mla_half_value(row: u64, block_index: i64, lane: i64) -> f32 {
    return attn_half(attn_ld_u16(row, (2 * (32 * block_index + lane)) as u64));
}

export fn fg_mla_attn_tiles(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
                        scale: f32, task: i64, query: i64, tile: i64, lane: i64) {
    let col = query % attn_word(cfg, 2);
    let keys = attn_active_keys(cfg, col);
    let segment = attn_word(cfg, 16);
    let first = tile * segment;
    if first >= keys { return; }
    let last = if first + segment < keys { first + segment } else { keys };
    let heads = attn_word(cfg, 3);
    let head = query / attn_word(cfg, 2);
    let qrow = q + ((col * attn_word(cfg, 7) + head * attn_word(cfg, 8)) as u64);
    let kh = k + (((head / (heads / attn_word(cfg, 4))) * attn_word(cfg, 10)) as u64);
    let vh = v + (((head / (heads / attn_word(cfg, 5))) * attn_word(cfg, 12)) as u64);
    let mr = mask + ((col * attn_word(cfg, 13)) as u64);
    let qblocks = attn_word(cfg, 0) / 32;
    let vblocks = attn_word(cfg, 1) / 32;
    let neg_inf = attn_neg_inf();
    var maximum = neg_inf;
    var mass: f32 = 0.0;
    var query_values: f32[18] = [0.0; 18];
    var sum: f32[16] = [0.0; 16];
    for b in qblocks {
        query_values[b] = attn_load(qrow + ((4 * (32 * b + lane)) as u64));
    }
    for row in first .. last {
        let bias = if mask != 0 { attn_half(attn_ld_u16(mr, (2 * row) as u64)) } else { 0.0 };
        if bias == neg_inf { continue; }
        let kp = kh + ((row * attn_word(cfg, 9)) as u64);
        let vp = vh + ((row * attn_word(cfg, 11)) as u64);
        var dot: f32 = 0.0;
        for b in qblocks { dot = attn_fma(query_values[b], mla_half_value(kp, b, lane), dot); }
        let score = attn_sum(dot) * scale + bias;
        let next = if score > maximum { score } else { maximum };
        let prev_weight = attn_exp(maximum - next);
        let weight = attn_exp(score - next);
        mass = mass * prev_weight + weight;
        maximum = next;
        for b in vblocks {
            sum[b] = sum[b] * prev_weight + weight * mla_half_value(vp, b, lane);
        }
    }
    let stride = attn_word(cfg, 1) + 2;
    let result = scratch + ((4 * ((task * attn_word(cfg, 17) + tile) * stride)) as u64);
    if lane == 0 {
        attn_store(result, maximum);
        attn_store(result + 4, mass);
    }
    for b in vblocks {
        attn_store(result + ((4 * (2 + b * 32 + lane)) as u64), sum[b]);
    }
}

export fn fg_mla_attn_merge(scratch: u64, out: u64, cfg: u64, task: i64, query: i64, lane: i64) {
    let stride = attn_word(cfg, 1) + 2;
    let tiles = attn_word(cfg, 17);
    let pos = query % attn_word(cfg, 2);
    let active_tiles = 1 + (attn_active_keys(cfg, pos) - 1) / attn_word(cfg, 16);
    let vblocks = attn_word(cfg, 1) / 32;
    var maximum = attn_neg_inf();
    var mass: f32 = 0.0;
    var sum: f32[16] = [0.0; 16];
    for i in active_tiles {
        let row = scratch + ((4 * ((task * tiles + i) * stride)) as u64);
        let partial = attn_load(row + 4);
        if partial == 0.0 { continue; }
        let peak = attn_load(row);
        let next = if peak > maximum { peak } else { maximum };
        let scale = attn_exp(maximum - next);
        let weight = attn_exp(peak - next);
        mass = mass * scale + partial * weight;
        maximum = next;
        for b in vblocks {
            let value = attn_load(row + ((4 * (2 + 32 * b + lane)) as u64));
            sum[b] = sum[b] * scale + value * weight;
        }
    }
    let head = query / attn_word(cfg, 2);
    let dst = out + ((head * attn_word(cfg, 14) + pos * attn_word(cfg, 15)) as u64);
    for b in vblocks {
        let value = if mass > 0.0 { sum[b] / mass } else { 0.0 };
        attn_store(dst + ((4 * (32 * b + lane)) as u64), value);
    }
}

export fn fg_dsa_score(q: u64, k: u64, weights: u64, mask: u64, out: u64,
                       cfg: u64, cell: i64, token: i64, lane: i64) {
    let cells = attn_word(cfg, 0);
    if cell >= cells { return; }
    let mask_row = mask + ((token * attn_word(cfg, 5)) as u64);
    let bias = attn_half(attn_ld_u16(mask_row, (2 * cell) as u64));
    let target = out + ((4 * (token * cells + cell)) as u64);
    if bias == attn_neg_inf() {
        if lane == 0 { attn_store(target, bias); }
        return;
    }
    let key = k + ((cell * attn_word(cfg, 2)) as u64);
    let key0 = attn_half(attn_ld_u16(key, (2 * lane) as u64));
    let key1 = attn_half(attn_ld_u16(key, (2 * (32 + lane)) as u64));
    let key2 = attn_half(attn_ld_u16(key, (2 * (64 + lane)) as u64));
    let key3 = attn_half(attn_ld_u16(key, (2 * (96 + lane)) as u64));
    let query = q + ((token * attn_word(cfg, 3) + 4 * lane) as u64);
    let weight = weights + ((token * attn_word(cfg, 4)) as u64);
    var score: f32 = 0.0;
    for head in 64 {
        let row = query + ((512 * head) as u64);
        var dot = attn_fma(attn_load(row), key0, 0.0);
        dot = attn_fma(attn_load(row + 128), key1, dot);
        dot = attn_fma(attn_load(row + 256), key2, dot);
        dot = attn_fma(attn_load(row + 384), key3, dot);
        dot = attn_sum(dot);
        if lane == 0 && dot > 0.0 {
            score = attn_fma(dot, attn_load(weight + ((head * 4) as u64)), score);
        }
    }
    if lane == 0 { attn_store(target, score + bias); }
}
