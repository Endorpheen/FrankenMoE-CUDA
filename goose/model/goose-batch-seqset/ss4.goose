// Sequence-set construction in Goose, 256 bits (APX: P6-07).
//
// Same decision as ss.goose, but LLAMA_MAX_SEQ is 256 (src/llama-cparams.h:8), so a set is FOUR 64-bit
// words and not one. The first version was written for a single word and its call site correctly refused to
// run; this is the version that can.
//
// Native (llama-batch.cpp:271-285):
//     for i in tokens: cur = {}; for s in seq_id[i]: cur.set(seq_id[i][s]); unq.set(seq_id[i][s])
//                      seq_set.push_back(cur); seq_set_map[cur].push_back(i)
//
// `seq_set_map` is KEYED by the set, so two tokens with the same ids must produce identical bits in all four
// words. A packing error in word 2 or 3 does not fail any token-level check; it silently changes how tokens
// group when the batch splits.
//
// Word index is sid/64 and bit is sid%64. Both are computed the same way in both places -- an asymmetry
// between how the per-token mask and the union are packed would be invisible at the top of the range and
// wrong at the bottom.
extern "fg240q_fail" fn fail(code: i64);
extern "fg240q_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg240q_st_i64" fn st_i64(base: u64, idx: i64, v: i64);
extern "fg240q_ld_i64" fn ld_i64(base: u64, idx: i64) -> i64;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// build_sets4 fills out[i*4 + w] with the four words for token i and writes the four union words to unq.
// Returns 1 on success.

fn build_sets4(n_tok: i64, flat: u64, off: u64, out: u64, unq: u64, n_max: i64) -> i64 {
    ensure(n_tok >= 0 as i64, 2100);
    ensure(n_max > 0 as i64 && n_max <= 256 as i64, 2101);
    var u0: i64 = 0;
    var u1: i64 = 0;
    var u2: i64 = 0;
    var u3: i64 = 0;
    var i: i64 = 0;

    while i < n_tok {
        let b: i64 = ld_i32(off, i);
        let e: i64 = ld_i32(off, i + 1 as i64);
        ensure(e >= b, 2102);
        var w0: i64 = 0;
        var w1: i64 = 0;
        var w2: i64 = 0;
        var w3: i64 = 0;
        var k: i64 = b;

        while k < e {
            let sid: i64 = ld_i32(flat, k);
            ensure(sid >= 0 as i64 && sid < n_max, 2103);
            let w: i64 = sid / (64 as i64);
            let bit: i64 = (1 as i64) << (sid & 63 as i64);

            if w == 0 as i64 {
                w0 = w0 | bit;
            }

            if w == 1 as i64 {
                w1 = w1 | bit;
            }

            if w == 2 as i64 {
                w2 = w2 | bit;
            }

            if w == 3 as i64 {
                w3 = w3 | bit;
            }
            k = k + 1 as i64;
        }
        st_i64(out, i * 4 as i64, w0);
        st_i64(out, i * 4 as i64 + 1 as i64, w1);
        st_i64(out, i * 4 as i64 + 2 as i64, w2);
        st_i64(out, i * 4 as i64 + 3 as i64, w3);
        u0 = u0 | w0;
        u1 = u1 | w1;
        u2 = u2 | w2;
        u3 = u3 | w3;
        i = i + 1 as i64;
    }
    st_i64(unq, 0 as i64, u0);
    st_i64(unq, 1 as i64, u1);
    st_i64(unq, 2 as i64, u2);
    st_i64(unq, 3 as i64, u3);
    1 as i64
}

fn main() {
    build_sets4(0 as i64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 256 as i64);
    build_sets4(2 as i64, 8 as u64, 16 as u64, 24 as u64, 40 as u64, 256 as i64);
    build_sets4(3 as i64, 48 as u64, 56 as u64, 64 as u64, 80 as u64, 256 as i64);
}

