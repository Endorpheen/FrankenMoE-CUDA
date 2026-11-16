// SLRU policy of the MoE expert cache: runtime/reconstructed/src/llama-moe-cache.cpp, lines 66-167.
//
// The native code keeps a slot-level SLRU with two segments (probation, protected) and decides, for each
// layer and each token, which slot every selected expert gets. Those are pure integer decisions on small
// arrays, so they belong here; the copies and the allocation stay native.
//
//   move       67  doubly-linked list move between the two segments
//   touch      95  promote a hit, demote the coldest protected slot when the protected half overflows
//   victim    102  the first slot that may be taken: empty, another layer, or not selected this request
//   plan      122  dedupe the ids, promote hits, then give each miss a victim
//
// Layout of `maps` (in/out, flat, n_layer*n_expert): the slot map of every layer, -1 when not cached.
// Layer il's own map starts at il*n_expert.
//
// `scal` (in/out, 5 i64): 0 seen_gen, 1 n_protected, 2 n_fills (out), 3 n_hit (out), 4 ok (out, 1/0).
// `uniq` is scratch of length >= n_slots. `fills_e`/`fills_s` receive one (expert, slot) pair per miss.
//
// Returns the number of fills, or -1 when the request cannot be satisfied (more distinct experts than
// slots, or no victim). The caller keeps the native abort in that case.

const SL_NIL: i64 = 0 - 1;

fn sl_move(prev: i64[:], next: i64[:], prot: i64[:], head: i64[:], tail: i64[:],
           scal: i64[:], s: i64, p: i64) {
    let from: i64 = prot[s];
    if from == p && s == tail[p] {
        return;
    }
    if prev[s] >= 0 {
        next[prev[s]] = next[s];
    } else {
        head[from] = next[s];
    }
    if next[s] >= 0 {
        prev[next[s]] = prev[s];
    } else {
        tail[from] = prev[s];
    }

    prev[s] = tail[p];
    next[s] = SL_NIL;
    if tail[p] >= 0 {
        next[tail[p]] = s;
    } else {
        head[p] = s;
    }
    tail[p] = s;
    prot[s] = p;
    scal[1] = scal[1] + p - from;
}

fn sl_touch(prev: i64[:], next: i64[:], prot: i64[:], head: i64[:], tail: i64[:],
            scal: i64[:], s: i64, n_slots: i64) {
    sl_move(prev, next, prot, head, tail, scal, s, 1);
    if scal[1] > n_slots / 2 {
        sl_move(prev, next, prot, head, tail, scal, head[1], 0);
    }
}

fn sl_victim(key_of: const i64[:], next: const i64[:], head: const i64[:],
             seen: const i64[:], gen: i64, n_expert: i64, il: i64) -> i64 {
    var seg: i64 = 0;
    while seg < 2 {
        var s: i64 = head[seg];
        while s >= 0 {
            let key: i64 = key_of[s];
            // keep every expert selected by this request, including earlier fills.
            // Written as nested branches on purpose: the index is only formed once key >= 0,
            // so a negative sentinel can never reach a slice access.
            if key < 0 {
                return s;
            }
            if key / n_expert != il {
                return s;
            }
            if seen[key % n_expert] != gen {
                return s;
            }
            s = next[s];
        }
        seg = seg + 1;
    }
    return SL_NIL;
}

export fn slru_plan(
    key_of: i64[:], prev: i64[:], next: i64[:], prot: i64[:],
    head: i64[:], tail: i64[:], seen: i64[:], maps: i64[:],
    ids: const i64[:], n_ids: i64,
    il: i64, n_expert: i64, n_slots: i64, n_layer: i64,
    scal: i64[:], uniq: i64[:], fills_e: i64[:], fills_s: i64[:]
) -> i64 {
    scal[2] = 0;
    scal[3] = 0;
    scal[4] = 1;

    var gen: i64 = scal[0] + 1;
    if gen == 0 {
        var z: i64 = 0;
        while z < n_expert {
            seen[z] = 0;
            z = z + 1;
        }
        gen = 1;
    }
    scal[0] = gen;

    // dedupe the ids of this request, keeping the order of first appearance
    var nu: i64 = 0;
    var i: i64 = 0;
    while i < n_ids {
        let e: i64 = ids[i];
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
            uniq[nu] = e;
            nu = nu + 1;
        }
        i = i + 1;
    }

    let base: i64 = il * n_expert;

    // promote hits before choosing victims for misses
    var n_hit: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = uniq[i];
        let slot: i64 = maps[base + e];
        if slot >= 0 {
            sl_touch(prev, next, prot, head, tail, scal, slot, n_slots);
            n_hit = n_hit + 1;
        }
        i = i + 1;
    }

    // sorted misses usually get consecutive slots, so the uploads can be merged
    var a: i64 = 1;
    while a < nu {
        let v: i64 = uniq[a];
        var b: i64 = a - 1;
        while b >= 0 && uniq[b] > v {
            uniq[b + 1] = uniq[b];
            b = b - 1;
        }
        uniq[b + 1] = v;
        a = a + 1;
    }

    var nf: i64 = 0;
    i = 0;
    while i < nu {
        let e: i64 = uniq[i];
        if maps[base + e] < 0 {
            let s: i64 = sl_victim(key_of, next, head, seen, gen, n_expert, il);
            if s < 0 {
                scal[4] = 0;
                return 0 - 1;
            }
            let old: i64 = key_of[s];
            if old >= 0 {
                // the evicted expert belongs to some layer; clear its entry in that layer's map.
                // Same reason as above: no index is formed from a negative sentinel.
                let ol: i64 = old / n_expert;
                if ol < n_layer {
                    maps[ol * n_expert + old % n_expert] = SL_NIL;
                }
            }
            key_of[s] = base + e;
            maps[base + e] = s;
            sl_move(prev, next, prot, head, tail, scal, s, 0);
            fills_e[nf] = e;
            fills_s[nf] = s;
            nf = nf + 1;
        }
        i = i + 1;
    }

    scal[2] = nf;
    scal[3] = n_hit;
    return nf;
}

