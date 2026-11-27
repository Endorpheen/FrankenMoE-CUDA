extern "ggml_tensor_overhead" fn giga_request_tensor_overhead() -> u64;
extern "ggml_new_tensor_2d" fn giga_request_tensor2(ctx: u64, kind: i32, a: i64, b: i64) -> u64;
extern "ggml_new_tensor_3d" fn giga_request_tensor3(ctx: u64, kind: i32, a: i64, b: i64, c: i64) -> u64;
extern "ggml_get_first_tensor" fn giga_request_first(ctx: u64) -> u64;
extern "ggml_get_next_tensor" fn giga_request_next(ctx: u64, tensor: u64) -> u64;
extern "ggml_set_zero" fn giga_request_zero(tensor: u64) -> u64;
extern "fg_cuda_reset_model_state" fn giga_request_reset_device() -> i64;

export fn giga_request_bytes(capacity: i64) -> u64 {
    if capacity < 1 || capacity > 1048576 { return 0; }
    return 66 * giga_request_tensor_overhead() +
           24 * (3 * 8192 + 128 * 128 * 32) * 4 +
           (capacity as u64) * 9 * (576 + 128) * 2;
}

fn giga_allocate_mla(ctx: u64, capacity: i64, state: u64[:]) {
    state[0] = giga_request_tensor2(ctx, 1, 576, capacity);
    state[1] = giga_request_tensor2(ctx, 1, 128, capacity);
}

export fn giga_request_allocate_state(ctx: u64, capacity: i64,
        kinds: const i64[:], state: u64[:], draft: u64[:]) -> i32 {
    if ctx == 0 || kinds.len != 32 || state.len != 128 || draft.len != 3 { return -1; }
    for layer in 32 {
        if kinds[layer] != 0 && kinds[layer] != 1 { return -1; }
    }
    for layer in 32 {
        let slot = layer * 4;
        if kinds[layer] == 0 {
            state[slot] = giga_request_tensor3(ctx, 0, 3, 8192, 1);
            state[slot + 1] = giga_tensor4(ctx, 0, 128, 128, 32, 1);
            state[slot + 2] = state[slot];
            state[slot + 3] = state[slot + 1];
        } else {
            giga_allocate_mla(ctx, capacity, state[slot..slot + 2]);
        }
    }
    giga_allocate_mla(ctx, capacity, draft);
    return 0;
}

export fn giga_request_clear_state(ctx: u64, device_state: bool) -> i64 {
    var tensor = giga_request_first(ctx);
    while tensor != 0 {
        let zeroed = giga_request_zero(tensor);
        tensor = giga_request_next(ctx, zeroed);
    }
    if device_state && giga_request_reset_device() != 0 { return -1; }
    return 0;
}
