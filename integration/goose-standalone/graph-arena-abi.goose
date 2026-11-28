struct GgmlArenaParams { size: u64, buffer: u64, no_alloc: bool }

extern fn fg143_cpu_init(argc: i32, argv: u64);
extern fn fg143_arena_init(size: u64, buffer: u64, no_alloc: bool) -> u64;
extern fn fg143_tensor_overhead() -> u64;
extern fn fg143_graph_hash_size(minimum: u64) -> u64;
extern fn fg143_graph_expand_forward(graph: u64, tensor: u64);
extern fn fg143_graph_overhead_custom(size: u64, grads: bool) -> u64;
extern fn ggml_unary(context: u64, tensor: u64, op: i32) -> u64;
extern "logf" fn ggml_logf(value: f32) -> f32;
extern "floorf" fn ggml_floorf(value: f32) -> f32;
extern "ceilf" fn ggml_ceilf(value: f32) -> f32;

var ggml_arena_ready: bool;

export fn ggml_init(params: GgmlArenaParams) -> u64 {
    ggml_critical_section_start();
    if !ggml_arena_ready {
        fg143_cpu_init(0, 0);
        ggml_arena_ready = true;
    }
    ggml_critical_section_end();
    return fg143_arena_init(params.size, params.buffer, params.no_alloc);
}

export fn ggml_tensor_overhead() -> u64 {
    fg143_cpu_init(0, 0);
    return fg143_tensor_overhead();
}

export fn ggml_hash_size(minimum: u64) -> u64 {
    fg143_cpu_init(0, 0);
    return fg143_graph_hash_size(minimum);
}

export fn ggml_build_forward_expand(graph: u64, tensor: u64) {
    fg143_graph_expand_forward(graph, tensor);
}

export fn ggml_graph_overhead_custom(size: u64, grads: bool) -> u64 {
    fg143_cpu_init(0, 0);
    return fg143_graph_overhead_custom(size, grads);
}

export fn fg143_ctor_caption(address: i64, source: i64, kind: i64) {
    let styles: i64[4] = [1,0,2,3];
    fg156_name(address, source, styles[kind]);
}

export fn ggml_softplus(context: u64, tensor: u64) -> u64 { return ggml_unary(context, tensor, 15); }
export fn ggml_tanh(context: u64, tensor: u64) -> u64 { return ggml_unary(context, tensor, 4); }
export fn ggml_relu(context: u64, tensor: u64) -> u64 { return ggml_unary(context, tensor, 6); }
export fn ggml_sigmoid(context: u64, tensor: u64) -> u64 { return ggml_unary(context, tensor, 7); }
export fn ggml_silu(context: u64, tensor: u64) -> u64 { return ggml_unary(context, tensor, 10); }

export fn ggml_prec_set_acc(address: i64, precision: i32) -> bool {
    let op = builder_i32(address + B_OP);
    if op == 29 || op == 30 { fg158_set_param(address, 0, precision as! i64); }
    else if op == GGML_ABI_FLASH_ATTN_EXT { fg158_set_param(address, 3, precision as! i64); }
    else { return false; }
    return true;
}

export fn ggml_mul_mat_set_hint(address: i64, hint: i32) {
    assert(builder_i32(address + B_OP) == 29);
    fg158_set_param(address, 1, hint as! i64);
}

fn ggml_rope_corr(n_dims: i32, context: i32, rotations: f32, base: f32) -> f32 {
    let two = 2.0 as! f32;
    let pi = 3.141592653589793 as! f32;
    return (n_dims as! f32) * ggml_logf((context as! f32) / (rotations * two * pi)) / (two * ggml_logf(base));
}

export fn ggml_rope_yarn_corr_dims(n_dims: i32, context: i32, base: f32, fast: f32, slow: f32, address: u64) {
    let lower = ggml_floorf(ggml_rope_corr(n_dims, context, fast, base));
    let upper = ggml_ceilf(ggml_rope_corr(n_dims, context, slow, base));
    let maximum = (n_dims - 1) as! f32;
    let first = if (0.0 as! f32) > lower { 0.0 as! f32 } else { lower };
    let last = if maximum < upper { maximum } else { upper };
    builder_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(first), 4);
    builder_copy(unsafe_c_ptr<c_ptr<void>>(address + 4), unsafe_c_address<c_ptr<const void>>(last), 4);
}

export fn ggml_flash_attn_ext_set_n_kv_max(address: i64, maximum: i32) {
    assert(builder_i32(address + B_OP) == GGML_ABI_FLASH_ATTN_EXT && maximum >= 0);
    fg158_set_param(address, 4, maximum as! i64);
}

type GgmlLogCallback = c_fnptr(i32, c_ptr<const u8>, u64) -> void;

extern fn ggml_log_callback_default(level: i32, text: c_ptr<const u8>, data: u64);

var ggml_log_callback: u64;
var ggml_log_data: u64;

export fn ggml_log_set(callback: u64, data: u64) {
    ggml_log_callback = callback;
    ggml_log_data = data;
}

export fn fg_ggml_log_message(level: i32, address: u64) {
    ggml_log_message(level, unsafe_c_ptr<c_ptr<const u8>>(address));
}

fn ggml_log_message(level: i32, text: c_ptr<const u8>) {
    if ggml_log_callback == 0 {
        ggml_log_callback_default(level, text, ggml_log_data);
    } else {
        unsafe_c_fnptr<GgmlLogCallback>(ggml_log_callback)(level, text, ggml_log_data);
    }
}
extern fn fg143_arena_abort(status: i32);
extern "memcpy" fn arena_error_copy(dst: c_ptr<void>, src: c_ptr<const void>, size: u64) -> c_ptr<void>;

fn arena_warn_text(message: const u8[:]) {
    var buffer: u8[192];
    let size = message.len;
    arena_error_copy(unsafe_c_address<c_ptr<void>>(buffer[0]), unsafe_c_slice_data<c_ptr<const void>>(message), size as! u64);
    buffer[size] = 0;
    ggml_log_message(3, unsafe_c_address<c_ptr<const u8>>(buffer[0]));
}

export fn fg143_arena_failure(status: i32, needed: u64, capacity: u64) {
    if status == -1 {
        fg143_arena_abort(status);
    } else if status == -4 {
        arena_warn_text(str("ggml_new_object: not enough space in the context's memory pool (needed ", needed,
            ", available ", capacity, ")\n"));
        fg143_arena_abort(status);
    } else {
        arena_warn_text("ggml_new_object: overflow detected in context allocation\n");
    }
}
