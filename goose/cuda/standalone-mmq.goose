struct MmqDot { a: i32, b: i32, c: i32, d: i32 }
struct MmqSum { a: f32, b: f32, c: f32, d: f32 }
struct MmqWords { a: u32, b: u32, c: u32, d: u32 }
struct MmqScales { d0: f32, d1: f32, m0: f32, m1: f32 }
struct MmqQ6Scales { a: i32, b: i32, c: i32, d: i32 }
struct MmqPairSum { left: MmqSum, right: MmqSum }
struct MmqPartition { work: i64, workers: i64, start: i64 }

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
extern "fg_mmq_ldm32" fn mmq_ldm32(address: u64) -> MmqWords;
extern "fg_mmq_ldm16" fn mmq_ldm16(address: u64) -> MmqWords;
extern "fg_mmq_mma32_shared" fn mmq_mma32_shared(address: u64, b0: u32, b1: u32) -> MmqDot;
extern "fg_mmq_mma16_shared" fn mmq_mma16_shared(address: u64, b0: u32) -> MmqDot;
extern "fg_mmq_shared_store" fn mmq_shared_store(address: u64, word: u32);
extern "fg_mmq_shared_i8" fn mmq_shared_i8(address: u64) -> i32;
extern "fg_mmq_shared_float" fn mmq_shared_float(address: u64) -> f32;
extern "fg_mmq_shared_store_float" fn mmq_shared_store_float(address: u64, value: f32);
extern "fg_mmq_shared_word" fn mmq_shared_word(address: u64) -> u32;
extern "fg_mmq_half_value" fn mmq_half_value(bits: u32) -> f32;
extern "fg_mmq_copy_16" fn mmq_copy_16(destination: u64, source: u64);
extern "fg_mmq_barrier" fn mmq_barrier();
extern "fg182d_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;
extern "fg_mmq_index" fn mmq_index(address: u64, index: i64) -> u64;

fn mmq_q4_fragment(weight_block: u64, group: i64, lane_word: i64) -> u32 {
    let addr = weight_block + ((16 + 32 * (group / 2) + 4 * lane_word) as u64);
    return q4_mmq_nibbles(mmq_word(addr) as i64, group % 2 != 0) as u32;
}

fn mmq_q6_high(weight: u64, tx: i64) -> u32 {
    return mmq_word(weight + ((128 + 4 * (8 * (tx / 16) + tx % 8)) as u64));
}

fn mmq_q6_fragment(weight_block: u64, word: i64) -> u32 {
    let tx = 16 * (word / 32) + (word % 16);
    let ql = mmq_word(weight_block + ((4 * tx) as u64));
    let qh = mmq_q6_high(weight_block, tx);
    return q6_mmq_bytes(ql as i64, qh as i64, (tx & 8) / 4, word % 32 >= 16) as u32;
}

fn mmq_q6_stage(weights: u64, stride: i64, tile: u64, thread: i64, threads: i64) {
    // The Q6_K loader expands each low word into two fragment groups.
    // Shared layout: 16 groups of 16 rows by 16 bytes, 16 signed scales
    // per row, then the 16 FP32 conversion factors.
    for pass in 512 / threads {
        let word = thread + threads * pass;
        let row = word / 32;
        let tx = word % 32;
        let source = weights + ((row * stride) as u64);
        let ql = mmq_word(source + ((4 * tx) as u64)) as i64;
        let qh = mmq_q6_high(source, tx) as i64;
        let shift = (tx & 8) / 4;
        let fragment = 2 * tx - tx % 16;
        let address = tile + ((256 * (fragment / 4) + 16 * row + 4 * (fragment % 4)) as u64);
        mmq_shared_store(address, q6_mmq_bytes(ql, qh, shift, false) as u32);
        mmq_shared_store(address + 1024, q6_mmq_bytes(ql, qh, shift, true) as u32);
    }
    let passes = if threads < 64 { 64 / threads } else { 1 };
    for pass in passes {
        let slot = thread + threads * pass;
        if slot < 64 {
            let row = slot / 4;
            let word = slot % 4;
            let source = weights + ((row * stride) as u64);
            mmq_shared_store(tile + ((4096 + 4 * slot) as u64), mmq_word(source + ((192 + 4 * word) as u64)));
        }
    }
    if thread < 16 {
        let source = weights + ((thread * stride) as u64);
        mmq_shared_store_float(tile + ((4352 + 4 * thread) as u64), mmq_half(source + 208));
    }
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

fn mmq_q4_accumulate(sum: MmqSum, dot: MmqDot, scales: MmqScales, ds0: u64, ds1: u64, fused: bool) -> MmqSum {
    return MmqSum {
        a: mmq_q4_acc(sum.a, dot.a, scales.d0, scales.m0, ds0, fused),
        b: mmq_q4_acc(sum.b, dot.b, scales.d0, scales.m0, ds1, fused),
        c: mmq_q4_acc(sum.c, dot.c, scales.d1, scales.m1, ds0, fused),
        d: mmq_q4_acc(sum.d, dot.d, scales.d1, scales.m1, ds1, fused)
    };
}

fn mmq_q4_shared_fragment(stage: u64, group: i64, lane: i64) -> u64 {
    // The four LDSM groups select the row-half and K-half independently.
    return stage + ((512 * group + 32 * (lane % 16) + 16 * (lane / 16)) as u64);
}

fn mmq_q4_scales_at(scales: u64, group: i64, lane: i64) -> MmqScales {
    let at = scales + ((8 * (16 * group + lane / 4)) as u64);
    return MmqScales { d0: mmq_shared_float(at), d1: mmq_shared_float(at + 64),
        m0: mmq_shared_float(at + 4), m1: mmq_shared_float(at + 68) };
}

fn mmq_q4_shared_scales(stage: u64, group: i64, lane: i64) -> MmqScales {
    return mmq_q4_scales_at(stage + 4096, group, lane);
}

fn mmq_q4_stage(weights: u64, stride: i64, tile: u64, thread: i64, threads: i64) {
    // Cooperatively unpack all 16 rows of the block once. Eight groups of
    // 32 int8 values are interleaved by row, with 16-byte LDSM alignment.
    for pass in 512 / threads {
        let word = thread + threads * pass;
        let row = word / 32;
        let pair = (word % 32) / 8;
        let lane = word % 8;
        let packed = mmq_word(weights + ((row * stride + 16 + 32 * pair + 4 * lane) as u64));
        let address = tile + ((1024 * pair + 32 * row + 4 * lane) as u64);
        mmq_shared_store(address, q4_mmq_nibbles(packed as i64, false) as u32);
        mmq_shared_store(address + 512, q4_mmq_nibbles(packed as i64, true) as u32);
    }
    if thread < 128 {
        let row = thread / 8;
        let group = thread % 8;
        let weight = weights + ((row * stride) as u64);
        let packed = mmq_scale_min(weight + 4, group);
        let d = mmq_half_round(mmq_half(weight) * ((packed >> 8) as f32));
        let minimum = mmq_half_round(-mmq_half(weight + 2) * ((packed & 255) as f32));
        let at = tile + ((4096 + 8 * (16 * group + row)) as u64);
        mmq_shared_store_float(at, d);
        mmq_shared_store_float(at + 4, minimum);
    }
}

fn mmq_q4_block(previous: MmqSum, a: u64, b: u64, source: u64, source0: u64, source1: u64,
    thread: i64, fused: bool, stage: u64, lane: i64) -> MmqSum {
    let da = if lane < 0 { mmq_half(a) } else { 0.0 };
    let db = if lane < 0 { mmq_half(b) } else { 0.0 };
    let min_a = if lane < 0 { -mmq_half(a + 2) } else { 0.0 };
    let min_b = if lane < 0 { -mmq_half(b + 2) } else { 0.0 };
    var sum = previous;
    for g in 8 {
        let qb = (144 * (g / 4)) as u64;
        let qs = qb + ((16 + 32 * (g % 4) + 4 * thread) as u64);
        let rhs0 = mmq_word(source + qs);
        let rhs1 = mmq_word(source + qs + 16);
        let dot = if lane >= 0 {
            mmq_mma32_shared(mmq_q4_shared_fragment(stage, g, lane), rhs0, rhs1)
        } else {
            mmq_mma32(mmq_q4_fragment(a, g, thread), mmq_q4_fragment(b, g, thread),
                mmq_q4_fragment(a, g, thread + 4), mmq_q4_fragment(b, g, thread + 4), rhs0, rhs1)
        };
        let scales = if lane >= 0 {
            mmq_q4_shared_scales(stage, g, lane)
        } else {
            let packed0 = mmq_scale_min(a + 4, g);
            let packed1 = mmq_scale_min(b + 4, g);
            MmqScales { d0: mmq_half_round(da * ((packed0 >> 8) as f32)),
                d1: mmq_half_round(db * ((packed1 >> 8) as f32)),
                m0: mmq_half_round(min_a * ((packed0 & 255) as f32)),
                m1: mmq_half_round(min_b * ((packed1 & 255) as f32)) }
        };
        let ds = qb + ((4 * (g % 4)) as u64);
        sum = mmq_q4_accumulate(sum, dot, scales, source0 + ds, source1 + ds, fused);
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

fn mmq_q6_shared_fragment(stage: u64, group: i64, lane: i64) -> u64 {
    return stage + ((256 * group + 16 * (lane % 16)) as u64);
}

fn mmq_q6_shared_dot(stage: u64, source: u64, group: i64, thread: i64, lane: i64) -> MmqDot {
    let qs = (16 + 16 * (group % 8) + 4 * thread) as u64;
    return mmq_mma16_shared(mmq_q6_shared_fragment(stage, group, lane), mmq_word(source + qs));
}

fn mmq_q6_shared_scales(stage: u64, row: i64, group: i64) -> MmqQ6Scales {
    let at = stage + ((4096 + 16 * row + group) as u64);
    return MmqQ6Scales { a: mmq_shared_i8(at), b: mmq_shared_i8(at + 1),
        c: mmq_shared_i8(at + 128), d: mmq_shared_i8(at + 129) };
}

fn mmq_q6_accumulate_pair(sum: MmqSum, x: MmqDot, y: MmqDot, scale: MmqQ6Scales, d0: f32, d1: f32) -> MmqSum {
    return MmqSum {
        a: mmq_fma((x.a * scale.a + y.a * scale.b) as f32, d0, sum.a),
        b: mmq_fma((x.b * scale.a + y.b * scale.b) as f32, d1, sum.b),
        c: mmq_fma((x.c * scale.c + y.c * scale.d) as f32, d0, sum.c),
        d: mmq_fma((x.d * scale.c + y.d * scale.d) as f32, d1, sum.d)
    };
}

fn mmq_q6_finish_half(sum: MmqSum, part: MmqSum, da: f32, db: f32) -> MmqSum {
    return MmqSum { a: mmq_fma(part.a, da, sum.a), b: mmq_fma(part.b, da, sum.b),
        c: mmq_fma(part.c, db, sum.c), d: mmq_fma(part.d, db, sum.d) };
}

fn mmq_q6_reference_half(sum: MmqSum, a: u64, b: u64, source: u64,
    source0: u64, source1: u64, first_group: i64, thread: i64, stage: u64, lane: i64) -> MmqSum {
    var part = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    let row = if lane < 0 { 0 } else { lane / 4 };
    for i in 4 {
        let g = first_group + 2 * i;
        let x = if lane < 0 { mmq_q6_dot(a, b, source, g, thread) }
            else { mmq_q6_shared_dot(stage, source, g, thread, lane) };
        let y = if lane < 0 { mmq_q6_dot(a, b, source, g + 1, thread) }
            else { mmq_q6_shared_dot(stage, source, g + 1, thread, lane) };
        let scales = if lane < 0 {
            MmqQ6Scales { a: mmq_i8(a + ((192 + g) as u64)), b: mmq_i8(a + ((193 + g) as u64)),
                c: mmq_i8(b + ((192 + g) as u64)), d: mmq_i8(b + ((193 + g) as u64)) }
        } else { mmq_q6_shared_scales(stage, row, g) };
        let d0 = mmq_float(source0 + ((4 * i) as u64));
        let d1 = mmq_float(source1 + ((4 * i) as u64));
        part = mmq_q6_accumulate_pair(part, x, y, scales, d0, d1);
    }
    let da = if lane < 0 { mmq_half(a + 208) } else { mmq_shared_float(stage + ((4352 + 4 * row) as u64)) };
    let db = if lane < 0 { mmq_half(b + 208) } else { mmq_shared_float(stage + ((4384 + 4 * row) as u64)) };
    return mmq_q6_finish_half(sum, part, da, db);
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

fn mmq_reference_partition(extent: i64, row: i64, rows: i64, route: i64, columns: i64, sms: i64) -> MmqPartition {
    if columns > 0 {
        let ntiles = 1 + (columns - 1) / 128;
        let width = mmq_reference_width(1 + (columns - 1) / ntiles, rows % 128 != 0);
        let tiles = (1 + (rows - 1) / 128) * ntiles;
        let tile = (row / 128) * ntiles + route / width;
        let work = tiles * extent;
        let waves = 1 + (tiles - 1) / sms;
        let workers = if 100 * tiles / (sms * waves) >= 90 { tiles }
            else { if sms < work { sms } else { work } };
        return MmqPartition { work: work, workers: workers, start: tile * extent };
    }
    return MmqPartition { work: extent, workers: 1, start: 0 };
}

fn mmq_reference(w0: u64, w1: u64, source: u64, source0: u64, source1: u64,
    extent: i64, kind: i64, row: i64, rows: i64, route: i64, columns: i64, sms: i64, thread: i64,
    stage: u64, shared_weights: u64, stride: i64, thread_block: i64) -> MmqSum {
    // Preserve the reference's 128-row tiles and backward fixup order.
    let partition = mmq_reference_partition(extent, row, rows, route, columns, sms);
    let work = partition.work;
    let workers = partition.workers;
    let start = partition.start;
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
            let lane = if thread_block < 0 { -1 } else { thread_block % 32 };
            if kind == 12 {
                let w = (144 * k) as u64;
                if lane >= 0 {
                    mmq_q4_stage(shared_weights + w, stride, stage, thread_block, 128);
                    mmq_barrier();
                }
                partial = mmq_q4_block(partial, w0 + w, w1 + w, source + q, source0 + q,
                    source1 + q, thread, true, stage, lane);
                if lane >= 0 { mmq_barrier(); }
            } else {
                let w = (210 * k) as u64;
                if lane >= 0 {
                    mmq_q6_stage(shared_weights + w, stride, stage, thread_block, 128);
                    mmq_barrier();
                }
                for half in 2 {
                    let at = q + ((144 * half) as u64);
                    partial = mmq_q6_reference_half(partial, w0 + w, w1 + w, source + at, source0 + at,
                        source1 + at, 8 * half, thread, stage, lane);
                }
                if lane >= 0 { mmq_barrier(); }
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
            reference_columns, reference_sms, thread_id, 0, 0, 0, -1);
        sum0 = sum.a;
        sum1 = sum.b;
        sum2 = sum.c;
        sum3 = sum.d;
    } else if kind == 12 {
        var sum = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
        for weight_block in k_begin .. k_end {
            let w = (144 * weight_block) as u64;
            let q = (288 * weight_block) as u64;
            sum = mmq_q4_block(sum, w0 + w, w1 + w, source + q, source0 + q, source1 + q,
                thread_id, false, 0, -1);
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

// Dense 4096-column reference tiles are 128 columns wide. All four
// warps in a 32-column group therefore use the same reference K splits.
export fn fg_device_mmq_dense_shared(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, kind: i64, row: i64, rows: i64, route: i64, first_route: i64,
    reference_sms: i64, thread: i64, stage: u64) {
    let lane = thread % 32;
    let columns = route + 8 * (thread / 32);
    let group = lane / 4;
    let sub = lane % 4;
    let col0 = 2 * sub;
    let batch = packed + (((columns - first_route) * 288 * extent) as u64);
    let packed_stride = 288 * extent;
    let source = batch + ((group * packed_stride) as u64);
    let source0 = batch + ((col0 * packed_stride) as u64);
    let source1 = source0 + (packed_stride as u64);
    let weight_base = weights + ((row * stride) as u64);
    let w0 = weight_base + ((group * stride) as u64);
    let w1 = w0 + ((8 * stride) as u64);
    let sum = mmq_reference(w0, w1, source, source0, source1, extent, kind, row, rows, columns,
        4096, reference_sms, sub, stage, weight_base, stride, thread);
    let out0 = out + mmq_index(jobs, 2 * (columns + col0) + 1);
    let out1 = out + mmq_index(jobs, 2 * (columns + col0 + 1) + 1);
    let offset = (4 * (row + group)) as u64;
    mmq_store(out0 + offset, sum.a);
    mmq_store(out1 + offset, sum.b);
    mmq_store(out0 + offset + 32, sum.c);
    mmq_store(out1 + offset + 32, sum.d);
}

// Every warp stages weights with the block, including inactive tail warps.
// Only live routes write outputs. Padded lanes use the first valid route.
export fn fg_device_mmq_routed_shared(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, kind: i64, row: i64, rows: i64, route: i64, first_route: i64, routes: i64,
    input_stride: i64, thread: i64, stage: u64) {
    let lane = thread % 32;
    let warp_start = 8 * (thread / 32);
    let tail = routes - warp_start;
    let live = if tail <= 0 { 0 } else if tail < 8 { tail } else { 8 };
    let first = route + (if live == 0 { 0 } else { warp_start });
    let group = lane / 4;
    let sub = lane % 4;
    let col = 2 * sub;
    let row_bytes = 288 * extent;
    let batch = packed + (if input_stride == 0 { ((first - first_route) * row_bytes) as u64 } else { 0 });
    let source = mmq_source(batch, group, live, row_bytes, jobs, first, 3, input_stride);
    let source0 = mmq_source(batch, col, live, row_bytes, jobs, first, 3, input_stride);
    let source1 = mmq_source(batch, col + 1, live, row_bytes, jobs, first, 3, input_stride);
    let base = weights + ((row * stride) as u64);
    let w0 = base + ((group * stride) as u64);
    let w1 = w0 + ((8 * stride) as u64);
    let sum = mmq_reference(w0, w1, source, source0, source1, extent, kind, row, rows,
        first, -1, 0, sub, stage, base, stride, thread);
    let offset = (4 * (row + group)) as u64;
    if col < live {
        let dst = out + mmq_index(jobs, 3 * (first + col) + 1) + offset;
        mmq_store(dst, sum.a);
        mmq_store(dst + 32, sum.c);
    }
    if col + 1 < live {
        let dst = out + mmq_index(jobs, 3 * (first + col + 1) + 1) + offset;
        mmq_store(dst, sum.b);
        mmq_store(dst + 32, sum.d);
    }
}

struct MmqOperand {
    route: i64, live: i64, group: u64, col0: u64, col1: u64
}

fn mmq_wide_operand(packed: u64, jobs: u64, route: i64, first_route: i64, routes: i64,
    start: i64, words: i64, stride: i64, input_stride: i64, lane: i64) -> MmqOperand {
    let tail = routes - start;
    let live = if tail <= 0 { 0 } else if tail < 8 { tail } else { 8 };
    let first = route + (if live == 0 { 0 } else { start });
    let batch = packed + (if input_stride == 0 { ((first - first_route) * stride) as u64 } else { 0 });
    let col = 2 * (lane % 4);
    return MmqOperand { route: first, live: live,
        group: mmq_source(batch, lane / 4, live, stride, jobs, first, words, input_stride),
        col0: mmq_source(batch, col, live, stride, jobs, first, words, input_stride),
        col1: mmq_source(batch, col + 1, live, stride, jobs, first, words, input_stride) };
}

fn mmq_q4_wide_group(sum: MmqSum, operand: MmqOperand, fragment: MmqWords, scales: MmqScales,
    q: u64, qs: u64, ds: u64) -> MmqSum {
    let rhs = operand.group + q + qs;
    let dot = mmq_mma32(fragment.a, fragment.b, fragment.c, fragment.d, mmq_word(rhs), mmq_word(rhs + 16));
    return mmq_q4_accumulate(sum, dot, scales, operand.col0 + q + ds, operand.col1 + q + ds, true);
}

fn mmq_q6_wide_group(sum: MmqSum, operand: MmqOperand, x: MmqWords, y: MmqWords,
    scales: MmqQ6Scales, q: u64, qs: u64, ds: u64) -> MmqSum {
    let rhs = operand.group + q + qs;
    let dot_x = mmq_mma16(x.a, x.b, mmq_word(rhs));
    let dot_y = mmq_mma16(y.a, y.b, mmq_word(rhs + 16));
    return mmq_q6_accumulate_pair(sum, dot_x, dot_y, scales,
        mmq_float(operand.col0 + q + ds), mmq_float(operand.col1 + q + ds));
}

fn mmq_q6_wide_half(sum_left: MmqSum, sum_right: MmqSum, left: MmqOperand, right: MmqOperand,
    q: u64, stage: u64, first_group: i64, lane: i64) -> MmqPairSum {
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    var part_left = zero;
    var part_right = zero;
    let row = lane / 4;
    for pair in 4 {
        let g = first_group + 2 * pair;
        let qs = (16 + 32 * pair + 4 * (lane % 4)) as u64;
        let ds = (4 * pair) as u64;
        let x = mmq_ldm16(mmq_q6_shared_fragment(stage, g, lane));
        let y = mmq_ldm16(mmq_q6_shared_fragment(stage, g + 1, lane));
        let scales = mmq_q6_shared_scales(stage, row, g);
        part_left = mmq_q6_wide_group(part_left, left, x, y, scales, q, qs, ds);
        if right.live > 0 {
            part_right = mmq_q6_wide_group(part_right, right, x, y, scales, q, qs, ds);
        }
    }
    let da = mmq_shared_float(stage + ((4352 + 4 * row) as u64));
    let db = mmq_shared_float(stage + ((4384 + 4 * row) as u64));
    let finish_right = if right.live > 0 { mmq_q6_finish_half(sum_right, part_right, da, db) } else { sum_right };
    return MmqPairSum { left: mmq_q6_finish_half(sum_left, part_left, da, db), right: finish_right };
}

fn mmq_sum_add(left: MmqSum, right: MmqSum) -> MmqSum {
    return MmqSum { a: left.a + right.a, b: left.b + right.b, c: left.c + right.c, d: left.d + right.d };
}

fn mmq_wide_store(out: u64, jobs: u64, words: i64, operand: MmqOperand, row: i64, lane: i64, sum: MmqSum) {
    let col = 2 * (lane % 4);
    let offset = (4 * (row + lane / 4)) as u64;
    if col < operand.live {
        let dst = out + mmq_index(jobs, words * (operand.route + col) + 1) + offset;
        mmq_store(dst, sum.a);
        mmq_store(dst + 32, sum.c);
    }
    if col + 1 < operand.live {
        let dst = out + mmq_index(jobs, words * (operand.route + col + 1) + 1) + offset;
        mmq_store(dst, sum.b);
        mmq_store(dst + 32, sum.d);
    }
}

// Share one weight tile across two eight-route accumulators in each warp.
// All warps stage weights and synchronize on each block of K. The planner
// keeps each dense block inside one reference partition.
export fn fg_device_mmq_wide(weights: u64, packed: u64, out: u64, jobs: u64, stride: i64,
    extent: i64, kind: i64, row: i64, rows: i64, route: i64, first_route: i64, routes: i64, words: i64,
    input_stride: i64, reference_sms: i64, thread: i64, stage: u64, threads: i64) {
    let lane = thread % 32;
    let warp_start = 16 * (thread / 32);
    let left = mmq_wide_operand(packed, jobs, route, first_route, routes, warp_start, words, 288 * extent, input_stride, lane);
    let right = mmq_wide_operand(packed, jobs, route, first_route, routes, warp_start + 8, words, 288 * extent, input_stride, lane);
    let base = weights + ((row * stride) as u64);
    let columns = if words == 2 { 4096 } else { -1 };
    let partition = mmq_reference_partition(extent, row, rows, route, columns, reference_sms);
    let work = partition.work;
    let workers = partition.workers;
    let start = partition.start;
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    var stop = extent;
    var worker = ((start + stop) * workers - 1) / work;
    let last = worker;
    var result_left = zero;
    var result_right = zero;
    var earlier_left = zero;
    var earlier_right = zero;
    while stop > 0 {
        let split = worker * work / workers;
        let begin = if split > start { split - start } else { 0 };
        var partial_left = zero;
        var partial_right = zero;
        for k in begin .. stop {
            let q = (288 * k) as u64;
            if kind == 12 {
                mmq_q4_stage(base + ((144 * k) as u64), stride, stage, thread, threads);
                mmq_barrier();
                if left.live > 0 {
                    for group in 8 {
                        let qb = (144 * (group / 4)) as u64;
                        let qs = qb + ((16 + 32 * (group % 4) + 4 * (lane % 4)) as u64);
                        let ds = qb + ((4 * (group % 4)) as u64);
                        let fragment = mmq_ldm32(mmq_q4_shared_fragment(stage, group, lane));
                        let scales = mmq_q4_shared_scales(stage, group, lane);
                        partial_left = mmq_q4_wide_group(partial_left, left, fragment, scales, q, qs, ds);
                        if right.live > 0 {
                            partial_right = mmq_q4_wide_group(partial_right, right, fragment, scales, q, qs, ds);
                        }
                    }
                }
            } else {
                mmq_q6_stage(base + ((210 * k) as u64), stride, stage, thread, threads);
                mmq_barrier();
                if left.live > 0 {
                    for half in 2 {
                        let out = mmq_q6_wide_half(partial_left, partial_right, left, right,
                            q + ((144 * half) as u64), stage, 8 * half, lane);
                        partial_left = out.left;
                        partial_right = out.right;
                    }
                }
            }
            mmq_barrier();
        }
        if worker == last {
            result_left = partial_left;
            result_right = partial_right;
        } else {
            earlier_left = mmq_sum_add(earlier_left, partial_left);
            earlier_right = mmq_sum_add(earlier_right, partial_right);
        }
        stop = begin;
        worker -= 1;
    }
    // Avoid a new zero addition when the reference tile had no fixup.
    if (last * work / workers) > start {
        result_left = mmq_sum_add(result_left, earlier_left);
        result_right = mmq_sum_add(result_right, earlier_right);
    }
    mmq_wide_store(out, jobs, words, left, row, lane, result_left);
    mmq_wide_store(out, jobs, words, right, row, lane, result_right);
}

fn mmq_q4_shared_acc(sum: MmqSum, dot: MmqDot, scale: MmqScales, ds0: u32, ds1: u32) -> MmqSum {
    let d0 = mmq_half_value(ds0 & 65535);
    let d1 = mmq_half_value(ds1 & 65535);
    let s0 = mmq_half_value(ds0 >> 16);
    let s1 = mmq_half_value(ds1 >> 16);
    return MmqSum {
        a: mmq_fma(scale.m0, s0, mmq_fma(mmq_mul(scale.d0, d0), dot.a as f32, sum.a)),
        b: mmq_fma(scale.m0, s1, mmq_fma(mmq_mul(scale.d0, d1), dot.b as f32, sum.b)),
        c: mmq_fma(scale.m1, s0, mmq_fma(mmq_mul(scale.d1, d0), dot.c as f32, sum.c)),
        d: mmq_fma(scale.m1, s1, mmq_fma(mmq_mul(scale.d1, d1), dot.d as f32, sum.d))
    };
}

fn mmq_q4_shared_column(sum: MmqPairSum, activation: u64, group: i64, lane: i64,
    lo: MmqWords, hi: MmqWords, lo_scales: MmqScales, hi_scales: MmqScales) -> MmqPairSum {
    let sub = lane % 4;
    let rhs = activation + ((144 * (lane / 4) + 16 + 32 * group + 4 * sub) as u64);
    let ds = activation + ((288 * sub + 4 * group) as u64);
    let b0 = mmq_shared_word(rhs);
    let b1 = mmq_shared_word(rhs + 16);
    let ds0 = mmq_shared_word(ds);
    let ds1 = mmq_shared_word(ds + 144);
    let dot_lo = mmq_mma32(lo.a, lo.b, lo.c, lo.d, b0, b1);
    let dot_hi = mmq_mma32(hi.a, hi.b, hi.c, hi.d, b0, b1);
    return MmqPairSum { left: mmq_q4_shared_acc(sum.left, dot_lo, lo_scales, ds0, ds1),
        right: mmq_q4_shared_acc(sum.right, dot_hi, hi_scales, ds0, ds1) };
}

fn mmq_dense_store_pair(out: u64, jobs: u64, tile: i64, col: i64, routes: i64,
    row: i64, lane: i64, sum: MmqPairSum) {
    let live = routes - col;
    let operand = MmqOperand { route: tile + col, live: live, group: 0, col0: 0, col1: 0 };
    mmq_wide_store(out, jobs, 2, operand, row, lane, sum.left);
    mmq_wide_store(out, jobs, 2, operand, row + 16, lane, sum.right);
}

fn mmq_stage_q8_half(stage: u64, packed_tile: u64, pitch: i64, routes: i64, thread: i64) {
    for pass in 5 {
        let vector = thread + 256 * pass;
        if vector < 1152 {
            let slot = vector / 9;
            let live_slot = if slot < routes { slot } else { 0 };
            let source = packed_tile + ((live_slot * pitch + 16 * (vector % 9)) as u64);
            mmq_copy_16(stage + ((16 * vector) as u64), source);
        }
    }
}

fn mmq_stage_q8_block(stage: u64, packed_tile: u64, routes: i64, thread: i64) {
    for pass in 5 {
        let vector = thread + 256 * pass;
        if vector < 1152 {
            let offset = 16 * vector;
            let source = if vector < routes * 9 { offset } else { 16 * (vector % 9) };
            mmq_copy_16(stage + (offset as u64), packed_tile + (source as u64));
        }
    }
}

fn mmq_q6_shared_column(part: MmqPairSum, activation: u64, pair: i64, lane: i64,
    lx: MmqWords, ly: MmqWords, hx: MmqWords, hy: MmqWords,
    ls: MmqQ6Scales, hs: MmqQ6Scales) -> MmqPairSum {
    let sub = lane % 4;
    let rhs = activation + ((144 * (lane / 4) + 16 + 32 * pair + 4 * sub) as u64);
    let ds = activation + ((288 * sub + 4 * pair) as u64);
    let b0 = mmq_shared_word(rhs);
    let b1 = mmq_shared_word(rhs + 16);
    let d0 = mmq_shared_float(ds);
    let d1 = mmq_shared_float(ds + 144);
    return MmqPairSum {
        left: mmq_q6_accumulate_pair(part.left, mmq_mma16(lx.a, lx.b, b0), mmq_mma16(ly.a, ly.b, b1), ls, d0, d1),
        right: mmq_q6_accumulate_pair(part.right, mmq_mma16(hx.a, hx.b, b0), mmq_mma16(hy.a, hy.b, b1), hs, d0, d1)
    };
}

fn mmq_q6_finish_rows(sum: MmqPairSum, part: MmqPairSum, lo_da: f32, lo_db: f32, hi_da: f32, hi_db: f32) -> MmqPairSum {
    return MmqPairSum { left: mmq_q6_finish_half(sum.left, part.left, lo_da, lo_db),
        right: mmq_q6_finish_half(sum.right, part.right, hi_da, hi_db) };
}

// A 128-by-128 CTA shares a full block of Q6_K weights and stages the
// contiguous Q8_1 operand in halves. Retain the integer pair and half
// scaling order of the existing Q6_K reference path.
export fn fg_device_mmq_q6_half_k(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, row: i64, route: i64, first_route: i64, routes: i64,
    packed_columns: i64, thread: i64, stage: u64) {
    let lane = thread % 32;
    let warp = thread / 32;
    let col = 64 * (warp % 2);
    let row_pair = 32 * (warp / 2);
    let activate = stage + 35328;
    let weight_stage = stage + ((4416 * warp) as u64);
    let weight_src = weights + ((stride * (row + 16 * warp)) as u64);
    let lo_stage = stage + ((8832 * (warp / 2)) as u64);
    let hi_stage = lo_stage + 4416;
    let activation = activate + ((144 * col) as u64);
    let packed_tile = packed + ((144 * (route - first_route)) as u64);
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    let empty = MmqPairSum { left: zero, right: zero };
    var sum0 = empty;
    var sum1 = empty;
    var sum2 = empty;
    var sum3 = empty;
    var sum4 = empty;
    var sum5 = empty;
    var sum6 = empty;
    var sum7 = empty;
    for k in extent {
        mmq_q6_stage(weight_src + ((210 * k) as u64), stride, weight_stage, lane, 32);
        for half in 2 {
            let offset = (144 * packed_columns * (2 * k + half)) as u64;
            mmq_stage_q8_block(activate, packed_tile + offset, routes, thread);
            mmq_barrier();
            var part0 = empty;
            var part1 = empty;
            var part2 = empty;
            var part3 = empty;
            var part4 = empty;
            var part5 = empty;
            var part6 = empty;
            var part7 = empty;
            for pair in 4 {
                let group = 8 * half + 2 * pair;
                let lx = mmq_ldm16(mmq_q6_shared_fragment(lo_stage, group, lane));
                let ly = mmq_ldm16(mmq_q6_shared_fragment(lo_stage, group + 1, lane));
                let hx = mmq_ldm16(mmq_q6_shared_fragment(hi_stage, group, lane));
                let hy = mmq_ldm16(mmq_q6_shared_fragment(hi_stage, group + 1, lane));
                let ls = mmq_q6_shared_scales(lo_stage, lane / 4, group);
                let hs = mmq_q6_shared_scales(hi_stage, lane / 4, group);
                part0 = mmq_q6_shared_column(part0, activation, pair, lane, lx, ly, hx, hy, ls, hs);
                part1 = mmq_q6_shared_column(part1, activation + 1152, pair, lane, lx, ly, hx, hy, ls, hs);
                part2 = mmq_q6_shared_column(part2, activation + 2304, pair, lane, lx, ly, hx, hy, ls, hs);
                part3 = mmq_q6_shared_column(part3, activation + 3456, pair, lane, lx, ly, hx, hy, ls, hs);
                part4 = mmq_q6_shared_column(part4, activation + 4608, pair, lane, lx, ly, hx, hy, ls, hs);
                part5 = mmq_q6_shared_column(part5, activation + 5760, pair, lane, lx, ly, hx, hy, ls, hs);
                part6 = mmq_q6_shared_column(part6, activation + 6912, pair, lane, lx, ly, hx, hy, ls, hs);
                part7 = mmq_q6_shared_column(part7, activation + 8064, pair, lane, lx, ly, hx, hy, ls, hs);
            }
            let d_offset = (4352 + 4 * (lane / 4)) as u64;
            let lo_da = mmq_shared_float(lo_stage + d_offset);
            let lo_db = mmq_shared_float(lo_stage + d_offset + 32);
            let hi_da = mmq_shared_float(hi_stage + d_offset);
            let hi_db = mmq_shared_float(hi_stage + d_offset + 32);
            sum0 = mmq_q6_finish_rows(sum0, part0, lo_da, lo_db, hi_da, hi_db);
            sum1 = mmq_q6_finish_rows(sum1, part1, lo_da, lo_db, hi_da, hi_db);
            sum2 = mmq_q6_finish_rows(sum2, part2, lo_da, lo_db, hi_da, hi_db);
            sum3 = mmq_q6_finish_rows(sum3, part3, lo_da, lo_db, hi_da, hi_db);
            sum4 = mmq_q6_finish_rows(sum4, part4, lo_da, lo_db, hi_da, hi_db);
            sum5 = mmq_q6_finish_rows(sum5, part5, lo_da, lo_db, hi_da, hi_db);
            sum6 = mmq_q6_finish_rows(sum6, part6, lo_da, lo_db, hi_da, hi_db);
            sum7 = mmq_q6_finish_rows(sum7, part7, lo_da, lo_db, hi_da, hi_db);
            mmq_barrier();
        }
    }
    mmq_dense_store_pair(out, jobs, route, col, routes, row + row_pair, lane, sum0);
    mmq_dense_store_pair(out, jobs, route, col + 8, routes, row + row_pair, lane, sum1);
    mmq_dense_store_pair(out, jobs, route, col + 16, routes, row + row_pair, lane, sum2);
    mmq_dense_store_pair(out, jobs, route, col + 24, routes, row + row_pair, lane, sum3);
    mmq_dense_store_pair(out, jobs, route, col + 32, routes, row + row_pair, lane, sum4);
    mmq_dense_store_pair(out, jobs, route, col + 40, routes, row + row_pair, lane, sum5);
    mmq_dense_store_pair(out, jobs, route, col + 48, routes, row + row_pair, lane, sum6);
    mmq_dense_store_pair(out, jobs, route, col + 56, routes, row + row_pair, lane, sum7);
}

// Eight warps cover 64 output rows and 128 columns. A half-block of Q8_1
// stays in shared memory while both 32-row warp groups consume it.
export fn fg_device_mmq_q4_m64(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, row: i64, route: i64, first_route: i64, routes: i64, thread: i64, stage: u64) {
    let lane = thread % 32;
    let warp = thread / 32;
    let col = 32 * (warp % 4);
    let row_pair = 32 * (warp / 4);
    let activate = stage + 20480;
    let weight_stage = stage + ((5120 * (thread / 128)) as u64);
    let weight_src = weights + ((stride * (row + 16 * (thread / 128))) as u64);
    let lo_stage = stage + ((320 * row_pair) as u64);
    let hi_stage = lo_stage + 5120;
    let activation = activate + ((144 * col) as u64);
    let pitch = 288 * extent;
    let packed_tile = packed + ((pitch * (route - first_route)) as u64);
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    let empty = MmqPairSum { left: zero, right: zero };
    var sum0 = empty;
    var sum1 = empty;
    var sum2 = empty;
    var sum3 = empty;
    for k in extent {
        let w = weight_src + ((144 * k) as u64);
        mmq_q4_stage(w, stride, weight_stage, thread % 128, 128);
        mmq_q4_stage(w + ((32 * stride) as u64), stride, weight_stage + 10240, thread % 128, 128);
        for half in 2 {
            let q = (288 * k + 144 * half) as u64;
            mmq_stage_q8_half(activate, packed_tile + q, pitch, routes, thread);
            mmq_barrier();
            for group in 4 {
                let g = 4 * half + group;
                let lo = mmq_ldm32(mmq_q4_shared_fragment(lo_stage, g, lane));
                let hi = mmq_ldm32(mmq_q4_shared_fragment(hi_stage, g, lane));
                let lo_scales = mmq_q4_shared_scales(lo_stage, g, lane);
                let hi_scales = mmq_q4_shared_scales(hi_stage, g, lane);
                sum0 = mmq_q4_shared_column(sum0, activation, group, lane, lo, hi, lo_scales, hi_scales);
                sum1 = mmq_q4_shared_column(sum1, activation + 1152, group, lane, lo, hi, lo_scales, hi_scales);
                sum2 = mmq_q4_shared_column(sum2, activation + 2304, group, lane, lo, hi, lo_scales, hi_scales);
                sum3 = mmq_q4_shared_column(sum3, activation + 3456, group, lane, lo, hi, lo_scales, hi_scales);
            }
            mmq_barrier();
        }
    }
    mmq_dense_store_pair(out, jobs, route, col, routes, row + row_pair, lane, sum0);
    mmq_dense_store_pair(out, jobs, route, col + 8, routes, row + row_pair, lane, sum1);
    mmq_dense_store_pair(out, jobs, route, col + 16, routes, row + row_pair, lane, sum2);
    mmq_dense_store_pair(out, jobs, route, col + 24, routes, row + row_pair, lane, sum3);
}

fn mmq_q4_stage_half(weights: u64, stride: i64, tile: u64, half: i64, lane: i64) {
    for pass in 8 {
        let word = lane + 32 * pass;
        let row = word / 16;
        let pair = (word % 16) / 8;
        let part = word % 8;
        let packed = mmq_word(weights + ((row * stride + 16 + 64 * half + 32 * pair + 4 * part) as u64));
        let address = tile + ((1024 * pair + 32 * row + 4 * part) as u64);
        mmq_shared_store(address, q4_mmq_nibbles(packed as i64, false) as u32);
        mmq_shared_store(address + 512, q4_mmq_nibbles(packed as i64, true) as u32);
    }
    for pass in 2 {
        let slot = lane + 32 * pass;
        let row = slot / 4;
        let group = slot % 4;
        let weight = weights + ((row * stride) as u64);
        let packed = mmq_scale_min(weight + 4, group + 4 * half);
        let d = mmq_half_round(mmq_half(weight) * ((packed >> 8) as f32));
        let minimum = mmq_half_round(-mmq_half(weight + 2) * ((packed & 255) as f32));
        let at = tile + ((2048 + 8 * (16 * group + row)) as u64);
        mmq_shared_store_float(at, d);
        mmq_shared_store_float(at + 4, minimum);
    }
}

// The larger CTA gives each warp a 32-by-64 region. Staging only half of
// each 16-row slab keeps the shared allocation equal to that of M64.
export fn fg_device_mmq_q4_m128(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, row: i64, route: i64, first_route: i64, routes: i64, thread: i64, stage: u64) {
    let lane = thread % 32;
    let warp = thread / 32;
    let col = 64 * (warp % 2);
    let row_pair = 32 * (warp / 2);
    let activate = stage + 20480;
    let weight_stage = stage + ((2560 * warp) as u64);
    let weight_src = weights + ((stride * (row + 16 * warp)) as u64);
    let lo_stage = stage + ((2560 * (2 * (warp / 2))) as u64);
    let hi_stage = lo_stage + 2560;
    let activation = activate + ((144 * col) as u64);
    let pitch = 288 * extent;
    let packed_tile = packed + ((pitch * (route - first_route)) as u64);
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    let empty = MmqPairSum { left: zero, right: zero };
    var sum0 = empty;
    var sum1 = empty;
    var sum2 = empty;
    var sum3 = empty;
    var sum4 = empty;
    var sum5 = empty;
    var sum6 = empty;
    var sum7 = empty;
    for k in extent {
        let w = weight_src + ((144 * k) as u64);
        for half in 2 {
            let q = (288 * k + 144 * half) as u64;
            mmq_q4_stage_half(w, stride, weight_stage, half, lane);
            mmq_stage_q8_half(activate, packed_tile + q, pitch, routes, thread);
            mmq_barrier();
            for group in 4 {
                let lo = mmq_ldm32(mmq_q4_shared_fragment(lo_stage, group, lane));
                let hi = mmq_ldm32(mmq_q4_shared_fragment(hi_stage, group, lane));
                let lo_scales = mmq_q4_scales_at(lo_stage + 2048, group, lane);
                let hi_scales = mmq_q4_scales_at(hi_stage + 2048, group, lane);
                sum0 = mmq_q4_shared_column(sum0, activation, group, lane, lo, hi, lo_scales, hi_scales);
                sum1 = mmq_q4_shared_column(sum1, activation + 1152, group, lane, lo, hi, lo_scales, hi_scales);
                sum2 = mmq_q4_shared_column(sum2, activation + 2304, group, lane, lo, hi, lo_scales, hi_scales);
                sum3 = mmq_q4_shared_column(sum3, activation + 3456, group, lane, lo, hi, lo_scales, hi_scales);
                sum4 = mmq_q4_shared_column(sum4, activation + 4608, group, lane, lo, hi, lo_scales, hi_scales);
                sum5 = mmq_q4_shared_column(sum5, activation + 5760, group, lane, lo, hi, lo_scales, hi_scales);
                sum6 = mmq_q4_shared_column(sum6, activation + 6912, group, lane, lo, hi, lo_scales, hi_scales);
                sum7 = mmq_q4_shared_column(sum7, activation + 8064, group, lane, lo, hi, lo_scales, hi_scales);
            }
            mmq_barrier();
        }
    }
    mmq_dense_store_pair(out, jobs, route, col, routes, row + row_pair, lane, sum0);
    mmq_dense_store_pair(out, jobs, route, col + 8, routes, row + row_pair, lane, sum1);
    mmq_dense_store_pair(out, jobs, route, col + 16, routes, row + row_pair, lane, sum2);
    mmq_dense_store_pair(out, jobs, route, col + 24, routes, row + row_pair, lane, sum3);
    mmq_dense_store_pair(out, jobs, route, col + 32, routes, row + row_pair, lane, sum4);
    mmq_dense_store_pair(out, jobs, route, col + 40, routes, row + row_pair, lane, sum5);
    mmq_dense_store_pair(out, jobs, route, col + 48, routes, row + row_pair, lane, sum6);
    mmq_dense_store_pair(out, jobs, route, col + 56, routes, row + row_pair, lane, sum7);
}

fn mmq_q4_stage_quarter(weights: u64, stride: i64, tile: u64, quarter: i64, lane: i64) {
    for pass in 4 {
        let word = lane + 32 * pass;
        let row = word / 8;
        let part = word % 8;
        let packed = mmq_word(weights + ((row * stride + 16 + 32 * quarter + 4 * part) as u64));
        let addr = tile + ((32 * row + 4 * part) as u64);
        mmq_shared_store(addr, q4_mmq_nibbles(packed as i64, false) as u32);
        mmq_shared_store(addr + 512, q4_mmq_nibbles(packed as i64, true) as u32);
    }
    let row = lane / 2;
    let group = lane % 2;
    let weight = weights + ((row * stride) as u64);
    let packed = mmq_scale_min(weight + 4, group + 2 * quarter);
    let d = mmq_half_round(mmq_half(weight) * ((packed >> 8) as f32));
    let minimum = mmq_half_round(-mmq_half(weight + 2) * ((packed & 255) as f32));
    let addr = tile + ((1024 + 8 * (16 * group + row)) as u64);
    mmq_shared_store_float(addr, d);
    mmq_shared_store_float(addr + 4, minimum);
}

fn mmq_stage_q8_quarter(stage: u64, packed_tile: u64, pitch: i64, routes: i64, columns: i64,
    quarter_in_half: i64, thread: i64) {
    let vectors = 5 * columns;
    for pass in 1 + (vectors - 1) / 256 {
        let vector = thread + 256 * pass;
        if vector < vectors {
            let slot = vector / 5;
            let live_slot = if slot < routes { slot } else { 0 };
            let part = vector % 5;
            let offset = if part == 0 { 0 } else { 64 * quarter_in_half + 16 * part };
            mmq_copy_16(stage + ((16 * vector) as u64), packed_tile + ((live_slot * pitch + offset) as u64));
        }
    }
}

fn mmq_q4_quarter_column(sum: MmqPairSum, activation: u64, group: i64, scale_offset: i64, lane: i64,
    lo: MmqWords, hi: MmqWords, lo_scales: MmqScales, hi_scales: MmqScales) -> MmqPairSum {
    let sub = lane % 4;
    let rhs = activation + ((80 * (lane / 4) + 16 + 32 * group + 4 * sub) as u64);
    let ds = activation + ((160 * sub + scale_offset + 4 * group) as u64);
    let b0 = mmq_shared_word(rhs);
    let b1 = mmq_shared_word(rhs + 16);
    let ds0 = mmq_shared_word(ds);
    let ds1 = mmq_shared_word(ds + 80);
    let dot_lo = mmq_mma32(lo.a, lo.b, lo.c, lo.d, b0, b1);
    let dot_hi = mmq_mma32(hi.a, hi.b, hi.c, hi.d, b0, b1);
    return MmqPairSum { left: mmq_q4_shared_acc(sum.left, dot_lo, lo_scales, ds0, ds1),
        right: mmq_q4_shared_acc(sum.right, dot_hi, hi_scales, ds0, ds1) };
}

// Both CTA shapes have eight 32-by-128 warp tiles. A quarter block keeps
// the staged weights and activations within the same shared allocation.
export fn fg_device_mmq_q4_large(weights: u64, packed: u64, out: u64, jobs: u64,
    stride: i64, extent: i64, row: i64, route: i64, first_route: i64, routes: i64,
    thread: i64, stage: u64, height: i64, columns: i64) {
    let lane = thread % 32;
    let warp = thread / 32;
    let column_warps = columns / 128;
    let col = 128 * (warp % column_warps);
    let row_pair = 32 * (warp / column_warps);
    let activate = stage + ((80 * height) as u64);
    let weight_stage = stage + ((1280 * warp) as u64);
    let weight_src = weights + ((stride * (row + 16 * warp)) as u64);
    let lo_stage = stage + ((80 * row_pair) as u64);
    let hi_stage = lo_stage + 1280;
    let activation = activate + ((80 * col) as u64);
    let pitch = 288 * extent;
    let packed_tile = packed + ((pitch * (route - first_route)) as u64);
    let zero = MmqSum { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    let empty = MmqPairSum { left: zero, right: zero };
    var sum0 = empty;
    var sum1 = empty;
    var sum2 = empty;
    var sum3 = empty;
    var sum4 = empty;
    var sum5 = empty;
    var sum6 = empty;
    var sum7 = empty;
    var sum8 = empty;
    var sum9 = empty;
    var sum10 = empty;
    var sum11 = empty;
    var sum12 = empty;
    var sum13 = empty;
    var sum14 = empty;
    var sum15 = empty;
    for k in extent {
        let w = weight_src + ((144 * k) as u64);
        for quarter in 4 {
            let q = (288 * k + 144 * (quarter / 2)) as u64;
            let local = quarter % 2;
            let scale_offset = 8 * local;
            mmq_q4_stage_quarter(w, stride, weight_stage, quarter, lane);
            if height == 256 {
                mmq_q4_stage_quarter(w + ((128 * stride) as u64), stride, weight_stage + 10240, quarter, lane);
            }
            mmq_stage_q8_quarter(activate, packed_tile + q, pitch, routes, columns, local, thread);
            mmq_barrier();
            for group in 2 {
                let lo = mmq_ldm32(mmq_q4_shared_fragment(lo_stage, group, lane));
                let hi = mmq_ldm32(mmq_q4_shared_fragment(hi_stage, group, lane));
                let lo_scales = mmq_q4_scales_at(lo_stage + 1024, group, lane);
                let hi_scales = mmq_q4_scales_at(hi_stage + 1024, group, lane);
                sum0 = mmq_q4_quarter_column(sum0, activation, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum1 = mmq_q4_quarter_column(sum1, activation + 640, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum2 = mmq_q4_quarter_column(sum2, activation + 1280, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum3 = mmq_q4_quarter_column(sum3, activation + 1920, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum4 = mmq_q4_quarter_column(sum4, activation + 2560, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum5 = mmq_q4_quarter_column(sum5, activation + 3200, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum6 = mmq_q4_quarter_column(sum6, activation + 3840, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum7 = mmq_q4_quarter_column(sum7, activation + 4480, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum8 = mmq_q4_quarter_column(sum8, activation + 5120, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum9 = mmq_q4_quarter_column(sum9, activation + 5760, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum10 = mmq_q4_quarter_column(sum10, activation + 6400, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum11 = mmq_q4_quarter_column(sum11, activation + 7040, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum12 = mmq_q4_quarter_column(sum12, activation + 7680, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum13 = mmq_q4_quarter_column(sum13, activation + 8320, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum14 = mmq_q4_quarter_column(sum14, activation + 8960, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
                sum15 = mmq_q4_quarter_column(sum15, activation + 9600, group, scale_offset, lane, lo, hi, lo_scales, hi_scales);
            }
            mmq_barrier();
        }
    }
    mmq_dense_store_pair(out, jobs, route, col, routes, row + row_pair, lane, sum0);
    mmq_dense_store_pair(out, jobs, route, col + 8, routes, row + row_pair, lane, sum1);
    mmq_dense_store_pair(out, jobs, route, col + 16, routes, row + row_pair, lane, sum2);
    mmq_dense_store_pair(out, jobs, route, col + 24, routes, row + row_pair, lane, sum3);
    mmq_dense_store_pair(out, jobs, route, col + 32, routes, row + row_pair, lane, sum4);
    mmq_dense_store_pair(out, jobs, route, col + 40, routes, row + row_pair, lane, sum5);
    mmq_dense_store_pair(out, jobs, route, col + 48, routes, row + row_pair, lane, sum6);
    mmq_dense_store_pair(out, jobs, route, col + 56, routes, row + row_pair, lane, sum7);
    mmq_dense_store_pair(out, jobs, route, col + 64, routes, row + row_pair, lane, sum8);
    mmq_dense_store_pair(out, jobs, route, col + 72, routes, row + row_pair, lane, sum9);
    mmq_dense_store_pair(out, jobs, route, col + 80, routes, row + row_pair, lane, sum10);
    mmq_dense_store_pair(out, jobs, route, col + 88, routes, row + row_pair, lane, sum11);
    mmq_dense_store_pair(out, jobs, route, col + 96, routes, row + row_pair, lane, sum12);
    mmq_dense_store_pair(out, jobs, route, col + 104, routes, row + row_pair, lane, sum13);
    mmq_dense_store_pair(out, jobs, route, col + 112, routes, row + row_pair, lane, sum14);
    mmq_dense_store_pair(out, jobs, route, col + 120, routes, row + row_pair, lane, sum15);
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
