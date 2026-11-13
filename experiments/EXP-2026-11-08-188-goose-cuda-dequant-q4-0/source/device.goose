// Apex CUDA Q4_0 dequantization (Goose), from ggml-cuda/convert.cu:
// dequantize_block_q4_0 and dequantize_q4_0.
//
// Scope: the F16 destination, which is the instantiation the Apex census shows
// (dequantize_block_q4_0<__half>). The Apex KV cache is q4_0 on both K and V, so this
// is on the attention path.
//
// Everything algorithmic is Goose: the 8-blocks-per-CTA decomposition
// (il = tid/8, ir = tid%8, ib = 8*i + ir), the out-of-range guard, the destination base
// 256*i + 32*ir + 4*il, the nibble split, and the affine d*q - 8d reconstruction.
// Extern symbols are only the raw byte load, the half unpack, and a half store.
// No original kernel or C++ routine is called.
//
// Layout of block_q4_0 (18 bytes): ggml_half d at 0, uint8 qs[16] at 2.
// QK4_0 = 32, so one CTA covers 8 blocks = 256 elements.
extern "fg188d_ld_u8" fn ld_u8(a: u64) -> u32;
extern "fg188d_ld_half" fn ld_half(a: u64) -> f32;
extern "fg188d_st_half" fn st_half(a: u64, v: f32);
extern "fg188d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One thread writes 8 of the 32 values of its own q4_0 block.
//   ib  = 8*i + ir                 block index
//   y   = yy + 256*i + 32*ir + 4*il  F16 destination
//   q   = x[ib].qs + 4*il
//   d   = x[ib].d , dm = -8*d
//   y[l]    = d*(q[l]&0xF) + dm
//   y[l+16] = d*(q[l]>>4)  + dm

fn fg188d_block(vx: u64, yy: u64, nb32: i64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ir: i64 = tid % 8;
    let ib: i64 = 8 * i + ir;

    if ib < nb32 {
        let xb: i64 = ib * (18 as i64);
        let ybase: i64 = 256 * i + 32 * ir + 4 * il;
        let d: f32 = ld_half(vx + ( (xb) as u64));
        let dm: f32 = d * (-8.0 as f32);
        let qb: i64 = xb + (2 as i64) + 4 * il;
        var l: i64 = 0;

        while l < 4 {
            let b: u32 = ld_u8(vx + ( (qb + l) as u64));
            let lo: u32 = b & 15;
            let hi: u32 = b >> 4;
            let o0: i64 = (ybase + l) * (2 as i64);
            let o1: i64 = (ybase + l + 16) * (2 as i64);
            st_half(yy + ( (o0) as u64), d * ( (lo) as f32) + dm);
            st_half(yy + ( (o1) as u64), d * ( (hi) as f32) + dm);
            l = l + 1;
        }
    }
}
// Retained entry; the parent never executes main.

fn main() {
    fg188d_block(0, 0, 0, 0, 0);
}

