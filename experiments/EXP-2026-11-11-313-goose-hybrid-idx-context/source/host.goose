// Apex hybrid indexed memory: context stepping and apply (P5-05).
//
// `next()` clears the staleness marks only after a successful ubatch (that is, at the first step),
// steps the indexer context, then advances the counter and steps the base context. `apply()` ANDs
// the base and indexer results, and only when both succeed and the pool state is tracked does it
// extend the layout, rebuild the pool state for this ubatch, and pin `i_kpool` to the current step —
// the pool state is only valid for the ubatch it was built for, and reading it early is an error.
//
// `kpool_track()` is derived, not cached: an indexer memory, a pool size above zero and a non-empty
// stream-count list.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 a, 3 b, 4 c, 5 d, 6 n, 7 arr
//   k=3  0 done, 1 ret
extern "fg313k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg313k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg313k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg313k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg313k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    st(arr, i * (8 as i64), v);
}

fn track(has_mem: bool, has_idx: bool, kpool: i64, ns_len: i64) -> bool {
    if !has_mem {
        return false;
    }

    if !has_idx {
        return false;
    }

    if kpool <= (0 as i64) {
        return false;
    }

    if ns_len <= (0 as i64) {
        return false;
    }
    return true;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let a: i64 = get(packet, 0, 2) as i64;
    let b: i64 = get(packet, 0, 3) as i64;
    let c: i64 = get(packet, 0, 4) as i64;
    let d: i64 = get(packet, 0, 5) as i64;
    let n: i64 = get(packet, 0, 6) as i64;
    let arr: u64 = get(packet, 0, 7);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // kpool_track(mem, idx, kpool, ns_len)
        var r: bool = track(a != (0 as i64), b != (0 as i64), c, d);

        if r {
            ret = 1 as i64;
        }
    } else {
        if op == (1 as i64) {
            // next(): a = i_cur, b = has_mem, c = has_idx, d = base_next result
            var i_cur: i64 = a;
            var ne: i64 = 0;

            if i_cur == (0 as i64) {
                if b != (0 as i64) {
                    setat(out, ne, 0 as i64);
                    ne = ne + 1 as i64;
                    // stale marks cleared
                }
            }

            if c != (0 as i64) {
                setat(out, ne, 1 as i64);
                ne = ne + 1 as i64;
                // indexer context stepped
            }
            i_cur = i_cur + (1 as i64);

            if d != (0 as i64) {
                setat(out, ne, 2 as i64);
                ne = ne + 1 as i64;
                // base context stepped
            }
            setat(out, 3 as i64, i_cur);
            ret = ne;
        } else {
            if op == (2 as i64) {
                // apply(): a = base result, b = idx result, c = has_idx, d = track
                var res: i64 = 0;

                if a != (0 as i64) {
                    if b != (0 as i64) {
                        res = 1 as i64;
                    }
                }
                var ne: i64 = 0;

                if res != (0 as i64) {
                    if d != (0 as i64) {
                        setat(out, ne, 0 as i64);
                        ne = ne + 1 as i64;
                        // layout extended
                        setat(out, ne, 1 as i64);
                        ne = ne + 1 as i64;
                        // pool state rebuilt
                        setat(out, ne, 2 as i64);
                        ne = ne + 1 as i64;
                        // i_kpool pinned
                    }
                }
                setat(out, 3 as i64, res);
                ret = ne;
            } else {
                if op == (3 as i64) {
                    // get_n_stream(): index ns_ubatch by the current step, rejecting an out-of-range step
                    let i_cur: i64 = a;

                    if i_cur < 0 as i64 {
                        ret = (0 as i64) - 1 as i64;
                    } else {
                        if i_cur >= n {
                            ret = (0 as i64) - 1 as i64;
                        } else {
                            ret = at(arr, i_cur);
                        }
                    }
                } else {
                    ensure(false, 60);
                }
            }
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

