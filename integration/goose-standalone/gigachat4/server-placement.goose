extern "fg_cuda_giga_kv_configure" fn giga_server_configure_kv(capacity: i64) -> bool;

export fn giga_server_place_weights(weights: u64[:], offsets: i64[:], kinds: i64[:],
        tuning: f32[:], mapping: u64, bytes: i64, dsa: bool, use_mla: bool,
        dense: bool, experts: bool) -> i32 {
    let gib: i64 = 1024 * 1024 * 1024;
    if dsa && giga_cuda_free() > 0 { tuning[5] = 1.0; }
    let mla = use_mla && giga_cuda_free() >= 6 * gib;
    if mla && giga_server_expand_mla() != 0 { return -1; }
    if mla || dense {
        let resident = giga_place_dense(weights, 3 * gib, offsets, kinds, mla);
        if resident < 0 { return -1; }
        giga_server_placement_trace(0, resident, 0);
    }
    if experts {
        var memory: i64[2] = [0; 2];
        let cached = giga_place_experts(weights, offsets, kinds, mapping, bytes, 3 * gib, 4 * gib, memory[..]);
        giga_server_placement_trace(1, cached, memory[0]);
        if cached < 0 { return -1; }
    }
    return 0;
}

export fn giga_server_place_state(state: u64[:], kinds: i64[:], capacity: i64,
        enabled: bool) -> i32 {
    let resident_kv = giga_server_configure_kv(capacity);
    if !enabled || giga_cuda_free() <= 0 { return 0; }
    let resident = giga_place_gdn_state(state, kinds, resident_kv);
    giga_server_placement_trace(2, resident, 0);
    return if resident > 0 { 1 } else { -1 };
}
