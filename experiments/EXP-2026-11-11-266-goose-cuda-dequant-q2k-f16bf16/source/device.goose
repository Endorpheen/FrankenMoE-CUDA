// Apex CUDA Q2_K dequantization to half and bfloat16 (Goose), from
// ggml-cuda/dequantize.cuh: dequantize_q2_K<dst_t> for half and nv_bfloat16.
// EXP250 covered the F32 destination.
//
// Arithmetic is EXP250's: 64-thread (n = tid/32, l = tid-32*n,
// is = 8*n + l/16), base i*QK_K + 128*n + l, dm read at the END of the block
// (offsets 80/82), four 2-bit fields from one qs byte, a 4-bit scale/min per
// scales byte, and dall*sc*q - dmin*min. Only the store differs.
//
// Layout of block_q2_K (84 bytes): uint8_t scales[16] at 0, uint8_t qs[64] at
// 16, ggml_half2 dm at 80 (d at 80, dmin at 82). QK_K = 256.
extern "fg266d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg266d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg266d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg266d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg266d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn q2_K_f16(vx: u64, yy: u64, i: i64, tid: i64) {
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
    st_half(
        yy,
        ( (ybase) * (2 as i64)) as u64,
        dall * ( (s0 & 15) as f32) * ( ( (q >> 0 as u32) & 3) as f32) - dmin * ( (s0 >> 4) as f32));
    st_half(
        yy,
        ( (ybase + 32 as i64) * (2 as i64)) as u64,
        dall * ( (s2 & 15) as f32) * ( ( (q >> 2 as u32) & 3) as f32) - dmin * ( (s2 >> 4) as f32));
    st_half(
        yy,
        ( (ybase + 64 as i64) * (2 as i64)) as u64,
        dall * ( (s4 & 15) as f32) * ( ( (q >> 4 as u32) & 3) as f32) - dmin * ( (s4 >> 4) as f32));
    st_half(
        yy,
        ( (ybase + 96 as i64) * (2 as i64)) as u64,
        dall * ( (s6 & 15) as f32) * ( ( (q >> 6 as u32) & 3) as f32) - dmin * ( (s6 >> 4) as f32));
}

fn q2_K_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
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
    st_bf16(
        yy,
        ( (ybase) * (2 as i64)) as u64,
        dall * ( (s0 & 15) as f32) * ( ( (q >> 0 as u32) & 3) as f32) - dmin * ( (s0 >> 4) as f32));
    st_bf16(
        yy,
        ( (ybase + 32 as i64) * (2 as i64)) as u64,
        dall * ( (s2 & 15) as f32) * ( ( (q >> 2 as u32) & 3) as f32) - dmin * ( (s2 >> 4) as f32));
    st_bf16(
        yy,
        ( (ybase + 64 as i64) * (2 as i64)) as u64,
        dall * ( (s4 & 15) as f32) * ( ( (q >> 4 as u32) & 3) as f32) - dmin * ( (s4 >> 4) as f32));
    st_bf16(
        yy,
        ( (ybase + 96 as i64) * (2 as i64)) as u64,
        dall * ( (s6 & 15) as f32) * ( ( (q >> 6 as u32) & 3) as f32) - dmin * ( (s6 >> 4) as f32));
}

fn main() {
    q2_K_f16(0, 0, 0 as i64, 0 as i64);
    q2_K_bf16(0, 0, 0 as i64, 0 as i64);
}

