struct InklingArenaParams { size: u64, buffer: u64, no_alloc: bool }

extern fn ggml_init(params: InklingArenaParams) -> u64;
extern fn ggml_new_tensor(ctx: u64, kind: i32, dimensions: i32, shape: u64) -> u64;
extern fn ggml_nelements(tensor: u64) -> i64;
extern fn ggml_new_graph_custom(ctx: u64, capacity: u64, gradients: bool) -> u64;
extern fn ggml_build_forward_expand(graph: u64, tensor: u64);
extern fn fg143_graph_nodes(graph: u64) -> i64;
extern fn fg_standalone_graph_plan_alloc(graph: u64, output: u64) -> u64;
extern fn fg_standalone_graph_alloc(graph: u64) -> u64;
extern fn fg_standalone_graph_compute(graph: u64) -> i32;
extern fn fg_standalone_graph_plan_compute(plan: u64) -> i32;
extern fn fg_i2_readback_state() -> u64;

fn fg_i2_readback_matches(output: u64, elements: u64) -> bool {
    let state = fg_i2_readback_state();
    return output == i2d_read(state) && elements == i2d_read(state + 8);
}

fn i2_prefill_set_readback(output: u64, elements: u64) {
    let state = fg_i2_readback_state();
    i2d_set(state, output);
    i2d_set(state + 8, elements);
}

fn i2_swap_direct_output(output: u64) -> u64 {
    let state = fg_i2_readback_state();
    let previous = i2d_read(state);
    if previous != 0 { i2d_set(state, output); }
    return previous;
}

fn fg_i2_host_context() -> u64 {
    let ctx = ggml_init(InklingArenaParams {size: 2097152, buffer: 0, no_alloc: true});
    assert(ctx != 0);
    return ctx;
}

fn fg_i2_host_weight(ctx: u64, decoder: u64, id: i64) -> u64 {
    let record = i2_tensor_record(decoder, id as! u64);
    let tensor = ggml_new_tensor(ctx, i2d_read(record + 16) as! i32, 4, record + 32);
    assert(tensor != 0);
    i2d_set(tensor + 248, i2d_read(decoder) + i2d_read(record + 80));
    return tensor;
}

fn i2_evaluate(ctx: u64, nodes: u64, count: i32, output: u64, size: i32) {
    let first = i2d_read(nodes);
    if count == 1 && first != 0 && fg_i2_readback_matches(output, size as! u64) &&
        i2d_read(first + 248) == 0 && i2d_read(first + 232) == 0 && i2d_int(first) == 0 &&
        ggml_nelements(first) == size as! i64 {
        i2d_set(first + 248, output);
    }
    let graph = ggml_new_graph_custom(ctx, 256, false);
    assert(graph != 0);
    for index in count as! i64 { ggml_build_forward_expand(graph, i2d_read(nodes + (index as! u64) * 8)); }
    var storage: u64 = 0;
    var plan: u64 = 0;
    if fg_i2_host_resident_plan() && count == 1 && fg143_graph_nodes(graph) > 2 {
        plan = fg_standalone_graph_plan_alloc(graph, first);
        assert(plan != 0 && fg_standalone_graph_plan_compute(plan) == 0);
    } else {
        storage = fg_standalone_graph_alloc(graph);
        assert(storage != 0 && fg_standalone_graph_compute(graph) == 0);
    }
    var at: i64 = 0;
    for index in count as! i64 {
        let tensor = i2d_read(nodes + (index as! u64) * 8);
        assert(tensor != 0);
        let length = ggml_nelements(tensor);
        assert(at + length <= size as! i64);
        let source = i2d_read(tensor + 248);
        let destination = output + (at as! u64) * 4;
        if destination != source { i2d_transfer(destination, source, (length as! u64) * 4); }
        at += length;
    }
    assert(at == size as! i64);
    fg_standalone_graph_plan_free(plan);
    i2d_free(unsafe_c_ptr<c_ptr<void>>(storage));
}

fn fg_i2_host_evaluate_one(ctx: u64, tensor: u64, output: u64, count: i32) {
    i2_evaluate(ctx, i2d_address(unsafe_c_address<c_ptr<const void>>(tensor)), 1, output, count);
}

fn fg_i2_evaluate_attention_inputs(ctx: u64, nodes: u64, matrices: i32, output: u64, count: i32) {
    i2_evaluate(ctx, nodes, matrices, output, count);
}

fn fg_i2_host_project(decoder: u64, id: i64, input: u64, width: i32, output: u64, rows: i32) {
    let ctx = fg_i2_host_context();
    let weight = fg_i2_host_weight(ctx, decoder, id);
    let tensor = fg_i2_host_rows_input(ctx, input, width, 1);
    let result = ggml_mul_mat(ctx, weight, tensor);
    assert(result != 0);
    fg_i2_host_evaluate_one(ctx, result, output, rows);
    ggml_free(ctx);
}

fn fg_i2_host_execute_entry(entry: u64, output: u64) {
    let tensor = i2d_read(entry + 16);
    var plan = i2d_read(entry + 8);
    if plan == 0 {
        let graph = ggml_new_graph_custom(i2d_read(entry), 256, false);
        assert(graph != 0);
        ggml_build_forward_expand(graph, tensor);
        plan = fg_standalone_graph_plan_alloc(graph, tensor);
        i2d_set(entry + 8, plan);
        assert(plan != 0);
    }
    assert(fg_standalone_graph_plan_compute(plan) == 0);
    i2d_transfer(output, i2d_read(tensor + 248), 9216);
}
