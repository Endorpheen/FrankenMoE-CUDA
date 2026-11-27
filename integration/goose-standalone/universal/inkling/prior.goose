extern "slru_ld_i32" fn old_i32(address: u64, byte: i64) -> i64;
extern "slru_st_i32" fn set_i32(address: u64, byte: i64, value: i64);

fn rank_before(a: i64, b: i64, frequency: const i64[:], visits: const i64[:], experts: i64) -> bool {
    if a < 0 { return b >= 0 || a < b; }
    if b < 0 { return false; }
    let lhs = frequency[a] * visits[b / experts];
    let rhs = frequency[b] * visits[a / experts];
    return lhs < rhs || (lhs == rhs && a < b);
}

// Reorder resident slots in-place. Slot keys and the layer maps never move.
export fn i2_slru_prefill_prior(keys: u64, prev: u64, next: u64, prot: u64, head: u64, tail: u64,
    frequency: const i64[:], visits: const i64[:], entries: i64[:], scratch: i64[:],
    protected_count: i64[:], experts: i64) -> i64 {
    let count = entries.len;
    if experts <= 0 || count < 2 || count > 3072 || scratch.len != count ||
        protected_count.len != 1 || frequency.len != visits.len * experts {
        return -1;
    }
    for i in count {
        let key = old_i32(keys, 4 * i);
        if key >= frequency.len || (key >= 0 && visits[key / experts] <= 0) { return -1; }
        entries[i] = i;
    }
    var width: i64 = 1;
    while width < count {
        var start: i64 = 0;
        while start < count {
            let mem = start + width;
            let middle = if mem < count { mem } else { count };
            let end0 = middle + width;
            let end = if end0 < count { end0 } else { count };
            var left = start;
            var right = middle;
            var at = start;
            while at < end {
                let take_left = right == end || (left < middle &&
                    rank_before(old_i32(keys, 4 * entries[left]),
                        old_i32(keys, 4 * entries[right]), frequency, visits, experts));
                if take_left {
                    scratch[at] = entries[left];
                    left += 1;
                } else {
                    scratch[at] = entries[right];
                    right += 1;
                }
                at += 1;
            }
            start = end;
        }
        for i in count { entries[i] = scratch[i]; }
        width *= 2;
    }

    let split = count - count / 2;
    for i in count {
        let slot = entries[i];
        let segment: i64 = if i < split { 0 } else { 1 };
        let first = i == 0 || i == split;
        let last = i + 1 == count || i + 1 == split;
        set_i32(prot, 4 * slot, segment);
        set_i32(prev, 4 * slot, if first { -1 } else { entries[i - 1] });
        set_i32(next, 4 * slot, if last { -1 } else { entries[i + 1] });
        if first { set_i32(head, 4 * segment, slot); }
        if last { set_i32(tail, 4 * segment, slot); }
    }
    protected_count[0] = count / 2;
    return 0;
}
