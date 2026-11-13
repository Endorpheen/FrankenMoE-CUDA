// Apex CUDA IQ3_S dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq3_s. Second 3-bit grid path after EXP258 (IQ3_XXS).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ3_S).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the two 9-bit grid indices assembled from a qs byte and one qh
// bit shifted by (8-2*il) and (7-2*il), the per-32 scale
// 1 + 2*((scales[ib/2] >> 4*(ib%2)) & 0xf), the sign byte at signs[4*ib+il],
// the destination base i*QK_K + 32*ib + 8*il, the 4-byte grid extraction and
// the conditional signs. The shared table iq3s_grid is DATA, imported through
// an explicit constant primitive; kmask_iq2xs is 1 << j and computed here.
//
// Layout of block_iq3_s (110 bytes): ggml_half d at 0, uint8_t qs[64] at 2,
// uint8_t qh[8] at 66, uint8_t signs[32] at 74, uint8_t scales[4] at 106.
// QK_K = 256.
extern "fg259d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg259d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg259d_grid" fn grid(i: u32) -> u32;
extern "fg259d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg259d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn sgn(b: i64) -> f32 {
    if b > 0 as i64 {
        return -1.0 as f32;
    }
    1.0 as f32
}
// One 256-value super-block, 32 threads, F32 destination.

fn iq3_s(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (110 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let qs: i64 = blk + 2 as i64 + 8 * ib;
    let qhb: u32 = ld_u8(vx, ( (blk + 66 as i64 + ib) as u64));
    let sh1: u32 = (8 - 2 * il) as u32;
    let sh2: u32 = (7 - 2 * il) as u32;
    let h1: u32 = (qhb << sh1) & (256 as u32);
    let h2: u32 = (qhb << sh2) & (256 as u32);
    let i1: u32 = ld_u8(vx, ( (qs + 2 * il) as u64)) | h1;
    let i2: u32 = ld_u8(vx, ( (qs + 2 * il + 1 as i64) as u64)) | h2;
    let scb: u32 = ld_u8(vx, ( (blk + 106 as i64 + ib / 2) as u64));
    let shs: u32 = (4 * (ib % 2)) as u32;
    let scv: i64 = ( (scb >> shs) & (15 as u32)) as i64;
    let d: f32 = ld_half(vx, (blk) as u64) * ( ( (1 as i64) + (2 * scv)) as f32);
    let sgnb: u32 = ld_u8(vx, ( (blk + 74 as i64 + 4 * ib + il) as u64));
    let G1: u32 = grid(i1);
    let G2: u32 = grid(i2);
    var j: i64 = 0;

    while j < 4 as i64 {
        let shk: u64 = (j * 8) as u64;
        let b1: i64 = ( (G1 >> shk) & (255 as u64)) as i64;
        let b2: i64 = ( (G2 >> shk) & (255 as u64)) as i64;
        let m1: i64 = (1 as i64) << j;
        let m2: i64 = (1 as i64) << (j + 4);
        let q1: i64 = (sgnb as i64) & m1;
        let q2: i64 = (sgnb as i64) & m2;
        st_f32(yy, ( ( (ybase + j) * (4 as i64)) as u64), d * ( (b1) as f32) * sgn(q1));
        st_f32(yy, ( ( (ybase + j + 4 as i64) * (4 as i64)) as u64), d * ( (b2) as f32) * sgn(q2));
        j = j + 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    iq3_s(0, 0, 0 as i64, 0 as i64);
}

