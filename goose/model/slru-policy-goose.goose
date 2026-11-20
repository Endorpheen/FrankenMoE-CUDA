// SLRU policy of the MoE expert cache: runtime/reconstructed/src/llama-moe-cache.cpp, lines 66-167.
//
//   move       67  doubly-linked list move between the two segments
//   touch      95  promote a hit, demote the coldest protected slot when the protected half overflows
//   victim    102  the first slot that may be taken: empty, another layer, or not selected this request
//   plan      122  dedupe the ids, promote hits, then give each miss a victim
//
// INTERFACE: raw addresses for the int32 state, typed slices only for the i64 scalars.
//
// Why not typed slices everywhere: the native arrays are int32_t except seen, and Goose slices are i64
// only. Passing typed slices would force a widening copy of four arrays on every call, and plan runs once
// per layer per token. The loads and stores below address the native arrays directly - the same shape the
// fg263c bridge uses for the copy decisions.
//
//   i32, by address: key_of[n_slots], prev, next, prot, slots[n_expert] (this layer), ids, uniq,
//                    fills_e, fills_s, evicted
//   i64, typed slice: seen[n_expert], scal[5]
//
// `scal`: 0 seen_gen, 1 n_protected, 2 n_fills (out), 3 n_hit (out), 4 ok (out, 1/0).
//
// The evicted expert may belong to another layer, whose map this module cannot reach; it is reported
// through `evicted` so the caller clears that layer's entry. Returns the fill count, or -1 when the
// request cannot be satisfied (more distinct experts than slots, or no victim); the caller keeps the
// native abort in that case.

const SL_NIL: i64 = 0 - 1;

extern "slru_ld_i32" fn sl_raw_load(base: u64, byte: i64) -> i64;
extern "slru_st_i32" fn sl_raw_store(base: u64, byte: i64, v: i64);

fn sl_read(a: u64, i: i64) -> i64 {
    return sl_raw_load(a, i * 4 as i64);
}

fn sl_write(a: u64, i: i64, v: i64) {
    sl_raw_store(a, i * 4 as i64, v);
}

fn sl_move(prev: u64, next: u64, prot: u64, head: u64, tail: u64,
           scal: i64[:], s: i64, p: i64) {
    let from: i64 = sl_read(prot, s);
    if from == p && s == sl_read(tail, p) {
        return;
    }
    let s_prev: i64 = sl_read(prev, s);
    let s_next: i64 = sl_read(next, s);
    if s_prev >= 0 {
        sl_write(next, s_prev, s_next);
    } else {
        sl_write(head, from, s_next);
    }
    if s_next >= 0 {
        sl_write(prev, s_next, s_prev);
    } else {
        sl_write(tail, from, s_prev);
    }

    let tail_p: i64 = sl_read(tail, p);
    sl_write(prev, s, tail_p);
    sl_write(next, s, SL_NIL);
    if tail_p >= 0 {
        sl_write(next, tail_p, s);
    } else {
        sl_write(head, p, s);
    }
    sl_write(tail, p, s);
    sl_write(prot, s, p);
    scal[1] = scal[1] + p - from;
}

fn sl_touch(prev: u64, next: u64, prot: u64, head: u64, tail: u64,
            scal: i64[:], s: i64, n_slots: i64) {
    sl_move(prev, next, prot, head, tail, scal, s, 1 as i64);
    if scal[1] > n_slots / 2 as i64 {
        sl_move(prev, next, prot, head, tail, scal, sl_read(head, 1), 0 as i64);
    }
}

fn sl_victim(key_of: u64, next: u64, head: u64, seen: const i64[:],
             gen: i64, n_expert: i64, il: i64) -> i64 {
    var seg: i64 = 0;
    while seg < 2 {
        var s: i64 = sl_read(head, seg);
        while s >= 0 {
            let key: i64 = sl_read(key_of, s);
            // keep every expert selected by this request, including earlier fills.
            // Nested branches on purpose: an index is only formed once key >= 0, so the -1 sentinel
            // can never reach a slice access.
            if key < 0 {
                return s;
            }
            if key / n_expert != il {
                return s;
            }
            if seen[key % n_expert] != gen {
                return s;
            }
            s = sl_read(next, s);
        }
        seg = seg + 1;
    }
    return SL_NIL;
}

fn slru_plan_impl(
    key_of: u64, prev: u64, next: u64, prot: u64,
    head: u64, tail: u64, seen: i64[:], slots: u64,
    ids: u64, n_ids: i64,
    il: i64, n_expert: i64, n_slots: i64,
    scal: i64[:], uniq: u64, fills_e: u64, fills_s: u64, evicted: u64,
    maps: const i64[:], n_layer: i64
) -> i64 {
    scal[2] = 0;
    scal[3] = 0;
    scal[4] = 1;

    var gen: i64 = scal[0] + 1 as i64;
    scal[0] = gen;

    // dedupe the ids of this request, keeping the order of first appearance
    var nu: i64 = 0;
    var i: i64 = 0;
    while i < n_ids {
        let e: i64 = sl_read(ids, i);
        if e < 0 || e >= n_expert {
            scal[4] = 0;
            return 0 - 1;
        }
        if seen[e] != gen {
            // the native vector grows; here uniq is caller scratch of n_slots, so the overflow is
            // detected before the write rather than after it
            if nu >= n_slots {
                scal[4] = 0;
                return 0 - 1;
            }
            seen[e] = gen;
            sl_write(uniq, nu, e);
            nu = nu + 1;
        }
        i = i + 1;
    }

    let base: i64 = il * n_expert;

    // promote hits before choosing victims for misses
    var n_hit: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = sl_read(uniq, i);
        let slot: i64 = sl_read(slots, e);
        if slot >= 0 {
            sl_touch(prev, next, prot, head, tail, scal, slot, n_slots);
            n_hit = n_hit + 1;
        }
        i = i + 1;
    }

    // sorted misses usually get consecutive slots, so the uploads can be merged
    var a: i64 = 1;
    while a < nu {
        let v: i64 = sl_read(uniq, a);
        var b: i64 = a - 1;
        while b >= 0 && sl_read(uniq, b) > v {
            sl_write(uniq, b + 1, sl_read(uniq, b));
            b = b - 1;
        }
        sl_write(uniq, b + 1, v);
        a = a + 1;
    }

    var nf: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = sl_read(uniq, i);
        if sl_read(slots, e) < 0 {
            let s: i64 = sl_victim(key_of, next, head, seen, gen, n_expert, il);
            if s < 0 {
                scal[4] = 0;
                return 0 - 1;
            }
            let old: i64 = sl_read(key_of, s);
            sl_write(evicted, nf, old);
            // the eviction's second half: the victim is cleared in ITS layer's map before the new owner is written.
            // The maps are pointers to each layer's slot map; the load/store FFI is the one the host adapter provides.
            if old >= 0 && maps.len > 0 {
                let old_il: i64 = old / n_expert;
                let old_e:  i64 = old - old_il * n_expert;
                if old_il >= 0 && old_il < n_layer && old_il < maps.len {
                    sl_raw_store(maps[old_il] as u64, old_e * 4, 0 - 1);
                }
            }
            sl_write(key_of, s, base + e);
            sl_write(slots, e, s);
            sl_move(prev, next, prot, head, tail, scal, s, 0 as i64);
            sl_write(fills_e, nf, e);
            sl_write(fills_s, nf, s);
            nf = nf + 1;
        }
        i = i + 1;
    }

    scal[2] = nf;
    scal[3] = n_hit;
    return nf;
}

export fn slru_plan(
    key_of: u64, prev: u64, next: u64, prot: u64,
    head: u64, tail: u64, seen: i64[:], slots: u64,
    ids: u64, n_ids: i64,
    il: i64, n_expert: i64, n_slots: i64,
    scal: i64[:], uniq: u64, fills_e: u64, fills_s: u64, evicted: u64,
    maps: const i64[:], n_layer: i64
) -> i64 {
    return slru_plan_impl(key_of, prev, next, prot, head, tail, seen, slots,
        ids, n_ids, il, n_expert, n_slots, scal, uniq, fills_e, fills_s, evicted, maps, n_layer);
}
