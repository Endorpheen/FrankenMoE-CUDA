// Apex CUDA Q3_K dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_q3_K. Fifth dequantisation path after EXP182 (Q4_K/F32),
// EXP188 (Q4_0/F16), EXP248 (Q6_K/F32), EXP249 (Q5_K/F32) and EXP250 (Q2_K/F32).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_Q3_K). The dst_t template also covers half and
// bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 64-thread decomposition
// (r = tid/4, t = r/2, is0 = r%2, l0 = 16*is0 + 4*(tid%4), n = t/4, j = t-4*n),
// the high-bit mask m = 1 << (4*n+j), the scale index is = 8*n + 2*j + is0 with
// its four source branches, the 2-bit quant field shifted by 2*j, the subtraction
// of 4 when the high-bit mask is clear, and dl = d*(us-32). Extern symbols are
// only byte loads, one half unpack and a float store.
//
// Layout of block_q3_K (110 bytes): uint8_t hmask[32] at 0, uint8_t qs[64] at 32,
// uint8_t scales[12] at 96, ggml_half d at 108. QK_K = 256.
extern "fg251d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg251d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg251d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg251d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// Reconstruct the signed scale index value from the 12-byte scales array.
// Values stay in 0..63 in every branch, so the native int8_t cast is identity.

fn us_of(vx: u64, base: i64, is: i64) -> i64 {
    var u: i64 = 0;

    if is < 4 as i64 {
        u = ( (ld_u8(vx, ( (base + is) as u64)) & 15) as i64) | ( ( ( (ld_u8(vx, ( (base + is + 8 as i64) as u64)) >> 0) & 3) as i64) << 4);
    } else {
        if is < 8 as i64 {
            u = ( (ld_u8(vx, ( (base + is) as u64)) & 15) as i64) | ( ( ( (ld_u8(vx, ( (base + is + 4 as i64) as u64)) >> 2) & 3) as i64) << 4);
        } else {
            if is < 12 as i64 {
                u = ( (ld_u8(vx, ( (base + is - 8 as i64) as u64)) >> 4) as i64) | ( ( ( (ld_u8(vx, ( (base + is) as u64)) >> 4) & 3) as i64) << 4);
            } else {
                u = ( (ld_u8(vx, ( (base + is - 8 as i64) as u64)) >> 4) as i64) | ( ( ( (ld_u8(vx, ( (base + is - 4 as i64) as u64)) >> 6) & 3) as i64) << 4);
            }
        }
    }
    u
}
// (hm[l] & m) ? 0 : 4

fn nohigh(b: i64) -> i64 {
    if b > 0 as i64 {
        return 0 as i64;
    }
    4 as i64
}
// One 256-value super-block, 64 threads, F32 destination. Each thread writes
// four consecutive values at ybase + l0 + {0..3}.

fn q3_K(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 64 as i64, 10);
    let r: i64 = tid / 4;
    let t: i64 = r / 2;
    let is0: i64 = r % 2;
    let l0: i64 = 16 * is0 + 4 * (tid % 4);
    let n: i64 = t / 4;
    let j: i64 = t - 4 * n;
    let m: i64 = (1 as i64) << (4 * n + j);
    let is: i64 = 8 * n + 2 * j + is0;
    let shift: i64 = 2 * j;
    let blk: i64 = i * (110 as i64);
    let dall: f32 = ld_half(vx, ( (blk + 108 as i64) as u64));
    let us: i64 = us_of(vx, blk + 96 as i64, is);
    let dl: f32 = dall * ( (us - 32 as i64) as f32);
    let ybase: i64 = i * (256 as i64) + 128 * n + 32 * j;
    let qb: i64 = blk + 32 as i64 + 32 * n;
    let hb: i64 = blk;
    let sh: u32 = shift as u32;
    var l: i64 = 0;

    while l < 4 as i64 {
        let idx: i64 = l0 + l;
        let q: u32 = ld_u8(vx, ( (qb + idx) as u64));
        let h: u32 = ld_u8(vx, ( (hb + idx) as u64));
        let bits: u32 = (q >> sh) & 3;
        let v: i64 = (bits as i64) - nohigh( (h as i64) & m);
        st_f32(yy, ( ( (ybase + idx) * (4 as i64)) as u64), dl * ( (v) as f32));
        l = l + 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    q3_K(0, 0, 0 as i64, 0 as i64);
}

