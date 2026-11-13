// KV cell state copy in Goose (APX: P5-01, the "copy" clause).
//
// From llama-kv-cells.h:166-192 `set(uint32_t i, const llama_kv_cells & other)` and its indexed
// form at 197-220. This is the rollback path: when an ubatch cannot be emplaced the saved cell
// states are copied back, so getting it wrong silently corrupts a restored context.
//
// Same arena split as EXP199: the host owns flat int64 storage, Goose owns every decision.
//   pos[n] | seq[n*W] | used[n] | shift[n],  W = 8 words (512 sequence bits)
//
// The native `ext` record is not modelled -- it carries no decision this profile makes. Native
// asserts shift == 0 on every copied cell; here that is a trap rather than a silent acceptance,
// because a shifted source cell would make the restored state wrong.
extern "fg204s_fail" fn fail(code: i64);
extern "fg204s_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg204s_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// W as a function, not a const: the const bug (compiler-limitations #2) made this read zero on
// the build EXP199 used. On subset-0989 consts work through gg_init_globals, but the bridge here
// does not call it, so the literal form stays.

fn w() -> i64 {
    return 8 as i64;
}

fn seq_base(n: i64) -> i64 {
    return n * 8 as i64;
}

fn used_base(n: i64) -> i64 {
    return (n + n * w()) * 8 as i64;
}

fn shift_base(n: i64) -> i64 {
    return (n + n * w() + n) * 8 as i64;
}

fn pos_at(a: u64, i: i64) -> i64 {
    return ld_i64(a, i * 8 as i64);
}

fn seqword(a: u64, n: i64, i: i64, ww: i64) -> i64 {
    return ld_i64(a, seq_base(n) + (i * w() + ww) * 8 as i64);
}
// set_cells: copies m cells, dst[di..di+m) <- src[sj..sj+m).
// The used-set and sequence words follow the source; the position decides the used flag.
// shift must be zero on the source cell, as native asserts.

fn set_cells(dst: u64, dn: i64, di: i64, src: u64, sn: i64, sj: i64, m: i64) -> i64 {
    ensure(dn > 0 && sn > 0, 300);
    ensure(di >= 0 && sj >= 0 && m >= 0, 301);
    ensure(di + m <= dn, 302);
    ensure(sj + m <= sn, 303);
    var j: i64 = 0;

    while j < m {
        let didx: i64 = di + j;
        let sidx: i64 = sj + j;
        let dpos: i64 = pos_at(dst, didx);
        let spos: i64 = pos_at(src, sidx);
        ensure(ld_i64(src, shift_base(sn) + sidx * 8 as i64) == 0 as i64, 304);

        if dpos == (0 as i64) - 1 as i64 {
            if spos != (0 as i64) - 1 as i64 {
                st_i64(dst, used_base(dn) + didx * 8 as i64, 1 as i64);
            }
        } else {
            if spos == (0 as i64) - 1 as i64 {
                st_i64(dst, used_base(dn) + didx * 8 as i64, 0 as i64);
            }
        }
        st_i64(dst, didx * 8 as i64, spos);
        var ww: i64 = 0;

        while ww < w() {
            st_i64(dst, seq_base(dn) + (didx * w() + ww) * 8 as i64, seqword(src, sn, sidx, ww));
            ww += 1;
        }
        j += 1;
    }
    return m;
}

fn main() {
    set_cells(0 as u64, 0 as i64, 0 as i64, 0 as u64, 0 as i64, 0 as i64, 0 as i64);
    set_cells(64 as u64, 2 as i64, 0 as i64, 128 as u64, 2 as i64, 1 as i64, 1 as i64);
}

