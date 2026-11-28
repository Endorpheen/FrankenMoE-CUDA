extern "fg_device_global_load" fn pack_f32(address: u64) -> f32;
extern "cuda_load_half_g" fn pack_f16(address: u64) -> f32;
extern "as_uint" fn pack_bits(x: f32) -> u32;
extern "as_float" fn pack_float(x: u32) -> f32;
extern "fg_dsa_pack_xor" fn pack_shuffle(x: u32, delta: u32) -> u32;
extern "fg_dsa_pack_rni" fn pack_rni(x: f32) -> i32;
extern "fg_device_mul_rn" fn pack_mul(a: f32, b: f32) -> f32;
extern "fg_dsa_pack_byte" fn pack_byte(address: u64, x: i32);
extern "fg_device_global_store" fn pack_save_scale(address: u64, x: f32);

export fn dsa_pack_power2(input: u64, hi: u64, lo: u64, scales: u64,
    stride: i64, row: i64, lane: i64, half_input: i32) {
    let half = half_input != 0;
    let source = input + (row * stride) as u64;
    let width: i64 = if half { 2 } else { 4 };
    var values: f32[4] = [0.0; 4];
    var peak: u32 = 0;
    for part in 4 {
        let index = 32 * part + lane;
        let address = source + ((index * width) as u64);
        let value = if half { pack_f16(address) } else { pack_f32(address) };
        let magnitude = pack_bits(value) & 0x7fffffff;
        values[part] = value;
        if magnitude > peak { peak = magnitude; }
    }
    var distance: u32 = 16;
    while distance > 0 {
        let other = pack_shuffle(peak, distance);
        if other > peak { peak = other; }
        distance >>= 1;
    }
    var exponent: u32 = peak >> 23;
    let output_scale = scales + ((row * 4) as u64);
    if peak == 0 {
        exponent = 141;
    } else if exponent < 15 || exponent >= 255 {
        if lane == 0 { pack_save_scale(output_scale, 0.0); }
        return;
    }
    var step_bits = exponent - 14;
    var inverse = pack_float((254 - step_bits) << 23);
    if pack_mul(pack_float(peak), inverse) > 32639.0 {
        step_bits = exponent - 13;
        inverse = pack_float((254 - step_bits) << 23);
    }
    for part in 4 {
        let index = ((row * 128 + 32 * part + lane) as u64);
        let rounded = pack_rni(pack_mul(values[part], inverse));
        let high = (rounded + 32896) / 256 - 128;
        pack_byte(hi + index, high);
        pack_byte(lo + index, rounded - 256 * high);
    }
    if lane == 0 { pack_save_scale(output_scale, pack_float(step_bits << 23)); }
}

struct MmqDot { a: i32, b: i32, c: i32, d: i32 }

extern "fg_mmq_mma32" fn score_mma(a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> MmqDot;
extern "fg_dsa_int_word" fn score_word(address: u64) -> u32;
extern "fg_device_global_load" fn score_load(address: u64) -> f32;
extern "fg_device_global_store" fn score_store(address: u64, x: f32);
extern "fg_device_fma" fn score_fma(a: f32, b: f32, acc: f32) -> f32;
extern "fg_mla_score_put" fn score_put(address: u64, at: i64, x: f32);
extern "fg_mla_score_get" fn score_get(address: u64, at: i64) -> f32;
extern "fg_mla_score_sync" fn score_sync();
extern "fg_dsa_integer_to_float" fn score_round(dot: i64) -> f32;

fn score_convert(dot: i64, a: f32, b: f32) -> f32 {
    return pack_mul(pack_mul(score_round(dot), a), b);
}
extern "fg175q_ld_u16" fn score_mask(address: u64, offset: u64) -> u32;
extern "fg175q_h2f" fn score_half(bits: u32) -> f32;

fn score_pair(x: MmqDot, y: MmqDot) -> MmqDot {
    return MmqDot { a: x.a + y.a, b: x.b + y.b, c: x.c + y.c, d: x.d + y.d };
}

fn score_fragment(a0: u64, a1: u64, b: u64, k: u64) -> MmqDot {
    return score_mma(score_word(a0+k), score_word(a1+k), score_word(a0+k+16),
        score_word(a1+k+16), score_word(b+k), score_word(b+k+16));
}

fn score_join(hh: i32, hl: i32, lh: i32, ll: i32) -> i64 {
    return 65536 * (hh as i64) + 256 * ((hl as i64) + (lh as i64)) + (ll as i64);
}

export fn dsa_integer_score_tile(khi: u64, klo: u64, qhi: u64, qlo: u64,
    kscale: u64, qscale: u64, weights: u64, mask: u64, output: u64,
    shared: u64, cells: i64, query: i64, tile: i64, thread_index: i64) {
    let warp = thread_index / 32;
    let lane = thread_index % 32;
    let group = lane / 4;
    let thread = lane % 4;
    let krow = tile * 16;
    let head = warp * 8 + group;
    let a0 = ((krow + group) * 128) as u64;
    let a1 = a0 + 8 * 128;
    let b0 = ((query * 64 + head) * 128) as u64;
    var hh = MmqDot {0, 0, 0, 0};
    var hl = hh;
    var lh = hh;
    var ll = hh;
    for part in 4 {
        let k = (32 * part + 4 * thread) as u64;
        hh = score_pair(hh, score_fragment(khi+a0, khi+a1, qhi+b0, k));
        hl = score_pair(hl, score_fragment(khi+a0, khi+a1, qlo+b0, k));
        lh = score_pair(lh, score_fragment(klo+a0, klo+a1, qhi+b0, k));
        ll = score_pair(ll, score_fragment(klo+a0, klo+a1, qlo+b0, k));
    }
    let h = warp * 8 + 2 * thread;
    let k0 = score_load(kscale + ((4 * (krow + group)) as u64));
    let k1 = score_load(kscale + ((4 * (krow + group + 8)) as u64));
    let q0 = score_load(qscale + ((4 * (query * 64 + h)) as u64));
    let q1 = score_load(qscale + ((4 * (query * 64 + h + 1)) as u64));
    let at = h * 16 + group;
    score_put(shared, at, score_convert(score_join(hh.a, hl.a, lh.a, ll.a), k0, q0));
    score_put(shared, at+16, score_convert(score_join(hh.b, hl.b, lh.b, ll.b), k0, q1));
    score_put(shared, at+8, score_convert(score_join(hh.c, hl.c, lh.c, ll.c), k1, q0));
    score_put(shared, at+24, score_convert(score_join(hh.d, hl.d, lh.d, ll.d), k1, q1));
    score_sync();
    if warp == 0 && lane < 16 {
        let index = query * cells + krow + lane;
        let bias = score_mask(mask, (2 * index) as u64);
        var total: f32 = 0.0;
        if bias != 0xfc00 {
            for h in 64 {
                let x = score_get(shared, h * 16 + lane);
                if x > 0.0 { total = score_fma(x, score_load(weights + ((4 * (query * 64 + h)) as u64)), total); }
            }
        }
        score_store(output + (4 * index) as u64, total + score_half(bias));
    }
}

extern "fg_dsa_bound_ceil_add" fn bound_add(a: f32, b: f32) -> f32;
extern "fg_dsa_bound_ceil_mul" fn bound_mul(a: f32, b: f32) -> f32;
extern "fg_device_shfl" fn bound_xor(v: f32, distance: i32) -> f32;
extern "fg_device_global_load" fn bound_f32(address: u64) -> f32;
extern "cuda_load_half_g" fn bound_f16(address: u64) -> f32;
extern "as_uint" fn bound_bits(x: f32) -> u32;
extern "as_float" fn bound_float(x: u32) -> f32;
extern "fg_device_global_store" fn bound_store(address: u64, x: f32);
extern "fg_dsa_bound_atomic_max" fn bound_atomic_max(address: u64, value: u32);

export fn dsa_bound_metadata(input: u64, scales: u64, l1: u64, key_max: u64,
    stride: i64, row: i64, lane: i64, key_input: i32) {
    let keys = key_input != 0;
    let source = input + (row * stride) as u64;
    let width: i64 = if keys { 2 } else { 4 };
    var norm: f32 = 0.0;
    var peak: u32 = 0;
    for part in 4 {
        let address = source + ((32 * part + lane) * width) as u64;
        let x = if keys { bound_f16(address) } else { bound_f32(address) };
        let magnitude = bound_bits(x) & 0x7fffffff;
        norm = bound_add(norm, bound_float(magnitude));
        if magnitude > peak { peak = magnitude; }
    }
    var delta: i32 = 16;
    while delta > 0 {
        norm = bound_add(norm, bound_xor(norm, delta));
        let other_peak = bound_bits(bound_xor(bound_float(peak), delta));
        if other_peak > peak { peak = other_peak; }
        delta /= 2;
    }
    if lane != 0 { return; }
    let step = bound_f32(scales + ((row * 4) as u64));
    let biased = (bound_bits(step) >> 23) & 255;
    let ok = biased >= 77 && biased <= 157 && peak < 0x7f800000 && bound_bits(norm) < 0x7f800000;
    if keys {
        if !ok {
            bound_atomic_max(key_max + 12, 0x3f800000);
            return;
        }
        bound_atomic_max(key_max, peak);
        bound_atomic_max(key_max + 4, bound_bits(norm));
        if peak != 0 { bound_atomic_max(key_max + 8, bound_bits(step)); }
    } else {
        bound_store(l1 + (row * 4) as u64, if ok { norm } else { bound_float(0x7f800000) });
    }
}

export fn dsa_bound_query(q_l1: u64, q_scales: u64, key_max: u64, weights: u64,
    output: u64, query: i64, lane: i64) {
    var magnitude: f32 = 0.0;
    var quant: f32 = 0.0;
    let invalid = bound_f32(key_max + 12);
    let largest_key = bound_f32(key_max);
    let key_l1 = bound_f32(key_max + 4);
    let key_half_step = bound_f32(key_max + 8) * 0.5;
    for part in 2 {
        let at = (4 * (query * 64 + 32 * part + lane)) as u64;
        let q = bound_f32(q_l1 + at);
        let half_step = if q == 0.0 { 0.0 } else { bound_f32(q_scales + at) * 0.5 };
        let w = bound_float(bound_bits(bound_f32(weights + at)) & 0x7fffffff);
        if w > 65536.0 || bound_bits(w) >= 0x7f800000 || bound_bits(q) >= 0x7f800000 {
            quant = bound_float(0x7f800000);
        } else {
            let one = bound_mul(q, key_half_step);
            let two = bound_mul(key_l1, half_step);
            let three = bound_mul(128.0, bound_mul(half_step, key_half_step));
            quant = bound_add(quant, bound_mul(w, bound_add(bound_add(one, two), three)));
            magnitude = bound_add(magnitude, bound_mul(w, bound_mul(q, largest_key)));
        }
    }
    var delta: i32 = 16;
    while delta > 0 {
        quant = bound_add(quant, bound_xor(quant, delta));
        magnitude = bound_add(magnitude, bound_xor(magnitude, delta));
        delta /= 2;
    }
    if lane == 0 {
        let scalar = bound_mul(0.0000044, magnitude);
        let candidate = bound_mul(0.0000039, bound_add(magnitude, quant));
        let error = bound_add(bound_add(quant, scalar), bound_add(candidate, 5.421010862427522e-20));
        let certified = invalid == 0.0 && bound_bits(quant) < 0x7f800000 &&
            bound_bits(error) < 0x7f800000;
        bound_store(output + (query * 4) as u64, if certified { error } else { bound_float(0x7f800000) });
    }
}

extern "fg_dsa_top_bits" fn score_bits(x: u64, index: i64) -> u32;
extern "fg_dsa_bins_clear" fn clear_bin(bins: u64, index: i64);
extern "fg_dsa_bins_get" fn read_bin(bins: u64, index: i64) -> u32;
extern "fg_dsa_bins_add" fn add_bin(bins: u64, index: i64) -> u32;
extern "fg_dsa_top_store" fn write_cutoff(out: u64, row: i64, bits: u32);
extern "fg_device_barrier" fn sync_cutoff();

fn ordered_score(bits: u32) -> u32 {
    if bits & 0x7fffffff == 0 { return 0x80000000; }
    return if bits & 0x80000000 != 0 { bits ^ 0xffffffff } else { bits ^ 0x80000000 };
}

export fn dsa_score_cutoff(scores: u64, out: u64, cells: i64, row: i64,
    lane: i64, lanes: i64, bins: u64) {
    var prefix: u32 = 0;
    var mask: u32 = 0;
    var remaining: u32 = 2048;
    var shift: i64 = 28;
    while shift >= 0 {
        var bin = lane;
        while bin < 16 {
            clear_bin(bins, bin);
            bin += lanes;
        }
        sync_cutoff();
        var i = lane;
        while i < cells {
            let ordered = ordered_score(score_bits(scores, row * cells + i));
            if ordered & mask == prefix {
                let ignored = add_bin(bins, ((ordered >> (shift as u32)) & 15) as i64);
            }
            i += lanes;
        }
        sync_cutoff();
        var digit: i64 = 15;
        while digit > 0 {
            let count = read_bin(bins, digit);
            if remaining <= count { break; }
            remaining -= count;
            digit -= 1;
        }
        prefix |= (digit as u32) << (shift as u32);
        mask |= (15 as u32) << (shift as u32);
        shift -= 4;
        sync_cutoff();
    }
    if lane == 0 {
        let bits = if prefix & 0x80000000 != 0 { prefix ^ 0x80000000 } else { prefix ^ 0xffffffff };
        write_cutoff(out, row, bits);
    }
}

extern "fg_dsa_top_bits" fn read_word(base: u64, index: i64) -> u32;
extern "fg_dsa_top_store" fn write_word(base: u64, index: i64, value: u32);
extern "fg_device_barrier" fn sync_block();
fn read_float(base: u64, index: i64) -> f32 { return score_load(base + ((index * 4) as u64)); }
fn read_mask(base: u64, index: i64) -> u32 { return score_mask(base, (index * 2) as u64); }

export fn dsa_compact(scores: u64, mask: u64, cutoffs: u64, errors: u64,
    fixed: u64, band: u64, counts: u64, bins: u64, cells: i64, row: i64,
    lane: i64, lanes: i64, band_capacity: i64) {
    if lane == 0 {
        clear_bin(bins, 0);
        clear_bin(bins, 1);
        clear_bin(bins, 2);
    }
    sync_block();
    let kth = read_float(cutoffs, row) as f64;
    let bound = read_float(errors, row) as f64;
    let max_value: f64 = 3.4028234663852886e38;
    let row_valid = kth >= -max_value && kth <= max_value && bound >= 0.0 && bound <= max_value;
    let radius = 2.0 * bound;
    if row_valid {
        var i = lane;
        while i < cells {
            let at = row * cells + i;
            let m = read_mask(mask, at);
            if m != 0xfc00 {
                let score = read_float(scores, at) as f64;
                if m & 0x7fff != 0 || !(score >= -max_value && score <= max_value) {
                    let ignored = add_bin(bins, 2);
                } else {
                    let distance = score - kth;
                    if distance > radius {
                        let j: i64 = add_bin(bins, 0);
                        if j < 2048 { write_word(fixed, row * 2048 + j, i as u32); }
                    } else if distance >= -radius {
                        let j: i64 = add_bin(bins, 1);
                        if j < band_capacity { write_word(band, row * band_capacity + j, i as u32); }
                    }
                }
            } else if read_word(scores, at) != 0xff800000 {
                let ignored = add_bin(bins, 2);
            }
            i += lanes;
        }
    }
    sync_block();
    if lane == 0 {
        let sure = read_bin(bins, 0);
        let edge = read_bin(bins, 1);
        let usable = row_valid && read_bin(bins, 2) == 0 && sure <= 2048 &&
            edge <= band_capacity as u32 && sure + edge >= 2048;
        write_word(counts, row * 2, if usable { sure } else { 0xffffffff });
        write_word(counts, row * 2 + 1, if usable { edge } else { 0 });
    }
}

extern "fg_device_global_load" fn attn_load(address: u64) -> f32;
extern "fg_device_global_store" fn attn_store(address: u64, value: f32);
extern "fg_device_fma" fn attn_fma(a: f32, b: f32, sum: f32) -> f32;
extern "fg175q_h2f" fn attn_half(bits: u32) -> f32;
extern "fg175q_ld_u16" fn attn_ld_u16(address: u64, offset: u64) -> u32;
extern "fg_attn_word" fn attn_word(config: u64, index: i64) -> i64;
extern "fg_attn_shfl" fn attn_shfl(value: f32, delta: i32) -> f32;
extern "fg_attn_neg_inf" fn attn_neg_inf() -> f32;
fn attn_sum(value: f32) -> f32 {
    var partial = value;
    partial += attn_shfl(partial, 16);
    partial += attn_shfl(partial, 8);
    partial += attn_shfl(partial, 4);
    partial += attn_shfl(partial, 2);
    partial += attn_shfl(partial, 1);
    return partial;
}
fn exact_key_score(q: u64, k: u64, weights: u64, mask: u64, out: u64,
                       cfg: u64, cell: i64, token: i64, lane: i64) {
    let cells = attn_word(cfg, 0);
    if cell >= cells { return; }
    let mask_row = mask + ((token * attn_word(cfg, 5)) as u64);
    let bias = attn_half(attn_ld_u16(mask_row, (2 * cell) as u64));
    let target = out;
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

extern "fg_dsa_top_bits" fn read_id(base: u64, index: i64) -> u32;

export fn dsa_replay(q: u64, k: u64, weights: u64, mask: u64, out: u64, cfg: u64,
    band: u64, counts: u64, capacity: i64, row: i64, tile: i64, thread_index: i64) {
    let slot = 4 * tile + thread_index / 32;
    let lane = thread_index % 32;
    if slot >= capacity || read_id(counts, 2 * row) == 0xffffffff { return; }
    let count: i64 = read_id(counts, 2 * row + 1);
    if slot >= count { return; }
    let at = row * capacity + slot;
    let key: i64 = read_id(band, at);
    exact_key_score(q, k, weights, mask, out + (4 * at) as u64, cfg, key, row, lane);
}

extern "fg_dsa_top_get_key" fn read_key(base: u64, index: i64) -> u64;
extern "fg_dsa_top_put_key" fn write_key(base: u64, index: i64, value: u64);

export fn dsa_finalize(scores: u64, band: u64, counts: u64, out: u64, status: u64,
    keys: u64, invalid: u64, row: i64, lane: i64, lanes: i64, cells: i64) {
    let sure: i64 = read_word(counts, row * 2);
    let count: i64 = read_word(counts, row * 2 + 1);
    if sure > 2048 || count > 1024 || sure + count < 2048 {
        if lane == 0 { write_word(status, row, 0xffffffff); }
        return;
    }
    if lane == 0 { clear_bin(invalid, 0); }
    sync_block();
    var width: i64 = 1;
    while width < count { width *= 2; }
    var i = lane;
    while i < width {
        var packed: u64 = 0;
        if i < count {
            let at = row * 1024 + i;
            let id = read_word(band, at);
            var bits = read_word(scores, at);
            if bits & 0x7f800000 == 0x7f800000 || id as i64 >= cells {
                let ignored = add_bin(invalid, 0);
            } else {
                if bits & 0x7fffffff == 0 { bits = 0; }
                let ordered = if bits & 0x80000000 != 0 { bits ^ 0xffffffff } else { bits ^ 0x80000000 };
                packed = ((ordered as u64) << 32) | (0xffffffff - id as u64);
            }
        }
        write_key(keys, i, packed);
        i += lanes;
    }
    sync_block();
    if read_bin(invalid, 0) != 0 {
        if lane == 0 { write_word(status, row, 0xffffffff); }
        return;
    }
    var window: i64 = 2;
    while window <= width {
        var span = window / 2;
        while span > 0 {
            var j = lane;
            while j < width {
                let partner = j ^ span;
                if partner > j {
                    let a = read_key(keys, j);
                    let b = read_key(keys, partner);
                    let descending = j & window == 0;
                    if (descending && a < b) || (!descending && a > b) {
                        write_key(keys, j, b);
                        write_key(keys, partner, a);
                    }
                }
                j += lanes;
            }
            sync_block();
            span /= 2;
        }
        window *= 2;
    }
    var j = lane;
    while j < 2048 - sure {
        let key = read_key(keys, j);
        write_word(out, row * 2048 + sure + j, 0xffffffff - ((key & 0xffffffff) as u32));
        j += lanes;
    }
    if lane == 0 { write_word(status, row, 0); }
}

extern "giga_dsa_score_shared_q_g" fn fallback_score_tile(q: u64, k: u64, weights: u64, mask: u64,
    scores: u64, config: u64, key: i64, row: i64, lane: i64);
extern "giga_dsa_top_wide_g" fn fallback_top_row(scores: u64, ids: u64, cells: i64,
    row: i64, lane: i64, bins: u64, keys: u64);

export fn dsa_fallback_score(q: u64, k: u64, weights: u64, mask: u64, scores: u64,
    config: u64, status: u64, row: i64, first_tile: i64, step: i64, thread: i64) {
    if read_id(status, row) == 0 { return; }
    let tiles = attn_word(config, 0) / 16;
    let warp_offset = 4 * (thread / 32);
    let lane = thread % 32;
    var tile = first_tile;
    while tile < tiles {
        fallback_score_tile(q, k, weights, mask, scores, config, 16 * tile + warp_offset, row, lane);
        tile += step;
    }
}

export fn dsa_fallback_top(scores: u64, ids: u64, status: u64, cells: i64,
    row: i64, lane: i64, bins: u64, keys: u64) {
    if read_id(status, row) != 0 {
        fallback_top_row(scores, ids, cells, row, lane, bins, keys);
    }
}
