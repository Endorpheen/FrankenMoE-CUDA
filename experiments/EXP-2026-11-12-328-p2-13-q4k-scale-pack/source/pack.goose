// Q4_K scale packing in Goose, from ggml-quants.c quantize_row_q4_K_impl (the j<4 / else block).
//
// Eight 6-bit scale bytes (Ls) and eight 6-bit min bytes (Lm) are packed into the twelve scale bytes of
// block_q4_K. Bytes 0..3 hold the low scale nibbles, 4..7 the low min nibbles, 8..11 the high bits of both,
// split as (ls & 0xF) | ((lm & 0xF) << 4) for j>=4 and the two high-bit pairs shifted in at the end.
extern "fg328q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg328q_st_u8" fn st_u8(a: u64, v: i64);
extern "fg328q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// pack: src points at 8 Ls bytes then 8 Lm bytes; dst at 12 scale bytes (zeroed by the caller)

fn fg328q_pack(src: u64, dst: u64) -> i64 {
    var j: i64 = 0 as i64;
    // first pass: the four low pairs and the four combined bytes

    while j < (8 as i64) {
        let ls: i64 = ld_u8(src + (j as u64));
        let lm: i64 = ld_u8(src + (8 as i64) + (j as u64));
        ensure(ls < (64 as i64), 3000 as i64);
        ensure(lm < (64 as i64), 3001 as i64);

        if j < (4 as i64) {
            st_u8(dst + (j as u64), ls);
            st_u8(dst + ( (j + (4 as i64)) as u64), lm);
        } else {
            st_u8(dst + ( (j + (4 as i64)) as u64), (ls & (15 as i64)) | ( (lm & (15 as i64)) << (4 as i64)));
        }
        j = j + (1 as i64);
    }
    // second pass: the high bits, in the same order the reference applies its ORs
    j = 4 as i64;

    while j < (8 as i64) {
        let ls: i64 = ld_u8(src + (j as u64));
        let lm: i64 = ld_u8(src + (8 as i64) + (j as u64));
        let o1: i64 = ld_u8(dst + ( (j - (4 as i64)) as u64)) | ( (ls >> (4 as i64)) << (6 as i64));
        st_u8(dst + ( (j - (4 as i64)) as u64), o1);
        let o2: i64 = ld_u8(dst + (j as u64)) | ( (lm >> (4 as i64)) << (6 as i64));
        st_u8(dst + (j as u64), o2);
        j = j + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg328q_pack(0 as u64, 0 as u64);
}

