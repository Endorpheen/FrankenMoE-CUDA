struct Q4Block { d: u16, dmin: u16, scales: u8[12], qs: u8[128] }
struct Q8Block { d: f32, qs: i8[256], bsums: i16[16] }
struct DotPair { first: f32, second: f32 }
extern fn half_to_float(x: u16) -> f32;
extern fn fmaf(x: f32, y: f32, z: f32) -> f32;

export checkfree simd fn goose_q4_dot2(x: const Q4Block[:], y0: const Q8Block[:], y1: const Q8Block[:]) -> DotPair {
    assert(y0.len >= x.len && y1.len >= x.len);
    var acc0: f32[8] = [0,0,0,0,0,0,0,0];
    var acc1: f32[8] = [0,0,0,0,0,0,0,0];
    var min0: f32[4] = [0,0,0,0];
    var min1: f32[4] = [0,0,0,0];
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
        var sc: i32[8] = [0,0,0,0,0,0,0,0];
        var mn: i32[8] = [0,0,0,0,0,0,0,0];
        for j in 4 {
            sc[j] = (xb.scales[j] & 63) as i32;
            mn[j] = (xb.scales[j+4] & 63) as i32;
            sc[j+4] = ((xb.scales[j+8] & 15) | ((xb.scales[j] >> 6) << 4)) as i32;
            mn[j+4] = ((xb.scales[j+8] >> 4) | ((xb.scales[j+4] >> 6) << 4)) as i32;
        }
        for k in 4 {
            let i = k*4;
            let ap0 = (a.bsums[i] as i32) + (a.bsums[i+1] as i32);
            let ap1 = (a.bsums[i+2] as i32) + (a.bsums[i+3] as i32);
            let bp0 = (b.bsums[i] as i32) + (b.bsums[i+1] as i32);
            let bp1 = (b.bsums[i+2] as i32) + (b.bsums[i+3] as i32);
            min0[k] = fmaf(dm0, (mn[2*k]*ap0 + mn[2*k+1]*ap1) as f32, min0[k]);
            min1[k] = fmaf(dm1, (mn[2*k]*bp0 + mn[2*k+1]*bp1) as f32, min1[k]);
        }
        for lane in 8 {
            var sum0: i32 = 0;
            var sum1: i32 = 0;
            for j in 4 {
                var low0: i32 = 0;
                var high0: i32 = 0;
                var low1: i32 = 0;
                var high1: i32 = 0;
                for r in 4 {
                    let qi = j*32 + lane*4 + r;
                    let yi = j*64 + lane*4 + r;
                    let packed = xb.qs[qi];
                    let ql = (packed & 15) as i32;
                    let qh = (packed >> 4) as i32;
                    low0 += ql * (a.qs[yi] as i32);
                    high0 += qh * (a.qs[yi+32] as i32);
                    low1 += ql * (b.qs[yi] as i32);
                    high1 += qh * (b.qs[yi+32] as i32);
                }
                sum0 += low0*sc[2*j] + high0*sc[2*j+1];
                sum1 += low1*sc[2*j] + high1*sc[2*j+1];
            }
            acc0[lane] = fmaf(d0, sum0 as f32, acc0[lane]);
            acc1[lane] = fmaf(d1, sum1 as f32, acc1[lane]);
        }
    }
    let s0 = ((acc0[4]+acc0[0])+(acc0[6]+acc0[2])) + ((acc0[5]+acc0[1])+(acc0[7]+acc0[3]));
    let s1 = ((acc1[4]+acc1[0])+(acc1[6]+acc1[2])) + ((acc1[5]+acc1[1])+(acc1[7]+acc1[3]));
    return DotPair { first: s0 + ((min0[0]+min0[2])+(min0[1]+min0[3])), second: s1 + ((min1[0]+min1[2])+(min1[1]+min1[3])) };
}
