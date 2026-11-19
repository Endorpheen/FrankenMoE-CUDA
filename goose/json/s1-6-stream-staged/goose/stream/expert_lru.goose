// P9-03 S1: the expert cache policy of runtime/core/src/moe/expert_stream_source.cpp.
//
// Every function here is a DECISION the native source makes on the eval thread:
//   touch_entry            917  hit/miss, promotion, residency accounting
//   lru_unlink             709  the intrusive list unlink
//   lru_push_front         721  MRU insert (a real hit)
//   lru_push_back          732  cold insert (a guess, or a committed spec read)
//   evict_tail             761  what the budget takes first
//   set_cache_budget       675  grow without evicting, shrink evicting to the new ceiling
//   retain                 777  promote resident predictions, never read
//   query_residency        882  0 miss, 1 resident, 2 speculative
//   worst_cycle_bytes      697  the cliff: top_k entries from every bound layer
//   account_demand         648  a token's working set, reset on a non-increasing layer
//   spec_integrate_done    812  a completed spec entry enters the list cold or HOT
//   prefetch (filter)      434  candidates: in-range, not resident, not already queued
//
// Not here, because they are I/O and not policy: commit_proj_pages (903),
// release_entry_pages (750), read_slice (299), the lane pool (336-400), the
// ready flags (155) and every syscall. The guest arrays stand in for the
// cache_* fields; page commit success is an argument, not a syscall.
//
// Layout of `lay` (input, per layer, 4 i64): bound, nb2 of projection 0..2
// (an absent projection of a fused layout is 0, exactly as entry_bytes reads it).
//
// `acc` (in/out, 5 i64): token_demand, layer_demand, demand_accum,
// layer_demand_accum, last_il.
// `st` (in/out, 2 i64): chead, ctail.
// `sacc` (in/out, 5 i64): cresident, chits, clookups, rereads, spec_useful.

const XL_P: i64 = 3;
const XL_NIL: i64 = 0 - 1;

fn xl_eb(lay: const i64[:], il: i64) -> i64 {
    var b: i64 = 0;
    var p: i64 = 0;

    while p < XL_P {
        b = b + lay[il * 4 + 1 + p];
        p = p + 1;
    }
    return b;
}

export fn xl_entry_bytes(lay: const i64[:], il: i64) -> i64 {
    return xl_eb(lay, il);
}

// A top_k wider than the bank asks for entries that do not exist: the native
// min(top_k, n_expert_) clamp is part of the number it reports.
export fn xl_worst_cycle(lay: const i64[:], n_layer: i64, n_expert: i64, top_k: i64) -> i64 {
    if top_k <= 0 {
        return 0;
    }
    var cycle: i64 = 0;
    var il: i64 = 0;
    var k: i64 = top_k;

    if k > n_expert {
        k = n_expert;
    }
    while il < n_layer {
        if lay[il * 4] != 0 {
            cycle = cycle + xl_eb(lay, il) * k;
        }
        il = il + 1;
    }
    return cycle;
}

fn xl_unlink(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    let pv: i64 = prev[id];
    let nx: i64 = next[id];

    if pv != XL_NIL {
        next[pv] = nx;
    } else {
        st[0] = nx;
    }
    if nx != XL_NIL {
        prev[nx] = pv;
    } else {
        st[1] = pv;
    }
    prev[id] = XL_NIL;
    next[id] = XL_NIL;
}

fn xl_front(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    prev[id] = XL_NIL;
    next[id] = st[0];
    if st[0] != XL_NIL {
        prev[st[0]] = id;
    } else {
        st[1] = id;
    }
    st[0] = id;
}

fn xl_back(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    next[id] = XL_NIL;
    prev[id] = st[1];
    if st[1] != XL_NIL {
        next[st[1]] = id;
    } else {
        st[0] = id;
    }
    st[1] = id;
}

export fn xl_unlink_e(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    xl_unlink(prev, next, st, id);
}

export fn xl_push_front(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    xl_front(prev, next, st, id);
}

export fn xl_push_back(prev: i64[:], next: i64[:], st: i64[:], id: i64) {
    xl_back(prev, next, st, id);
}

// touch_entry, with the commit outcome as an argument. Returns -1 when the
// commit failed (native returns false before cvalid), 1 on a hit, 0 on a miss.
// `promote` = 0 skips the hit-path LRU move (the overlap path re-orders every
// touched id afterwards); `co_proj` >= 0 commits only that projection on a miss.
export fn xl_touch(lay: const i64[:], cv: i64[:], cs: i64[:], prev: i64[:], next: i64[:],
                   ever: i64[:], st: i64[:], sacc: i64[:], n_expert: i64,
                   il: i64, e: i64, promote: i64, co_proj: i64, commit_ok: i64) -> i64 {
    let id: i64 = il * n_expert + e;

    sacc[2] = sacc[2] + 1;
    if cv[id] != 0 {
        sacc[1] = sacc[1] + 1;
        if cs[id] != 0 {
            sacc[4] = sacc[4] + 1;
            cs[id] = 0;
        }
        if promote != 0 {
            xl_unlink(prev, next, st, id);
            xl_front(prev, next, st, id);
        }
        return 1;
    }
    if commit_ok == 0 {
        return XL_NIL;
    }
    if ever[id] != 0 {
        sacc[3] = sacc[3] + 1;
    }
    cv[id] = 1;
    cs[id] = 0;
    sacc[0] = sacc[0] + xl_eb(lay, id / n_expert);
    xl_front(prev, next, st, id);
    return 0;
}

// cstamp_ is written by touch_entry as well; split out because the port takes
// the generation as an argument and the fixture keeps the array.
export fn xl_stamp(cstamp: i64[:], gen: i64, il: i64, e: i64, n_expert: i64) {
    cstamp[il * n_expert + e] = gen;
}

// One eviction of the coldest entry. Returns the evicted id, or -1 on an empty
// cache (the native while-loop guard is ctail_ != -1).
fn xl_evict_one(lay: const i64[:], cv: i64[:], cs: i64[:], prev: i64[:], next: i64[:],
                ever: i64[:], st: i64[:], sacc: i64[:], n_expert: i64) -> i64 {
    if st[1] == XL_NIL {
        return XL_NIL;
    }
    let id: i64 = st[1];

    sacc[0] = sacc[0] - xl_eb(lay, id / n_expert);
    ever[id] = 1;
    cv[id] = 0;
    cs[id] = 0;
    xl_unlink(prev, next, st, id);
    return id;
}

export fn xl_evict_tail(lay: const i64[:], cv: i64[:], cs: i64[:], prev: i64[:], next: i64[:],
                        ever: i64[:], st: i64[:], sacc: i64[:], n_expert: i64) -> i64 {
    return xl_evict_one(lay, cv, cs, prev, next, ever, st, sacc, n_expert);
}

// set_cache_budget. Growing evicts nothing and needs no quiesce; a shrink is
// floored at 1 byte (load_layer branches on cache_max_ == 0 to pick the
// shared-slot path, whose buffers do not exist in LRU mode) and evicts down to
// the new ceiling. `cache_off` = 1 models cache_max_ == 0 at entry: no-op.
// Returns the budget in force after the call.
export fn xl_set_budget(lay: const i64[:], cv: i64[:], cs: i64[:], prev: i64[:], next: i64[:],
                        ever: i64[:], st: i64[:], sacc: i64[:], budget: i64, cur: i64,
                        total: i64, cache_off: i64, n_expert: i64) -> i64 {
    if cache_off != 0 {
        return cur;
    }
    if budget > cur {
        if budget <= total {
            return budget;
        }
        return total;
    }
    if budget == cur {
        return cur;
    }
    var b: i64 = budget;

    if b < 1 {
        b = 1;
    }
    while sacc[0] > b && st[1] != XL_NIL {
        xl_evict_one(lay, cv, cs, prev, next, ever, st, sacc, n_expert);
    }
    return b;
}

// retain: move already-resident predictions to the MRU end. An absent entry is
// never promoted and never read — keeping it is prefetch's job.
export fn xl_retain(cv: const i64[:], prev: i64[:], next: i64[:], st: i64[:], il: i64,
                    ids: const i64[:], n: i64, n_expert: i64) -> i64 {
    var moved: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let e: i64 = ids[i];

        if e >= 0 && e < n_expert {
            let id: i64 = il * n_expert + e;

            if cv[id] != 0 {
                xl_unlink(prev, next, st, id);
                xl_front(prev, next, st, id);
                moved = moved + 1;
            }
        }
        i = i + 1;
    }
    return moved;
}

// query_residency: 0 = miss, 1 = resident, 2 = a speculative prefetch guessed
// it right. With the cache off (cache_off) every routing re-reads.
export fn xl_query(cv: const i64[:], cs: const i64[:], il: i64, ids: const i64[:], n: i64,
                   n_expert: i64, cache_off: i64, out: i64[:]) -> i64 {
    var i: i64 = 0;
    var resident: i64 = 0;

    while i < n {
        out[i] = 0;
        i = i + 1;
    }
    if cache_off != 0 {
        return 0;
    }
    i = 0;
    while i < n {
        let e: i64 = ids[i];

        if e >= 0 && e < n_expert {
            let id: i64 = il * n_expert + e;

            if cv[id] != 0 {
                if cs[id] != 0 {
                    out[i] = 2;
                } else {
                    out[i] = 1;
                }
                resident = resident + 1;
            }
        }
        i = i + 1;
    }
    return resident;
}

// prefetch's candidate filter: in range, not resident, not already queued.
// Duplicates are NOT removed — the native loop appends every surviving id.
export fn xl_filter(cv: const i64[:], rem: const i64[:], il: i64, ids: const i64[:], n: i64,
                    n_expert: i64, cache_off: i64, out: i64[:]) -> i64 {
    var k: i64 = 0;

    if cache_off != 0 {
        return 0;
    }
    var i: i64 = 0;

    while i < n {
        let e: i64 = ids[i];

        if e >= 0 && e < n_expert {
            let id: i64 = il * n_expert + e;

            if cv[id] == 0 && rem[id] == 0 {
                out[k] = e;
                k = k + 1;
            }
        }
        i = i + 1;
    }
    return k;
}

// spec_integrate_done: every completed entry whose projections all finished
// becomes resident. A guess enters COLD; under route-ahead (adopt) the read was
// committed by a staged layer, so it enters HOT — entering it cold let the
// intervening layers evict it before use and re-buy it on demand.
// The !cvalid && rem == 0 eligibility is the guard that keeps a stale
// completion from pushing an entry with live jobs into the list.
export fn xl_integrate(lay: const i64[:], cv: i64[:], cs: i64[:], cstamp: i64[:],
                       prev: i64[:], next: i64[:], st: i64[:], sacc: i64[:],
                       done: const i64[:], ndone: i64, rem: const i64[:],
                       n_expert: i64, adopt: i64) -> i64 {
    var got: i64 = 0;
    var i: i64 = 0;

    while i < ndone {
        let id: i64 = done[i];

        if cv[id] == 0 && rem[id] == 0 {
            cv[id] = 1;
            cs[id] = 1;
            cstamp[id] = 0;
            sacc[0] = sacc[0] + xl_eb(lay, id / n_expert);
            if adopt != 0 {
                xl_front(prev, next, st, id);
            } else {
                xl_back(prev, next, st, id);
            }
            got = got + 1;
        }
        i = i + 1;
    }
    return got;
}

// account_demand. A token's pass over the layer stack is monotonic in il: a
// non-increasing index means the previous pass ended, so its bytes are known.
// Returns the new last_il.
export fn xl_account(acc: i64[:], il: i64, n_unique: i64, ebytes: i64) -> i64 {
    if il <= acc[4] {
        acc[0] = acc[2];
        acc[1] = acc[3];
        acc[2] = 0;
        acc[3] = 0;
    }
    acc[4] = il;
    let bytes: i64 = n_unique * ebytes;

    acc[2] = acc[2] + bytes;
    if bytes > acc[3] {
        acc[3] = bytes;
    }
    return acc[4];
}

// The batch's dedup pass: the first occurrence of an expert is staged and a
// repeat is only promoted, so the LRU order reflects the LAST token that used
// it. The router's own top-k ids are distinct, but cache-aware dropping
// repoints a dropped slot at the routing's top expert, so repeats are normal
// there. `seen` is per-load scratch, cleared by the caller like seen_.
// `kind[i]` is 0 for an out-of-range id, 1 for the first occurrence (stage),
// 2 for a repeat (promote). out[0] = n_unique, out[1] = duplicates.
export fn xl_load_plan(seen: i64[:], ids: const i64[:], n: i64, n_expert: i64,
                       kind: i64[:], out: i64[:]) -> i64 {
    var nf: i64 = 0;
    var nd: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let e: i64 = ids[i];

        kind[i] = 0;
        if e >= 0 && e < n_expert {
            if seen[e] != 0 {
                kind[i] = 2;
                nd = nd + 1;
            } else {
                seen[e] = 1;
                kind[i] = 1;
                nf = nf + 1;
            }
        }
        i = i + 1;
    }
    out[0] = nf;
    out[1] = nd;
    return nf;
}

// The post-load eviction of the serial path. Note the whole loop is guarded by
// the generation stamp, not just each step: if the COLDEST entry was used by
// this very load, the eviction stops rather than skipping down the list.
// Returns the number of entries evicted.
export fn xl_evict_load(lay: const i64[:], cv: i64[:], cs: i64[:], prev: i64[:], next: i64[:],
                        ever: i64[:], st: i64[:], sacc: i64[:], cstamp: const i64[:],
                        gen: i64, cache_max: i64, n_expert: i64) -> i64 {
    var n: i64 = 0;

    while sacc[0] > cache_max && st[1] != XL_NIL && cstamp[st[1]] != gen {
        xl_evict_one(lay, cv, cs, prev, next, ever, st, sacc, n_expert);
        n = n + 1;
    }
    return n;
}
