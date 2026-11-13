// CUDA_MMQ: the Q5_K MMA-layout tile loader, qs staging, in Goose.
//
// block_q5_K = { ggml_half2 dm; uint8_t scales[12]; uint8_t qh[32]; uint8_t qs[128]; } = 176 bytes:
//   dm @0, scales @4, qh @16, qs @48.
//
// Native (MMA branch), first loop:
//   ky  = QR5_K*txi
//   ql  = get_int_b4(bxi->qs, txi) -> 4 bytes at qs + 4*txi
//   qh  = get_int_b4(bxi->qh, txi % (QI5_K/4))
//   qh0 = ((qh >> (2*(txi/(QI5_K/4)) + 0)) << 4) & 0x10101010
//   qh1 = ((qh >> (2*(txi/(QI5_K/4)) + 1)) << 4) & 0x10101010
//   kq0 = ky - ky % (QI5_K/2) + txi % (QI5_K/4)
//   kq1 = kq0 + QI5_K/4
//   x_qs[i*sram_stride + kq0] = ql0 | qh0 ; x_qs[... + kq1] = ql1 | qh1
//
// QI5_K = 32, QR5_K = 2 for QK_K = 256; both arrive as parameters so the constants stay visible
// to the caller rather than being restated here.
extern "fg259q_fail" fn fail(c: i64);
extern "fg259q_ld_i32" fn ld_i32(base: u64, byte: i64) -> i64;
extern "fg259q_st_i32" fn st_i32(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn write_qs5(
    x: u64,
    stage: u64,
    kbx0: i64,
    stride: i64,
    i: i64,
    txi: i64,
    sram_stride: i64,
    qr5k: i64,
    qi5k: i64) -> bool {
    let qi: i64 = qi5k / (4 as i64);
    let bxi: u64 = x + ( (kbx0 + i * stride) * 176) as u64;
    let ql: i64 = ld_i32(bxi, 48 as i64 + txi * 4 as i64);
    let ql0: i64 = (ql >> 0 as i64) & 0x0F0F0F0F as i64;
    let ql1: i64 = (ql >> 4 as i64) & 0x0F0F0F0F as i64;
    let qh: i64 = ld_i32(bxi, 16 as i64 + (txi % qi) * 4 as i64);
    let sh: i64 = 2 * (txi / qi);
    let qh0: i64 = ( ( (qh >> sh) << 4 as i64) & 0x10101010 as i64);
    let qh1: i64 = ( ( (qh >> (sh + 1 as i64)) << 4 as i64) & 0x10101010 as i64);
    let ky: i64 = qr5k * txi;
    let kq0: i64 = ky - ky % (qi5k / (2 as i64)) + txi % qi;
    let kq1: i64 = kq0 + qi;
    st_i32(stage, (i * sram_stride + kq0) * 4 as i64, ql0 | qh0);
    st_i32(stage, (i * sram_stride + kq1) * 4 as i64, ql1 | qh1);
    true
}

fn main() {
    write_qs5(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 2 as i64, 32 as i64);
}

