// Apex CUDA IQ4_XS dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq4_xs. Second IQ-family path after EXP252 (IQ4_NL).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ4_XS). The dst_t template also covers half
// and bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the destination base i*QK_K + 32*ib + 4*il, the per-32 scale
// assembled from scales_l and scales_h, d = x[i].d * (scale - 32), the nibble
// split and d*kvalues_iq4nl[q]. The table is reproduced as an explicit Goose
// constant function, as in EXP252.
//
// Layout of block_iq4_xs (136 bytes): ggml_half d at 0, uint16_t scales_h at 2,
// uint8_t scales_l[4] at 4, uint8_t qs[128] at 8. QK_K = 256.
extern "fg253d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg253d_ld_u16" fn ld_u16(vx: u64, o: u64) -> u32;
extern "fg253d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg253d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg253d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// kvalues_iq4nl = {-127,-104,-83,-65,-49,-35,-22,-10,1,13,25,38,53,69,89,113}

fn kv(i: i64) -> i64 {
    if i == 0 as i64 {
        return -127 as i64;
    }

    if i == 1 as i64 {
        return -104 as i64;
    }

    if i == 2 as i64 {
        return -83 as i64;
    }

    if i == 3 as i64 {
        return -65 as i64;
    }

    if i == 4 as i64 {
        return -49 as i64;
    }

    if i == 5 as i64 {
        return -35 as i64;
    }

    if i == 6 as i64 {
        return -22 as i64;
    }

    if i == 7 as i64 {
        return -10 as i64;
    }

    if i == 8 as i64 {
        return 1 as i64;
    }

    if i == 9 as i64 {
        return 13 as i64;
    }

    if i == 10 as i64 {
        return 25 as i64;
    }

    if i == 11 as i64 {
        return 38 as i64;
    }

    if i == 12 as i64 {
        return 53 as i64;
    }

    if i == 13 as i64 {
        return 69 as i64;
    }

    if i == 14 as i64 {
        return 89 as i64;
    }
    113 as i64
}
// One 256-element CTA, 32 threads, F32 destination.
//   scale = ((scales_l[ib/2] >> 4*(ib%2)) & 0xF) | (((scales_h >> 2*ib) & 3) << 4)
//   d     = x[i].d * (scale - 32)

fn q4_xs(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let blk: i64 = i * (136 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let dall: f32 = ld_half(vx, (blk) as u64);
    let scl: i64 = blk + 4 as i64 + ib / 2;
    let sl: u32 = ld_u8(vx, (scl) as u64);
    let sh: u32 = ld_u16(vx, (blk + 2 as i64) as u64);
    let slb: u32 = (4 * (ib % 2)) as u32;
    let shb: u32 = (2 * ib) as u32;
    let lo: i64 = ( ( (sl >> slb) & 15) as i64);
    let hi: i64 = ( ( ( (sh >> shb) & 3) as i64) << 4);
    let sc: i64 = lo | hi;
    let d: f32 = dall * ( (sc - 32 as i64) as f32);
    let qb: i64 = blk + 8 as i64 + 16 * ib + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_f32(yy, ( ( (ybase + j) * (4 as i64)) as u64), d * ( (v0) as f32));
        st_f32(yy, ( ( (ybase + j + 16 as i64) * (4 as i64)) as u64), d * ( (v1) as f32));
        j = j + 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    q4_xs(0, 0, 0 as i64, 0 as i64);
}

