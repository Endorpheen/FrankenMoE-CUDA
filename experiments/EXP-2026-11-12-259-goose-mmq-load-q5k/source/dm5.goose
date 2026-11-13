// CUDA_MMQ: the Q5_K MMA-layout tile loader, dm rows, in Goose.
//
// block_q5_K = { ggml_half2 dm; uint8_t scales[12]; uint8_t qh[32]; uint8_t qs[128]; } = 176 bytes.
//
// Native (MMA branch), second loop, per thread:
//   ksc  = threadIdx.x % 2
//   sc32 = unpack_scales_q45_K(scales, ksc + 0)
//   m32  = unpack_scales_q45_K(scales, ksc + 2)
//   dm   = bxi->dm * make_half2(1.0f, -1.0f)
//   for l in 0..3: x_dm[i*sram_stride + 4*ksc + l] = dm * make_half2(sc8[l], m8[l])
//
// The scale unpack and the half2 multiply stay primitives (data and FP16 arithmetic); the row
// addressing, the ksc selection and the byte extraction are Goose.
extern "fg259m_fail" fn fail(c: i64);
extern "fg259m_ld_i32" fn ld_i32(base: u64, byte: i64) -> i64;
extern "fg259m_st_i32" fn st_i32(base: u64, byte: i64, v: i64);
extern "fg259m_unpack_scales" fn unpack_scales(scales: u64, k: i64) -> i64;
extern "fg259m_dm_scale" fn dm_scale(dm_bits: i64, a: i64, b: i64) -> i64;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn write_dm5(x: u64, dm_stage: u64, kbx0: i64, stride: i64, i: i64, tx: i64, sram_stride: i64) -> bool {
    let bxi: u64 = x + ( (kbx0 + i * stride) * 176) as u64;
    let ksc: i64 = tx % (2 as i64);
    let sc32: i64 = unpack_scales(bxi + 4 as u64, ksc + 0 as i64);
    let m32: i64 = unpack_scales(bxi + 4 as u64, ksc + 2 as i64);
    let dm_bits: i64 = ld_i32(bxi, 0 as i64);
    var l: i64 = 0 as i64;

    while l < 4 {
        let sc8: i64 = (sc32 >> ( (l * 8) as i64)) & 0xff as i64;
        let m8: i64 = (m32 >> ( (l * 8) as i64)) & 0xff as i64;
        st_i32(dm_stage, (i * sram_stride + 4 * ksc as i64 + l) * 4 as i64, dm_scale(dm_bits, sc8, m8));
        l = l + 1 as i64;
    }
    true
}

fn main() {
    write_dm5(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

