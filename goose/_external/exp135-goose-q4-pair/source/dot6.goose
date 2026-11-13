import packed_simd;
struct Q6Block { ql: u8[32][4], qh: u8[32][2], scales: i8[16], d: u16 }
struct Q8Block { d: f32, qs: i8[32][8], bsums: i16[16] }
struct DotPair { first: f32, second: f32 }
extern fn half_to_float(x: u16) -> f32;
checkfree inline fn join_bits(low: u8[32], high: u8[32]) -> u8[32] {
    var out: u8[32] = default<u8[32]>();
    for i in 32 { out[i] = low[i] | ((high[i] & 3) << 4); }
    return out;
}
export checkfree simd fn goose_q6_dot2(x: const Q6Block[:], y0: const Q8Block[:], y1: const Q8Block[:]) -> DotPair {
    assert(y0.len >= x.len && y1.len >= x.len);
    var acc0: f32[8] = [0; 8];
    var acc1: f32[8] = [0; 8];
    for ib in x.len {
        let xb = &x[ib]; let a = &y0[ib]; let b = &y1[ib];
        var wide: i16[16] = default<i16[16]>();
        for i in 16 { wide[i] = xb.scales[i] as i16; }
        var sum0: i32[8] = [0; 8]; var sum1: i32[8] = [0; 8];
        for j in 2 {
            for segment in 4 {
                let low = if segment < 2 { packed_simd::and8(xb.ql[j*2+segment], 15) } else { packed_simd::shr8(xb.ql[j*2+segment-2], 4) };
                let high = packed_simd::shr8(xb.qh[j], (segment*2) as! u8);
                let values = join_bits(low, high);
                var scales: i16[16] = default<i16[16]>();
                for lane in 16 { scales[lane] = wide[j*8+segment*2+lane/8]; }
                sum0 = packed_simd::add32(sum0, packed_simd::madd16(scales, packed_simd::maddubs(values, a.qs[j*4+segment])));
                sum1 = packed_simd::add32(sum1, packed_simd::madd16(scales, packed_simd::maddubs(values, b.qs[j*4+segment])));
            }
        }
        var correction_scale: i32[8] = [-32; 8];
        sum0 = packed_simd::add32(sum0, packed_simd::mul32(correction_scale, packed_simd::madd16(a.bsums, wide)));
        sum1 = packed_simd::add32(sum1, packed_simd::mul32(correction_scale, packed_simd::madd16(b.bsums, wide)));
        let d = half_to_float(xb.d);
        acc0 = packed_simd::fma32(a.d*d, sum0, acc0);
        acc1 = packed_simd::fma32(b.d*d, sum1, acc1);
    }
    let s0 = ((acc0[4]+acc0[0])+(acc0[6]+acc0[2])) + ((acc0[5]+acc0[1])+(acc0[7]+acc0[3]));
    let s1 = ((acc1[4]+acc1[0])+(acc1[6]+acc1[2])) + ((acc1[5]+acc1[1])+(acc1[7]+acc1[3]));
    return DotPair { first: s0, second: s1 };
}
