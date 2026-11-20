// Host span, byte count, offset in the CUDA arena, host-current flag.
const ACT_WORDS: i64 = 4;

extern "fg_cuda_activation_read" fn activation_read(data: const u8[:]) -> i64;
extern "fg_cuda_activation_read_address" fn activation_read_address(address: u64, bytes: i64) -> i64;
extern "fg_cuda_activation_read_f32" fn activation_read_f32(data: const f32[:]) -> i64;

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

export fn fg143_activation_overlap(ranges: const i64[:], host: u64, bytes: i64, first: i64) -> i64 {
    if bytes <= 0 || first < 0 { return -1; }
    for i in first .. ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        if size <= 0 { continue; }
        let base = ranges[at] as! u64;
        let overlaps = if host < base { base - host < bytes as u64 } else { host - base < size as u64 };
        if overlaps { return i; }
    }
    return -1;
}

// Reserve aligned space. -2 asks the adapter to publish old results before
// reusing the arena. A tensor larger than the arena uses the normal buffer.
export fn fg143_activation_slot(used: i64, capacity: i64, bytes: i64) -> i64 {
    if bytes <= 0 || capacity <= 0 || bytes > capacity || used < 0 || used > capacity { return -1; }
    let padding = (128 - used % 128) % 128;
    if padding > capacity - used || bytes > capacity - used - padding { return -2; }
    return used + padding;
}
