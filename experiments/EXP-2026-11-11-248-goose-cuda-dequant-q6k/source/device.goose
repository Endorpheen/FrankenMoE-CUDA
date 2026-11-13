// Apex CUDA Q6_K dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_q6_K. This is the second dequantisation path after EXP182 (Q4_K/F32)
// and EXP188 (Q4_0/F16); it is a prerequisite for the MMQ family, not a change to
// the shipped profile.
//
// Scope: the F32 destination case, reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_Q6_K). The dst_t template also covers half and
// bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 64-thread decomposition (ip = tid/32,
// il = tid-32*ip, is = 8*ip + il/16), the destination base 128*ip + il, the
// super-block scale, the 4-bit low nibble combined with the 2-bit high part from
// qh, the signed per-16 scale, and the affine d*sc*(q-32) reconstruction.
// Extern symbols are only raw byte loads, the signed-byte load, the half unpack
// and a float store. No original kernel or C++ routine is called.
//
// Layout of block_q6_K (210 bytes): uint8_t ql[128] at 0, uint8_t qh[64] at 128,
// int8_t scales[16] at 192, ggml_half d at 208. QK_K = 256.
//
// Raw ABI: `vx` is a global byte address of the block array, `yy` a global byte
// address of the float destination, both already divided down to bytes.
extern "fg248d_ld_u8" fn ld_u8(p: u64) -> u32;
extern "fg248d_ld_i8" fn ld_i8(p: u64) -> i32;
extern "fg248d_ld_half" fn ld_half(p: u64) -> f32;
extern "fg248d_st_f32" fn st_f32(p: u64, v: f32);
extern "fg248d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One 256-value super-block, 64 threads, F32 destination.
//   ip = tid/32         (0 or 1)
//   il = tid - 32*ip    (0..31)
//   is = 8*ip + il/16
//   y  = yy + i*QK_K + 128*ip + il
//   d  = x[i].d
//   ql = x[i].ql + 64*ip + il
//   qh = x[i].qh[32*ip + il]
//   sc = x[i].scales + is

fn q6_K(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 64 as i64, 10);
    let ip: i64 = tid / 32;
    let il: i64 = tid - 32 * ip;
    let is: i64 = 8 * ip + il / 16;
    let blk: i64 = i * (210 as i64);
    let ybase: i64 = i * (256 as i64) + 128 * ip + il;
    let d: f32 = ld_half(vx + ( (blk + 208 as i64) as u64));
    let ql: i64 = blk + 64 * ip + il;
    let qhp: i64 = blk + 128 as i64 + 32 * ip + il;
    let scl: i64 = blk + 192 as i64 + is;
    let qh: u32 = ld_u8(vx + ( (qhp) as u64));
    let s0: i64 = (ld_i8(vx + ( (scl) as u64))) as i64;
    let s2: i64 = (ld_i8(vx + ( (scl + 2 as i64) as u64))) as i64;
    let s4: i64 = (ld_i8(vx + ( (scl + 4 as i64) as u64))) as i64;
    let s6: i64 = (ld_i8(vx + ( (scl + 6 as i64) as u64))) as i64;
    let v0: i64 = ( ( (ld_u8(vx + ( (ql) as u64)) & 15) | ( ( (qh >> 0) & 3) << 4)) as i64) - 32;
    let v32: i64 = ( ( (ld_u8(vx + ( (ql + 32 as i64) as u64)) & 15) | ( ( (qh >> 2) & 3) << 4)) as i64) - 32;
    let v64: i64 = ( ( (ld_u8(vx + ( (ql) as u64)) >> 4) | ( ( (qh >> 4) & 3) << 4)) as i64) - 32;
    let v96: i64 = ( ( (ld_u8(vx + ( (ql + 32 as i64) as u64)) >> 4) | ( ( (qh >> 6) & 3) << 4)) as i64) - 32;
    st_f32(yy + ( ( (ybase) * (4 as i64)) as u64), d * ( (s0) as f32) * ( (v0) as f32));
    st_f32(yy + ( ( (ybase + 32 as i64) * (4 as i64)) as u64), d * ( (s2) as f32) * ( (v32) as f32));
    st_f32(yy + ( ( (ybase + 64 as i64) * (4 as i64)) as u64), d * ( (s4) as f32) * ( (v64) as f32));
    st_f32(yy + ( ( (ybase + 96 as i64) * (4 as i64)) as u64), d * ( (s6) as f32) * ( (v96) as f32));
}
// Retained entry; the parent never executes main.

fn main() {
    q6_K(0, 0, 0 as i64, 0 as i64);
}

