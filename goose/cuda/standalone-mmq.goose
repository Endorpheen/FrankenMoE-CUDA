struct MmqDot { a: i32, b: i32, c: i32, d: i32 }
struct MmqSum { a: f32, b: f32, c: f32, d: f32 }

extern "fg_mmq_word" fn mmq_word(address: u64) -> u32;
extern "fg248d_ld_half" fn mmq_half(address: u64) -> f32;
extern "fg248d_ld_i8" fn mmq_i8(address: u64) -> i32;
extern "fg_device_global_load" fn mmq_float(address: u64) -> f32;
extern "fg_device_global_store" fn mmq_store(address: u64, value: f32);
extern "fg_mmq_half_round" fn mmq_half_round(value: f32) -> f32;
extern "fg_device_mul_rn" fn mmq_mul(a: f32, b: f32) -> f32;
extern "fg_device_fma" fn mmq_fma(a: f32, b: f32, sum: f32) -> f32;
extern "fg_mmq_mma32" fn mmq_mma32(a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> MmqDot;
extern "fg_mmq_mma16" fn mmq_mma16(a0: u32, a1: u32, b0: u32) -> MmqDot;
extern "fg182d_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;
extern "fg_mmq_index" fn mmq_index(address: u64, index: i64) -> u64;

fn mmq_q4_fragment(weight_block: u64, group: i64, lane_word: i64) -> u32 {
    let addr = weight_block + ((16 + 32 * (group / 2) + 4 * lane_word) as u64);
    return q4_mmq_nibbles(mmq_word(addr) as i64, group % 2 != 0) as u32;
}

fn mmq_q6_fragment(weight_block: u64, word: i64) -> u32 {
    let tx = 16 * (word / 32) + (word % 16);
    let ql = mmq_word(weight_block + ((4 * tx) as u64));
    let qh = mmq_word(weight_block + ((128 + 4 * (8 * (tx / 16) + tx % 8)) as u64));
    return q6_mmq_bytes(ql as i64, qh as i64, (tx & 8) / 4, word % 32 >= 16) as u32;
}

fn mmq_q4_acc(sum: f32, dot: i32, dm: f32, minimum: f32, ds: u64, fused: bool) -> f32 {
    let scaled = mmq_mul(dm, mmq_half(ds));
    if fused {
        let value = mmq_fma(scaled, dot as f32, sum);
        return mmq_fma(minimum, mmq_half(ds + 2), value);
    }
    let value = sum + mmq_mul(scaled, dot as f32);
    return value + mmq_mul(minimum, mmq_half(ds + 2));
}

fn mmq_q4_block(previous: MmqSum, a: u64, b: u64, source: u64, source0: u64, source1: u64,
    thread: i64, fused: bool) -> MmqSum {
    let da = mmq_half(a);
    let db = mmq_half(b);
    let min_a = -mmq_half(a + 2);
    let min_b = -mmq_half(b + 2);
    var sum = previous;
    for g in 8 {
        let qb = (144 * (g / 4)) as u64;
        let qs = qb + ((16 + 32 * (g % 4) + 4 * thread) as u64);
        let dot = mmq_mma32(mmq_q4_fragment(a, g, thread), mmq_q4_fragment(b, g, thread),
            mmq_q4_fragment(a, g, thread + 4), mmq_q4_fragment(b, g, thread + 4),
            mmq_word(source + qs), mmq_word(source + qs + 16));
        let packed0 = scale_min(a + 4, g);
        let packed1 = scale_min(b + 4, g);
        let d0 = mmq_half_round(da * ((packed0 >> 8) as f32));
        let d1 = mmq_half_round(db * ((packed1 >> 8) as f32));
        let m0 = mmq_half_round(min_a * ((packed0 & 255) as f32));
        let m1 = mmq_half_round(min_b * ((packed1 & 255) as f32));
        let ds = qb + ((4 * (g % 4)) as u64);
        sum.a = mmq_q4_acc(sum.a, dot.a, d0, m0, source0 + ds, fused);
        sum.b = mmq_q4_acc(sum.b, dot.b, d0, m0, source1 + ds, fused);
        sum.c = mmq_q4_acc(sum.c, dot.c, d1, m1, source0 + ds, fused);
        sum.d = mmq_q4_acc(sum.d, dot.d, d1, m1, source1 + ds, fused);
    }
    return sum;
}

fn mmq_q6_acc(sum: f32, dot: i32, scale: i32, d: f32, activation: f32) -> f32 {
    let term = mmq_mul((dot * scale) as f32, d);
    return sum + mmq_mul(term, activation);
}

fn mmq_q6_dot(a: u64, b: u64, source: u64, group: i64, thread: i64) -> MmqDot {
    let qs = (16 + 16 * (group % 8) + 4 * thread) as u64;
    return mmq_mma16(mmq_q6_fragment(a, 4 * group + thread),
        mmq_q6_fragment(b, 4 * group + thread), mmq_word(source + qs));
}

fn mmq_q6_reference_half(sum: MmqSum, a: u64, b: u64, source: u64,
    source0: u64, source1: u64, first_group: i64, thread: i64) -> MmqSum {
    var t0: f32 = 0.0;
    var t1: f32 = 0.0;
    var t2: f32 = 0.0;
    var t3: f32 = 0.0;
    for i in 4 {
        let g = first_group + 2 * i;
        let x = mmq_q6_dot(a, b, source, g, thread);
        let y = mmq_q6_dot(a, b, source, g + 1, thread);
        let s0 = mmq_i8(a + ((192 + g) as u64));
        let s1 = mmq_i8(a + ((193 + g) as u64));
        let s2 = mmq_i8(b + ((192 + g) as u64));
        let s3 = mmq_i8(b + ((193 + g) as u64));
        let d0 = mmq_float(source0 + ((4 * i) as u64));
        let d1 = mmq_float(source1 + ((4 * i) as u64));
        t0 = mmq_fma((x.a * s0 + y.a * s1) as f32, d0, t0);
        t1 = mmq_fma((x.b * s0 + y.b * s1) as f32, d1, t1);
        t2 = mmq_fma((x.c * s2 + y.c * s3) as f32, d0, t2);
        t3 = mmq_fma((x.d * s2 + y.d * s3) as f32, d1, t3);
    }
    let da = mmq_half(a + 208);
    let db = mmq_half(b + 208);
    return MmqSum { a: mmq_fma(t0, da, sum.a), b: mmq_fma(t1, da, sum.b),
        c: mmq_fma(t2, db, sum.c), d: mmq_fma(t3, db, sum.d) };
}

fn mmq_reference_width(minimum: i64, fallback: bool) -> i64 {
    if fallback {
        if minimum <= 8 { return 8; }
        if minimum <= 16 { return 16; }
        if minimum <= 32 { return 32; }
        if minimum <= 64 { return 64; }
        return 128;
    }
    if minimum <= 48 { return 8 * (1 + (minimum - 1) / 8); }
    return 16 * (1 + (minimum - 1) / 16);
}

fn mmq_reference(w0: u64, w1: u64, source: u64, source0: u64, source1: u64,
    extent: i64, kind: i64, row: i64, rows: i64, route: i64, columns: i64, sms: i64, thread: i64) -> MmqSum {
    // Preserve the reference's 128-row tiles and backward fixup order
    // inside each output warp, without a global partial-sum buffer.
    var work = extent;
    var workers: i64 = 1;
    var start: i64 = 0;
    if columns > 0 {
        let ntiles = 1 + (columns - 1) / 128;
        let width = mmq_reference_width(1 + (columns - 1) / ntiles, rows % 128 != 0);
        let tiles = (1 + (rows - 1) / 128) * ntiles;
        let tile = (row / 128) * ntiles + route / width;
        work = tiles * extent;
        let waves = 1 + (tiles - 1) / sms;
        workers = if 100 * tiles / (sms * waves) >= 90 { tiles }
            else { if sms < work { sms } else { work } };
        start = tile * extent;
    }
    var stop = extent;
    var worker = ((start + stop) * workers - 1) / work;
    let last = worker;
    var result = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    var earlier = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    while stop > 0 {
        let split = worker * work / workers;
        let begin = if split > start { split - start } else { 0 };
        var partial = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
        for k in begin .. stop {
            let q = (288 * k) as u64;
            if kind == 12 {
                let w = (144 * k) as u64;
                partial = mmq_q4_block(partial, w0 + w, w1 + w, source + q, source0 + q,
                    source1 + q, thread, true);
            } else {
                let w = (210 * k) as u64;
                for half in 2 {
                    let at = q + ((144 * half) as u64);
                    partial = mmq_q6_reference_half(partial, w0 + w, w1 + w, source + at, source0 + at,
                        source1 + at, 8 * half, thread);
                }
            }
        }
        if worker == last {
            if begin == 0 { return partial; }
            result = partial;
        } else {
            earlier.a += partial.a;
            earlier.b += partial.b;
            earlier.c += partial.c;
            earlier.d += partial.d;
        }
        stop = begin;
        worker -= 1;
    }
    return MmqSum { a: result.a + earlier.a, b: result.b + earlier.b,
        c: result.c + earlier.c, d: result.d + earlier.d };
}

// PTX ISA m16n8k32 and m16n8k16 lane fragments. Each warp writes one
// 16-row by 8-route tile. Padded fragment lanes still enter every MMA.
fn mmq_source(q: u64, column: i64, count: i64, row_bytes: i64, jobs: u64, route: i64,
    words: i64, input_stride: i64) -> u64 {
    let slot = if column < count { column } else { 0 };
    let source = if input_stride == 0 { slot }
        else { (mmq_index(jobs, (route + slot) * words) / (input_stride as u64)) as i64 };
    return q + ((source * row_bytes) as u64);
}

export fn fg_device_mmq(weights: u64, q: u64, out: u64, row_stride: i64, extent: i64, kind: i64,
    row: i64, rows: i64, route_count: i64, lane: i64, out0: u64, out1: u64,
    k_begin: i64, k_end: i64, output_row_base: i64, jobs: u64, route: i64, words: i64, input_stride: i64,
    reference_columns: i64, reference_sms: i64) {
    let group_id = lane / 4;
    let thread_id = lane % 4;
    let row0 = row + group_id;
    let row1 = row0 + 8;
    let col0 = 2 * thread_id;
    let col1 = col0 + 1;
    let row_bytes = 288 * extent;
    let source = mmq_source(q, group_id, route_count, row_bytes, jobs, route, words, input_stride);
    let source0 = mmq_source(q, col0, route_count, row_bytes, jobs, route, words, input_stride);
    let source1 = mmq_source(q, col1, route_count, row_bytes, jobs, route, words, input_stride);
    let w0 = weights + (((if row0 < rows { row0 } else { 0 }) * row_stride) as u64);
    let w1 = weights + (((if row1 < rows { row1 } else { 0 }) * row_stride) as u64);
    var sum0: f32 = 0.0;
    var sum1: f32 = 0.0;
    var sum2: f32 = 0.0;
    var sum3: f32 = 0.0;
    if reference_columns != 0 {
        let sum = mmq_reference(w0, w1, source, source0, source1, extent, kind, row, rows, route,
            reference_columns, reference_sms, thread_id);
        sum0 = sum.a;
        sum1 = sum.b;
        sum2 = sum.c;
        sum3 = sum.d;
    } else if kind == 12 {
        var sum = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
        for weight_block in k_begin .. k_end {
            let w = (144 * weight_block) as u64;
            let q = (288 * weight_block) as u64;
            sum = mmq_q4_block(sum, w0 + w, w1 + w, source + q, source0 + q, source1 + q, thread_id, false);
        }
        sum0 = sum.a;
        sum1 = sum.b;
        sum2 = sum.c;
        sum3 = sum.d;
    } else {
        for weight_block in k_begin .. k_end {
            let a = w0 + ((210 * weight_block) as u64);
            let b = w1 + ((210 * weight_block) as u64);
            let d0 = mmq_half(a + 208);
            let d1 = mmq_half(b + 208);
            for g in 16 {
                let qb = (288 * weight_block + 144 * (g / 8)) as u64;
                let dot = mmq_q6_dot(a, b, source + qb, g, thread_id);
                let scale = (192 + g) as u64;
                let s0 = mmq_i8(a + scale);
                let s1 = mmq_i8(b + scale);
                let ds = qb + ((4 * ((g % 8) / 2)) as u64);
                let act0 = mmq_float(source0 + ds);
                let act1 = mmq_float(source1 + ds);
                sum0 = mmq_q6_acc(sum0, dot.a, s0, d0, act0);
                sum1 = mmq_q6_acc(sum1, dot.b, s0, d0, act1);
                sum2 = mmq_q6_acc(sum2, dot.c, s1, d1, act0);
                sum3 = mmq_q6_acc(sum3, dot.d, s1, d1, act1);
            }
        }
    }
    if col0 < route_count {
        if row0 < rows { mmq_store(out + out0 + ((4 * (row0 - output_row_base)) as u64), sum0); }
        if row1 < rows { mmq_store(out + out0 + ((4 * (row1 - output_row_base)) as u64), sum2); }
    }
    if col1 < route_count {
        if row0 < rows { mmq_store(out + out1 + ((4 * (row0 - output_row_base)) as u64), sum1); }
        if row1 < rows { mmq_store(out + out1 + ((4 * (row1 - output_row_base)) as u64), sum3); }
    }
}

// Stream-K divides the flattened output-tile/K-block range among workers.
// Only the worker finishing a tile writes the output. Earlier fragments
// use the worker's private 64-row by 8-route slot.
const MMQ_STREAM_TILE_BYTES: i64 = 64 * 8 * 4;

export fn fg_device_mmq_stream(weights: u64, packed: u64, out: u64, jobs: u64, tiles: u64, fixup: u64,
    stride: i64, extent: i64, kind: i64, rows: i64, first: i64, first_route: i64,
    words: i64, count: i64, input_stride: i64, worker: i64, workers: i64, thread: i64) {
    let total = (1 + (rows - 1) / 64) * count * extent;
    var at = worker * total / workers;
    let stop = (worker + 1) * total / workers;
    let lane = thread % 32;
    let col0 = 2 * (lane % 4);
    let col1 = col0 + 1;
    while at < stop {
        let tile = at / extent;
        let row_base = (tile / count) * 64;
        let row = row_base + 16 * (thread / 32);
        let kb = at % extent;
        let steps = if stop - at < extent - kb { stop - at } else { extent - kb };
        let end = kb + steps;
        if row < rows {
            let plan = (first + tile % count) * 3;
            let route = mmq_index(tiles, plan) as i64;
            let routes = mmq_index(tiles, plan + 1) as i64;
            let weight = weights + mmq_index(tiles, plan + 2);
            let q = if input_stride == 0 { packed + (((route - first_route) * 288 * extent) as u64) } else { packed };
            if end == extent {
                let out0 = if col0 < routes { mmq_index(jobs, (route + col0) * words + 1) } else { 0 };
                let out1 = if col1 < routes { mmq_index(jobs, (route + col1) * words + 1) } else { 0 };
                fg_device_mmq(weight, q, out, stride, extent, kind, row, rows, routes, lane, out0, out1, kb, end, 0,
                    jobs, route, words, input_stride, 0, 0);
            } else {
                let partial = fixup + ((worker * MMQ_STREAM_TILE_BYTES) as u64);
                fg_device_mmq(weight, q, partial, stride, extent, kind, row, rows, routes, lane,
                    (col0 * 256) as u64, (col1 * 256) as u64, kb, end, row_base, jobs, route, words, input_stride, 0, 0);
            }
        }
        at += steps;
    }
}

export fn fg_device_mmq_fixup(out: u64, jobs: u64, tiles: u64, fixup: u64, extent: i64, rows: i64,
    first: i64, words: i64, count: i64, worker: i64, workers: i64, thread: i64) {
    let total = (1 + (rows - 1) / 64) * count * extent;
    let begin = worker * total / workers;
    let end = (worker + 1) * total / workers;
    let tile = begin / extent;
    let tile_start = tile * extent;
    if begin == tile_start || end < tile_start + extent { return; }
    let plan = (first + tile % count) * 3;
    let route = mmq_index(tiles, plan) as i64;
    let count_routes = mmq_index(tiles, plan + 1) as i64;
    let row_base = (tile / count) * 64;
    var item = thread;
    while item < count_routes * 64 {
        let row = row_base + item % 64;
        if row < rows {
            var sum: f32 = 0.0;
            var previous = worker - 1;
            while previous >= 0 {
                sum += mmq_float(fixup + ((previous * MMQ_STREAM_TILE_BYTES + item * 4) as u64));
                if previous * total / workers <= tile_start { break; }
                previous -= 1;
            }
            let position = mmq_index(jobs, (route + item / 64) * words + 1);
            let address = out + position + ((row * 4) as u64);
            mmq_store(address, mmq_float(address) + sum);
        }
        item += 128;
    }
}
