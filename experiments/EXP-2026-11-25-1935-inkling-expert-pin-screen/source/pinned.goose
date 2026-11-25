const EC_GROUP: i64 = 9;
const EC_LAYER: i64 = 4;
extern "fg_cuda_expert_pin_info" fn ec_pin_info(info: i64[:]) -> i64;
extern "fg_cuda_expert_pin_register" fn ec_pin_register(spans: const i64[:], bytes: i64);
extern "fg_cuda_expert_pin_reason" fn ec_pin_reason(reason: i64, bytes: i64, available: i64);
extern "dw_ram_residency" fn dw_ram_residency(mapping: u64, bytes: i64, page: i64, state: i64[:]) -> i64;

fn dw_sort_pairs(a: i64[:], b: i64[:], n: i64) {
    var i: i64 = 1;

    while i < n {
        let ka: i64 = a[i];
        let kb: i64 = b[i];
        var j: i64 = i;

        while j > 0 && (a[j - 1] > ka || (a[j - 1] == ka && b[j - 1] > kb)) {
            a[j] = a[j - 1];
            b[j] = b[j - 1];
            j = j - 1;
        }
        a[j] = ka;
        b[j] = kb;
        i = i + 1;
    }
}

export fn i2_pin_experts(groups: const i64[:], layers: const i64[:]) {
    var info: i64[4] = [0; 4];
    if ec_pin_info(info[..]) != 1 { return; }
    let page = info[0];
    let available = info[2];
    if page <= 0 || (page & (page - 1)) != 0 || info[1] <= 0 { return; }

    let count = layers.len / EC_LAYER;
    var begin: i64[>..] = [];
    var end: i64[>..] = [];
    begin.resize(3 * count, 0);
    end.resize(3 * count, 0);
    var n: i64 = 0;
    for layer in count {
        let at = layer * EC_LAYER;
        let group = layers[at];
        if group < 0 { continue; }
        let g = group * EC_GROUP;
        for projection in 3 {
            let address = layers[at + 1 + projection];
            let bytes = groups[g + projection] * groups[g + 3];
            if address == 0 && bytes == 0 { continue; }
            if address <= 0 || bytes <= 0 || address > 9223372036854775807 - bytes - (page - 1) { return; }
            begin[n] = address / page * page;
            end[n] = (address + bytes + page - 1) / page * page;
            n += 1;
        }
    }
    dw_sort_pairs(begin[..], end[..], n);
    var used: i64 = 0;
    for i in n {
        if used > 0 && begin[i] <= end[used - 1] {
            if end[i] > end[used - 1] { end[used - 1] = end[i]; }
        } else {
            begin[used] = begin[i];
            end[used] = end[i];
            used += 1;
        }
    }
    var spans: i64[>..] = [];
    spans.resize(2 * used, 0);
    if used == 0 { return; }
    var all_bytes: i64 = 0;
    for i in used {
        let bytes = end[i] - begin[i];
        if bytes > 9223372036854775807 - all_bytes { return; }
        all_bytes += bytes;
    }
    var total: i64 = 0;
    var pinned: i64 = 0;
    let share = info[3] / used;
    for i in used {
        let bytes = end[i] - begin[i];
        // Admit complete spans so an expert transfer never crosses a pin boundary.
        if info[3] < all_bytes && bytes > share * (i + 1) - total { continue; }
        total += bytes;
        spans[2 * pinned] = begin[i];
        spans[2 * pinned + 1] = bytes;
        pinned += 1;
    }
    if total <= 0 { return; }
    const headroom: i64 = 8 * 1024 * 1024 * 1024;
    if total > info[1] / 2 || available < total || available - total < headroom {
        ec_pin_reason(1, total, available);
        return;
    }
    var resident: i64[4] = [0; 4];
    for i in pinned {
        let bytes = spans[2 * i + 1];
        if dw_ram_residency(spans[2 * i] as! u64, bytes, page, resident[..]) != 0 || resident[1] != bytes {
            ec_pin_reason(2, total, available);
            return;
        }
    }
    ec_pin_register(spans[0 .. 2 * pinned], total);
}
