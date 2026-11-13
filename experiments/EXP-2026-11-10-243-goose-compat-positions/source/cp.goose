// llama_batch_compat::init position decision in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:1441-1458):
//     if (batch_inp.pos) {
//         if (has_token) t.pos[0] = pos[i];                       // one position per token
//         else for j: t.pos[j] = pos[j*n_tokens + i];             // embedding: section-major
//     } else {
//         t.pos[0] = pos_next[sids[0]]++;                          // auto-generate from the FIRST seq id
//     }
//
// Three things here are easy to get wrong and none of them changes a token VALUE:
//
//  1. The embedding layout is section-major, pos[j*n_tokens + i], not row-major pos[i*n_pos + j]. Row-major
//     reads plausible numbers for every entry and silently transposes the positions of a multi-section
//     (M-RoPE) batch. The asymmetry with the token branch -- which uses a single pos[i] -- is what makes it
//     tempting to "unify" them.
//  2. Auto-generation uses sids[0], the FIRST sequence id of the entry, and increments THAT counter. Using
//     sids[n_sid-1] picks a different sequence when the set has several members.
//  3. The counter is per sequence and shared across the whole batch, so entries of the same sequence get
//     consecutive positions in iteration order. Starting each entry from the same value produces duplicate
//     positions and a batch that looks correct until attention reads it.
extern "fg243c_fail" fn fail(code: i64);
extern "fg243c_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg243c_ld_i64" fn ld_i64(base: u64, idx: i64) -> i64;
extern "fg243c_st_i64" fn st_i64(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// positions fills out[i*n_pos + j]. sid_flat/sid_off describe each entry's seq_id list; pos_next is the
// per-sequence "next position" array and is advanced in place when pos is absent.
// Returns 1; out is n_tokens*n_pos_per_embd long.

fn positions(
    n_tokens: i64,
    has_token: i64,
    has_pos: i64,
    n_pos_per_embd: i64,
    pos: u64,
    sid_flat: u64,
    sid_off: u64,
    pos_next: u64,
    out: u64,
    n_seq_max: i64) -> i64 {
    ensure(n_tokens >= 0 as i64, 2400);
    ensure(n_pos_per_embd > 0 as i64, 2401);
    ensure(has_token == 0 as i64 || has_token == 1 as i64, 2402);
    ensure(has_pos == 0 as i64 || has_pos == 1 as i64, 2403);
    var i: i64 = 0;

    while i < n_tokens {
        if has_pos != 0 as i64 {
            if has_token != 0 as i64 {
                // token batch: exactly one position, taken straight from pos[i]
                st_i64(out, i * n_pos_per_embd, ld_i64(pos, i));
                var j: i64 = 1 as i64;

                while j < n_pos_per_embd {
                    st_i64(out, i * n_pos_per_embd + j, 0 as i64);
                    j = j + 1 as i64;
                }
            } else {
                // embedding batch: section-major, pos[j*n_tokens + i]
                var j: i64 = 0;

                while j < n_pos_per_embd {
                    st_i64(out, i * n_pos_per_embd + j, ld_i64(pos, j * n_tokens + i));
                    j = j + 1 as i64;
                }
            }
        } else {
            // auto-generate from the FIRST seq id of this entry and advance that sequence's counter
            let b: i64 = ld_i32(sid_off, i);
            let e: i64 = ld_i32(sid_off, i + 1 as i64);
            ensure(e > b, 2404);
            let sid0: i64 = ld_i32(sid_flat, b);
            ensure(sid0 >= 0 as i64 && sid0 < n_seq_max, 2405);
            let v: i64 = ld_i64(pos_next, sid0);
            st_i64(out, i * n_pos_per_embd, v);
            st_i64(pos_next, sid0, v + 1 as i64);
            var j: i64 = 1 as i64;

            while j < n_pos_per_embd {
                st_i64(out, i * n_pos_per_embd + j, 0 as i64);
                j = j + 1 as i64;
            }
        }
        i = i + 1 as i64;
    }
    1 as i64
}

fn main() {
    positions(0 as i64, 1 as i64, 0 as i64, 1 as i64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 64 as i64);
    positions(2 as i64, 0 as i64, 1 as i64, 2 as i64, 8 as u64, 24 as u64, 32 as u64, 40 as u64, 48 as u64, 64 as i64);
    positions(
        3 as i64,
        1 as i64,
        0 as i64,
        1 as i64,
        0 as u64,
        72 as u64,
        96 as u64,
        104 as u64,
        112 as u64,
        64 as i64);
}

