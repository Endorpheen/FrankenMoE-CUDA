// Standalone block examples. Kernel computation is Goose; no ggml helper calls.
namespace moe_quant;
import cpu_simd;

inline fn read_u16(data: const u8[:], offset: i64) -> u16 {
    return (data[offset] as u16) | ((data[offset + 1] as u16) << 8);
}

inline fn read_i16(data: const u8[:], offset: i64) -> i16 {
    return read_u16(data, offset) as! i16;
}

inline fn read_f32(data: const u8[:], offset: i64) -> f32 {
    var bits: u32 = 0;
    for i in 4 {
        bits |= (data[offset + i] as u32) << ((8 * i) as u8);
    }
    return cpu_simd::f32_from_bits(bits);
}

fn read_f16(data: const u8[:], offset: i64) -> f32 {
    let half = read_u16(data, offset) as u32;
    let sign = (half & 32768) << 16;
    let exponent = (half >> 10) & 31;
    var fraction = half & 1023;
    if exponent == 31 {
        return cpu_simd::f32_from_bits(sign | 2139095040 | (fraction << 13));
    }
    if exponent == 0 {
        if fraction == 0 {
            return cpu_simd::f32_from_bits(sign);
        }
        var power: i32 = -14;
        while (fraction & 1024) == 0 {
            fraction <<= 1;
            power -= 1;
        }
        fraction &= 1023;
        return cpu_simd::f32_from_bits(sign | (((power + 127) as u32) << 23) | (fraction << 13));
    }
    return cpu_simd::f32_from_bits(sign | ((exponent + 112) << 23) | (fraction << 13));
}

fn scale_min(data: const u8[:], group: i64) -> i32, i32 {
    assert(group >= 0 && group < 8);
    if group < 4 {
        return (data[4 + group] & 63) as i32, (data[8 + group] & 63) as i32;
    }
    let scale = (data[8 + group] & 15) | ((data[group] >> 6) << 4);
    let minimum = (data[8 + group] >> 4) | ((data[4 + group] >> 6) << 4);
    return scale as i32, minimum as i32;
}

// Return both integer witnesses and the native generic scalar FP32 order.
fn q4_k_q8_k(a: const u8[:], b: const u8[:]) -> i32, i32, f32 {
    assert(a.len == 144 && b.len == 292);
    var accum: cpu_simd::v8i32 = default<cpu_simd::v8i32>();
    var minimum_sum: i32 = 0;
    let mask = cpu_simd::splat_u8(15);
    for group in 8 {
        let packed = cpu_simd::loadu_u8(a, 16 + (group / 2) * 32);
        let weights = if (group & 1) == 0 {
            cpu_simd::and_u8(packed, mask)
        } else {
            cpu_simd::shr_u8(packed, 4)
        };
        let activation = cpu_simd::loadu_i8(b, 4 + group * 32);
        let scale, minimum = scale_min(a, group);
        minimum_sum += minimum * (
            (read_i16(b, 260 + group * 4) as i32) +
            (read_i16(b, 262 + group * 4) as i32)
        );
        let scales = cpu_simd::splat_i32(scale);
        for part in 4 {
            let w = cpu_simd::widen_u8_group(weights, part as u8);
            let x = cpu_simd::widen_i8_group(activation, part as u8);
            let products = cpu_simd::mul_i32(w, x);
            accum = cpu_simd::add_i32(accum, cpu_simd::mul_i32(products, scales));
        }
    }
    let d: f32 = read_f16(a, 0) * read_f32(b, 0);
    let dmin: f32 = read_f16(a, 2) * read_f32(b, 0);
    let scaled = cpu_simd::add_f32(cpu_simd::splat_f32(0.0),
        cpu_simd::mul_f32(cpu_simd::splat_f32(d), cpu_simd::cvt_i32_f32(accum)));
    var value: f32 = 0.0 - dmin * (minimum_sum as f32);
    for lane in 8 {
        value += scaled[lane];
    }
    return cpu_simd::reduce_i32(accum), minimum_sum, value;
}

fn q6_k_q8_k(a: const u8[:], b: const u8[:]) -> i32, i32, f32 {
    assert(a.len == 210 && b.len == 292);
    var decoded: i8[256] = default<i8[256]>();
    for half in 2 {
        for i in 32 {
            let low0 = a[half * 64 + i];
            let low1 = a[half * 64 + i + 32];
            let high = a[128 + half * 32 + i];
            decoded[half * 128 + i] = (((low0 & 15) | ((high & 3) << 4)) as i32 - 32) as i8;
            decoded[half * 128 + i + 32] = (((low1 & 15) | (((high >> 2) & 3) << 4)) as i32 - 32) as i8;
            decoded[half * 128 + i + 64] = (((low0 >> 4) | (((high >> 4) & 3) << 4)) as i32 - 32) as i8;
            decoded[half * 128 + i + 96] = (((low1 >> 4) | (((high >> 6) & 3) << 4)) as i32 - 32) as i8;
        }
    }
    var accum: cpu_simd::v8i32 = default<cpu_simd::v8i32>();
    for group in 8 {
        var weights: cpu_simd::v32i8 = default<cpu_simd::v32i8>();
        for lane in 32 {
            weights[lane] = decoded[group * 32 + lane];
        }
        let activation = cpu_simd::loadu_i8(b, 4 + group * 32);
        for part in 4 {
            let scale = a[192 + group * 2 + part / 2] as! i8;
            let w = cpu_simd::widen_i8_group(weights, part as u8);
            let x = cpu_simd::widen_i8_group(activation, part as u8);
            let products = cpu_simd::mul_i32(w, x);
            accum = cpu_simd::add_i32(accum, cpu_simd::mul_i32(products, cpu_simd::splat_i32(scale as i32)));
        }
    }
    let d: f32 = read_f16(a, 208) * read_f32(b, 0);
    let scaled = cpu_simd::mul_f32(cpu_simd::splat_f32(d), cpu_simd::cvt_i32_f32(accum));
    var value: f32 = 0.0;
    for lane in 8 {
        value += scaled[lane];
    }
    return cpu_simd::reduce_i32(accum), 0, value;
}

fn q8_0_q8_0(a: const u8[:], b: const u8[:]) -> i32, i32, f32 {
    assert(a.len == 34 && b.len == 34);
    let left = cpu_simd::loadu_i8(a, 2);
    let right = cpu_simd::loadu_i8(b, 2);
    let products = cpu_simd::dot4_i8(left, right);
    let sum = cpu_simd::reduce_i32(products);
    let scale: f32 = read_f16(a, 0) * read_f16(b, 0);
    return sum, 0, 0.0 + (sum as f32) * scale;
}

fn q4_0_q8_0(a: const u8[:], b: const u8[:]) -> i32, i32, f32 {
    assert(a.len == 18 && b.len == 34);
    var weights: cpu_simd::v32i8 = default<cpu_simd::v32i8>();
    for i in 16 {
        weights[i] = ((a[2 + i] & 15) as i32 - 8) as i8;
        weights[i + 16] = ((a[2 + i] >> 4) as i32 - 8) as i8;
    }
    let activation = cpu_simd::loadu_i8(b, 2);
    let sum = cpu_simd::reduce_i32(cpu_simd::dot4_i8(weights, activation));
    let first: f32 = (sum as f32) * read_f16(a, 0);
    return sum, 0, 0.0 + first * read_f16(b, 0);
}
