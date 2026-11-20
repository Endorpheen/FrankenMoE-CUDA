// Runtime layout: each device slot stores the up, gate and down planes of
// one expert. Layers with the same plane sizes and expert count share an SLRU.
const EC_GROUP: i64 = 9;
const EC_LAYER: i64 = 4;
const EC_MAX_BYTES: i64 = 4992 * 1024 * 1024;

extern "fg_cuda_load_experts" fn ec_load(groups: const i64[:], layers: const i64[:],
                                         device_bytes: i64, words32: i64, words64: i64) -> i64;
extern "fg_cuda_expert_upload" fn ec_upload(offset: i64, source: u64, bytes: i64) -> i64;

fn ec_plane_bytes(t: i64, owner: const i64[:], experts: i64) -> i64 {
    if t == 0 || experts <= 0 || ne(t, 2) != experts || ne(t, 3) != 1 { return 0; }
    let kind = prop(t, 0);
    if kind != 12 && kind != 14 { return 0; }
    let bytes = (qa_get(t, 1, 1) as i64) * ne(t, 1);
    let base = owner[1] as! u64;
    let addr = qa_get(t, 4, 0);
    let file_bytes = owner[2] as u64;
    if bytes <= 0 || addr < base || addr - base >= file_bytes ||
       qa_get(t, 1, 2) != (bytes as u64) || bytes > ((file_bytes - (addr - base)) as i64) / experts {
        return 0;
    }
    return bytes;
}

export fn qr_cuda_experts(owner: const i64[:], records: const i64[:], head: i64) -> i64 {
    if owner.len < 13 || owner[0] != 9191 || owner[5] <= 0 ||
       records.len / 32 < owner[5] || head == 0 { return -1; }
    let free = mr_cuda_free();
    if free <= 0 { return free; }
    let reserve = mr_cuda_reserve(owner, head);
    if free <= reserve { return 0; }
    let available = free - reserve;
    let budget = if available < EC_MAX_BYTES { available } else { EC_MAX_BYTES };

    let n = owner[5];
    var groups: i64[>..] = [];
    var layers: i64[>..] = [];
    groups.resize(n * EC_GROUP, 0);
    layers.resize(n * EC_LAYER, 0);
    var found: i64 = 0;
    for layer in n {
        let at = layer * EC_LAYER;
        let row = layer * 32;
        layers[at] = -1;
        let up = records[row + 13];
        if up == 0 { continue; }
        let experts = ne(up, 2);
        let sizes: i64[3] = [ec_plane_bytes(up, owner, experts),
            ec_plane_bytes(records[row + 14], owner, experts),
            ec_plane_bytes(records[row + 15], owner, experts)];
        if sizes[0] == 0 || sizes[1] == 0 || sizes[2] == 0 { continue; }
        var group: i64 = 0;
        while group < found {
            let g = group * EC_GROUP;
            if groups[g] == sizes[0] && groups[g + 1] == sizes[1] &&
               groups[g + 2] == sizes[2] && groups[g + 3] == experts { break; }
            group += 1;
        }
        let g = group * EC_GROUP;
        if group == found {
            for p in 3 { groups[g + p] = sizes[p]; }
            groups[g + 3] = experts;
            found += 1;
        }
        groups[g + 4] += 1;
        layers[at] = group;
        for p in 3 { layers[at + 1 + p] = qa_get(records[row + 13 + p], 4, 0) as! i64; }
    }
    if found == 0 { return 0; }

    var used: i64 = 0;
    var words32: i64 = 0;
    var words64 = n;
    let share = budget / found;
    for j in found {
        let g = j * EC_GROUP;
        let span = groups[g] + groups[g + 1] + groups[g + 2];
        let experts = groups[g + 3];
        let limit = experts * groups[g + 4];
        let slots = if share / span < limit { share / span } else { limit };
        // Keep every distinct route in a full prefill batch simultaneously.
        if slots < experts { continue; }
        groups[g + 5] = slots;
        groups[g + 6] = used;
        used += slots * span;
        words32 += limit;
    }
    if used == 0 { return 0; }
    for j in found {
        let g = j * EC_GROUP;
        let slots = groups[g + 5];
        if slots == 0 { continue; }
        let experts = groups[g + 3];
        groups[g + 7] = words32;
        groups[g + 8] = words64;
        words32 += 4 * slots + 4 + 5 * experts;
        words64 += experts + 5;
    }
    return ec_load(groups[0 .. found * EC_GROUP], layers[..], used, words32, words64);
}

// The i32 arena starts with a slot map per admitted layer. The group state
// follows in the order passed to the existing SLRU. words64 starts with the
// addresses of the layer maps, then seen and scalar counters for each group.
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

// Resolve one indexed matrix into resident device offsets, including any
// fills. All effects run under the CUDA driver's current-context lock.
export fn ec_prepare(groups: const i64[:], layers: const i64[:], words32: u64, words64: i64[:],
                     tensor: u64, bytes: i64, heads: const i64[:], result: i64[:]) -> i64 {
    let n = layers.len / EC_LAYER;
    for il in n {
        let r = il * EC_LAYER;
        let v = layers[r];
        if v < 0 || groups[v * EC_GROUP + 5] == 0 { continue; }
        let g = v * EC_GROUP;
        let experts = groups[g + 3];
        let slots = groups[g + 5];
        if experts != heads.len { continue; }
        var projection: i64 = -1;
        for p in 3 {
            if (layers[r + 1 + p] as! u64) == tensor && groups[g + p] == bytes { projection = p; }
        }
        if projection < 0 { continue; }
        if result.len < experts { return -1; }

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
        for e in experts {
            result[e] = -1;
            if heads[e] >= 0 { sl_write(active, count, e); count += 1; }
        }
        let fills = slru_plan_impl(keys, prev, next, prot, head, tail, words64[start .. start + experts], map,
            active, count, il, experts, slots, words64[start + experts .. start + experts + 5],
            uniq, fills_e, fills_s, evicted, words64[0 .. n], n);
        if fills < 0 { return -1; }
        let span = groups[g] + groups[g + 1] + groups[g + 2];
        for i in fills {
            let e = sl_read(fills_e, i);
            var offset = groups[g + 6] + span * sl_read(fills_s, i);
            for p in 3 {
                let part = groups[g + p];
                let from = (layers[r + 1 + p] as! u64) + ((part * e) as u64);
                if ec_upload(offset, from, part) != 0 { return -1; }
                offset += part;
            }
        }
        var plane: i64 = 0;
        for p in projection { plane += groups[g + p]; }
        for e in experts {
            if heads[e] >= 0 {
                let slot = sl_read(map, e);
                if slot < 0 || slot >= slots { return -1; }
                result[e] = groups[g + 6] + span * slot + plane;
            }
        }
        return 1;
    }
    return 0;
}
