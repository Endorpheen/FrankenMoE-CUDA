extern "fg_cuda_giga_partial_pin" fn giga_partial_pin() -> bool;
extern "qr_cuda_experts" fn giga_shared_expert_cache(owner: i64[:], records: i64[:],
                                                     head: i64, requested: i64, capacity: i64[:]) -> i64;

export fn giga_place_experts(weights: u64[:], offsets: i64[:], kinds: i64[:],
                             mapping: u64, length: i64, reserve: i64, requested: i64,
                             capacity: i64[:]) -> i64 {
    if weights.len != 1036 || offsets.len < 34 || kinds.len < 33 || mapping == 0 ||
       length < 1 || reserve < 0 || requested < 0 || capacity.len != 2 { return -1; }
    var owner: i64[14] = [0; 14];
    var records: i64[1024] = [0; 1024];
    owner[0] = 9191;
    owner[1] = mapping as! i64;
    owner[2] = length;
    owner[5] = 32;
    owner[12] = reserve;
    owner[13] = if giga_partial_pin() { 1 } else { 0 };
    for layer in 1 .. 32 {
        if kinds[layer] != 0 && kinds[layer] != 1 { return -1; }
        let ffn = offsets[layer] + (if kinds[layer] == 0 { 21 } else { 29 });
        if ffn < 0 || ffn + 8 != offsets[layer + 1] || ffn + 8 > weights.len { return -1; }
        let row = layer * 32;
        records[row + 13] = weights[ffn + 3] as! i64;
        records[row + 14] = weights[ffn + 2] as! i64;
        records[row + 15] = weights[ffn + 4] as! i64;
    }
    return giga_shared_expert_cache(owner, records, weights[4] as! i64, requested, capacity);
}
