// KV cell state gather in Goose (APX: P5-01, the "copy" clause).
//
// From llama-kv-cells.h:128-155 `cp(uint32_t i, uint32_t n)` and 158-180 `cp(idxs)`.
// This is the save half of the rollback pair: llama-kv-cache.cpp:805 gathers the current cell
// state into a recovery stack before an ubatch is emplaced.
//
// COMPACT, and the host enumerates: the source cells named by the caller are gathered by the host
// into m-entry arrays, Goose copies them into place and enforces the shift invariant. The cost is
// O(m) and never O(cache) -- this profile's cache is 262144 cells, and marshalling it per ubatch is
// the mistake that took EXP204 to 8.34 tok/s and that EXP205 documented.
//
// What Goose owns here is the copy decision and the shift assertion, which is the whole
// behavioural content of cp() apart from the container allocation. The native `ext` record is not
// modelled: for this profile has_cell_ext() is false (n_pos_per_embd==1, ple_n_heads==0), so it is
// never read. That becomes a real gap for an M-RoPE or PLE model, so it is stated, not hidden.
extern "fg206d_fail" fn fail(code: i64);
extern "fg206d_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg206d_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// gather: copy m compact source cells into m compact destination cells.
//   src pos/shift/seq are already compacted by the host, in the same order as dst.
//   shift must be zero on every source cell, as native asserts.

fn gather(dpos: u64, dseq: u64, spos: u64, sseq: u64, sshift: u64, m: i64, w: i64) -> i64 {
    ensure(m >= 0 && w > 0, 600);
    var j: i64 = 0;

    while j < m {
        ensure(ld_i64(sshift, j * 8 as i64) == 0 as i64, 601);
        st_i64(dpos, j * 8 as i64, ld_i64(spos, j * 8 as i64));
        var k: i64 = 0;

        while k < w {
            st_i64(dseq, (j * w + k) * 8 as i64, ld_i64(sseq, (j * w + k) * 8 as i64));
            k += 1;
        }
        j += 1;
    }
    return m;
}

fn main() {
    gather(0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64);
    gather(8 as u64, 16 as u64, 24 as u64, 32 as u64, 40 as u64, 1 as i64, 4 as i64);
}

