// KV cell state gather in Goose (APX: P5-01, the "copy" clause).
//
// From llama-kv-cells.h:128-155 `llama_kv_cells cp(uint32_t i, uint32_t n)` and its indexed form
// at 158-180. This is the save half of the rollback pair: before an ubatch is emplaced the current
// cell state is gathered into a recovery stack, and llama-kv-cache.cpp:805 calls it for every
// stream of every ubatch that goes through the recovery path.
//
// Compact and lazy by construction, for the reason EXP204 and EXP205 learned the hard way: the
// host gathers only the m cells named in idxs, never the whole cache.
//
// Native also copies the `ext` record (12 bytes: x, y, tok). That is NOT modelled here and the
// omission is deliberate and recorded: for this profile hparams.n_pos_per_embd()==1 and
// ple_n_heads==0, so llama_kv_cache::has_cell_ext() is false, is_2d is false, and ext is never
// read. It becomes a real gap the moment an M-RoPE or PLE model is served, so it is stated here
// rather than silently dropped.
extern "fg206c_fail" fn fail(code: i64);
extern "fg206c_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg206c_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// gather_cells: compact form of cp(idxs).
//   src pos/seq/shift are the full containers; idxs names m source cells
//   dst pos/seq are compact, m entries
//   shift must be 0 on every gathered cell, as native asserts

fn gather_cells(dpos: u64, dseq: u64, spos: u64, sseq: u64, sshift: u64, idxs: u64, m: i64, w: i64) -> i64 {
    ensure(m >= 0 && w > 0, 600);
    var j: i64 = 0;

    while j < m {
        let i: i64 = ld_i64(idxs, j * 8 as i64);
        ensure(i >= 0, 601);
        ensure(ld_i64(sshift, i * 8 as i64) == 0 as i64, 602);
        st_i64(dpos, j * 8 as i64, ld_i64(spos, i * 8 as i64));
        var k: i64 = 0;

        while k < w {
            st_i64(dseq, (j * w + k) * 8 as i64, ld_i64(sseq, (i * w + k) * 8 as i64));
            k += 1;
        }
        j += 1;
    }
    return m;
}
// gather_contig: compact form of cp(i, n) -- source cells are contiguous from i.

fn gather_contig(dpos: u64, dseq: u64, spos: u64, sseq: u64, sshift: u64, i: i64, m: i64, w: i64) -> i64 {
    ensure(i >= 0 && m >= 0 && w > 0, 610);
    var j: i64 = 0;

    while j < m {
        let s: i64 = i + j;
        ensure(ld_i64(sshift, s * 8 as i64) == 0 as i64, 611);
        st_i64(dpos, j * 8 as i64, ld_i64(spos, s * 8 as i64));
        var k: i64 = 0;

        while k < w {
            st_i64(dseq, (j * w + k) * 8 as i64, ld_i64(sseq, (s * w + k) * 8 as i64));
            k += 1;
        }
        j += 1;
    }
    return m;
}

fn main() {
    gather_cells(0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64);
    gather_cells(8 as u64, 16 as u64, 24 as u64, 32 as u64, 40 as u64, 48 as u64, 1 as i64, 4 as i64);
    gather_contig(0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    gather_contig(56 as u64, 64 as u64, 72 as u64, 80 as u64, 88 as u64, 2 as i64, 1 as i64, 4 as i64);
}

