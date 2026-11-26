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

struct MlaMma4 { a: f32, b: f32, c: f32, d: f32 }
extern "fg_mla_mma_qpair" fn mla_mma_qpair(base: u64) -> u32;
extern "fg_mla_mma_kpair" fn mla_mma_kpair(base: u64) -> u32;
extern "fg_mla_mma_f16" fn mla_mma_f16(acc: MlaMma4, a0: u32, a1: u32, a2: u32,
    a3: u32, b0: u32, b1: u32) -> MlaMma4;

extern "fg_mla_score_put" fn mla_score_put(base: u64, index: i64, v: f32);
extern "fg_mla_score_get" fn mla_score_get(base: u64, index: i64) -> f32;
extern "fg_mla_score_sync" fn mla_score_sync();

export fn giga_mla_mma_tile(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, shared: u64,
    scale: f32, task: i64, query: i64, tile: i64, warp: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let col = query % positions;
    let head = query / positions;
    let qstride = attn_word(cfg, 7);
    let kstride = attn_word(cfg, 9);
    let vstride = attn_word(cfg, 11);
    let cells = attn_word(cfg, 6);
    let first = tile * 256;
    let qbase = q + ((head * attn_word(cfg, 8) + col * qstride) as u64);
    let mask_row = mask + (((col + warp) * attn_word(cfg, 13)) as u64);
    let group = lane / 4;
    let thread = lane % 4;
    let neg_inf = attn_neg_inf();
    var maximum = neg_inf;
    var mass: f32 = 0.0;
    var sum: f32[16] = [0.0; 16];

    for phase in 2 {
        let start = first + phase * 128;
        if start >= cells { break; }
        let block_key = start + 16 * warp;
        if block_key < cells {
            let k0 = k + ((block_key + group) * kstride) as u64;
            let k1 = k0 + (8 * kstride) as u64;
            let qr = qbase + (group * qstride) as u64;
            var acc = MlaMma4 {0.0, 0.0, 0.0, 0.0};
            for part in 36 {
                let c = 16 * part + 2 * thread;
                let kb = (2 * c) as u64;
                let qb = (4 * c) as u64;
                acc = mla_mma_f16(acc, mla_mma_kpair(k0 + kb), mla_mma_kpair(k1 + kb),
                    mla_mma_kpair(k0 + kb + 16), mla_mma_kpair(k1 + kb + 16),
                    mla_mma_qpair(qr + qb), mla_mma_qpair(qr + qb + 32));
            }
            let at = (2 * thread) * 128 + 16 * warp + group;
            mla_score_put(shared, at, acc.a);
            mla_score_put(shared, at + 128, acc.b);
            mla_score_put(shared, at + 8, acc.c);
            mla_score_put(shared, at + 136, acc.d);
        }
        mla_score_sync();
        let count = if cells - start < 128 { cells - start } else { 128 };
        for j in count {
            let row = start + j;
            let bias = if mask != 0 { attn_half(attn_ld_u16(mask_row, (2 * row) as u64)) } else { 0.0 };
            if bias == neg_inf { continue; }
            let score = mla_score_get(shared, warp * 128 + j) * scale + bias;
            let next = if score > maximum { score } else { maximum };
            let prev_weight = attn_exp(maximum - next);
            let weight = attn_exp(score - next);
            mass = attn_fma(mass, prev_weight, weight);
            maximum = next;
            let vp = v + ((row * vstride) as u64);
            for b in 16 {
                let value = mla_half_value(vp, b, lane);
                sum[b] = attn_fma(weight, value, sum[b] * prev_weight);
            }
        }
        mla_score_sync();
    }

    let result = scratch + ((4 * (((task + warp) * attn_word(cfg, 17) + tile) * 514)) as u64);
    if lane == 0 { attn_store(result, maximum); attn_store(result + 4, mass); }
    for b in 16 {
        attn_store(result + (4 * (2 + b * 32 + lane)) as u64, sum[b]);
    }
}

extern "fg_mla_pack_probability" fn mla_pack_probability(a: f32, b: f32) -> u32;

fn mla_pv_rescale(acc: MlaMma4, p0: f32, p1: f32) -> MlaMma4 {
    return MlaMma4 {acc.a*p0, acc.b*p1, acc.c*p0, acc.d*p1};
}

fn mla_pv_kpair(v: u64, stride: i64, row: i64, column: i64) -> u32 {
    let a = attn_ld_u16(v, (row * stride + 2 * column) as u64);
    let b = attn_ld_u16(v, ((row+1) * stride + 2 * column) as u64);
    return a | (b << 16);
}

fn mla_pv_accumulate(v: u64, cfg: u64, shared: u64, start: i64, count: i64, column: i64,
    group: i64, thread: i64, initial: MlaMma4) -> MlaMma4 {
    let stride = attn_word(cfg, 11);
    var acc = initial;
    for tile in count / 16 {
        let c = 16 * tile + 2 * thread;
        let row = start+c;
        let index = group*128+c;
        let p0 = mla_pack_probability(mla_score_get(shared,index),mla_score_get(shared,index+1));
        let p1 = mla_pack_probability(mla_score_get(shared,index+8),mla_score_get(shared,index+9));
        acc = mla_mma_f16(acc, mla_pv_kpair(v,stride,row,column),
            mla_pv_kpair(v,stride,row,column+8), mla_pv_kpair(v,stride,row+8,column),
            mla_pv_kpair(v,stride,row+8,column+8), p0,p1);
    }
    return acc;
}

fn mla_pv_store(scratch: u64, slot0: i64, slot1: i64, column: i64, value: MlaMma4) {
    attn_store(scratch + (4 * (slot0 + 2 + column)) as u64, value.a);
    attn_store(scratch + (4 * (slot1 + 2 + column)) as u64, value.b);
    attn_store(scratch + (4 * (slot0 + 10 + column)) as u64, value.c);
    attn_store(scratch + (4 * (slot1 + 10 + column)) as u64, value.d);
}
export fn giga_mla_mma_pv_tile(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, shared: u64,
    scale: f32, task: i64, query: i64, tile: i64, warp: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let col = query % positions;
    let head = query / positions;
    let qstride = attn_word(cfg, 7);
    let kstride = attn_word(cfg, 9);
    let cells = attn_word(cfg, 6);
    let first = tile * 256;
    let qbase = q + ((head * attn_word(cfg, 8) + col * qstride) as u64);
    let mask_row = mask + (((col + warp) * attn_word(cfg, 13)) as u64);
    let group = lane / 4;
    let thread = lane % 4;
    let neg_inf = attn_neg_inf();
    var maximum = neg_inf;
    var mass: f32 = 0.0;
    var a = MlaMma4 {0.0, 0.0, 0.0, 0.0};
    var b = a;
    var c = a;
    var d = a;

    for phase in 2 {
        let start = first + phase * 128;
        if start >= cells { break; }
        let block_key = start + 16 * warp;
        if block_key < cells {
            let k0 = k + ((block_key + group) * kstride) as u64;
            let k1 = k0 + (8 * kstride) as u64;
            let qr = qbase + (group * qstride) as u64;
            var acc = MlaMma4 {0.0, 0.0, 0.0, 0.0};
            for part in 36 {
                let c = 16 * part + 2 * thread;
                let kb = (2 * c) as u64;
                let qb = (4 * c) as u64;
                acc = mla_mma_f16(acc, mla_mma_kpair(k0 + kb), mla_mma_kpair(k1 + kb),
                    mla_mma_kpair(k0 + kb + 16), mla_mma_kpair(k1 + kb + 16),
                    mla_mma_qpair(qr + qb), mla_mma_qpair(qr + qb + 32));
            }
            let at = (2 * thread) * 128 + 16 * warp + group;
            mla_score_put(shared, at, acc.a);
            mla_score_put(shared, at + 128, acc.b);
            mla_score_put(shared, at + 8, acc.c);
            mla_score_put(shared, at + 136, acc.d);
        }
        mla_score_sync();
        let count = if cells - start < 128 { cells - start } else { 128 };
        var peak = neg_inf;
        for offset in 4 {
            let at = offset * 32 + lane;
            if at < count {
                let row = start + at;
                let bias = if mask != 0 { attn_half(attn_ld_u16(mask_row, (2 * row) as u64)) } else { 0.0 };
                let score = if bias == neg_inf { neg_inf } else { mla_score_get(shared, warp * 128 + at) * scale + bias };
                mla_score_put(shared, warp * 128 + at, score);
                if score > peak { peak = score; }
            }
        }
        var delta: i32 = 16;
        while delta > 0 {
            let other = attn_shfl(peak, delta);
            if other > peak { peak = other; }
            delta /= 2;
        }
        let next = if peak > maximum { peak } else { maximum };
        let rescale = if maximum == neg_inf { 0.0 } else { attn_exp(maximum - next) };
        maximum = next;
        var local_mass: f32 = 0.0;
        for offset in 4 {
            let at = offset * 32 + lane;
            var probability: f32 = 0.0;
            if at < count {
                let score = mla_score_get(shared, warp * 128 + at);
                if score != neg_inf { probability = attn_exp(score - next); }
            }
            mla_score_put(shared, warp * 128 + at, probability);
            local_mass += probability;
        }
        mass = attn_fma(mass, rescale, attn_sum(local_mass));
        if lane == 0 { mla_score_put(shared, 1024 + warp, rescale); }
        mla_score_sync();

        let p0 = mla_score_get(shared, 1024 + 2 * thread);
        let p1 = mla_score_get(shared, 1024 + 2 * thread + 1);
        let column = warp * 64 + group;
        a = mla_pv_accumulate(v, cfg, shared, start, count, column, group, thread, mla_pv_rescale(a,p0,p1));
        b = mla_pv_accumulate(v, cfg, shared, start, count, column+16, group, thread, mla_pv_rescale(b,p0,p1));
        c = mla_pv_accumulate(v, cfg, shared, start, count, column+32, group, thread, mla_pv_rescale(c,p0,p1));
        d = mla_pv_accumulate(v, cfg, shared, start, count, column+48, group, thread, mla_pv_rescale(d,p0,p1));
        mla_score_sync();
    }

    let tiles = attn_word(cfg, 17);
    let moments = scratch + ((4 * (((task + warp) * tiles + tile) * 514)) as u64);
    if lane == 0 { attn_store(moments, maximum); attn_store(moments + 4, mass); }
    let slot0 = ((task + 2 * thread) * tiles + tile) * 514;
    let slot1 = slot0 + tiles * 514;
    let column = warp * 64 + group;
    mla_pv_store(scratch, slot0, slot1, column, a);
    mla_pv_store(scratch, slot0, slot1, column+16, b);
    mla_pv_store(scratch, slot0, slot1, column+32, c);
    mla_pv_store(scratch, slot0, slot1, column+48, d);
}
