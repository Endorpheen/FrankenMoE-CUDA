// Apex CUDA IQ1_S dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq1_s. Last IQ path after EXP255-259.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ1_S).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the 11-bit grid index assembled from a qs byte and three qh
// bits shifted by 3*il, the delta selection from qh bit 15, the per-thread
// scale 2*((qh>>12)&7)+1, the low/high nibble split of the 32-bit grid word,
// the destination base i*QK_K + 32*ib + 8*il and d*(q+delta). The shared table
// iq1s_grid_gpu is DATA, imported through an explicit constant primitive.
//
// Layout of block_iq1_s (66 bytes): ggml_half d at 0, uint8_t qs[32] at 2,
// uint16_t qh[8] at 34. QK_K = 256.
extern "fg260d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg260d_ld_u16" fn ld_u16(vx: u64, o: u64) -> u32;
extern "fg260d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg260d_grid" fn grid(i: u32) -> u32;
extern "fg260d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg260d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One 256-value super-block, 32 threads, F32 destination.

fn iq1_s(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (50 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let qh: u32 = ld_u16(vx, ( (blk + 34 as i64 + 2 * ib) as u64));
    let qs: u32 = ld_u8(vx, ( (blk + 2 as i64 + 4 * ib + il) as u64));
    let s3: u32 = (3 * il) as u32;
    let idx: u32 = qs | ( ( (qh >> s3) & (7 as u32)) << (8 as u32));
    let hb: u32 = qh & (0x8000 as u32);
    var delta: f32 = -0.875 as f32;

    if hb > (0 as u32) {
        delta = -1.125 as f32;
    }
    let sc7: i64 = ( (qh >> (12 as u32)) & (7 as u32)) as i64;
    let d: f32 = ld_half(vx, (blk) as u64) * ( ( (2 * sc7) + (1 as i64)) as f32);
    let g: u32 = grid(idx);
    let g0: u32 = g & (0x0f0f0f0f as u32);
    let g1: u32 = (g >> (4 as u32)) & (0x0f0f0f0f as u32);
    var j: i64 = 0;

    while j < 8 as i64 {
        var w: u32 = 0;

        if j < 4 as i64 {
            let sj: u32 = (j * 8) as u32;
            w = g0 >> sj;
        } else {
            let sj: u32 = ( (j - 4 as i64) * 8) as u32;
            w = g1 >> sj;
        }
        let qb: i64 = (w & (255 as u32)) as i64;
        st_f32(yy, ( ( (ybase + j) * (4 as i64)) as u64), d * ( ( (qb) as f32) + delta));
        j = j + 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    iq1_s(0, 0, 0 as i64, 0 as i64);
}

