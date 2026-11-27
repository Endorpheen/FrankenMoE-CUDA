const GIGA_MODEL_WORDS: i64 = 1181;
const GIGA_MODEL_BYTES: u64 = 9448;
const GIGA_MODEL_CONFIG: u64 = 8840;
const GIGA_MODEL_MLA: u64 = 8;
const GIGA_TENSOR_OPERATION_OFFSET: u64 = 80;
const GIGA_TENSOR_PARAMETERS_OFFSET: u64 = 84;
const GIGA_TENSOR_SOURCES_OFFSET: u64 = 152;

extern "ggml_new_tensor" fn giga_new_tensor(ctx: u64, kind: i32, dims: i32, shape: u64) -> u64;
extern fn fg_cuda_host_getenv(name: u64) -> u64;
extern fn fg_cuda_host_env_equal(value: u64, expected: u64) -> bool;

export fn giga_abi_giga_unbind_model(bindings: u64) {
    if bindings == 0 { return; }
    let mla = giga_tensor_word(bindings + GIGA_MODEL_MLA);
    let descriptors = giga_tensor_word(bindings);
    if mla != 0 { giga_release_context(mla); }
    if descriptors != 0 { giga_release_context(descriptors); }
    giga_zero_raw(unsafe_c_ptr<c_ptr<void>>(bindings), 0, GIGA_MODEL_BYTES);
}

export fn giga_open_descriptors() -> u64 {
    return fg143_arena_init(1036 * giga_request_tensor_overhead(), 0, true);
}

export fn giga_create_descriptor(ctx: u64, data: u64, available: u64, metadata: const i64[:]) -> u64 {
    if metadata.len < 2 { return 0; }
    let kind = metadata[0];
    let dims = metadata[1];
    if kind < 0 || kind >= 43 || dims < 1 || dims > 4 || metadata.len < dims + 2 { return 0; }
    let shape = giga_pointer_address(unsafe_c_address<c_ptr<const void>>(metadata[2]));
    let tensor = giga_new_tensor(ctx, kind as! i32, dims as! i32, shape);
    if giga_tensor_bytes(tensor) > available { return 0; }
    giga_write_word(tensor + GIGA_TENSOR_DATA_OFFSET, data);
    return tensor;
}

export fn giga_abi_giga_bind_model(out: u64, mapping: u64, bytes: u64) -> i32 {
    if out == 0 || giga_tensor_word(out) != 0 || mapping == 0 ||
        bytes < 24 || bytes > 0x7fffffffffffffff { return -1; }
    let limit: u64 = 64 * 1024 * 1024;
    let header_bytes = (if bytes < limit { bytes } else { limit }) as! i64;
    var pending: u64[GIGA_MODEL_WORDS];
    let base = giga_pointer_address(unsafe_c_address<c_ptr<const void>>(pending[0]));
    let config = base + GIGA_MODEL_CONFIG;
    var flags: i64[2];
    var context: u64[1];
    let status = giga_parse_bound_model(giga_borrow_bytes(mapping, header_bytes), mapping, bytes as! i64,
        giga_request_i64(base + GIGA_BINDINGS_KINDS, 33), giga_request_i64(base + GIGA_MODEL_OFFSETS, 34),
        giga_request_words(base + GIGA_MODEL_WEIGHTS, 1036), giga_borrow_floats(config, 5),
        giga_borrow_floats(config + GIGA_CONFIG_ROPE, 6), giga_borrow_floats(config + GIGA_CONFIG_ROUTED, 32),
        giga_borrow_floats(config + GIGA_CONFIG_SHARED, 33), flags, context);
    if status != 0 { return status; }
    pending[0] = context[0];
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(config + GIGA_CONFIG_INDEX_ROPE),
        unsafe_c_ptr<c_ptr<const void>>(config + GIGA_CONFIG_ROPE), 24);
    let normalize: u64 = if flags[1] != 0 { 1 } else { 0 };
    giga_write_word(config + GIGA_CONFIG_ORIGINAL_CONTEXT, ((flags[0] as! u32) as! u64) | (normalize << 32));
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(out), unsafe_c_address<c_ptr<const void>>(pending[0]), GIGA_MODEL_BYTES);
    return 0;
}

export fn giga_abi_giga_expand_mla_weights(bindings: u64) -> i32 {
    if bindings == 0 || giga_tensor_word(bindings + GIGA_MODEL_MLA) != 0 { return -1; }
    let bytes = 9 * (giga_request_tensor_overhead() + 128 * 512 * 32 * 4);
    let ctx = fg143_arena_init(bytes, 0, false);
    if ctx == 0 { return -1; }
    let status = giga_expand_mla_descriptors(ctx, giga_request_i64(bindings + GIGA_BINDINGS_KINDS, 33),
        giga_request_i64(bindings + GIGA_MODEL_OFFSETS, 34), giga_request_words(bindings + GIGA_MODEL_WEIGHTS, 1036));
    if status != 0 {
        giga_release_context(ctx);
        return -1;
    }
    giga_write_word(bindings + GIGA_MODEL_MLA, ctx);
    return 0;
}

fn giga_env_value(name: const u8[:]) -> u64 {
    var key: u8[32];
    if name.len >= key.len { return 0; }
    for i in name.len { key[i] = name[i]; }
    return fg_cuda_host_getenv(giga_pointer_address(unsafe_c_address<c_ptr<const void>>(key[0])));
}

fn giga_env_is(value: u64, expected: const u8[:]) -> bool {
    var key: u8[16];
    if expected.len >= key.len { return false; }
    for i in expected.len { key[i] = expected[i]; }
    return fg_cuda_host_env_equal(value, giga_pointer_address(unsafe_c_address<c_ptr<const void>>(key[0])));
}

fn giga_setting_is(name: const u8[:], expected: const u8[:]) -> bool {
    return giga_env_is(giga_env_value(name), expected);
}

fn giga_env_disabled(name: const u8[:]) -> bool { return giga_setting_is(name, "0"); }

export fn giga_dsa_mask_node(ctx: u64, original: u64, selected: u64) -> u64 {
    if giga_env_disabled("GOOSE_GIGA_DSA_MASK") { return 0; }
    let tensor = giga_request_tensor2(ctx, 1, giga_tensor_extent(original, 0) as! i64,
                                      giga_tensor_extent(original, 1) as! i64);
    var operation = giga_request_i32(tensor + GIGA_TENSOR_OPERATION_OFFSET, 1);
    var params = giga_request_i32(tensor + GIGA_TENSOR_PARAMETERS_OFFSET, 1);
    var sources = giga_request_words(tensor + GIGA_TENSOR_SOURCES_OFFSET, 2);
    operation[0] = 95;
    sources[0] = original;
    sources[1] = selected;
    params[0] = 0x4453414d;
    return tensor;
}

export fn giga_dsa_prefix_enabled() -> bool {
    return !giga_env_disabled("GOOSE_GIGA_DSA_PREFIX") && !giga_env_disabled("GOOSE_GIGA_DSA_MASK");
}
