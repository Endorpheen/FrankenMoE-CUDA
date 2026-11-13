// Apex CUDA IQ2_S dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq2_s. Third grid-table IQ path after EXP255/256.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ2_S).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the 10-bit grid index assembled from a low 8-bit qs byte and the
// high 2 bits of qh[ib] shifted by (8-2*il), the per-sub-block scale from
// scales[ib], the sign byte at qs[32 + 4*ib + il], the destination base
// i*QK_K + 32*ib + 8*il, grid byte extraction and the conditional sign. The
// shared tables iq2s_grid and ksigns_iq2xs are DATA, imported through explicit
// constant primitives.
//
// Layout of block_iq2_s (82 bytes): ggml_half d at 0, uint8_t qs[64] at 2,
// uint8_t qh[8] at 66, uint8_t scales[8] at 74. QK_K = 256.
extern "fg257d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg257d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg257d_grid" fn grid(i: u32) -> u64;
extern "fg257d_ksign" fn ksign(i: u32) -> u32;
extern "fg257d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg257d_fail" fn fail(code: i64);

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
// One 256-value super-block, 32 threads, F32 destination. Eight values per
// thread at ybase..ybase+7.

fn iq2_s(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (82 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let qs: i64 = blk + 2 as i64;
    let lo: u32 = ld_u8(vx, ( (qs + 4 * ib + il) as u64));
    let qhb: u32 = ld_u8(vx, ( (blk + 66 as i64 + ib) as u64));
    let shi: u32 = (8 - 2 * il) as u32;
    let hb: u32 = (qhb << shi) & (0x300 as u32);
    let gi: u32 = lo | hb;
    let sc: u32 = ld_u8(vx, ( (blk + 74 as i64 + ib) as u64));
    let sh: u32 = (4 * (il / 2)) as u32;
    let d: f32 = ld_half(vx, (blk) as u64) * ( (0.5 as f32) + ( ( (sc >> sh) & (15 as u32)) as f32)) * (0.25 as f32);
    let sgnb: u32 = ld_u8(vx, ( (qs + 32 as i64 + 4 * ib + il) as u64));
    let g: u64 = grid(gi);
    var j: i64 = 0;

    while j < 8 as i64 {
        let shk: u64 = (j * 8) as u64;
        let gb: i64 = ( (g >> shk) & (255 as u64)) as i64;
        let m: i64 = (1 as i64) << j;
        let bit: i64 = (sgnb as i64) & m;
        st_f32(yy, ( ( (ybase + j) * (4 as i64)) as u64), d * ( (gb) as f32) * sgn(bit));
        j = j + 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    iq2_s(0, 0, 0 as i64, 0 as i64);
}

