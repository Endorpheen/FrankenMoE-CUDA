// Apex CUDA Q4_K dequantization to half and bfloat16 (Goose), from
// ggml-cuda/dequantize.cuh: dequantize_q4_K<dst_t> for dst_t = half and
// nv_bfloat16. Extends EXP182, which covered the F32 destination.
//
// The arithmetic is identical to the F32 path; only the final store differs.
// Native semantics: ggml_cuda_cast<half>(float) falls through to float(x) and
// the assignment performs the round-to-nearest-even float->half conversion;
// ggml_cuda_cast<nv_bfloat16>(float) is __float2bfloat16(float(x)). The Goose
// stores use the same two primitives, so the conversion is the same operation.
//
// Layout of block_q4_K (144 bytes): ggml_half d at 0, ggml_half dmin at 2,
// uint8_t scales[12] at 4, uint8_t qs[128] at 16. QK_K = 256.
extern "fg263d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg263d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg263d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg263d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg263d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn scale_min(vx: u64, base: i64, j: i64) -> i64 {
    var dv: i64 = 0;
    var mv: i64 = 0;

    if j < 4 as i64 {
        dv = (ld_u8(vx, ( (base + j) as u64)) & 63) as i64;
        mv = (ld_u8(vx, ( (base + j + 4 as i64) as u64)) & 63) as i64;
    } else {
        let a: u32 = ld_u8(vx, ( (base + j + 4 as i64) as u64));
        let b: u32 = ld_u8(vx, ( (base + j - 4 as i64) as u64));
        let c: u32 = ld_u8(vx, ( (base + j) as u64));
        dv = ( (a & 15) as i64) | ( ( ( (b >> 6) as i64) << 4) as i64);
        mv = ( ( (a >> 4) as i64)) | ( ( ( (c >> 6) as i64) << 4) as i64);
    }
    (dv << 8) | mv
}
// One 256-value super-block, 32 threads, half destination.

fn q4_K_f16(vx: u64, yy: u64, ib: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ir: i64 = tid % 8;
    let is: i64 = 2 * il;
    let n: i64 = 4;
    let blk: i64 = ib * (144 as i64);
    let ybase: i64 = ib * (256 as i64) + 64 * il + n * ir;
    let d: f32 = ld_half(vx, (blk) as u64);
    let dmin: f32 = ld_half(vx, ( (blk) + (2 as i64)) as u64);
    let q: u64 = vx + ( (blk + (16 as i64) + 32 * il + n * ir) as u64);
    let s1: i64 = scale_min(vx, blk + (4 as i64), is);
    let s2: i64 = scale_min(vx, blk + (4 as i64), is + 1);
    let d1: f32 = d * ( (s1 >> 8) as f32);
    let m1: f32 = dmin * ( (s1 & 255) as f32);
    let d2: f32 = d * ( (s2 >> 8) as f32);
    let m2: f32 = dmin * ( (s2 & 255) as f32);
    var l: i64 = 0;

    while l < n {
        let b: u32 = ld_u8(q, (l as u64));
        let lo: u32 = b & 15;
        let hi: u32 = b >> 4;
        let o0: i64 = (ybase + l) * (2 as i64);
        let o1: i64 = (ybase + l + 32) * (2 as i64);
        st_half(yy, (o0 as u64), d1 * (lo as f32) - m1);
        st_half(yy, (o1 as u64), d2 * (hi as f32) - m2);
        l += 1;
    }
}
// One 256-value super-block, 32 threads, bfloat16 destination.

fn q4_K_bf16(vx: u64, yy: u64, ib: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ir: i64 = tid % 8;
    let is: i64 = 2 * il;
    let n: i64 = 4;
    let blk: i64 = ib * (144 as i64);
    let ybase: i64 = ib * (256 as i64) + 64 * il + n * ir;
    let d: f32 = ld_half(vx, (blk) as u64);
    let dmin: f32 = ld_half(vx, ( (blk) + (2 as i64)) as u64);
    let q: u64 = vx + ( (blk + (16 as i64) + 32 * il + n * ir) as u64);
    let s1: i64 = scale_min(vx, blk + (4 as i64), is);
    let s2: i64 = scale_min(vx, blk + (4 as i64), is + 1);
    let d1: f32 = d * ( (s1 >> 8) as f32);
    let m1: f32 = dmin * ( (s1 & 255) as f32);
    let d2: f32 = d * ( (s2 >> 8) as f32);
    let m2: f32 = dmin * ( (s2 & 255) as f32);
    var l: i64 = 0;

    while l < n {
        let b: u32 = ld_u8(q, (l as u64));
        let lo: u32 = b & 15;
        let hi: u32 = b >> 4;
        let o0: i64 = (ybase + l) * (2 as i64);
        let o1: i64 = (ybase + l + 32) * (2 as i64);
        st_bf16(yy, (o0 as u64), d1 * (lo as f32) - m1);
        st_bf16(yy, (o1 as u64), d2 * (hi as f32) - m2);
        l += 1;
    }
}

fn main() {
    q4_K_f16(0, 0, 0 as i64, 0 as i64);
    q4_K_bf16(0, 0, 0 as i64, 0 as i64);
}

