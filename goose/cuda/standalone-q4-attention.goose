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

export fn fg_mla_attn_pair(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
    scale: f32, task: i64, head: i64, tile: i64, lane: i64) {
    let keys = attn_active_keys(cfg, 0);
    let segment = attn_word(cfg, 16);
    let first = tile * segment;
    if first >= keys { return; }
    let last = if first + segment < keys { first + segment } else { keys };
    let qstride = attn_word(cfg, 8);
    let q0 = q + ((head * qstride) as u64);
    let q1 = q0 + (qstride as u64);
    let neg_inf = attn_neg_inf();
    var max0 = neg_inf;
    var max1 = neg_inf;
    var mass0: f32 = 0.0;
    var mass1: f32 = 0.0;
    var query0: f32[18] = [0.0; 18];
    var query1: f32[18] = [0.0; 18];
    var sum0: f32[16] = [0.0; 16];
    var sum1: f32[16] = [0.0; 16];
    for b in 18 {
        let offset = (4 * (32 * b + lane)) as u64;
        query0[b] = attn_load(q0 + offset);
        query1[b] = attn_load(q1 + offset);
    }
    let ks = attn_word(cfg, 9);
    let vs = attn_word(cfg, 11);
    for row in first .. last {
        let bias = if mask != 0 { attn_half(attn_ld_u16(mask, (2 * row) as u64)) } else { 0.0 };
        if bias == neg_inf { continue; }
        let kp = k + ((row * ks) as u64);
        let vp = v + ((row * vs) as u64);
        var dot0: f32 = 0.0;
        var dot1: f32 = 0.0;
        for b in 18 {
            let item = mla_half_value(kp, b, lane);
            dot0 = attn_fma(query0[b], item, dot0);
            dot1 = attn_fma(query1[b], item, dot1);
        }
        let score0 = attn_sum(dot0) * scale + bias;
        let score1 = attn_sum(dot1) * scale + bias;
        let next0 = if score0 > max0 { score0 } else { max0 };
        let next1 = if score1 > max1 { score1 } else { max1 };
        let prev0 = attn_exp(max0 - next0);
        let prev1 = attn_exp(max1 - next1);
        let weight0 = attn_exp(score0 - next0);
        let weight1 = attn_exp(score1 - next1);
        mass0 = mass0 * prev0 + weight0;
        mass1 = mass1 * prev1 + weight1;
        max0 = next0;
        max1 = next1;
        for b in 16 {
            let item = mla_half_value(vp, b, lane);
            sum0[b] = sum0[b] * prev0 + weight0 * item;
            sum1[b] = sum1[b] * prev1 + weight1 * item;
        }
    }
    let stride: i64 = 514 * 4;
    let tiles = attn_word(cfg, 17);
    let result0 = scratch + ((stride * (task * tiles + tile)) as u64);
    let result1 = result0 + ((stride * tiles) as u64);
    if lane == 0 {
        attn_store(result0, max0);
        attn_store(result0 + 4, mass0);
        attn_store(result1, max1);
        attn_store(result1 + 4, mass1);
    }
    for b in 16 {
        let offset = (4 * (2 + b * 32 + lane)) as u64;
        attn_store(result0 + offset, sum0[b]);
        attn_store(result1 + offset, sum1[b]);
    }
}

export fn fg_mla_attn_batch_pair(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
    scale: f32, task: i64, query: i64, tile: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let col = query % positions;
    let head = query / positions;
    let keys = attn_active_keys(cfg, col);
    let segment = attn_word(cfg, 16);
    let first = tile * segment;
    if first >= keys { return; }
    let last = if first + segment < keys { first + segment } else { keys };
    let qstride = attn_word(cfg, 8);
    let q0 = q + ((head * qstride + col * attn_word(cfg, 7)) as u64);
    let q1 = q0 + (qstride as u64);
    let neg_inf = attn_neg_inf();
    var max0 = neg_inf;
    var max1 = neg_inf;
    var mass0: f32 = 0.0;
    var mass1: f32 = 0.0;
    var query0: f32[18] = [0.0; 18];
    var query1: f32[18] = [0.0; 18];
    var sum0: f32[16] = [0.0; 16];
    var sum1: f32[16] = [0.0; 16];
    for b in 18 {
        let offset = (4 * (32 * b + lane)) as u64;
        query0[b] = attn_load(q0 + offset);
        query1[b] = attn_load(q1 + offset);
    }
    let ks = attn_word(cfg, 9);
    let vs = attn_word(cfg, 11);
    for row in first .. last {
        let bias = if mask != 0 { attn_half(attn_ld_u16(mask, (col * attn_word(cfg, 13) + 2 * row) as u64)) } else { 0.0 };
        if bias == neg_inf { continue; }
        let kp = k + ((row * ks) as u64);
        let vp = v + ((row * vs) as u64);
        var dot0: f32 = 0.0;
        var dot1: f32 = 0.0;
        for b in 18 {
            let item = mla_half_value(kp, b, lane);
            dot0 = attn_fma(query0[b], item, dot0);
            dot1 = attn_fma(query1[b], item, dot1);
        }
        let score0 = attn_sum(dot0) * scale + bias;
        let score1 = attn_sum(dot1) * scale + bias;
        let next0 = if score0 > max0 { score0 } else { max0 };
        let next1 = if score1 > max1 { score1 } else { max1 };
        let prev0 = attn_exp(max0 - next0);
        let prev1 = attn_exp(max1 - next1);
        let weight0 = attn_exp(score0 - next0);
        let weight1 = attn_exp(score1 - next1);
        mass0 = mass0 * prev0 + weight0;
        mass1 = mass1 * prev1 + weight1;
        max0 = next0;
        max1 = next1;
        for b in 16 {
            let item = mla_half_value(vp, b, lane);
            sum0[b] = if positions == 1 { attn_fma(sum0[b], prev0, weight0 * item) } else { attn_fma(weight0, item, sum0[b] * prev0) };
            sum1[b] = if positions == 1 { attn_fma(sum1[b], prev1, weight1 * item) } else { attn_fma(weight1, item, sum1[b] * prev1) };
        }
    }
    let stride: i64 = 514 * 4;
    let tiles = attn_word(cfg, 17);
    let result0 = scratch + ((stride * (task * tiles + tile)) as u64);
    let result1 = result0 + ((stride * tiles) as u64);
    if lane == 0 {
        attn_store(result0, max0);
        attn_store(result0 + 4, mass0);
        attn_store(result1, max1);
        attn_store(result1 + 4, mass1);
    }
    for b in 16 {
        let offset = (4 * (2 + b * 32 + lane)) as u64;
        attn_store(result0 + offset, sum0[b]);
        attn_store(result1 + offset, sum1[b]);
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

extern "fg_dsa_store_u16" fn dsa_mask_store(base: u64, index: i64, value: u32);
extern "fg_dsa_load_i32" fn dsa_mask_pick(base: u64, index: i64) -> i32;

export fn giga_device_mask_clear(mask: u64, out: u64, total: i64, prefix: i64, index: i64) {
    if index < total { dsa_mask_store(out, index, if index < prefix { attn_ld_u16(mask, (2 * index) as u64) } else { 0xfc00 }); }
}

export fn giga_device_mask_scatter(mask: u64, picks: u64, out: u64,
    cells: i64, count: i64, total: i64, prefix: i64, index: i64) {
    if index >= total { return; }
    let key = dsa_mask_pick(picks, index) as i64;
    if key < 0 || key >= cells { return; }
    let at = (index / count + prefix) * cells + key;
    dsa_mask_store(out, at, attn_ld_u16(mask, (2 * at) as u64));
}
