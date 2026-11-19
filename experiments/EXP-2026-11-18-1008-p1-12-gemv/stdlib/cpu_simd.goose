// Portable lane semantics; the compiler lowers complete checked bodies only.
// Fixed vectors have an explicit packed ABI and never require aligned input.
namespace cpu_simd;
import packed_simd;
import math;

type v32i8 = i8[32];
type v32u8 = u8[32];
type v16i16 = i16[16];
type v8i32 = i32[8];
type v8f32 = f32[8];
type v4f64 = f64[4];
type v4i64 = i64[4];

extern "gs_simd_f32_from_bits" fn f32_from_bits(value: u32) -> f32;
extern "gs_simd_f64_from_bits" fn f64_from_bits(value: u64) -> f64;
extern "gs_simd_f32_to_bits" fn f32_to_bits(value: f32) -> u32;
extern "gs_simd_f64_to_bits" fn f64_to_bits(value: f64) -> u64;
extern "gs_simd_cpu_features" fn cpu_features() -> u64;

inline fn splat_i8(value: i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = value;
    }
    return out;
}

inline fn add_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = ((a[i] as i64) + (b[i] as i64)) as! i8;
    }
    return out;
}

inline fn sub_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = ((a[i] as i64) - (b[i] as i64)) as! i8;
    }
    return out;
}

inline fn mul_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = ((a[i] as i64) * (b[i] as i64)) as! i8;
    }
    return out;
}

inline fn min_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_i8(mask: v32i8, a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_i8(a: v32i8, index: i64) -> i8 {
    return a[index];
}

inline fn insert_i8(a: v32i8, index: i64, value: i8) -> v32i8 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn and_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = a[i] & b[i];
    }
    return out;
}

inline fn or_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = a[i] | b[i];
    }
    return out;
}

inline fn xor_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

inline fn shl_i8(a: v32i8, count: u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = a[i] << (count & 7);
    }
    return out;
}

inline fn shr_i8(a: v32i8, count: u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = a[i] >> (count & 7);
    }
    return out;
}

inline fn sat_add_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        let value = (a[i] as i64) + (b[i] as i64);
        out[i] = (if value < -128 { -128 } else if value > 127 { 127 } else { value }) as! i8;
    }
    return out;
}

inline fn sat_sub_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        let value = (a[i] as i64) - (b[i] as i64);
        out[i] = (if value < -128 { -128 } else if value > 127 { 127 } else { value }) as! i8;
    }
    return out;
}

inline fn sat_mul_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        let value = (a[i] as i64) * (b[i] as i64);
        out[i] = (if value < -128 { -128 } else if value > 127 { 127 } else { value }) as! i8;
    }
    return out;
}

inline fn splat_u8(value: u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = value;
    }
    return out;
}

inline fn add_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = ((a[i] as i64) + (b[i] as i64)) as! u8;
    }
    return out;
}

inline fn sub_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = ((a[i] as i64) - (b[i] as i64)) as! u8;
    }
    return out;
}

inline fn mul_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = ((a[i] as i64) * (b[i] as i64)) as! u8;
    }
    return out;
}

inline fn min_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_u8(a: v32u8, b: v32u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_u8(a: v32u8, b: v32u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_u8(a: v32u8, b: v32u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_u8(mask: v32i8, a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_u8(a: v32u8, index: i64) -> u8 {
    return a[index];
}

inline fn insert_u8(a: v32u8, index: i64, value: u8) -> v32u8 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn and_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = a[i] & b[i];
    }
    return out;
}

inline fn or_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = a[i] | b[i];
    }
    return out;
}

inline fn xor_u8(a: v32u8, b: v32u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

inline fn shl_u8(a: v32u8, count: u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = a[i] << (count & 7);
    }
    return out;
}

inline fn shr_u8(a: v32u8, count: u8) -> v32u8 {
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = a[i] >> (count & 7);
    }
    return out;
}

inline fn splat_i16(value: i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = value;
    }
    return out;
}

inline fn add_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = ((a[i] as i64) + (b[i] as i64)) as! i16;
    }
    return out;
}

inline fn sub_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = ((a[i] as i64) - (b[i] as i64)) as! i16;
    }
    return out;
}

inline fn mul_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = ((a[i] as i64) * (b[i] as i64)) as! i16;
    }
    return out;
}

inline fn min_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_i16(mask: v16i16, a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_i16(a: v16i16, index: i64) -> i16 {
    return a[index];
}

inline fn insert_i16(a: v16i16, index: i64, value: i16) -> v16i16 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn and_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = a[i] & b[i];
    }
    return out;
}

inline fn or_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = a[i] | b[i];
    }
    return out;
}

inline fn xor_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

inline fn shl_i16(a: v16i16, count: u8) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = a[i] << (count & 15);
    }
    return out;
}

inline fn shr_i16(a: v16i16, count: u8) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = a[i] >> (count & 15);
    }
    return out;
}

inline fn sat_add_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        let value = (a[i] as i64) + (b[i] as i64);
        out[i] = (if value < -32768 { -32768 } else if value > 32767 { 32767 } else { value }) as! i16;
    }
    return out;
}

inline fn sat_sub_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        let value = (a[i] as i64) - (b[i] as i64);
        out[i] = (if value < -32768 { -32768 } else if value > 32767 { 32767 } else { value }) as! i16;
    }
    return out;
}

inline fn sat_mul_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        let value = (a[i] as i64) * (b[i] as i64);
        out[i] = (if value < -32768 { -32768 } else if value > 32767 { 32767 } else { value }) as! i16;
    }
    return out;
}

inline fn splat_i32(value: i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = value;
    }
    return out;
}

inline fn add_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = ((a[i] as i64) + (b[i] as i64)) as! i32;
    }
    return out;
}

inline fn sub_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = ((a[i] as i64) - (b[i] as i64)) as! i32;
    }
    return out;
}

inline fn mul_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = ((a[i] as i64) * (b[i] as i64)) as! i32;
    }
    return out;
}

inline fn min_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_i32(mask: v8i32, a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_i32(a: v8i32, index: i64) -> i32 {
    return a[index];
}

inline fn insert_i32(a: v8i32, index: i64, value: i32) -> v8i32 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn and_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[i] & b[i];
    }
    return out;
}

inline fn or_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[i] | b[i];
    }
    return out;
}

inline fn xor_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[i] ^ b[i];
    }
    return out;
}

inline fn shl_i32(a: v8i32, count: u8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[i] << (count & 31);
    }
    return out;
}

inline fn shr_i32(a: v8i32, count: u8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[i] >> (count & 31);
    }
    return out;
}

inline fn sat_add_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        let value = (a[i] as i64) + (b[i] as i64);
        out[i] = (if value < -2147483648 { -2147483648 } else if value > 2147483647 { 2147483647 } else { value }) as! i32;
    }
    return out;
}

inline fn sat_sub_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        let value = (a[i] as i64) - (b[i] as i64);
        out[i] = (if value < -2147483648 { -2147483648 } else if value > 2147483647 { 2147483647 } else { value }) as! i32;
    }
    return out;
}

inline fn sat_mul_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        let value = (a[i] as i64) * (b[i] as i64);
        out[i] = (if value < -2147483648 { -2147483648 } else if value > 2147483647 { 2147483647 } else { value }) as! i32;
    }
    return out;
}

inline fn splat_f32(value: f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = value;
    }
    return out;
}

inline fn add_f32(a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = a[i] + b[i];
    }
    return out;
}

inline fn sub_f32(a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = a[i] - b[i];
    }
    return out;
}

inline fn mul_f32(a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = a[i] * b[i];
    }
    return out;
}

inline fn min_f32(a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_f32(a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_f32(a: v8f32, b: v8f32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_f32(a: v8f32, b: v8f32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_f32(a: v8f32, b: v8f32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_f32(mask: v8i32, a: v8f32, b: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_f32(a: v8f32, index: i64) -> f32 {
    return a[index];
}

inline fn insert_f32(a: v8f32, index: i64, value: f32) -> v8f32 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn fma_f32(a: v8f32, b: v8f32, c: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = packed_simd::fmaf(a[i], b[i], c[i]);
    }
    return out;
}

inline fn floor_f32(a: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = floor(a[i]);
    }
    return out;
}

inline fn ceil_f32(a: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = ceil(a[i]);
    }
    return out;
}

inline fn trunc_f32(a: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = trunc(a[i]);
    }
    return out;
}

inline fn round_f32(a: v8f32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = round(a[i]);
    }
    return out;
}

inline fn splat_f64(value: f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = value;
    }
    return out;
}

inline fn add_f64(a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = a[i] + b[i];
    }
    return out;
}

inline fn sub_f64(a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = a[i] - b[i];
    }
    return out;
}

inline fn mul_f64(a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = a[i] * b[i];
    }
    return out;
}

inline fn min_f64(a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = if a[i] < b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn max_f64(a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = if a[i] > b[i] { a[i] } else { b[i] };
    }
    return out;
}

inline fn cmp_eq_f64(a: v4f64, b: v4f64) -> v4i64 {
    var out: v4i64 = default<v4i64>();
    for i in 4 {
        out[i] = if a[i] == b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_gt_f64(a: v4f64, b: v4f64) -> v4i64 {
    var out: v4i64 = default<v4i64>();
    for i in 4 {
        out[i] = if a[i] > b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn cmp_lt_f64(a: v4f64, b: v4f64) -> v4i64 {
    var out: v4i64 = default<v4i64>();
    for i in 4 {
        out[i] = if a[i] < b[i] { -1 } else { 0 };
    }
    return out;
}

inline fn select_f64(mask: v4i64, a: v4f64, b: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = if mask[i] != 0 { a[i] } else { b[i] };
    }
    return out;
}

inline fn extract_f64(a: v4f64, index: i64) -> f64 {
    return a[index];
}

inline fn insert_f64(a: v4f64, index: i64, value: f64) -> v4f64 {
    var out = a;
    out[index] = value;
    return out;
}

inline fn fma_f64(a: v4f64, b: v4f64, c: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = packed_simd::fma(a[i], b[i], c[i]);
    }
    return out;
}

inline fn floor_f64(a: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = floor(a[i]);
    }
    return out;
}

inline fn ceil_f64(a: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = ceil(a[i]);
    }
    return out;
}

inline fn trunc_f64(a: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = trunc(a[i]);
    }
    return out;
}

inline fn round_f64(a: v4f64) -> v4f64 {
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        out[i] = round(a[i]);
    }
    return out;
}

inline fn maddubs(a: v32u8, b: v32i8) -> v16i16 {
    return packed_simd::maddubs(a, b);
}

inline fn madd16(a: v16i16, b: v16i16) -> v8i32 {
    return packed_simd::madd16(a, b);
}

inline fn widen_i8_group(a: v32i8, group: u8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[((group & 3) as i64) * 8 + i] as i32;
    }
    return out;
}

inline fn widen_u8_group(a: v32u8, group: u8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[((group & 3) as i64) * 8 + i] as i32;
    }
    return out;
}

inline fn widen_i16_group(a: v16i16, group: u8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[((group & 1) as i64) * 8 + i] as i32;
    }
    return out;
}

inline fn dot4_u8_i8(a: v32u8, b: v32i8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        let value = (
            (a[4 * i + 0] as i64) * (b[4 * i + 0] as i64) +
            (a[4 * i + 1] as i64) * (b[4 * i + 1] as i64) +
            (a[4 * i + 2] as i64) * (b[4 * i + 2] as i64) +
            (a[4 * i + 3] as i64) * (b[4 * i + 3] as i64)
        );
        out[i] = value as! i32;
    }
    return out;
}

inline fn dot4_i8(a: v32i8, b: v32i8) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        let value = (
            (a[4 * i + 0] as i64) * (b[4 * i + 0] as i64) +
            (a[4 * i + 1] as i64) * (b[4 * i + 1] as i64) +
            (a[4 * i + 2] as i64) * (b[4 * i + 2] as i64) +
            (a[4 * i + 3] as i64) * (b[4 * i + 3] as i64)
        );
        out[i] = value as! i32;
    }
    return out;
}

inline fn reduce_i32(a: v8i32) -> i32 {
    var total: i64 = 0;
    for i in 8 {
        total += a[i] as i64;
    }
    return total as! i32;
}

inline fn reduce_i16(a: v16i16) -> i32 {
    var total: i64 = 0;
    for i in 16 {
        total += a[i] as i64;
    }
    return total as! i32;
}

inline fn reduce_f32(a: v8f32) -> f32 {
    let t0: f32 = a[0] + a[4];
    let t1: f32 = a[1] + a[5];
    let t2: f32 = a[2] + a[6];
    let t3: f32 = a[3] + a[7];
    let s0: f32 = t0 + t1;
    let s1: f32 = t2 + t3;
    return s0 + s1;
}

inline fn reduce_f64(a: v4f64) -> f64 {
    let t0: f64 = a[0] + a[2];
    let t1: f64 = a[1] + a[3];
    return t0 + t1;
}

inline fn cvt_i32_f32(a: v8i32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = a[i] as f32;
    }
    return out;
}

inline fn cvt_f32_i32(a: v8f32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        assert(a[i] >= -2147483648.0 && a[i] < 2147483648.0);
        out[i] = a[i] as! i32;
    }
    return out;
}

inline fn permute_i32(a: v8i32, indices: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = a[((indices[i] as! u32) & 7) as i64];
    }
    return out;
}

inline fn permute_f32(a: v8f32, indices: v8i32) -> v8f32 {
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        out[i] = a[((indices[i] as! u32) & 7) as i64];
    }
    return out;
}

inline fn shuffle_i8(a: v32i8, control: v32u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if (control[i] & 128) != 0 { 0 } else { a[(i / 16) * 16 + ((control[i] & 15) as i64)] };
    }
    return out;
}

inline fn zip_lo_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if (i & 1) == 0 { a[(i / 16) * 16 + (i % 16) / 2] } else { b[(i / 16) * 16 + (i % 16) / 2] };
    }
    return out;
}

inline fn zip_hi_i8(a: v32i8, b: v32i8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = if (i & 1) == 0 { a[(i / 16) * 16 + (i % 16) / 2 + 8] } else { b[(i / 16) * 16 + (i % 16) / 2 + 8] };
    }
    return out;
}

inline fn zip_lo_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if (i & 1) == 0 { a[(i / 8) * 8 + (i % 8) / 2] } else { b[(i / 8) * 8 + (i % 8) / 2] };
    }
    return out;
}

inline fn zip_hi_i16(a: v16i16, b: v16i16) -> v16i16 {
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        out[i] = if (i & 1) == 0 { a[(i / 8) * 8 + (i % 8) / 2 + 4] } else { b[(i / 8) * 8 + (i % 8) / 2 + 4] };
    }
    return out;
}

inline fn zip_lo_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if (i & 1) == 0 { a[(i / 4) * 4 + (i % 4) / 2] } else { b[(i / 4) * 4 + (i % 4) / 2] };
    }
    return out;
}

inline fn zip_hi_i32(a: v8i32, b: v8i32) -> v8i32 {
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        out[i] = if (i & 1) == 0 { a[(i / 4) * 4 + (i % 4) / 2 + 2] } else { b[(i / 4) * 4 + (i % 4) / 2 + 2] };
    }
    return out;
}

inline fn alignr_i8(a: v32i8, b: v32i8, count: u8) -> v32i8 {
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        let position = (i % 16) + (count as i64);
        let base = (i / 16) * 16;
        if position < 16 {
            out[i] = b[base + position];
        } else if position < 32 {
            out[i] = a[base + position - 16];
        } else {
            out[i] = 0;
        }
    }
    return out;
}

inline fn loadu_i8(data: const u8[:], offset: i64) -> v32i8 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v32i8 = default<v32i8>();
    for i in 32 {
        out[i] = data[offset + i] as! i8;
    }
    return out;
}

inline fn load_i8(data: const u8[:], offset: i64) -> v32i8 {
    return loadu_i8(data, offset);
}

inline fn storeu_i8(data: u8[:], offset: i64, a: v32i8) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 32 {
        data[offset + i] = a[i] as! u8;
    }
}

inline fn store_i8(data: u8[:], offset: i64, a: v32i8) {
    storeu_i8(data, offset, a);
}

inline fn loadu_u8(data: const u8[:], offset: i64) -> v32u8 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v32u8 = default<v32u8>();
    for i in 32 {
        out[i] = data[offset + i] as! u8;
    }
    return out;
}

inline fn load_u8(data: const u8[:], offset: i64) -> v32u8 {
    return loadu_u8(data, offset);
}

inline fn storeu_u8(data: u8[:], offset: i64, a: v32u8) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 32 {
        data[offset + i] = a[i] as! u8;
    }
}

inline fn store_u8(data: u8[:], offset: i64, a: v32u8) {
    storeu_u8(data, offset, a);
}

inline fn loadu_i16(data: const u8[:], offset: i64) -> v16i16 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v16i16 = default<v16i16>();
    for i in 16 {
        var value: u32 = 0;
        for k in 2 {
            value |= (data[offset + i * 2 + k] as u32) << ((8 * k) as u8);
        }
        out[i] = value as! i16;
    }
    return out;
}

inline fn load_i16(data: const u8[:], offset: i64) -> v16i16 {
    return loadu_i16(data, offset);
}

inline fn storeu_i16(data: u8[:], offset: i64, a: v16i16) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 16 {
        let value = a[i] as! u32;
        for k in 2 {
            data[offset + i * 2 + k] = (value >> ((8 * k) as u8)) as! u8;
        }
    }
}

inline fn store_i16(data: u8[:], offset: i64, a: v16i16) {
    storeu_i16(data, offset, a);
}

inline fn loadu_i32(data: const u8[:], offset: i64) -> v8i32 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v8i32 = default<v8i32>();
    for i in 8 {
        var value: u32 = 0;
        for k in 4 {
            value |= (data[offset + i * 4 + k] as u32) << ((8 * k) as u8);
        }
        out[i] = value as! i32;
    }
    return out;
}

inline fn load_i32(data: const u8[:], offset: i64) -> v8i32 {
    return loadu_i32(data, offset);
}

inline fn storeu_i32(data: u8[:], offset: i64, a: v8i32) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 8 {
        let value = a[i] as! u32;
        for k in 4 {
            data[offset + i * 4 + k] = (value >> ((8 * k) as u8)) as! u8;
        }
    }
}

inline fn store_i32(data: u8[:], offset: i64, a: v8i32) {
    storeu_i32(data, offset, a);
}

inline fn loadu_f32(data: const u8[:], offset: i64) -> v8f32 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v8f32 = default<v8f32>();
    for i in 8 {
        var value: u32 = 0;
        for k in 4 {
            value |= (data[offset + i * 4 + k] as u32) << ((8 * k) as u8);
        }
        out[i] = f32_from_bits(value);
    }
    return out;
}

inline fn load_f32(data: const u8[:], offset: i64) -> v8f32 {
    return loadu_f32(data, offset);
}

inline fn storeu_f32(data: u8[:], offset: i64, a: v8f32) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 8 {
        let value = f32_to_bits(a[i]);
        for k in 4 {
            data[offset + i * 4 + k] = (value >> ((8 * k) as u8)) as! u8;
        }
    }
}

inline fn store_f32(data: u8[:], offset: i64, a: v8f32) {
    storeu_f32(data, offset, a);
}

inline fn loadu_f64(data: const u8[:], offset: i64) -> v4f64 {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    var out: v4f64 = default<v4f64>();
    for i in 4 {
        var value: u64 = 0;
        for k in 8 {
            value |= (data[offset + i * 8 + k] as u64) << ((8 * k) as u8);
        }
        out[i] = f64_from_bits(value);
    }
    return out;
}

inline fn load_f64(data: const u8[:], offset: i64) -> v4f64 {
    return loadu_f64(data, offset);
}

inline fn storeu_f64(data: u8[:], offset: i64, a: v4f64) {
    assert(data.len >= 32 && offset >= 0 && offset <= data.len - 32);
    for i in 4 {
        let value = f64_to_bits(a[i]);
        for k in 8 {
            data[offset + i * 8 + k] = (value >> ((8 * k) as u8)) as! u8;
        }
    }
}

inline fn store_f64(data: u8[:], offset: i64, a: v4f64) {
    storeu_f64(data, offset, a);
}
