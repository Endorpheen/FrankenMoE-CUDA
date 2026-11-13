// Apex CUDA Q6_K dequantization to half and bfloat16 (Goose), from
// ggml-cuda/dequantize.cuh: dequantize_q6_K<dst_t> for half and nv_bfloat16.
// EXP248 covered the F32 destination; EXP263 established the conversion
// semantics for these two.
//
// Arithmetic is the F32 module's: 64-thread decomposition (ip/il/is),
// destination base i*QK_K + 128*ip + il, the low nibble combined with the 2-bit
// high part from qh, the signed per-16 scale and d*sc*(q-32). Only the store
// differs: __float2half (the same round-to-nearest-even conversion the implicit
// float->half assignment performs) and __float2bfloat16.
//
// Layout of block_q6_K (210 bytes): uint8_t ql[128] at 0, qh[64] at 128,
// int8_t scales[16] at 192, ggml_half d at 208. QK_K = 256.
extern "fg264d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg264d_ld_i8" fn ld_i8(vx: u64, o: u64) -> i32;
extern "fg264d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg264d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg264d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg264d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn q6_K_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 64 as i64, 10);
    let ip: i64 = tid / 32;
    let il: i64 = tid - 32 * ip;
    let is: i64 = 8 * ip + il / 16;
    let blk: i64 = i * (210 as i64);
    let ybase: i64 = i * (256 as i64) + 128 * ip + il;
    let d: f32 = ld_half(vx, ( (blk + 208 as i64) as u64));
    let ql: i64 = blk + 64 * ip + il;
    let qhp: i64 = blk + 128 as i64 + 32 * ip + il;
    let scl: i64 = blk + 192 as i64 + is;
    let qh: u32 = ld_u8(vx, ( (qhp) as u64));
    let s0: i64 = (ld_i8(vx, ( (scl) as u64))) as i64;
    let s2: i64 = (ld_i8(vx, ( (scl + 2 as i64) as u64))) as i64;
    let s4: i64 = (ld_i8(vx, ( (scl + 4 as i64) as u64))) as i64;
    let s6: i64 = (ld_i8(vx, ( (scl + 6 as i64) as u64))) as i64;
    let v0: i64 = ( ( (ld_u8(vx, ( (ql) as u64)) & 15) | ( ( (qh >> 0 as u32) & 3) << 4)) as i64) - 32;
    let v32: i64 = ( ( (ld_u8(vx, ( (ql + 32 as i64) as u64)) & 15) | ( ( (qh >> 2 as u32) & 3) << 4)) as i64) - 32;
    let v64: i64 = ( ( (ld_u8(vx, ( (ql) as u64)) >> 4) | ( ( (qh >> 4 as u32) & 3) << 4)) as i64) - 32;
    let v96: i64 = ( ( (ld_u8(vx, ( (ql + 32 as i64) as u64)) >> 4) | ( ( (qh >> 6 as u32) & 3) << 4)) as i64) - 32;
    st_half(yy, ( ( (ybase) * (2 as i64)) as u64), d * ( (s0) as f32) * ( (v0) as f32));
    st_half(yy, ( ( (ybase + 32 as i64) * (2 as i64)) as u64), d * ( (s2) as f32) * ( (v32) as f32));
    st_half(yy, ( ( (ybase + 64 as i64) * (2 as i64)) as u64), d * ( (s4) as f32) * ( (v64) as f32));
    st_half(yy, ( ( (ybase + 96 as i64) * (2 as i64)) as u64), d * ( (s6) as f32) * ( (v96) as f32));
}

fn q6_K_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 64 as i64, 10);
    let ip: i64 = tid / 32;
    let il: i64 = tid - 32 * ip;
    let is: i64 = 8 * ip + il / 16;
    let blk: i64 = i * (210 as i64);
    let ybase: i64 = i * (256 as i64) + 128 * ip + il;
    let d: f32 = ld_half(vx, ( (blk + 208 as i64) as u64));
    let ql: i64 = blk + 64 * ip + il;
    let qhp: i64 = blk + 128 as i64 + 32 * ip + il;
    let scl: i64 = blk + 192 as i64 + is;
    let qh: u32 = ld_u8(vx, ( (qhp) as u64));
    let s0: i64 = (ld_i8(vx, ( (scl) as u64))) as i64;
    let s2: i64 = (ld_i8(vx, ( (scl + 2 as i64) as u64))) as i64;
    let s4: i64 = (ld_i8(vx, ( (scl + 4 as i64) as u64))) as i64;
    let s6: i64 = (ld_i8(vx, ( (scl + 6 as i64) as u64))) as i64;
    let v0: i64 = ( ( (ld_u8(vx, ( (ql) as u64)) & 15) | ( ( (qh >> 0 as u32) & 3) << 4)) as i64) - 32;
    let v32: i64 = ( ( (ld_u8(vx, ( (ql + 32 as i64) as u64)) & 15) | ( ( (qh >> 2 as u32) & 3) << 4)) as i64) - 32;
    let v64: i64 = ( ( (ld_u8(vx, ( (ql) as u64)) >> 4) | ( ( (qh >> 4 as u32) & 3) << 4)) as i64) - 32;
    let v96: i64 = ( ( (ld_u8(vx, ( (ql + 32 as i64) as u64)) >> 4) | ( ( (qh >> 6 as u32) & 3) << 4)) as i64) - 32;
    st_bf16(yy, ( ( (ybase) * (2 as i64)) as u64), d * ( (s0) as f32) * ( (v0) as f32));
    st_bf16(yy, ( ( (ybase + 32 as i64) * (2 as i64)) as u64), d * ( (s2) as f32) * ( (v32) as f32));
    st_bf16(yy, ( ( (ybase + 64 as i64) * (2 as i64)) as u64), d * ( (s4) as f32) * ( (v64) as f32));
    st_bf16(yy, ( ( (ybase + 96 as i64) * (2 as i64)) as u64), d * ( (s6) as f32) * ( (v96) as f32));
}

fn main() {
    q6_K_f16(0, 0, 0 as i64, 0 as i64);
    q6_K_bf16(0, 0, 0 as i64, 0 as i64);
}

