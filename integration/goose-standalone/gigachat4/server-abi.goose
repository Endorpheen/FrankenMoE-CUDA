const GIGA_GRAPH_STEP_WORDS: i64 = 10;

extern "ggml_graph_overhead_custom" fn giga_graph_overhead(nodes: u64, grads: bool) -> u64;
extern "ggml_new_graph_custom" fn giga_new_graph(ctx: u64, nodes: u64, grads: bool) -> u64;
extern "free" fn giga_free_workspace(pointer: c_ptr<void>);
extern fn giga_server_trace_execution(value: i64, draft: bool, finished: bool);

export fn giga_execute_bound(request: u64, model: u64, previous: u64, tokens: u64,
        count: i64, emit: i64, logits: u64, draft_input: u64, next_input: u64,
        planned_enabled: bool, planned_prefill_only: bool, trace: bool) -> i64 {
    if count < 1 || count > 4096 || emit < 0 || emit > count { return -1; }
    let draft = draft_input != 0;
    let max_nodes: u64 = if draft { 512 } else { 8192 };
    let bytes = giga_graph_overhead(max_nodes, false) + max_nodes * giga_request_tensor_overhead();
    let ctx = fg143_arena_init(bytes, 0, true);
    if ctx == 0 { return -1; }

    var hidden: u64 = 0;
    if draft {
        hidden = giga_request_tensor2(ctx, 0, 2560, count);
        giga_write_word(hidden + GIGA_TENSOR_DATA_OFFSET, draft_input);
        giga_step_mark_input(hidden);
    }
    let graph = giga_new_graph(ctx, max_nodes, false);
    var step: u64[GIGA_GRAPH_STEP_WORDS];
    var output: u64[2];
    var arena: u64[1];
    let step_addr = giga_pointer_address(unsafe_c_address<c_ptr<const void>>(step[0]));
    let output_addr = giga_pointer_address(unsafe_c_address<c_ptr<const void>>(output[0]));
    var status = giga_abi_giga_step_prepare(request, ctx, count, emit, draft, step_addr) as! i64;
    if status == 0 {
        if next_input != 0 { step[9] |= 256; }
        status = giga_abi_giga_step_graph(request, model, ctx, graph, step_addr, hidden,
                                         model + GIGA_MODEL_CONFIG, output_addr) as! i64;
    }
    let planned = planned_enabled && !draft && next_input == 0 && (!planned_prefill_only || count > 1);
    if status == 0 {
        if trace { giga_server_trace_execution(count, draft, false); }
        let cursor = if draft { GIGA_REQUEST_DRAFT_COMMITTED } else { GIGA_REQUEST_COMMITTED };
        let capacity = giga_tensor_word(request + GIGA_REQUEST_CAPACITY) as! i64;
        status = giga_execute_step(request, step_addr, graph, tokens, output[1], planned, arena,
            giga_request_i64(request + cursor, 1), step[5] as! i64, step[6] as! i64, capacity);
    }
    if trace { giga_server_trace_execution(status, draft, true); }
    if status == 0 && emit > 0 {
        giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(logits), unsafe_c_ptr<c_ptr<const void>>(giga_tensor_data(output[1])),
                      (emit as! u64) * 128256 * 4);
    }
    if status == 0 && next_input != 0 {
        let values = count * 2560;
        status = giga_mtp_align(giga_borrow_floats(giga_tensor_data(output[0]), values),
                                giga_borrow_floats(previous, 2560), giga_borrow_floats(next_input, values), count);
    }
    if planned { giga_host_plan_free(arena[0]); }
    else { giga_free_workspace(unsafe_c_ptr<c_ptr<void>>(arena[0])); }
    giga_release_context(ctx);
    return status;
}

const GIGA_SERVER_REQUEST_WORDS: i64 = 136;

var giga_server_model: u64[GIGA_MODEL_WORDS];
var giga_server_request: u64[GIGA_SERVER_REQUEST_WORDS];
var giga_server_previous: f32[2560];
var giga_server_draft: bool;
var giga_server_trace: bool;
var giga_server_planned: bool;
var giga_server_planned_prefill: bool;

extern fn giga_server_log_placement(kind: i32, bytes: i64, available: i64);
extern fn giga_server_log_open(status: i32, finished: bool);

fn giga_server_model_address() -> u64 {
    return giga_pointer_address(unsafe_c_address<c_ptr<const void>>(giga_server_model[0]));
}

fn giga_server_request_address() -> u64 {
    return giga_pointer_address(unsafe_c_address<c_ptr<const void>>(giga_server_request[0]));
}

fn giga_server_previous_address() -> u64 {
    return giga_pointer_address(unsafe_c_address<c_ptr<const void>>(giga_server_previous[0]));
}

export fn giga_server_expand_mla() -> i32 {
    return giga_abi_giga_expand_mla_weights(giga_server_model_address());
}

export fn giga_server_placement_trace(kind: i32, bytes: i64, available: i64) {
    if giga_server_trace { giga_server_log_placement(kind, bytes, available); }
}

export fn giga_server_open_bound(mapping: u64, bytes: u64, capacity: i64) -> i32 {
    giga_server_trace = giga_env_value("GOOSE_GIGACHAT4_TRACE") != 0;
    let planned = giga_env_value("GOOSE_GIGACHAT4_PLANNED");
    giga_server_planned_prefill = giga_env_is(planned, "prefill");
    giga_server_planned = planned == 0 || giga_server_planned_prefill || giga_env_is(planned, "1");
    if giga_server_trace { giga_server_log_open(0, false); }
    let model = giga_server_model_address();
    let request = giga_server_request_address();
    var status = giga_abi_giga_bind_model(model, mapping, bytes);
    if status != 0 { return status; }

    let kinds = giga_request_i64(model + GIGA_BINDINGS_KINDS, 33);
    status = giga_server_place_weights(giga_request_words(model + GIGA_MODEL_WEIGHTS, 1036),
        giga_request_i64(model + GIGA_MODEL_OFFSETS, 34), kinds,
        giga_borrow_floats(model + GIGA_MODEL_CONFIG, 6), mapping, bytes as! i64,
        !giga_env_disabled("GOOSE_GIGACHAT4_DSA_CUDA"), !giga_env_disabled("GOOSE_GIGACHAT4_MLA_F32"),
        giga_setting_is("GOOSE_GIGACHAT4_DENSE", "1"), giga_setting_is("GOOSE_GIGACHAT4_EXPERTS", "1"));
    if status != 0 {
        giga_abi_giga_unbind_model(model);
        return status;
    }
    status = giga_abi_giga_request_init(request, model, capacity);
    if status == 0 {
        let placed = giga_server_place_state(giga_request_words(request + GIGA_REQUEST_STATE, 128),
            kinds, capacity, !giga_env_disabled("GOOSE_GIGACHAT4_GDN_STATE"));
        if placed < 0 { status = -1; }
        else { giga_write_word(request + GIGA_REQUEST_DEVICE_STATE, if placed > 0 { 1 } else { 0 }); }
    }
    if status != 0 {
        giga_abi_giga_request_free(request);
        giga_abi_giga_unbind_model(model);
    }
    giga_server_draft = status == 0 && giga_setting_is("GOOSE_GIGACHAT4_MTP_CATCHUP", "1");
    if giga_server_trace { giga_server_log_open(status, true); }
    return status;
}

export fn giga_server_reset() {
    giga_abi_giga_request_reset(giga_server_request_address());
    giga_zero_raw(unsafe_c_ptr<c_ptr<void>>(giga_server_previous_address()), 0, 2560 * 4);
}

export fn giga_server_execute(tokens: u64, count: i64, emit: i64, logits: u64, draft_input: u64, next_input: u64) -> i64 {
    return giga_execute_bound(giga_server_request_address(), giga_server_model_address(),
        giga_server_previous_address(), tokens, count, emit, logits, draft_input, next_input,
        giga_server_planned, giga_server_planned_prefill, giga_server_trace);
}

export fn giga_abi_giga_server_step(tokens: u64, count: i64, rows: i64, logits: u64, logit_capacity: i64) -> i64 {
    return giga_run_server_step(tokens, giga_request_i32(tokens, count), rows, logits, logit_capacity, giga_server_draft);
}
