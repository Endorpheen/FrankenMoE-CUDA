// Q6_K x Q8_K super-block dot, reproducing the dispatched AVX2 association.
//
// block_q6_K (210 bytes): d (f16) at 0, ql[128] at 2, qh[64] at 130, scales[16] at 194
// block_q8_K (292 bytes): d (f32) at 0, qs[256] at 4, bsums[16] at 260
//
// AVX2 builds four 6-bit groups per 128-block, multiplies each with the signed scale, subtracts the
// q6 "-32 offset" term derived from bsums, accumulates d*sumi per lane with fma and reduces with hsum_float_8.

extern "fg410k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg410k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg410k_ldu8" fn ldu8(base:u64,byte:i64)->i64;
extern "fg410k_ldi8" fn ldi8(base:u64,byte:i64)->i64;
extern "fg410k_ldi16" fn ldi16(base:u64,byte:i64)->i64;
extern "fg410k_ldf" fn ldf(base:u64,byte:i64)->f32;
extern "fg410k_f16f32" fn f16f32(h:i64)->f32;
extern "fg410k_f32bits" fn f32b(x:f32)->i64;
extern "fg410k_fma" fn fma(a:f32,b:f32,c:f32)->f32;
extern "fg410k_fail" fn fail(c:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool {
    let nb:i64 = get(packet,0,0) as i64;
    let x:u64 = get(packet,0,1);
    let y:u64 = get(packet,0,2);
    ensure(nb > 0, 10);

    var acc:f32[8] = default<f32[8]>();
    var q:i64 = 0 as i64;
    while q < 8 { acc[q] = 0.0 as f32; q = q + 1 as i64; }

    var i:i64 = 0 as i64;
    while i < nb {
        let xb:u64 = x + (i*210) as u64;
        let yb:u64 = y + (i*292) as u64;
        let dh:i64 = ldi16(xb, 208 as i64) & 0xffff;
        let d:f32 = ldf(yb, 0 as i64) * f16f32(dh);

        // q8sclsub[k] = (bsums[2k]*scales[2k] + bsums[2k+1]*scales[2k+1]) << 5
        var sub:i64[8] = default<i64[8]>();
        var k:i64 = 0 as i64;
        while k < 8 {
            let bs0:i64 = ldi16(yb, 260 as i64 + k*4 as i64);
            let bs1:i64 = ldi16(yb, 262 as i64 + k*4 as i64);
            let sc0:i64 = ldi8(xb, 192 as i64 + k*2 as i64);
            let sc1:i64 = ldi8(xb, 193 as i64 + k*2 as i64);
            sub[k] = (bs0*sc0 + bs1*sc1) << 5 as i64;
            k = k + 1 as i64;
        }

        var sumi:i64[8] = default<i64[8]>();
        k = 0 as i64;
        while k < 8 { sumi[k] = 0 as i64; k = k + 1 as i64; }

        var j:i64 = 0 as i64;
        while j < 2 {
            let ql1:u64 = xb + (j*64 as i64) as u64;
            let ql2:u64 = ql1 + 32 as u64;
            let qh:u64 = xb + (128 as i64 + j*32 as i64) as u64;
            let q8b:u64 = yb + (4 as i64 + j*128 as i64) as u64;

            var t:i64 = 0 as i64;
            while t < 32 {
                let b1:i64 = ldu8(ql1, t);
                let b2:i64 = ldu8(ql2, t);
                let bh:i64 = ldu8(qh, t);
                let q0:i64 = (b1 & 15 as i64) | ((bh & 3 as i64) << 4 as i64);
                let q1:i64 = (b2 & 15 as i64) | ((bh & 12 as i64) << 2 as i64);
                let q2:i64 = ((b1 >> 4 as i64) & 15 as i64) | (bh & 48 as i64);
                let q3:i64 = ((b2 >> 4 as i64) & 15 as i64) | ((bh & 192 as i64) >> 2 as i64);
                let lane:i64 = t / 4 as i64;
                let half:i64 = if lane >= 4 as i64 { 1 as i64 } else { 0 as i64 };
                let s0:i64 = ldi8(xb, 192 as i64 + (j*4 as i64 + 0 as i64)*2 as i64 + half);
                let s1:i64 = ldi8(xb, 192 as i64 + (j*4 as i64 + 1 as i64)*2 as i64 + half);
                let s2:i64 = ldi8(xb, 192 as i64 + (j*4 as i64 + 2 as i64)*2 as i64 + half);
                let s3:i64 = ldi8(xb, 192 as i64 + (j*4 as i64 + 3 as i64)*2 as i64 + half);
                let x0:i64 = ldi8(q8b, t) as i64;
                let x1:i64 = ldi8(q8b, 32 as i64 + t) as i64;
                let x2:i64 = ldi8(q8b, 64 as i64 + t) as i64;
                let x3:i64 = ldi8(q8b, 96 as i64 + t) as i64;
                sumi[lane] = sumi[lane] + s0*q0*x0 + s1*q1*x1 + s2*q2*x2 + s3*q3*x3;
                t = t + 1 as i64;
            }
            j = j + 1 as i64;
        }

        k = 0 as i64;
        while k < 8 {
            acc[k] = fma(d, (sumi[k] - sub[k]) as f32, acc[k]);
            k = k + 1 as i64;
        }
        i = i + 1 as i64;
    }

    let r0:f32 = acc[4] + acc[0];
    let r1:f32 = acc[5] + acc[1];
    let r2:f32 = acc[6] + acc[2];
    let r3:f32 = acc[7] + acc[3];
    let hsum:f32 = (r0 + r2) + (r1 + r3);
    put(packet,2,0, f32b(hsum) as u64);
    return true;
}

fn main(){run(0);}
