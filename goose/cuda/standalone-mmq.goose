struct MmqDot { a: i32, b: i32, c: i32, d: i32 }

extern "fg_mmq_word" fn mmq_word(address: u64) -> u32;
extern "fg248d_ld_half" fn mmq_half(address: u64) -> f32;
extern "fg248d_ld_i8" fn mmq_i8(address: u64) -> i32;
extern "fg_device_global_load" fn mmq_float(address: u64) -> f32;
extern "fg_device_global_store" fn mmq_store(address: u64, value: f32);
extern "fg_mmq_half_round" fn mmq_half_round(value: f32) -> f32;
extern "fg_device_mul_rn" fn mmq_mul(a: f32, b: f32) -> f32;
extern "fg_mmq_mma32" fn mmq_mma32(a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> MmqDot;
extern "fg_mmq_mma16" fn mmq_mma16(a0: u32, a1: u32, b0: u32) -> MmqDot;
extern "fg182d_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;

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

fn mmq_q4_acc(sum: f32, dot: i32, dm: f32, minimum: f32, ds: u64) -> f32 {
    let scaled = mmq_mul(dm, mmq_half(ds));
    let value = sum + mmq_mul(scaled, dot as f32);
    return value + mmq_mul(minimum, mmq_half(ds + 2));
}

fn mmq_q6_acc(sum: f32, dot: i32, scale: i32, d: f32, activation: f32) -> f32 {
    let term = mmq_mul((dot * scale) as f32, d);
    return sum + mmq_mul(term, activation);
}

// PTX ISA m16n8k32 and m16n8k16 lane fragments. Each warp writes one
// 16-row by 8-route tile. Padded fragment lanes still enter every MMA.
export fn fg_device_mmq(weights: u64, q: u64, out: u64, row_stride: i64, extent: i64, kind: i64,
    row: i64, rows: i64, route_count: i64, lane: i64, out0: u64, out1: u64) {
    let group_id = lane / 4;
    let thread_id = lane % 4;
    let row0 = row + group_id;
    let row1 = row0 + 8;
    let col0 = 2 * thread_id;
    let col1 = col0 + 1;
    let row_bytes = 288 * extent;
    let source = q + (((if group_id < route_count { group_id } else { 0 }) * row_bytes) as u64);
    let source0 = q + (((if col0 < route_count { col0 } else { 0 }) * row_bytes) as u64);
    let source1 = q + (((if col1 < route_count { col1 } else { 0 }) * row_bytes) as u64);
    let w0 = weights + (((if row0 < rows { row0 } else { 0 }) * row_stride) as u64);
    let w1 = weights + (((if row1 < rows { row1 } else { 0 }) * row_stride) as u64);
    var sum0: f32 = 0.0;
    var sum1: f32 = 0.0;
    var sum2: f32 = 0.0;
    var sum3: f32 = 0.0;
    if kind == 12 {
        for weight_block in extent {
            let a = w0 + ((144 * weight_block) as u64);
            let b = w1 + ((144 * weight_block) as u64);
            let da = mmq_half(a);
            let db = mmq_half(b);
            let min_a = -mmq_half(a + 2);
            let min_b = -mmq_half(b + 2);
            for g in 8 {
                let qb = (288 * weight_block + 144 * (g / 4)) as u64;
                let qs = qb + ((16 + 32 * (g % 4) + 4 * thread_id) as u64);
                let dot = mmq_mma32(mmq_q4_fragment(a, g, thread_id), mmq_q4_fragment(b, g, thread_id),
                    mmq_q4_fragment(a, g, thread_id + 4), mmq_q4_fragment(b, g, thread_id + 4),
                    mmq_word(source + qs), mmq_word(source + qs + 16));
                let packed0 = scale_min(a + 4, g);
                let packed1 = scale_min(b + 4, g);
                let d0 = mmq_half_round(da * ((packed0 >> 8) as f32));
                let d1 = mmq_half_round(db * ((packed1 >> 8) as f32));
                let m0 = mmq_half_round(min_a * ((packed0 & 255) as f32));
                let m1 = mmq_half_round(min_b * ((packed1 & 255) as f32));
                let ds = qb + ((4 * (g % 4)) as u64);
                sum0 = mmq_q4_acc(sum0, dot.a, d0, m0, source0 + ds);
                sum1 = mmq_q4_acc(sum1, dot.b, d0, m0, source1 + ds);
                sum2 = mmq_q4_acc(sum2, dot.c, d1, m1, source0 + ds);
                sum3 = mmq_q4_acc(sum3, dot.d, d1, m1, source1 + ds);
            }
        }
    } else {
        for weight_block in extent {
            let a = w0 + ((210 * weight_block) as u64);
            let b = w1 + ((210 * weight_block) as u64);
            let d0 = mmq_half(a + 208);
            let d1 = mmq_half(b + 208);
            for g in 16 {
                let qb = (288 * weight_block + 144 * (g / 8)) as u64;
                let qs = qb + ((16 + 16 * (g % 8) + 4 * thread_id) as u64);
                let dot = mmq_mma16(mmq_q6_fragment(a, 4 * g + thread_id),
                    mmq_q6_fragment(b, 4 * g + thread_id), mmq_word(source + qs));
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
        if row0 < rows { mmq_store(out + out0 + ((4 * row0) as u64), sum0); }
        if row1 < rows { mmq_store(out + out0 + ((4 * row1) as u64), sum2); }
    }
    if col1 < route_count {
        if row0 < rows { mmq_store(out + out1 + ((4 * row0) as u64), sum1); }
        if row1 < rows { mmq_store(out + out1 + ((4 * row1) as u64), sum3); }
    }
}
