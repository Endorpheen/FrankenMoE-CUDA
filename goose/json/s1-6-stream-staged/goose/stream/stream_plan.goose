// P9-03 S3: the row-gathered stream plan, from runtime/core/src/moe/row_stream.cpp.
//
// A row-gathered table is bound to reserved address space and served from flash
// a slab at a time. The decisions, all on the eval thread, are:
//
//   init budget          36  a zero budget means the built-in window; a budget
//                            below one slab is rounded UP to a slab, because a
//                            smaller window would evict what it just read
//   n_slabs / slab_len   71  the reservation geometry; the last slab is short
//   gather              170  gen bump (and wrap), invalid indices skipped, a row
//                            that straddles a slab boundary contributes both,
//                            dedup by stamp, missing bytes priced at full slabs
//   evict_to_budget     148  oldest-first, stamped-for-this-gather slabs are
//                            SKIPPED: evicting a row the node is about to read
//                            is a correctness bug, not a cache choice
//   fetch order         207  ascending file offsets — the drive's order
//   materialize         213  a table larger than the window is refused rather
//                            than pulled in whole (never violate the RAM limit)
//
// Not here: FileReader, O_DIRECT, read(), vm_reserve/vm_commit/vm_evict, the
// ggml tensor rebind, and the RouterHook capture that decides WHICH tables
// qualify. The fixture supplies the table shape and the kernel outcomes.
//
// Slab ids are flat: table * n_slabs + slab. The LRU order is an oldest-first
// array of those ids, the pure form of the native std::list.

// Budget: 0 selects the default, and the window is never below one slab.
export fn sp_round_budget(budget: i64, slab: i64, def: i64) -> i64 {
    var b: i64 = budget;

    if b == 0 {
        b = def;
    }
    if b < slab {
        b = slab;
    }
    return b;
}

export fn sp_n_slabs(size: i64, slab: i64) -> i64 {
    if slab <= 0 {
        return 0;
    }
    return (size + slab - 1) / slab;
}

// The last slab is short by whatever the table's size is not a multiple of.
export fn sp_slab_len(size: i64, s: i64, slab: i64) -> i64 {
    let off: i64 = s * slab;
    let len: i64 = size - off;

    if len > slab {
        return slab;
    }
    return len;
}

// gather(): which slabs this node needs, deduplicated, and what they cost.
// Returns need count, missing bytes, and the gather generation to keep.
// A row covers [r*row_bytes, min(r*row_bytes + row_bytes, size)), so a row
// crossing a slab boundary stamps every slab it touches.
fn sp_gather(n_rows: i64, row_bytes: i64, size: i64, idx: const i64[:], n: i64,
             n_slabs: i64, slab: i64, stamp: i64[:], gen_in: i64,
             resident: const i64[:], need: i64[:]) -> i64, i64, i64 {
    var gen: i64 = (gen_in + 1) & 4294967295;

    if gen == 0 {
        var s0: i64 = 0;

        while s0 < n_slabs {
            stamp[s0] = 0;
            s0 = s0 + 1;
        }
        gen = 1;
    }
    var k: i64 = 0;
    var missing: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r: i64 = idx[i];

        if r >= 0 && r < n_rows {
            let a: i64 = r * row_bytes;
            var b: i64 = a + row_bytes;

            if b > size {
                b = size;
            }
            var s: i64 = a / slab;

            while s * slab < b {
                if s >= n_slabs {
                    break;
                }
                if stamp[s] != gen {
                    stamp[s] = gen;
                    need[k] = s;
                    k = k + 1;
                    if resident[s] == 0 {
                        missing = missing + slab;
                    }
                }
                s = s + 1;
            }
        }
        i = i + 1;
    }
    return k, missing, gen;
}

// The exported form: out[0] = need count, out[1] = missing bytes,
// out[2] = the gather generation the caller must keep.
export fn sp_gather_plan(n_rows: i64, row_bytes: i64, size: i64, idx: const i64[:], n: i64,
                         n_slabs: i64, slab: i64, stamp: i64[:], gen_in: i64,
                         resident: const i64[:], need: i64[:], out: i64[:]) -> i64 {
    let k, missing, gen = sp_gather(n_rows, row_bytes, size, idx, n, n_slabs,
                                    slab, stamp, gen_in, resident, need);

    out[0] = k;
    out[1] = missing;
    out[2] = gen;
    return k;
}

// fetch order: ascending file offsets, i.e. ascending slab ids. The native
// sort covers the whole need_ list; a resident slab is a no-op in fetch_slab,
// so the read list is the non-resident subset.
export fn sp_fetch_order(need: const i64[:], n: i64, resident: const i64[:], out: i64[:]) -> i64 {
    var k: i64 = 0;
    var i: i64 = 0;

    while i < n {
        out[i] = need[i];
        i = i + 1;
    }
    i = 1;
    while i < n {
        let v: i64 = out[i];
        var j: i64 = i;

        while j > 0 && out[j - 1] > v {
            out[j] = out[j - 1];
            j = j - 1;
        }
        out[j] = v;
        i = i + 1;
    }
    i = 0;
    while i < n {
        if resident[out[i]] == 0 {
            out[k] = out[i];
            k = k + 1;
        }
        i = i + 1;
    }
    return k;
}

// evict_to_budget: walk the LRU oldest-first and hand slabs back until
// `headroom` more bytes fit under `budget`. A slab stamped for the gather in
// flight is skipped, never evicted. Returns (evictions, resident bytes after,
// surviving order count); the surviving order is written to `keep`.
fn sp_evict_core(resident: i64[:], stamp: const i64[:], gen: i64, budget: i64,
                 resident_bytes_in: i64, headroom: i64, order: const i64[:], n_order: i64,
                 bytes: const i64[:], keep: i64[:]) -> i64, i64, i64 {
    var res: i64 = resident_bytes_in;
    var evicted: i64 = 0;
    var k: i64 = 0;
    var i: i64 = 0;

    while i < n_order {
        let s: i64 = order[i];

        if res + headroom <= budget {
            break;
        }
        if stamp[s] == gen {
            keep[k] = s;
            k = k + 1;
        } else {
            res = res - bytes[s];
            resident[s] = 0;
            evicted = evicted + 1;
        }
        i = i + 1;
    }
    while i < n_order {
        keep[k] = order[i];
        k = k + 1;
        i = i + 1;
    }
    return evicted, res, k;
}

// The exported form: out[0] = resident bytes after, returns evictions, and the
// surviving LRU order is left in `keep` (count in out[1]).
export fn sp_evict(resident: i64[:], stamp: const i64[:], gen: i64, budget: i64,
                   resident_bytes_in: i64, headroom: i64, order: const i64[:], n_order: i64,
                   bytes: const i64[:], keep: i64[:], out: i64[:]) -> i64 {
    let evicted, res, k = sp_evict_core(resident, stamp, gen, budget,
                                        resident_bytes_in, headroom, order,
                                        n_order, bytes, keep);

    out[0] = res;
    out[1] = k;
    return evicted;
}

// materialize(): a fallback for graphs that are not plain row gathers. Pulling
// a table bigger than the configured window in whole would break the RAM limit,
// so it is refused instead.
export fn sp_materialize_ok(size: i64, budget: i64) -> i64 {
    if size > budget {
        return 0;
    }
    return 1;
}
