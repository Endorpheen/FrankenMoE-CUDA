// Host span, byte count, device offset, host-current flag, last consumer.
const ACT_WORDS: i64 = 5;

extern "fg_cuda_activation_read" fn activation_read(data: const u8[:]) -> i64;
extern "fg_cuda_activation_read_address" fn activation_read_address(address: u64, bytes: i64) -> i64;
extern "fg_cuda_activation_read_f32" fn activation_read_f32(data: const f32[:]) -> i64;

fn activation_spans_overlap(a: u64, na: u64, b: u64, nb: u64) -> bool {
    if na == 0 || nb == 0 { return false; }
    return if a < b { b - a < na } else { a - b < nb };
}

export fn fg143_activation_lookup(ranges: const i64[:], host: u64, bytes: i64) -> i64 {
    if bytes <= 0 { return -1; }
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let base = ranges[at] as! u64;
        let size = ranges[at + 1];
        if size < bytes || host < base { continue; }
        let relative = host - base;
        if relative <= (size - bytes) as u64 {
            return ranges[at + 2] + (relative as i64);
        }
    }
    return -1;
}

fn activation_host_overlap(ranges: const i64[:], host: u64, bytes: i64, first: i64) -> i64 {
    if bytes <= 0 || first < 0 { return -1; }
    for i in first .. ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        if size <= 0 { continue; }
        let base = ranges[at] as! u64;
        if activation_spans_overlap(host, bytes as u64, base, size as u64) { return i; }
    }
    return -1;
}

export fn fg143_activation_overlap(ranges: const i64[:], host: u64, bytes: i64, first: i64) -> i64 {
    return activation_host_overlap(ranges, host, bytes, first);
}

// A fully covered record will be replaced by a disjoint full-output operation.
// Partially covered records must still be published to preserve their tails.
export fn fg143_activation_discard_overwritten(ranges: i64[:], host: u64, bytes: i64) -> u64 {
    if bytes <= 0 { return 0; }
    var skipped: u64 = 0;
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let base = ranges[at] as! u64;
        let size = ranges[at + 1];
        if size <= 0 || size > bytes || base < host || base - host > (bytes - size) as u64 { continue; }
        if ranges[at + 3] == 0 { skipped += size as u64; }
        ranges[at + 1] = 0;
    }
    return skipped;
}

export fn fg143_activation_expire(ranges: i64[:], step: i64) -> u64 {
    var skipped: u64 = 0;
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        let deadline = ranges[at + 4];
        if size <= 0 || deadline < 0 || deadline >= step { continue; }
        if ranges[at + 3] == 0 { skipped += size as u64; }
        ranges[at + 1] = 0;
    }
    return skipped;
}

// Find the first aligned gap between live allocations. -2 asks the adapter
// to publish live records before reusing the arena.
export fn fg143_activation_slot(ranges: const i64[:], capacity: i64, bytes: i64) -> i64 {
    if bytes <= 0 || capacity <= 0 || bytes > capacity { return -1; }
    var start: i64 = 0;
    while start <= capacity - bytes {
        var next = start;
        for i in ranges.len / ACT_WORDS {
            let at = i * ACT_WORDS;
            let size = ranges[at + 1];
            if size <= 0 { continue; }
            let offset = ranges[at + 2];
            if offset < 0 || offset > capacity || size > capacity - offset { return -1; }
            if offset < start + bytes && offset + size > start { next = high(next, offset + size); }
        }
        if next == start { return start; }
        let padding = (128 - next % 128) % 128;
        if padding > capacity - next { return -2; }
        start = next + padding;
    }
    return -2;
}

export fn fg143_activation_record_slot(ranges: const i64[:]) -> i64 {
    let count = ranges.len / ACT_WORDS;
    for i in count {
        if ranges[i * ACT_WORDS + 1] == 0 { return i; }
    }
    return count;
}

fn activation_device_overlap(ranges: const i64[:], offset: i64, bytes: i64, first: i64) -> i64 {
    if offset < 0 || bytes <= 0 || first < 0 { return -1; }
    for i in first .. ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        let base = ranges[at + 2];
        if size > 0 && base >= 0 && activation_spans_overlap(offset as u64, bytes as u64, base as u64, size as u64) {
            return i;
        }
    }
    return -1;
}

export fn fg143_activation_device_overlap(ranges: const i64[:], offset: i64, bytes: i64, first: i64) -> i64 {
    return activation_device_overlap(ranges, offset, bytes, first);
}

// Relocate an entire stale input on its last use if allocation would evict it.
export fn fg143_activation_last_input(ranges: const i64[:], host: u64, bytes: i64,
                                      step: i64, window: i64, output_bytes: i64) -> i64 {
    if step < 0 || bytes <= 0 || window < 0 || output_bytes <= 0 { return -1; }
    let found = activation_host_overlap(ranges, host, bytes, 0);
    if found < 0 { return -1; }
    let at = found * ACT_WORDS;
    let offset = ranges[at + 2];
    if offset < 0 || ranges[at] as! u64 != host || ranges[at + 1] != bytes ||
       ranges[at + 3] != 0 || ranges[at + 4] != step { return -1; }
    if !activation_spans_overlap(offset as u64, bytes as u64, window as u64, output_bytes as u64) {
        return -1;
    }
    return found;
}

// An eviction window costs one potential upload per victim plus a download
// when its host copy is stale. Only zero and aligned ends of allocations can
// start a cheaper window: moving right otherwise cannot remove a victim.
export fn fg143_activation_eviction_slot(ranges: const i64[:], capacity: i64, bytes: i64) -> i64 {
    if bytes <= 0 || capacity <= 0 || bytes > capacity || capacity > 0x3fffffffffffffff { return -1; }
    let count = ranges.len / ACT_WORDS;
    var best: i64 = -1;
    var best_cost = 2 * capacity + 1;
    for candidate in count + 1 {
        var offset: i64 = 0;
        if candidate != 0 {
            let at = (candidate - 1) * ACT_WORDS;
            let size = ranges[at + 1];
            let base = ranges[at + 2];
            if size <= 0 { continue; }
            if base < 0 || base > capacity || size > capacity - base { return -1; }
            let end = base + size;
            offset = end + (128 - end % 128) % 128;
            if offset > capacity - bytes { continue; }
        }
        var cost: i64 = 0;
        var first: i64 = 0;
        while cost < best_cost {
            let found = activation_device_overlap(ranges, offset, bytes, first);
            if found < 0 { break; }
            let at = found * ACT_WORDS;
            let size = ranges[at + 1];
            cost += if ranges[at + 3] == 0 { 2 * size } else { size };
            first = found + 1;
        }
        if cost < best_cost {
            best = offset;
            best_cost = cost;
        }
    }
    return best;
}
