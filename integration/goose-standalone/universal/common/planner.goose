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
            scal: i64[:], s: i64, protected_limit: i64) {
    sl_move(prev, next, prot, head, tail, scal, s, 1 as i64);
    while scal[1] > protected_limit {
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
    maps: const i64[:], n_layer: i64, protected_limit: i64
) -> i64 {
    scal[2] = 0;
    scal[3] = 0;
    scal[4] = 1;

    var gen: i64 = scal[0] + 1 as i64;
    scal[0] = gen;

    while scal[1] > protected_limit {
        sl_move(prev, next, prot, head, tail, scal, sl_read(head, 1), 0 as i64);
    }

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
            sl_touch(prev, next, prot, head, tail, scal, slot, protected_limit);
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
        ids, n_ids, il, n_expert, n_slots, scal, uniq, fills_e, fills_s, evicted, maps, n_layer, n_slots / 2);
}

// Runtime layout: each device slot stores the up, gate and down planes of
// one expert. Layers with the same plane sizes and expert count share an SLRU.
const EC_GROUP: i64 = 9;
const EC_LAYER: i64 = 4;
const EC_DEFAULT_BYTES: i64 = 4992 * 1024 * 1024;

extern "fg_cuda_expert_upload" fn ec_upload(offset: i64, source: u64, bytes: i64) -> i64;
extern "fg_cuda_expert_stage_upload" fn ec_stage_upload(offset: i64, source: u64, bytes: i64) -> i64;
extern "fg_cuda_expert_stage_enabled" fn ec_stage_enabled(active_experts: i64, stage_all: bool) -> bool;
extern "fg_cuda_expert_record" fn ec_record(hits: i64, fills: i64, evictions: i64, bytes: i64);
export fn ec_reset(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:]) {
    for i in words64.len { words64[i] = 0; }
    let n = layers.len / EC_LAYER;
    var at: i64 = 0;
    for layer in n {
        let v = layers[layer * EC_LAYER];
        if v < 0 || groups[v * EC_GROUP + 5] == 0 { continue; }
        let map = words32 + ((4 * at) as u64);
        words64[layer] = map as! i64;
        let ne = groups[v * EC_GROUP + 3];
        for e in ne { sl_write(map, e, -1); }
        at += ne;
    }
    for group in groups.len / EC_GROUP {
        let g = group * EC_GROUP;
        let slots = groups[g + 5];
        if slots == 0 { continue; }
        let keys = words32 + ((4 * groups[g + 7]) as u64);
        let prev = keys + ((4 * slots) as u64);
        let next = prev + ((4 * slots) as u64);
        let prot = next + ((4 * slots) as u64);
        let head = prot + ((4 * slots) as u64);
        let tail = head + 8;
        for s in slots {
            sl_write(keys, s, -1);
            sl_write(prev, s, s - 1);
            sl_write(next, s, if s + 1 < slots { s + 1 } else { -1 });
            sl_write(prot, s, 0);
        }
        sl_write(head, 0, 0); sl_write(head, 1, -1);
        sl_write(tail, 0, slots - 1); sl_write(tail, 1, -1);
    }
}

fn ec_fill_plane(layer: const i64[:], group: const i64[:], span: i64,
                  expert: i64, slot: i64, plane: i64, staged: bool) -> bool {
    let up = group[0];
    let gate = group[1];
    let before = if plane == 0 { 0 } else if plane == 1 { up } else { up + gate };
    let bytes = group[plane];
    let offset = group[6] + span * slot + before;
    let source = (layer[1 + plane] as! u64) + ((bytes * expert) as u64);
    return if staged { ec_stage_upload(offset, source, bytes) == 0 }
        else { ec_upload(offset, source, bytes) == 0 };
}

// Resolve one indexed matrix into resident device offsets, including any
// fills. All effects run under the CUDA driver's current-context lock.
fn ec_prepare_impl(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:],
                     tensor: u64, bytes: i64, heads: const i64[:], result: i64[:], stage_all: bool, compact: bool, policy: i64) -> i64 {
    let n = layers.len / EC_LAYER;
    for il in n {
        let r = il * EC_LAYER;
        let v = layers[r];
        if v < 0 || groups[v * EC_GROUP + 5] == 0 { continue; }
        let g = v * EC_GROUP;
        let experts = groups[g + 3];
        let slots = groups[g + 5];
        if !compact && experts != heads.len { continue; }
        var projection: i64 = -1;
        for p in 3 {
            if (layers[r + 1 + p] as! u64) == tensor && groups[g + p] == bytes { projection = p; }
        }
        if projection < 0 { continue; }
        if result.len < heads.len { return -1; }

        let keys = words32 + ((4 * groups[g + 7]) as u64);
        let prev = keys + ((4 * slots) as u64);
        let next = prev + ((4 * slots) as u64);
        let prot = next + ((4 * slots) as u64);
        let head = prot + ((4 * slots) as u64);
        let tail = head + 8;
        let active = tail + 8;
        let uniq = active + ((4 * experts) as u64);
        let fills_e = uniq + ((4 * experts) as u64);
        let fills_s = fills_e + ((4 * experts) as u64);
        let evicted = fills_s + ((4 * experts) as u64);
        let start = groups[g + 8];
        let map = words64[il] as! u64;
        var count: i64 = 0;
        if compact {
            var previous: i64 = -1;
            for i in heads.len {
                let e = heads[i];
                if e <= previous || e >= experts { return -1; }
                previous = e;
                sl_write(active, i, e);
            }
            count = heads.len;
        } else {
            for e in experts {
                result[e] = -1;
                if heads[e] >= 0 { sl_write(active, count, e); count += 1; }
            }
        }
        if policy == 2 {
            if !compact { return -1; }
            let span = groups[g] + groups[g + 1] + groups[g + 2];
            let plane = if projection == 0 { 0 } else if projection == 1 { groups[g] } else { groups[g] + groups[g + 1] };
            for i in heads.len {
                let slot = sl_read(map, heads[i]);
                if slot < 0 || slot >= slots { return -1; }
                result[i] = groups[g + 6] + span * slot + plane;
            }
            ec_record(heads.len, 0, 0, 0);
            return 1;
        }
        let limit = if policy == 1 { slots / 5 * 4 + (slots % 5) * 4 / 5 } else { slots / 2 };
        let fills = slru_plan_impl(keys, prev, next, prot, head, tail, words64[start .. start + experts], map,
            active, count, il, experts, slots, words64[start + experts .. start + experts + 5],
            uniq, fills_e, fills_s, evicted, words64[0 .. n], n, limit);
        if fills < 0 { return -1; }
        let span = groups[g] + groups[g + 1] + groups[g + 2];
        var evictions: i64 = 0;
        for i in fills { if sl_read(evicted, i) >= 0 { evictions += 1; } }
        let layer = layers[r .. r + EC_LAYER];
        let group = groups[g .. g + EC_GROUP];
        if fills > 0 && ec_stage_enabled(count, stage_all) {
            for wave in 3 {
                let p = (projection + wave) % 3;
                for i in fills {
                    if !ec_fill_plane(layer, group, span, sl_read(fills_e, i), sl_read(fills_s, i), p, stage_all || wave != 0) {
                        return -1;
                    }
                }
            }
        } else {
            for i in fills {
                for p in 3 {
                    if !ec_fill_plane(layer, group, span, sl_read(fills_e, i), sl_read(fills_s, i), p, false) {
                        return -1;
                    }
                }
            }
        }
        var plane: i64 = 0;
        for p in projection { plane += groups[g + p]; }
        for i in heads.len {
            if compact || heads[i] >= 0 {
                let e = if compact { heads[i] } else { i };
                let slot = sl_read(map, e);
                if slot < 0 || slot >= slots { return -1; }
                result[i] = groups[g + 6] + span * slot + plane;
            }
        }
        ec_record(words64[start + experts + 3], fills, evictions, fills * span);
        return 1;
    }
    return 0;
}

export fn ec_prepare(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:],
                     tensor: u64, bytes: i64, heads: const i64[:], result: i64[:], stage_all: bool) -> i64 {
    return ec_prepare_impl(groups, layers, words32, words64, tensor, bytes, heads, result, stage_all, false, 0);
}

export fn ec_prepare_compact(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:],
                             tensor: u64, bytes: i64, ids: const i64[:], result: i64[:], stage_all: bool) -> i64 {
    if ids.len != 8 { return -1; }
    return ec_prepare_impl(groups, layers, words32, words64, tensor, bytes, ids, result, stage_all, true, 0);
}

export fn exp1744_prepare(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:],
    tensor: u64, bytes: i64, ids: const i64[:], result: i64[:], stage_all: bool, compact: bool, policy: i64) -> i64 {
    if compact && ids.len != 8 { return -1; }
    return ec_prepare_impl(groups, layers, words32, words64, tensor, bytes, ids, result, stage_all, compact, policy);
}
