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

extern "slru_fail" fn fail(c: i64);
extern "slru_ld_i32" fn ld_i32(base: u64, byte: i64) -> i64;
extern "slru_st_i32" fn st_i32(base: u64, byte: i64, v: i64);

fn g32(a: u64, i: i64) -> i64 {
    return ld_i32(a, i * 4 as i64);
}

fn p32(a: u64, i: i64, v: i64) {
    st_i32(a, i * 4 as i64, v);
}

fn sl_move(prev: u64, next: u64, prot: u64, head: u64, tail: u64,
           scal: i64[:], s: i64, p: i64) {
    let from: i64 = g32(prot, s);
    if from == p && s == g32(tail, p) {
        return;
    }
    let s_prev: i64 = g32(prev, s);
    let s_next: i64 = g32(next, s);
    if s_prev >= 0 {
        p32(next, s_prev, s_next);
    } else {
        p32(head, from, s_next);
    }
    if s_next >= 0 {
        p32(prev, s_next, s_prev);
    } else {
        p32(tail, from, s_prev);
    }

    let tail_p: i64 = g32(tail, p);
    p32(prev, s, tail_p);
    p32(next, s, SL_NIL);
    if tail_p >= 0 {
        p32(next, tail_p, s);
    } else {
        p32(head, p, s);
    }
    p32(tail, p, s);
    p32(prot, s, p);
    scal[1] = scal[1] + p - from;
}

fn sl_touch(prev: u64, next: u64, prot: u64, head: u64, tail: u64,
            scal: i64[:], s: i64, n_slots: i64) {
    sl_move(prev, next, prot, head, tail, scal, s, 1 as i64);
    if scal[1] > n_slots / 2 as i64 {
        sl_move(prev, next, prot, head, tail, scal, g32(head, 1), 0 as i64);
    }
}

fn sl_victim(key_of: u64, next: u64, head: u64, seen: const i64[:],
             gen: i64, n_expert: i64, il: i64) -> i64 {
    var seg: i64 = 0;
    while seg < 2 {
        var s: i64 = g32(head, seg);
        while s >= 0 {
            let key: i64 = g32(key_of, s);
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
            s = g32(next, s);
        }
        seg = seg + 1;
    }
    return SL_NIL;
}

export fn slru_plan(
    key_of: u64, prev: u64, next: u64, prot: u64,
    head: u64, tail: u64, seen: i64[:], slots: u64,
    ids: u64, n_ids: i64,
    il: i64, n_expert: i64, n_slots: i64,
    scal: i64[:], uniq: u64, fills_e: u64, fills_s: u64, evicted: u64
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
        let e: i64 = g32(ids, i);
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
            p32(uniq, nu, e);
            nu = nu + 1;
        }
        i = i + 1;
    }

    let base: i64 = il * n_expert;

    // promote hits before choosing victims for misses
    var n_hit: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = g32(uniq, i);
        let slot: i64 = g32(slots, e);
        if slot >= 0 {
            sl_touch(prev, next, prot, head, tail, scal, slot, n_slots);
            n_hit = n_hit + 1;
        }
        i = i + 1;
    }

    // sorted misses usually get consecutive slots, so the uploads can be merged
    var a: i64 = 1;
    while a < nu {
        let v: i64 = g32(uniq, a);
        var b: i64 = a - 1;
        while b >= 0 && g32(uniq, b) > v {
            p32(uniq, b + 1, g32(uniq, b));
            b = b - 1;
        }
        p32(uniq, b + 1, v);
        a = a + 1;
    }

    var nf: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = g32(uniq, i);
        if g32(slots, e) < 0 {
            let s: i64 = sl_victim(key_of, next, head, seen, gen, n_expert, il);
            if s < 0 {
                scal[4] = 0;
                return 0 - 1;
            }
            p32(evicted, nf, g32(key_of, s));
            p32(key_of, s, base + e);
            p32(slots, e, s);
            sl_move(prev, next, prot, head, tail, scal, s, 0 as i64);
            p32(fills_e, nf, e);
            p32(fills_s, nf, s);
            nf = nf + 1;
        }
        i = i + 1;
    }

    scal[2] = nf;
    scal[3] = n_hit;
    return nf;
}
