// Apex CUDA Q2_K dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_q2_K. Fourth dequantisation path after EXP182 (Q4_K/F32),
// EXP188 (Q4_0/F16), EXP248 (Q6_K/F32) and EXP249 (Q5_K/F32).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_Q2_K). The dst_t template also covers half and
// bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 64-thread decomposition (n = tid/32,
// l = tid-32*n, is = 8*n + l/16), the destination base i*QK_K + 128*n + l, the
// packed dm scale/min split, the 2-bit field extracted from one qs byte, the
// 4-bit scale and 4-bit min in each scales byte, and the affine
// dall*sc*q - dmin*min reconstruction. Extern symbols are only byte loads, two
// half unpacks and a float store.
//
// Layout of block_q2_K (84 bytes): uint8_t scales[16] at 0, uint8_t qs[64] at 16,
// ggml_half2 dm at 80 (d at 80, dmin at 82). QK_K = 256.
extern "fg250d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg250d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg250d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg250d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One 256-value super-block, 64 threads, F32 destination.
//   n = tid/32 (0 or 1) ; l = tid - 32*n ; is = 8*n + l/16
//   y = yy + i*QK_K + 128*n + l
//   one qs byte supplies all four 2-bit fields for this thread

fn q2_K(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 64 as i64, 10);
    let n: i64 = tid / 32;
    let l: i64 = tid - 32 * n;
    let is: i64 = 8 * n + l / 16;
    let blk: i64 = i * (84 as i64);
    let ybase: i64 = i * (256 as i64) + 128 * n + l;
    let dall: f32 = ld_half(vx, ( (blk + 80 as i64) as u64));
    let dmin: f32 = ld_half(vx, ( (blk + 82 as i64) as u64));
    let q: u32 = ld_u8(vx, ( (blk + 16 as i64 + 32 * n + l) as u64));
    let s0: i64 = (ld_u8(vx, ( (blk + is) as u64))) as i64;
    let s2: i64 = (ld_u8(vx, ( (blk + is + 2 as i64) as u64))) as i64;
    let s4: i64 = (ld_u8(vx, ( (blk + is + 4 as i64) as u64))) as i64;
    let s6: i64 = (ld_u8(vx, ( (blk + is + 6 as i64) as u64))) as i64;
    st_f32(
        yy,
        ( (ybase) * (4 as i64)) as u64,
        dall * ( (s0 & 15) as f32) * ( ( (q >> 0) & 3) as f32) - dmin * ( (s0 >> 4) as f32));
    st_f32(
        yy,
        ( (ybase + 32 as i64) * (4 as i64)) as u64,
        dall * ( (s2 & 15) as f32) * ( ( (q >> 2) & 3) as f32) - dmin * ( (s2 >> 4) as f32));
    st_f32(
        yy,
        ( (ybase + 64 as i64) * (4 as i64)) as u64,
        dall * ( (s4 & 15) as f32) * ( ( (q >> 4) & 3) as f32) - dmin * ( (s4 >> 4) as f32));
    st_f32(
        yy,
        ( (ybase + 96 as i64) * (4 as i64)) as u64,
        dall * ( (s6 & 15) as f32) * ( ( (q >> 6) & 3) as f32) - dmin * ( (s6 >> 4) as f32));
}
// Retained entry; the parent never executes main.

fn main() {
    q2_K(0, 0, 0 as i64, 0 as i64);
}

