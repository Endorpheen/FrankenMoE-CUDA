extern "giga_open_descriptors" fn giga_open_bound_descriptors() -> u64;
extern "giga_create_descriptor" fn giga_create_bound_descriptor(ctx: u64, data: u64,
        available: u64, metadata: const i64[:]) -> u64;
extern "ggml_free" fn giga_free_bound_descriptors(ctx: u64);

export fn giga_parse_bound_model(header: const u8[:], mapping: u64, bytes: i64,
        kinds: i64[:], offsets: i64[:], weights: u64[:],
        tuning: f32[:], rope: f32[:], routed: f32[:], shared: f32[:],
        flags: i64[:], context: u64[:]) -> i32 {
    var state: i64[8] = [0; 8];
    var kv: i64[4096] = [0; 4096];
    var tensors: i64[24576] = [0; 24576];
    var kv_slots: i64[1024] = [0; 1024];
    var slots: i64[4096] = [0; 4096];
    var info: i64[12] = [0; 12];
    var ids: i64[1036] = [0; 1036];
    if gr_parse(header, bytes, state[..], kv[..], tensors[..], kv_slots[..], slots[..]) != 0 ||
       giga_read_layout(header, state[..], kv[..], kv_slots[..], info[..], kinds) != 0 ||
       giga_bind_tensors(header, state[..], tensors[..], slots[..], kinds, ids[..], offsets) != 1036 {
        return -2;
    }
    var limits: f32[65] = [0.0; 65];
    if giga_read_config(header, state[..], kv[..], kv_slots[..], tuning, rope, limits[..], flags) != 0 { return -5; }
    for i in 32 { routed[i] = limits[i]; }
    for i in 33 { shared[i] = limits[32 + i]; }

    let ctx = giga_open_bound_descriptors();
    if ctx == 0 { return -3; }
    var descriptor: i64[9] = [0; 9];
    for i in 1036 {
        if gr_tensor_info(state[..], tensors[..], ids[i], descriptor[..]) != 0 ||
           descriptor[8] < 0 || descriptor[8] >= bytes {
            giga_free_bound_descriptors(ctx);
            return -4;
        }
        let offset = descriptor[8];
        let tensor = giga_create_bound_descriptor(ctx, mapping + (offset as u64),
                                                  (bytes - offset) as u64, descriptor[..]);
        if tensor == 0 {
            giga_free_bound_descriptors(ctx);
            return -4;
        }
        weights[i] = tensor;
    }
    context[0] = ctx;
    return 0;
}

extern "giga_expand_mla_descriptor" fn giga_expand_bound_mla(ctx: u64, source: u64) -> u64;

export fn giga_expand_mla_descriptors(ctx: u64, kinds: const i64[:], offsets: const i64[:],
        weights: u64[:]) -> i32 {
    if ctx == 0 || kinds.len != 33 || offsets.len < 33 { return -1; }
    var expanded: u64[33] = [0; 33];
    for layer in 33 {
        if kinds[layer] == 0 { continue; }
        let offset = offsets[layer];
        if offset < 0 || offset >= weights.len - 16 { return -1; }
        let target = giga_expand_bound_mla(ctx, weights[offset + 16]);
        if target == 0 { return -1; }
        expanded[layer] = target;
    }
    for layer in 33 {
        if expanded[layer] != 0 { weights[offsets[layer] + 16] = expanded[layer]; }
    }
    return 0;
}
