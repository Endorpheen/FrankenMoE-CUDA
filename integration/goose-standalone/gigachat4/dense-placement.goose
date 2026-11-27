extern "fg_cuda_free_bytes" fn giga_cuda_free() -> i64;
extern "fg_cuda_load_dense" fn giga_cuda_load(ranges: i64[:], bytes: i64) -> i64;
extern "giga_dense_span" fn giga_weight_span(tensor: u64, out: i64[:]) -> i64;

export fn giga_place_dense(weights: u64[:], reserve_bytes: i64,
                            offsets: i64[:], kinds: i64[:], mla: bool) -> i64 {
    let available = giga_cuda_free() - reserve_bytes;
    if weights.len != 1036 || reserve_bytes < 0 || available < 1 { return 0; }
    if offsets.len < 34 || kinds.len < 33 { return -1; }
    var ranges: i64[3972] = [0; 3972];
    var pair: i64[2] = [0; 2];
    var used: i64 = 0;
    var words: i64 = 0;
    for i in weights.len {
        let slot = if i == 0 { 4 } else if i == 4 { 0 } else { i };
        if giga_weight_span(weights[slot], pair) == 0 { continue; }
        let offset = used + (16 - used % 16) % 16;
        if offset > available || pair[1] > available - offset { continue; }
        ranges[words] = pair[0];
        ranges[words + 1] = pair[1];
        ranges[words + 2] = offset;
        used = offset + pair[1];
        words += 3;
    }
    if mla {
        let plane: i64 = 128 * 512 * 4;
        for layer in 33 {
            if kinds[layer] == 0 { continue; }
            let slot = offsets[layer] + 16;
            if slot < 0 || slot >= weights.len ||
               giga_state_descriptor(weights[slot], pair) == 0 || pair[1] != 32 * plane ||
               pair[1] > available - used { return -1; }
            for head in 32 {
                ranges[words] = pair[0] + head * plane;
                ranges[words + 1] = plane;
                ranges[words + 2] = used;
                used += plane;
                words += 3;
            }
        }
    }
    if words == 0 { return 0; }
    return giga_cuda_load(ranges[0 .. words], used);
}
