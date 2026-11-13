// Apex CUDA IQ4_NL dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq4_nl. First IQ-family path, after the K-quants EXP182/188/248/249/250/251.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ4_NL). The dst_t template also covers half
// and bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the out-of-range guard against nb32, the 8-blocks-per-CTA block
// index, the destination base i*QK_K + 32*ib + 4*il, the nibble split, and the
// d*table[q] reconstruction. The 16-entry kvalues_iq4nl table is data, not code:
// it is reproduced here as an explicit constant function so the module needs no
// native table symbol and cannot silently drift from it.
//
// Layout of block_iq4_nl (18 bytes): ggml_half d at 0, uint8_t qs[16] at 2.
// QK4_NL = 32, QK_K = 256, so one CTA covers 8 blocks = 256 elements.
extern "fg252d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg252d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg252d_st_f32" fn st_f32(yy: u64, o: u64, v: f32);
extern "fg252d_fail" fn fail(code: i64);

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
// One 256-element CTA, 32 threads, F32 destination. Each thread writes 8 values
// of its own iq4_nl block: four from the low nibbles and four from the high ones.
// The wrapper's guard index (8*i + tid%8) is NOT the block index used by the
// destination arithmetic: inside dequantize_iq4_nl, ib = tid%8 and the source
// block is addressed as x = base + i*8 blocks.

fn q4_nl(vx: u64, yy: u64, i: i64, nb32: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ibg: i64 = 8 * i + tid % 8;

    if ibg >= nb32 {
        return;
    }
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let bidx: i64 = i * 8 + ib;
    let blk: i64 = bidx * (18 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let d: f32 = ld_half(vx, (blk) as u64);
    let qb: i64 = blk + 2 as i64 + 4 * il;
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
    q4_nl(0, 0, 0 as i64, 0 as i64, 0 as i64);
}

