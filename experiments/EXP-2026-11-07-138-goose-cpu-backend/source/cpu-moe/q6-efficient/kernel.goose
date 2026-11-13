import.packed_simd;
struct Q6Block {
    ql: u8[32][4], qh: u8[32][2], scales: i8[16], d: u16
}
struct Q8Block {
    d: f32, qs: i8[32][8], bsums: i16[16]
}
extern "fg_half_to_float" fn half_to_float(x: u16) -> f32;

fn widen(a: i8[16]) -> i16[16] {
    var out: i16[16] = default < i16[16] > ();
    for i in 16 {
        out[i] = a[i] as i16;
    }
    return out;
}

fn scales_for(a: i8[16], pair: i64) -> i16[16] {
    let low = a[pair * 2] as i16;
    let high = a[pair * 2 + 1] as i16;
    var out: i16[16] = default < i16[16] > ();
    for i in 8 {
        out[i] = low;
        out[i + 8] = high;
    }
    return out;
}

fn or8(a: u8[32], b: u8[32]) -> u8[32] {
    var out: u8[32] = default < u8[32] > ();
    for i in 32 {
        out[i] = a[i] | b[i];
    }
    return out;
}

fn shl8(a: u8[32], count: u8) -> u8[32] {
    var out: u8[32] = default < u8[32] > ();
    for i in 32 {
        out[i] = a[i] << count;
    }
    return out;
}

fn sub32(a: i32[8], b: i32[8]) -> i32[8] {
    var out: i32[8] = default < i32[8] > ();
    for i in 8 {
        out[i] = a[i] - b[i];
    }
    return out;
}

fn times32(a: i32[8]) -> i32[8] {
    var out: i32[8] = default < i32[8] > ();
    for i in 8 {
        out[i] = a[i] * 32;
    }
    return out;
}

fn goose_q6_dot(x: const Q6Block[:], y: const Q8Block[:]) -> f32 {
    assert(y.len >= x.len);
    var acc: f32[8] = default < f32[8] > ();
    for ib in x.len {
        let a = & x[ib];
        let b = & y[ib];
        let d = b.d * half_to_float(a.d);
        let correction = times32(packed_simd :: madd16(b.bsums, widen(a.scales)));
        var sum: i32[8] = default < i32[8] > ();
        for j in 2 {
            let h = a.qh[j];
            let q0 = or8(packed_simd :: and8(a.ql[2 * j], 15), shl8(packed_simd :: and8(h, 3), 4));
            let q1 = or8(packed_simd :: and8(a.ql[2 * j + 1], 15), shl8(packed_simd :: and8(h, 12), 2));
            let q2 = or8(packed_simd :: shr8(a.ql[2 * j], 4), packed_simd :: and8(h, 48));
            let q3 = or8(packed_simd :: shr8(a.ql[2 * j + 1], 4), packed_simd :: shr8(packed_simd :: and8(h, 192), 2));
            let p0 = packed_simd :: madd16(scales_for(a.scales, 4 * j), packed_simd :: maddubs(q0, b.qs[4 * j]));
            let p1 = packed_simd :: madd16(
                scales_for(a.scales, 4 * j + 1),
                packed_simd :: maddubs(q1, b.qs[4 * j + 1]));
            let p2 = packed_simd :: madd16(
                scales_for(a.scales, 4 * j + 2),
                packed_simd :: maddubs(q2, b.qs[4 * j + 2]));
            let p3 = packed_simd :: madd16(
                scales_for(a.scales, 4 * j + 3),
                packed_simd :: maddubs(q3, b.qs[4 * j + 3]));
            sum = packed_simd :: add32(sum, packed_simd :: add32(p0, p1));
            sum = packed_simd :: add32(sum, packed_simd :: add32(p2, p3));
        }
        acc = packed_simd :: fma32(d, sub32(sum, correction), acc);
    }
    return ( (acc[4] + acc[0]) + (acc[6] + acc[2])) + ( (acc[5] + acc[1]) + (acc[7] + acc[3]));
}

fn goose_q6_tile(x: const Q6Block[:], y: const Q8Block[:], blocks: i64, rows: i64) -> f32[16] {
    assert(blocks > 0 && rows > 0 && rows <= 16 && x.len >= blocks * rows && y.len >= blocks);
    var out: f32[16] = default < f32[16] > ();
    for row in rows {
        out[row] = goose_q6_dot(x[row * blocks .. (row + 1) * blocks], y[ .. blocks]);
    }
    return out;
}

fn main() {
    let x: Q6Block[16] = default < Q6Block[16] > ();
    let y: Q8Block[1] = default < Q8Block[1] > ();
    let out = goose_q6_tile(x, y, 1, 16);
    print(out[0]);
}

