import .packed_simd;
struct Q4Block { d: u16, dmin: u16, scales: u8[12], qs: u8[32][4] }
struct Q8Block { d: f32, qs: i8[32][8], bsums: i16[16] }
struct DotPair { first: f32, second: f32 }
extern "fg_half_to_float" fn half_to_float(x: u16) -> f32;
extern "fmaf" fn fmaf(x: f32, y: f32, z: f32) -> f32;

fn goose_q4_dot2(x: const Q4Block[:], y0: const Q8Block[:], y1: const Q8Block[:]) -> DotPair {
    assert(y0.len >= x.len && y1.len >= x.len);
    var acc0: f32[8] = default<f32[8]>();
    var acc1: f32[8] = default<f32[8]>();
    var min0: f32[4] = default<f32[4]>();
    var min1: f32[4] = default<f32[4]>();
    for ib in x.len {
        let xb = &x[ib];
        let a = &y0[ib];
        let b = &y1[ib];
        let xd = half_to_float(xb.d);
        let xm = half_to_float(xb.dmin);
        let d0 = a.d * xd;
        let d1 = b.d * xd;
        let dm0 = (-a.d) * xm;
        let dm1 = (-b.d) * xm;
        var sc: i32[8] = default<i32[8]>();
        var mn: i32[8] = default<i32[8]>();
        for j in 4 {
            sc[j] = (xb.scales[j] & 63) as i32;
            mn[j] = (xb.scales[j+4] & 63) as i32;
            sc[j+4] = ((xb.scales[j+8] & 15) | ((xb.scales[j] >> 6) << 4)) as i32;
            mn[j+4] = ((xb.scales[j+8] >> 4) | ((xb.scales[j+4] >> 6) << 4)) as i32;
        }
        min0 = packed_simd::fma4(dm0, packed_simd::pair_sum32(packed_simd::mul32(mn, packed_simd::pair_sum16(a.bsums))), min0);
        min1 = packed_simd::fma4(dm1, packed_simd::pair_sum32(packed_simd::mul32(mn, packed_simd::pair_sum16(b.bsums))), min1);
        var sum0: i32[8] = default<i32[8]>();
        var sum1: i32[8] = default<i32[8]>();
        for j in 4 {
            let low = packed_simd::and8(xb.qs[j], 15);
            let high = packed_simd::shr8(xb.qs[j], 4);
            let sl = packed_simd::splat16(sc[2*j] as! i16);
            let sh = packed_simd::splat16(sc[2*j+1] as! i16);
            let al = packed_simd::madd16(sl, packed_simd::maddubs(low, a.qs[2*j]));
            let ah = packed_simd::madd16(sh, packed_simd::maddubs(high, a.qs[2*j+1]));
            let bl = packed_simd::madd16(sl, packed_simd::maddubs(low, b.qs[2*j]));
            let bh = packed_simd::madd16(sh, packed_simd::maddubs(high, b.qs[2*j+1]));
            sum0 = packed_simd::add32(sum0, packed_simd::add32(al, ah));
            sum1 = packed_simd::add32(sum1, packed_simd::add32(bl, bh));
        }
        acc0 = packed_simd::fma32(d0, sum0, acc0);
        acc1 = packed_simd::fma32(d1, sum1, acc1);
    }
    let s0 = ((acc0[4]+acc0[0])+(acc0[6]+acc0[2])) + ((acc0[5]+acc0[1])+(acc0[7]+acc0[3]));
    let s1 = ((acc1[4]+acc1[0])+(acc1[6]+acc1[2])) + ((acc1[5]+acc1[1])+(acc1[7]+acc1[3]));
    return DotPair { first: s0 + ((min0[0]+min0[2])+(min0[1]+min0[3])), second: s1 + ((min1[0]+min1[2])+(min1[1]+min1[3])) };
}

fn main() { let x:Q4Block[1]=default<Q4Block[1]>();let y:Q8Block[1]=default<Q8Block[1]>();let v=goose_q4_dot2(x,y,y);print(v.first);print(v.second); }
