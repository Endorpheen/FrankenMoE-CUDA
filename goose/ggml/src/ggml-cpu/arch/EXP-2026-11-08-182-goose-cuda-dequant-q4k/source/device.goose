// Apex CUDA Q4_K dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_q4_K and its helper get_scale_min_k4.
//
// Scope: the F32 destination case, which is what mul_mat_q and the quantized
// get_rows paths consume on this profile. The dst_t template also covers half and
// bfloat16 in native; those stay native here and the parent refuses them.
//
// Everything algorithmic is Goose: the 8x4 thread decomposition (il = tid/8,
// ir = tid%8, is = 2*il, n = 4), the row base 64*il + n*ir, the super-block scale
// and min unpack from the packed half2, the two 6-bit scale/min pairs from the
// interleaved `scales` array, the nibble split of each qs byte into the low half
// (element l) and the high half (element l+32), and the affine reconstruction
// d*q - m. Extern symbols are only the raw byte loads, the half2 unpack and the two
// half-to-float conversions. No original kernel, GGML struct or C++ routine is
// called.
//
// Layout of block_q4_K (144 bytes): ggml_half d at 0, ggml_half dmin at 2,
// uint8_t scales[12] at 4, uint8_t qs[128] at 16. QK_K = 256, K_SCALE_SIZE = 12.
//
// Raw ABI: `vx` is a global byte address of the block array, `yy` a global byte
// address of the float destination, both already divided down to bytes.
extern "fg182d_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;
extern "fg182d_ld_half" fn ld_half(src: u64, byte: u64) -> f32;
extern "fg182d_st_f32" fn st_f32(dst: u64, byte: u64, v: f32);
extern "fg182d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// get_scale_min_k4: the 12-byte `scales` array packs eight 6-bit scales and eight
// 6-bit mins. For j < 4 they are the low 6 bits of q[j] and q[j+4]; above that the
// high 2 bits come from the top bits of the lower half.

fn scale_min(q: u64, j: i64) -> i64 {
    // returns (d << 8) | m, so the caller unpacks without a tuple type
    var dv: i64 = 0;
    var mv: i64 = 0;

    if j < 4 {
        dv = (ld_u8(q, (j as u64)) & 63) as i64;
        mv = (ld_u8(q, ( (j + 4) as u64)) & 63) as i64;
    } else {
        let a: u32 = ld_u8(q, ( (j + 4) as u64));
        let b: u32 = ld_u8(q, ( (j - 4) as u64));
        let c: u32 = ld_u8(q, (j as u64));
        dv = ( (a & 15) as i64) | ( ( ( (b >> 6) as i64) << 4) as i64);
        mv = ( ( (a >> 4) as i64)) | ( ( ( (c >> 6) as i64) << 4) as i64);
    }
    return (dv << 8) | mv;
}
// One 256-value super-block, 32 threads, F32 destination.
//   il = tid/8 ; ir = tid%8 ; is = 2*il ; n = 4
//   y  = yy + 64*il + 4*ir
//   dall/dmin from the packed half2 at block+0
//   q  = qs + 32*il + 4*ir
//   d1,m1 from scale index is ; d2,m2 from is+1
//   y[l]    = d1*(q[l]&0xF) - m1
//   y[l+32] = d2*(q[l]>>4)  - m2

fn fg182d_q4_K(vx: u64, yy: u64, ib: i64, tid: i32) {
    ensure(tid >= 0 && tid < 32, 10);
    let il: i64 = (tid as i64) / 8;
    let ir: i64 = (tid as i64) % 8;
    let is: i64 = 2 * il;
    let n: i64 = 4;
    let blk: i64 = ib * (144 as i64);
    // Native calls dequantize_q4_K(vx, i, yy + i*QK_K, tid), so the destination
    // base carries the block offset in FLOATS. QK_K is 256.
    let ybase: i64 = ib * (256 as i64) + 64 * il + n * ir;
    let d: f32 = ld_half(vx, (blk) as u64);
    let dmin: f32 = ld_half(vx, ( (blk) + (2 as i64)) as u64);
    let q: u64 = vx + ( (blk + (16 as i64) + 32 * il + n * ir) as u64);
    let s1: i64 = scale_min(vx + ( (blk + (4 as i64)) as u64), is);
    let s2: i64 = scale_min(vx + ( (blk + (4 as i64)) as u64), is + 1);
    let d1: f32 = d * ( (s1 >> 8) as f32);
    let m1: f32 = dmin * ( (s1 & 255) as f32);
    let d2: f32 = d * ( (s2 >> 8) as f32);
    let m2: f32 = dmin * ( (s2 & 255) as f32);
    var l: i64 = 0;

    while l < n {
        let b: u32 = ld_u8(q, (l as u64));
        let lo: u32 = b & 15;
        let hi: u32 = b >> 4;
        let o0: i64 = (ybase + l) * (4 as i64);
        let o1: i64 = (ybase + l + 32) * (4 as i64);
        st_f32(yy, (o0 as u64), d1 * (lo as f32) - m1);
        st_f32(yy, (o1 as u64), d2 * (hi as f32) - m2);
        l += 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    let z: u64 = 0;
    fg182d_q4_K(z, z, 0 as i64, 0 as i32);
}

