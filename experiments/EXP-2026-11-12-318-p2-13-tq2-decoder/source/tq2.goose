// TQ2_0 decode in Goose, from ggml-quants.c: dequantize_row_tq2_0.
//
// block_tq2_0 is 66 bytes in this fork, verified with offsetof: qs[64] 0, d (f16) 64. QK_K = 256.
// Two 32-byte chunks, four 2-bit passes per chunk, value (q - 1) * d.
extern "fg318q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg318q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg318q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg318q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg318q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (66 as u64);
    let d: f32 = ld_f16(base + (64 as u64));
    let y0: u64 = dst + ui * (1024 as u64);
    var chunk: i64 = 0 as i64;

    while chunk < (2 as i64) {
        let q: u64 = base + (chunk as u64) * (32 as u64);
        var l: i64 = 0 as i64;

        while l < (4 as i64) {
            let sh: i64 = l * (2 as i64);
            let y: u64 = y0 + (chunk as u64) * (512 as u64) + (l as u64) * (128 as u64);
            var m: i64 = 0 as i64;

            while m < (32 as i64) {
                let um: u64 = m as u64;
                let v: i64 = (ld_u8(q + um) >> sh) & (3 as i64);
                st_f32(y + um * (4 as u64), d * ( (v - (1 as i64)) as f32));
                m = m + (1 as i64);
            }
            l = l + (1 as i64);
        }
        chunk = chunk + (1 as i64);
    }
    return 1 as i64;
}

fn fg318q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 1100 as i64);
    ensure(k % (256 as i64) == (0 as i64), 1101 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg318q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg318q_decode(0 as u64, 0 as u64, 0 as i64);
}

