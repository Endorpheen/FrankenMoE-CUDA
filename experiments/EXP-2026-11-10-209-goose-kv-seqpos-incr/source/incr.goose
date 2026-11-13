// Per-sequence position min/max maintained incrementally (APX: P5-01, "sequence" clause).
//
// EXP207 answered seq_pos_min/max with a linear scan and cost 59% of decode: the two queries are
// called from can_use_cell inside the slot-search walk, once per tested cell
// (llama-kv-cache.cpp:1080), so O(cells) there is quadratic with a 262144-cell cache.
//
// This module keeps the answer instead of recomputing it. Native reads std::set begin()/rbegin()
// in O(1); here three int64 slots per sequence are updated on every write that can move them.
// Update is O(1), query is O(1).
//
// Scope: only min/max. The third consumer of the ordered set, seq_pos_get + upper_bound
// (llama-memory-hybrid-idx.cpp:414), is not instantiated in this profile and is not modelled.
//
// Arena, int64 slots: mn[LLAMA_MAX_SEQ] | mx[LLAMA_MAX_SEQ] | cnt[LLAMA_MAX_SEQ].
// mn/mx start at -1 = sequence absent, which is what native returns for an empty set.
//
// The count is required, not optional. Native keys the set on (pos, cell), so one position may
// appear more than once for the same sequence -- during a cache reuse via (rm + add), and with
// inputs that repeat positions. Min/max alone cannot tell "one entry at p" from "two entries both
// at p", so removing one of a duplicate pair would wrongly report the set empty. The gate caught
// exactly this: min == max == 5 with two entries, remove one, the module said empty.
extern "fg209i_fail" fn fail(code: i64);
extern "fg209i_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg209i_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn mn_at(a: u64, s: i64) -> i64 {
    return ld_i64(a, s * 8 as i64);
}

fn mx_at(a: u64, s: i64, nseq: i64) -> i64 {
    return ld_i64(a, (nseq + s) * 8 as i64);
}

fn cnt_at(a: u64, s: i64, nseq: i64) -> i64 {
    return ld_i64(a, (2 as i64 * nseq + s) * 8 as i64);
}

fn cnt_set(a: u64, s: i64, nseq: i64, v: i64) {
    st_i64(a, (2 as i64 * nseq + s) * 8 as i64, v);
}
// on_insert: a cell at position p joined sequence s.

fn on_insert(a: u64, s: i64, nseq: i64, p: i64) -> i64 {
    ensure(s >= 0 && s < nseq, 10);

    if p == (0 as i64) - 1 as i64 {
        return 0 as i64;
    }
    let c: i64 = cnt_at(a, s, nseq);
    cnt_set(a, s, nseq, c + 1 as i64);

    if c == 0 as i64 {
        st_i64(a, s * 8 as i64, p);
        st_i64(a, (nseq + s) * 8 as i64, p);
        return 1 as i64;
    }
    let mn: i64 = mn_at(a, s);

    if p < mn {
        st_i64(a, s * 8 as i64, p);
    }

    if p > mx_at(a, s, nseq) {
        st_i64(a, (nseq + s) * 8 as i64, p);
    }
    return 0 as i64;
}
// on_remove: a cell at position p left sequence s.
//   0 = nothing to do, 2 = the set became empty, 1 = a bound moved and must be rescanned

fn on_remove(a: u64, s: i64, nseq: i64, p: i64) -> i64 {
    ensure(s >= 0 && s < nseq, 20);
    let c: i64 = cnt_at(a, s, nseq);

    if c == 0 as i64 {
        return 0 as i64;
    }
    let nc: i64 = c - 1 as i64;
    cnt_set(a, s, nseq, nc);

    if nc == 0 as i64 {
        st_i64(a, s * 8 as i64, (0 as i64) - 1 as i64);
        st_i64(a, (nseq + s) * 8 as i64, (0 as i64) - 1 as i64);
        return 2 as i64;
    }
    let mn: i64 = mn_at(a, s);
    let mx: i64 = mx_at(a, s, nseq);
    // a duplicated position keeps the bound, so only a real bound move needs the rescan

    if p == mn {
        return 1 as i64;
    }

    if p == mx {
        return 1 as i64;
    }
    return 0 as i64;
}
// commit_bound: the host rescanned and hands back the new bound (-1..-1 when empty).

fn commit_bound(a: u64, s: i64, nseq: i64, lo: i64, hi: i64) -> i64 {
    ensure(s >= 0 && s < nseq, 30);
    st_i64(a, s * 8 as i64, lo);
    st_i64(a, (nseq + s) * 8 as i64, hi);
    return 1 as i64;
}

fn main() {
    on_insert(0 as u64, 0 as i64, 0 as i64, 0 as i64);
    on_insert(16 as u64, 1 as i64, 4 as i64, 7 as i64);
    on_remove(0 as u64, 0 as i64, 0 as i64, 0 as i64);
    on_remove(16 as u64, 1 as i64, 4 as i64, 7 as i64);
    commit_bound(0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
    commit_bound(16 as u64, 1 as i64, 4 as i64, 1 as i64, 9 as i64);
}

