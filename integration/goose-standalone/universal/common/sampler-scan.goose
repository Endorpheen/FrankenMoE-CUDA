
simd fn probability_scan_mask(values: const f32[:], threshold: f32) -> u32 {
    if values.len != 8 { return 0; }
    var matches: u32[8];
    for i in 8 { matches[i] = if values[i] > threshold { 1 } else { 0 }; }
    return matches[0] | (matches[1] << 1) | (matches[2] << 2) | (matches[3] << 3) |
        (matches[4] << 4) | (matches[5] << 5) | (matches[6] << 6) | (matches[7] << 7);
}

fn probability_scan_above(values: const f32[:], start: i64, end: i64, threshold: f32) -> i64 {
    var at = start;
    while end - at >= 8 {
        var mask = probability_scan_mask(values[at .. at + 8], threshold);
        if mask != 0 {
            while (mask & 1) == 0 { mask >>= 1; at += 1; }
            return at;
        }
        at += 8;
    }
    while at < end {
        if values[at] > threshold { return at; }
        at += 1;
    }
    return end;
}

fn probability_scan_checked(values: const f32[:], start: i64, end: i64, threshold: f32) -> i64 {
    if start < 0 || end < start || end > values.len || unsafe_c_slice_data<c_ptr<const void>>(values) == null {
        return end;
    }
    return probability_scan_above(values, start, end, threshold);
}

export fn fg_q35_topk_first_above(values: const f32[:], start: i64, end: i64, threshold: f32) -> i64 {
    return probability_scan_checked(values, start, end, threshold);
}

export fn fg_i2_sampler_first_above(values: const f32[:], start: i64, end: i64, threshold: f32) -> i64 {
    return probability_scan_checked(values, start, end, threshold);
}

export fn fg_i2_sampler_init_ids(ids: i64[:], count: i64) -> i64 {
    if count < 0 || count > ids.len || unsafe_c_slice_data<c_ptr<void>>(ids) == null { return -1; }
    for i in count { ids[i] = i; }
    return 0;
}
