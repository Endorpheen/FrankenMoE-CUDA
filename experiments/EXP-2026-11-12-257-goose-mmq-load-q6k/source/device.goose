// CUDA_MMQ: the Q6_K MMA-layout tile loader, qs part, in Goose.
//
// block_q6_K = { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; ggml_half d; } = 210 bytes:
//   ql @0, qh @128, scales @192, d @208.
//
// Native (MMA branch), for one (i, txi):
//   ql  = get_int_b2(bxi->ql, txi)            ; 4 bytes at ql + 4*txi
//   ql0 = (ql >> 0) & 0x0F0F0F0F ; ql1 = (ql >> 4) & 0x0F0F0F0F
//   qh  = get_int_b2(bxi->qh, (QI6_K/4)*(txi/(QI6_K/2)) + txi%(QI6_K/4))
//   qh0 = ((qh >> ((txi & 8) >> 2)) << 4) & 0x30303030
//   qh1 =  (qh >> ((txi & 8) >> 2))       & 0x30303030
//   kq0 = 2*txi - txi%(QI6_K/2) ; kq1 = kq0 + QI6_K/2
//   x_qs[i*sram_stride + kq0] = vsub4(ql0|qh0, 0x20202020)
//   x_qs[i*sram_stride + kq1] = vsub4(ql1|qh1, 0x20202020)
// vsub4 is four independent byte subtractions -- real arithmetic, ported below.

extern "fg257d_fail" fn fail(c:i64);
extern "fg257d_ld_i32" fn ld_i32(base:u64, byte:i64)->i64;
extern "fg257d_st_i32" fn st_i32(base:u64, byte:i64, v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// four independent bytes: (a - b) mod 256 per byte
fn vsub4(a:i64, b:i64) -> i64 {
    var out:i64 = 0 as i64;
    var k:i64 = 0 as i64;
    while k < 4 {
        let ab:i64 = (a >> (k*8 as i64)) & 0xff as i64;
        let bb:i64 = (b >> (k*8 as i64)) & 0xff as i64;
        let d:i64  = (ab - bb) & 0xff as i64;
        out = out | (d << (k*8 as i64));
        k = k + 1 as i64;
    }
    out
}

fn write_qs6(x:u64, stage:u64, kbx0:i64, stride:i64, i:i64, txi:i64, sram_stride:i64, qi6k:i64) -> bool {
    let bxi:u64 = x + ((kbx0 + i*stride) * 210) as u64;
    let ql:i64 = ld_i32(bxi, txi*4 as i64);
    let ql0:i64 = (ql >> 0 as i64) & 0x0F0F0F0F as i64;
    let ql1:i64 = (ql >> 4 as i64) & 0x0F0F0F0F as i64;

    let qidx:i64 = (qi6k/4 as i64)*(txi/(qi6k/2 as i64)) + txi % (qi6k/4 as i64);
    let qh:i64 = ld_i32(bxi + 128 as u64, qidx*4 as i64);
    let sh:i64 = (txi & 8 as i64) >> 2 as i64;
    let qh0:i64 = (((qh >> sh) << 4 as i64) & 0x30303030 as i64);
    let qh1:i64 = ((qh >> sh) & 0x30303030 as i64);

    let kq0:i64 = 2*txi - txi % (qi6k/2 as i64);
    let kq1:i64 = kq0 + qi6k/2 as i64;
    st_i32(stage, (i*sram_stride + kq0) * 4 as i64, vsub4(ql0 | qh0, 0x20202020 as i64));
    st_i32(stage, (i*sram_stride + kq1) * 4 as i64, vsub4(ql1 | qh1, 0x20202020 as i64));
    true
}

fn main(){ write_qs6(0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,32 as i64); }
