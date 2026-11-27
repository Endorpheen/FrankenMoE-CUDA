extern "fg_cuda_gigachat4_small_expert_wave" fn giga_small_wave(enabled: bool);
extern "fg_standalone_graph_alloc" fn giga_host_arena(graph: u64) -> u64;
extern "fg_standalone_graph_compute" fn giga_host_compute(graph: u64) -> i32;
extern "fg_standalone_graph_plan_alloc" fn giga_host_plan(graph: u64, output: u64) -> u64;
extern "fg_standalone_graph_plan_compute" fn giga_host_plan_compute(plan: u64) -> i32;
extern "fg_standalone_graph_plan_free" fn giga_host_plan_free(plan: u64);
extern "giga_step_upload" fn giga_host_upload(step: u64, tokens: u64) -> i32;
extern "giga_request_reset" fn giga_host_reset(request: u64);
extern "fg_q35_release_graph" fn giga_host_free(memory: u64);

fn giga_commit_step(cursor: i64[:], base: i64, count: i64, capacity: i64) -> bool {
    let position = cursor[0];
    if position != base || count < 1 || count > capacity - position { return false; }
    cursor[0] = position + count;
    return true;
}

// The caller owns descriptors and releases the returned arena after reading the outputs.
export fn giga_execute_step(request: u64, step: u64, graph: u64,
                            tokens: u64, output: u64, planned: bool, arena_out: u64[:],
                            cursor: i64[:], base: i64, count: i64, capacity: i64) -> i64 {
    if request == 0 || step == 0 || graph == 0 || tokens == 0 ||
       arena_out.len < 1 || arena_out[0] != 0 || cursor.len != 1 { return -1; }
    let arena = if planned { giga_host_plan(graph, output) } else { giga_host_arena(graph) };
    if arena == 0 { return -2; }
    if giga_host_upload(step, tokens) != 0 {
        if planned { giga_host_plan_free(arena); } else { giga_host_free(arena); }
        return -3;
    }
    giga_small_wave(planned);
    let status = if planned { giga_host_plan_compute(arena) } else { giga_host_compute(graph) };
    giga_small_wave(false);
    if status != 0 || !giga_commit_step(cursor, base, count, capacity) {
        giga_host_reset(request);
        if planned { giga_host_plan_free(arena); } else { giga_host_free(arena); }
        return -4;
    }
    arena_out[0] = arena;
    return 0;
}
