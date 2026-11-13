// KV cell state copy in Goose, real container shape (APX: P5-01, the "copy" clause).
//
// From llama-kv-cells.h:166-192 (contiguous) and 197-220 (indexed). This is the rollback path:
// when an ubatch cannot be emplaced the saved cell states are copied back, so a wrong copy
// corrupts a restored context silently.
//
// Unlike the first version, this takes the three containers as they really are, not a modelled
// arena: the caller passes separate pointers for positions, sequence words and used flags, plus
// the destination index list. LLAMA_MAX_SEQ is 256, so a cell's sequence bitset is four 64-bit
// words; W is passed in rather than fixed, so a different build's MAX_SEQ is not silently wrong.
//
// The native `ext` record is not modelled: it carries no decision this profile makes. Native
// asserts shift == 0 on every copied cell; that is a trap here rather than a silent acceptance.
extern "fg204w_fail" fn fail(code: i64);
extern "fg204w_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg204w_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// set_cell_one: one cell of the loop, shared by both forms.
//   dpos,dseq,dused  destination containers
//   didx             destination cell index
//   spos,sseq        source containers
//   sidx             source cell index
//   w                sequence words per cell

fn set_cell_one(dpos: u64, dseq: u64, dused: u64, didx: i64, spos: u64, sseq: u64, sidx: i64, w: i64) -> i64 {
    let dp: i64 = ld_i64(dpos, didx * 8 as i64);
    let sp: i64 = ld_i64(spos, sidx * 8 as i64);

    if dp == (0 as i64) - 1 as i64 {
        if sp != (0 as i64) - 1 as i64 {
            st_i64(dused, didx * 8 as i64, 1 as i64);
        }
    } else {
        if sp == (0 as i64) - 1 as i64 {
            st_i64(dused, didx * 8 as i64, 0 as i64);
        }
    }
    st_i64(dpos, didx * 8 as i64, sp);
    var k: i64 = 0;

    while k < w {
        st_i64(dseq, (didx * w + k) * 8 as i64, ld_i64(sseq, (sidx * w + k) * 8 as i64));
        k += 1;
    }
    return 1 as i64;
}
// set_indexed: llama-kv-cells.h:197. dst cell = idxs[j], source cell = j.

fn set_indexed(dpos: u64, dseq: u64, dused: u64, spos: u64, sseq: u64, idxs: u64, m: i64, w: i64) -> i64 {
    ensure(m >= 0 && w > 0, 400);
    var j: i64 = 0;

    while j < m {
        let idx: i64 = ld_i64(idxs, j * 8 as i64);
        ensure(idx >= 0, 401);
        set_cell_one(dpos, dseq, dused, idx, spos, sseq, j, w);
        j += 1;
    }
    return m;
}
// set_contig: llama-kv-cells.h:166. dst cell = di+j, source cell = j.

fn set_contig(dpos: u64, dseq: u64, dused: u64, dn: i64, di: i64, spos: u64, sseq: u64, m: i64, w: i64) -> i64 {
    ensure(m >= 0 && w > 0, 410);
    ensure(di >= 0 && di + m <= dn, 411);
    var j: i64 = 0;

    while j < m {
        set_cell_one(dpos, dseq, dused, di + j, spos, sseq, j, w);
        j += 1;
    }
    return m;
}

fn main() {
    set_indexed(0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64);
    set_indexed(8 as u64, 16 as u64, 24 as u64, 32 as u64, 40 as u64, 48 as u64, 1 as i64, 4 as i64);
    set_contig(0 as u64, 0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as u64, 0 as u64, 0 as i64, 0 as i64);
    set_contig(56 as u64, 64 as u64, 72 as u64, 2 as i64, 0 as i64, 80 as u64, 88 as u64, 1 as i64, 4 as i64);
}

