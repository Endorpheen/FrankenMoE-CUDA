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

export fn fg_route_norm(x: u64, ids: u64, out: u64, cfg: u64, row: i64, lane: i64) {
    let experts = rt_word(cfg, 4);
    let used = rt_word(cfg, 12);
    let scale = rt_float(cfg, 21);
    var sum: f64 = 0.0;
    var picked: f32[2] = default <f32[2]>();
    var slot = lane;
    while slot < used {
        let expert = rt_read_i32(ids, row * used + slot) as i64;
        if expert < 0 || expert >= experts { rt_fail(1511); return; }
        let value = rt_load(x + ((4 * (row * experts + expert)) as u64));
        picked[slot / 32] = value;
        sum += value as f64;
        slot += 32;
    }
    let denominator = fmaxf(cv_warp_sum(sum) as f32, 0.00006103515625);
    slot = lane;
    while slot < used {
        let weight = picked[slot / 32] / denominator;
        let scaled = if scale == 0.0 || scale == 1.0 { weight } else { weight * scale + 0.0 };
        rt_store(out + ((4 * (row * used + slot)) as u64), scaled);
        slot += 32;
    }
}
fn warp_max(x: f32) -> f32 {
    var v: f32 = x;
    var off: i64 = 16 as i64;

    while off >= 1 as i64 {
        let t: f32 = shfl_xor(v, off as i32);
        v = fmaxf(v, t);
        off = off / 2 as i64;
    }
    v
}
fn cv_warp_sum(value: f64) -> f64 {
    var sum = value;
    for step in 5 {
        sum += cv_shuffle_f64(sum, (16 >> step) as i32);
    }
    return sum;
}
// Apex CUDA argsort (bitonic) in Goose, from ggml-cuda/argsort.cu.
//
// Device side: shared index initialization, the bitonic (k, j) stages with a
// barrier per stage, the bitonic_step compare/swap with the padding rule
// (index >= ncols sorts to the end), the strict comparisons that leave ties in
// place, and the final copy without padding.
// Host side: ncols_pad = next_power_of_2(ncols), block = min(ncols_pad, 1024),
// grid = nrows, shared = ncols_pad*4, with a shared-memory limit check.
//
// Shared memory and barriers are explicit primitives; every comparison, index
// and swap decision is Goose.
extern "fg283d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg283d_st_i32" fn st_i32(dst: u64, o: u64, v: i32);
extern "fg283d_sload_i32" fn sload_i32(a: u64) -> i32;
extern "fg283d_sstore_i32" fn sstore_i32(a: u64, v: i32);
extern "fg283d_sync" fn sync();
extern "fg283d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn sort_ordered(a: f32, b: f32, order: i64) -> bool {
    return if order == 0 { a <= b } else { a >= b };
}
// order: 0 = ASC, 1 = DESC.

fn bitstep(x: u64, dr: u64, ncols: i64, col: i64, k: i64, j: i64, order: i64, stable: bool) {
    let ixj: i64 = col ^ j;

    if ixj <= col as i64 {
        return;
    }
    let a: i64 = (sload_i32(dr + ( (col * 4 as i64) as u64))) as i64;
    let b: i64 = (sload_i32(dr + ( (ixj * 4 as i64) as u64))) as i64;
    var doswap: i64 = 0 as i64;

    if stable {
        let low = if (col & k) == 0 { a } else { b };
        let high = if (col & k) == 0 { b } else { a };
        if low >= ncols || high >= ncols {
            if low > high { doswap = 1; }
        } else {
            let left = ld_f32(x, (low * 4) as u64);
            let right = ld_f32(x, (high * 4) as u64);
            if !sort_ordered(left, right, order) || (left == right && low > high) { doswap = 1; }
        }
    } else if ( (col & k) == (0 as i64)) {
        if a >= ncols as i64 {
            doswap = 1 as i64;
        } else {
            if b < ncols as i64 {
                if order == (0 as i64) {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) > ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                } else {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) < ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                }
            }
        }
    } else {
        if b >= ncols as i64 {
            doswap = 1 as i64;
        } else {
            if a < ncols as i64 {
                if order == (0 as i64) {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) < ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                } else {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) > ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                }
            }
        }
    }

    if doswap != (0 as i64) {
        sstore_i32(dr + ( (col * 4 as i64) as u64), b as i32);
        sstore_i32(dr + ( (ixj * 4 as i64) as u64), a as i32);
    }
}

fn argsort_row(x: u64, dst: u64, ncols: i64, ncols_pad: i64, nout: i64, order: i64,
               blk: i64, tid: i64, dr: u64, nan_flag: u64) {
    ensure(ncols > 0 as i64 && ncols_pad >= ncols as i64, 20);
    ensure(blk > 0 as i64 && tid >= 0 as i64, 21);
    let stable = nan_flag != 0;
    var col: i64 = tid;
    var found_nan: i32 = 0;

    while col < ncols_pad as i64 {
        sstore_i32(dr + ( (col * 4 as i64) as u64), col as i32);
        if stable && col < ncols {
            let value = ld_f32(x, (4 * col) as u64);
            if value != value { found_nan = 1; }
        }
        col = col + blk;
    }
    if stable { sstore_i32(nan_flag + ((4 * tid) as u64), found_nan); }
    sync();
    if stable {
        if tid == 0 {
            var found: i32 = 0;
            for i in blk { found |= sload_i32(nan_flag + ((4 * i) as u64)); }
            sstore_i32(nan_flag, found);
        }
        sync();
        found_nan = sload_i32(nan_flag);
    }
    if stable && found_nan != 0 {
        // Insertion order is observable for unordered values in the CPU graph.
        if tid == 0 {
            for i in ncols {
                var j = i;
                while j > 0 {
                    let at = dr + ((4 * j) as u64);
                    let a = sload_i32(at - 4);
                    let b = sload_i32(at);
                    if sort_ordered(ld_f32(x, (4 * (a as i64)) as u64),
                                    ld_f32(x, (4 * (b as i64)) as u64), order) { break; }
                    sstore_i32(at - 4, b);
                    sstore_i32(at, a);
                    j -= 1;
                }
            }
        }
        sync();
    } else {
        var k: i64 = 2;
        while k <= ncols_pad {
            var j = k / 2;
            while j > 0 {
                col = tid;
                while col < ncols_pad {
                    bitstep(x, dr, ncols, col, k, j, order, stable);
                    col += blk;
                }
                sync();
                j /= 2;
            }
            k *= 2;
        }
    }
    col = tid;

    while col < nout {
        let v: i32 = sload_i32(dr + ( (col * 4 as i64) as u64));
        st_i32(dst, (4 * col) as u64, v);
        col = col + blk;
    }
}

extern "fg169t_shuffle" fn shuffle(v: f32, offset: i32) -> f32;
extern "fg169t_ishuffle" fn ishuffle(v: i32, offset: i32) -> i32;
struct RouteWinner { value: f32, key: f32, expert: i32 }
fn topk_winner(value: f32, key: f32, expert: i32) -> RouteWinner {
    var winner = RouteWinner { value, key, expert };
    var offset: i32 = 16;
    while offset > 0 {
        let alt = RouteWinner { shuffle(winner.value, offset), shuffle(winner.key, offset), ishuffle(winner.expert, offset) };
        if alt.key > winner.key || (alt.key == winner.key && alt.expert < winner.expert) { winner = alt; }
        offset >>= 1;
    }
    return winner;
}
