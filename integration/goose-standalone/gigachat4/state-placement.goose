extern "fg_cuda_load_model_state" fn giga_cuda_state_load(ranges: i64[:], bytes: i64) -> i64;
extern "giga_state_span" fn giga_state_descriptor(tensor: u64, out: i64[:]) -> i64;

export fn giga_place_gdn_state(state: u64[:], kinds: i64[:], kv: bool) -> i64 {
    if state.len != 128 || kinds.len < 32 { return -1; }
    var ranges: i64[256] = [0; 256];
    var pair: i64[2] = [0; 2];
    var words: i64 = 0;
    var total: i64 = 0;
    for layer in 32 {
        let mla = kinds[layer] == 1;
        if mla && !kv { continue; }
        if (!mla && kinds[layer] != 0) || words > ranges.len - 8 { return -1; }
        for kind in 2 {
            if giga_state_descriptor(state[layer * 4 + kind], pair) == 0 { return -1; }
            ranges[words] = pair[0];
            ranges[words + 1] = pair[1];
            ranges[words + 2] = total;
            ranges[words + 3] = if mla { 0 } else { 2 - kind };
            total += pair[1];
            words += 4;
        }
    }
    if words != (if kv { 256 } else { 192 }) || total < 52690944 { return -1; }
    return giga_cuda_state_load(ranges[0 .. words], total);
}
