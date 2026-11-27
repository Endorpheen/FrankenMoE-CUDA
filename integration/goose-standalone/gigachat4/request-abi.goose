const GIGA_REQUEST_BYTES: u64 = 1088;
const GIGA_REQUEST_STATE: u64 = 8;
const GIGA_REQUEST_DRAFT: u64 = 1032;
const GIGA_REQUEST_CAPACITY: u64 = 1056;
const GIGA_REQUEST_COMMITTED: u64 = 1064;
const GIGA_REQUEST_DRAFT_COMMITTED: u64 = 1072;
const GIGA_REQUEST_DEVICE_STATE: u64 = 1080;
const GIGA_BINDINGS_KINDS: u64 = 8576;

const GIGA_STEP_BYTES: u64 = 80;
const GIGA_STEP_BASE: u64 = 40;
const GIGA_STEP_COUNT: u64 = 48;
const GIGA_STEP_OUTPUT_COUNT: u64 = 56;
const GIGA_STEP_CELLS: u64 = 64;
const GIGA_STEP_FLAGS: u64 = 72;

extern fn fg143_arena_init(bytes: u64, buffer: u64, no_alloc: bool) -> u64;
extern "ggml_free" fn giga_release_context(context: u64);
extern "memset" fn giga_zero_raw(dst: c_ptr<void>, value: i32, bytes: u64) -> c_ptr<void>;
extern "fg_cuda_host_view_u64" fn giga_request_words(address: u64, count: i64) -> u64[:];
extern "fg_cuda_host_view_i64" fn giga_request_i64(address: u64, count: i64) -> i64[:];
extern "fg_cuda_host_view_i32" fn giga_request_i32(address: u64, count: i64) -> i32[:];
extern "fg_cuda_host_view_u16" fn giga_request_u16(address: u64, count: i64) -> u16[:];

fn giga_write_word(address: u64, value: u64) {
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 8);
}

export fn giga_abi_giga_request_free(request: u64) {
    if request == 0 { return; }
    let context = giga_tensor_word(request);
    if context != 0 { giga_release_context(context); }
    giga_zero_raw(unsafe_c_ptr<c_ptr<void>>(request), 0, GIGA_REQUEST_BYTES);
}

export fn giga_abi_giga_request_init(request: u64, model: u64, capacity: i64) -> i32 {
    if request == 0 || model == 0 || giga_tensor_word(request) != 0 || giga_tensor_word(model) == 0 {
        return -1;
    }
    let bytes = giga_request_bytes(capacity);
    if bytes == 0 { return -1; }
    let context = fg143_arena_init(bytes, 0, false);
    if context == 0 { return -2; }
    let kinds = giga_request_i64(model + GIGA_BINDINGS_KINDS, 32);
    var state: u64[128];
    var draft: u64[3];
    if giga_request_allocate_state(context, capacity, kinds, state, draft) != 0 {
        giga_release_context(context);
        return -1;
    }
    let cursor = giga_request_clear_state(context, false);
    giga_zero_raw(unsafe_c_ptr<c_ptr<void>>(request), 0, GIGA_REQUEST_BYTES);
    giga_write_word(request, context);
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(request + GIGA_REQUEST_STATE),
                  unsafe_c_slice_data<c_ptr<const void>>(state[..]), 128 * 8);
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(request + GIGA_REQUEST_DRAFT),
                  unsafe_c_slice_data<c_ptr<const void>>(draft[..]), 3 * 8);
    giga_write_word(request + GIGA_REQUEST_CAPACITY, capacity as! u64);
    giga_write_word(request + GIGA_REQUEST_COMMITTED, cursor as! u64);
    giga_write_word(request + GIGA_REQUEST_DRAFT_COMMITTED, cursor as! u64);
    return 0;
}

export fn giga_abi_giga_request_reset(request: u64) {
    if request == 0 { return; }
    let context = giga_tensor_word(request);
    if context == 0 { return; }
    let device = (giga_tensor_word(request + GIGA_REQUEST_DEVICE_STATE) & 255) != 0;
    let cursor = giga_request_clear_state(context, device);
    giga_write_word(request + GIGA_REQUEST_COMMITTED, cursor as! u64);
    giga_write_word(request + GIGA_REQUEST_DRAFT_COMMITTED, cursor as! u64);
}

export fn giga_abi_giga_step_prepare(request: u64, context: u64, count: i64,
        output_count: i64, draft: bool, step: u64) -> i32 {
    if request == 0 || context == 0 || step == 0 { return -1; }
    let cursor = if draft { GIGA_REQUEST_DRAFT_COMMITTED } else { GIGA_REQUEST_COMMITTED };
    let base = giga_tensor_word(request + cursor) as! i64;
    let capacity = giga_tensor_word(request + GIGA_REQUEST_CAPACITY) as! i64;
    var tensors: u64[5];
    let cells = giga_prepare_step_inputs(context, capacity, base, count, output_count, tensors);
    if cells < 0 { return -1; }
    giga_zero_raw(unsafe_c_ptr<c_ptr<void>>(step), 0, GIGA_STEP_BYTES);
    for i in 5 { giga_write_word(step + (i as! u64) * 8, tensors[i]); }
    giga_write_word(step + GIGA_STEP_BASE, base as! u64);
    giga_write_word(step + GIGA_STEP_COUNT, count as! u64);
    giga_write_word(step + GIGA_STEP_OUTPUT_COUNT, output_count as! u64);
    giga_write_word(step + GIGA_STEP_CELLS, cells as! u64);
    giga_write_word(step + GIGA_STEP_FLAGS, if draft { 1 } else { 0 });
    return 0;
}

export fn giga_abi_giga_step_upload(step: u64, tokens: u64) -> i32 {
    if step == 0 || tokens == 0 { return -1; }
    let tensors = giga_request_words(step, 5);
    var data: u64[5];
    for i in 5 {
        if tensors[i] == 0 {
            if i != 4 { return -1; }
            continue;
        }
        data[i] = giga_tensor_data(tensors[i]);
        if data[i] == 0 { return -1; }
    }
    let base = giga_tensor_word(step + GIGA_STEP_BASE) as! i64;
    let count = giga_tensor_word(step + GIGA_STEP_COUNT) as! i64;
    let cells = giga_tensor_word(step + GIGA_STEP_CELLS) as! i64;
    let output_count = if data[4] == 0 { 0 } else { giga_tensor_word(step + GIGA_STEP_OUTPUT_COUNT) as! i64 };
    giga_fill_step_inputs(base, count, cells, giga_request_i32(data[1], count), giga_request_i64(data[2], count),
                          giga_request_u16(data[3], count * cells), giga_request_i32(data[4], output_count));
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(data[0]), unsafe_c_ptr<c_ptr<const void>>(tokens), (count as! u64) * 4);
    return 0;
}

const GIGA_MODEL_WEIGHTS: u64 = 16;
const GIGA_MODEL_OFFSETS: u64 = 8304;
const GIGA_CONFIG_WORDS: i64 = 76;
const GIGA_CONFIG_BYTES: u64 = 608;
const GIGA_CONFIG_ROUTED: u64 = 24;
const GIGA_CONFIG_SHARED: u64 = 152;
const GIGA_CONFIG_FACTORS: u64 = 288;
const GIGA_CONFIG_ROPE: u64 = 552;
const GIGA_CONFIG_INDEX_ROPE: u64 = 576;
const GIGA_CONFIG_ORIGINAL_CONTEXT: u64 = 600;

extern "ggml_set_output" fn giga_mark_step_output(tensor: u64);

fn giga_pointer_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

export fn giga_abi_giga_step_graph(request: u64, model: u64, context: u64, graph: u64,
                                  step: u64, draft_hidden: u64, config: u64, output: u64) -> i32 {
    if request == 0 || model == 0 || context == 0 || graph == 0 || step == 0 || config == 0 || output == 0 {
        return -1;
    }
    let flags = giga_tensor_word(step + GIGA_STEP_FLAGS);
    let draft = (flags & 255) != 0;
    if draft && draft_hidden == 0 { return -1; }

    var options: u64[GIGA_CONFIG_WORDS];
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(options[0]), unsafe_c_ptr<c_ptr<const void>>(config), GIGA_CONFIG_BYTES);
    let selected = giga_pointer_address(unsafe_c_address<c_ptr<const void>>(options[0]));
    var batch: u64[5];
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(batch[0]), unsafe_c_ptr<c_ptr<const void>>(step), 40);
    let original_context = giga_request_i32(selected + GIGA_CONFIG_ORIGINAL_CONTEXT, 1);
    var shape: i64[7] = [
        giga_tensor_word(step + GIGA_STEP_COUNT) as! i64, giga_tensor_word(step + GIGA_STEP_CELLS) as! i64,
        giga_tensor_word(request + GIGA_REQUEST_CAPACITY) as! i64, 1,
        giga_tensor_word(step + GIGA_STEP_OUTPUT_COUNT) as! i64, original_context[0] as! i64,
        if ((flags >> 8) & 255) != 0 { 1 } else { 0 },
    ];
    var strides: u64[2] = [576 * 2, 128 * 2];
    let tuning = giga_borrow_floats(selected, 6);
    let rope = giga_borrow_floats(selected + GIGA_CONFIG_ROPE, 6);
    let index_rope = giga_borrow_floats(selected + GIGA_CONFIG_INDEX_ROPE, 6);
    let shared = giga_borrow_floats(selected + GIGA_CONFIG_SHARED, 33);
    let factors = giga_request_words(selected + GIGA_CONFIG_FACTORS, 33);
    let offsets = giga_request_i64(model + GIGA_MODEL_OFFSETS, 34);
    var result: u64[2];
    var status: i64 = 0;
    if draft {
        let first = offsets[32];
        let end = offsets[33];
        if first < 0 || end < first || end > 1036 { return -1; }
        let weights = giga_request_words(model + GIGA_MODEL_WEIGHTS + (first as! u64) * 8, end - first);
        let state = giga_request_words(request + GIGA_REQUEST_DRAFT, 3);
        status = giga_mtp_graph(context, graph, draft_hidden, weights, state, batch, shape, strides,
            tuning, shared[32], factors[32], rope, index_rope, result);
    } else {
        let weights = giga_request_words(model + GIGA_MODEL_WEIGHTS, 1036);
        let kinds = giga_request_i64(model + GIGA_BINDINGS_KINDS, 33);
        let state = giga_request_words(request + GIGA_REQUEST_STATE, 128);
        let routed = giga_borrow_floats(selected + GIGA_CONFIG_ROUTED, 32);
        let normalize = ((options[75] >> 32) & 255) != 0;
        status = giga_decoder_graph(context, graph, weights, offsets, kinds, state, batch, shape, strides,
            tuning, routed, shared[0 .. 32], factors[0 .. 32], rope, index_rope, normalize, result);
    }
    if status != 0 { return status as! i32; }
    giga_write_word(output, result[0]);
    giga_write_word(output + 8, result[1]);
    giga_mark_step_output(result[0]);
    if result[1] != 0 { giga_mark_step_output(result[1]); }
    return 0;
}
