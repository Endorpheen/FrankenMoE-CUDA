extern fn fg_host_graph_tls_address() -> u64;
extern fn fg_host_atomic_load_u64(address: u64) -> u64;
extern fn fg_host_atomic_store_u64(address: u64, value: u64);
extern fn fg_host_atomic_add_u64(address: u64, value: u64);
extern fn fg_host_atomic_swap_u64(address: u64, value: u64) -> u64;
extern fn fg_cuda_host_load_acquire_u32(address: u64) -> u32;
extern fn fg_cuda_host_store_release_u32(address: u64, value: u32);
extern fn fg_goose_host_ensure_init();
extern fn fg143_cpu_init(argc: i32, argv: u64);
extern fn graph_state_mutex_lock(lock: c_ptr<void>) -> i32;
extern fn graph_state_mutex_unlock(lock: c_ptr<void>) -> i32;

var graph_cpu_initialized: u32;
var graph_cpu_mutex: u64[5] = [GRAPH_INIT_MUTEX, 0, 0, 0, 0];
var graph_cpu_threads: u64[2] = [1, 1];
var graph_cpu_values: u64;

fn fg_graph_native_cpu_init() {
    let ready = graph_host_address(unsafe_c_address<c_ptr<const void>>(graph_cpu_initialized));
    if fg_cuda_host_load_acquire_u32(ready) != 0 { return; }
    let mutex = unsafe_c_address<c_ptr<void>>(graph_cpu_mutex[0]);
    assert(graph_state_mutex_lock(mutex) == 0);
    if fg_cuda_host_load_acquire_u32(ready) == 0 {
        fg_goose_host_ensure_init();
        fg143_cpu_init(0, 0);
        fg_cuda_host_store_release_u32(ready, 1);
    }
    assert(graph_state_mutex_unlock(mutex) == 0);
}

fn fg_graph_native_threads(prefill: bool, threads: i64) -> i64 {
    let index = if prefill { 0 } else { 1 };
    let slot = graph_host_address(unsafe_c_address<c_ptr<const void>>(graph_cpu_threads[index]));
    if threads != 0 { fg_host_atomic_store_u64(slot, threads as! u64); }
    return fg_host_atomic_load_u64(slot) as! i64;
}

fn fg_graph_native_set_prefill(prefill: bool) -> bool {
    let slot = fg_host_graph_tls_address();
    let previous = graph_host_word(slot) == 0;
    let value: u64 = if prefill { 0 } else { 1 };
    graph_host_copy(unsafe_c_ptr<c_ptr<void>>(slot), unsafe_c_address<c_ptr<const void>>(value), 8);
    return previous;
}

fn fg_graph_native_prefill() -> bool { return graph_host_word(fg_host_graph_tls_address()) == 0; }

fn fg_graph_native_trace_get() -> u64 { return graph_host_word(fg_host_graph_tls_address() + 8); }

fn fg_graph_native_trace_set(address: u64) {
    let slot = fg_host_graph_tls_address() + 8;
    graph_host_copy(unsafe_c_ptr<c_ptr<void>>(slot), unsafe_c_address<c_ptr<const void>>(address), 8);
}

fn fg_graph_native_checkpoint() -> bool { return fg_graph_native_trace_get() != 0; }

export fn fg143_witness(op: i64, values: i64) {
    let address = graph_host_address(unsafe_c_address<c_ptr<const void>>(graph_cpu_values));
    fg_host_atomic_add_u64(address, values as! u64);
}

export fn fg141_witness(stage: i64, kind: i64, values: i64) {
    fg143_witness(kind, values);
}

export fn fg143_graph_take_cpu_values() -> u64 {
    return fg_host_atomic_swap_u64(graph_host_address(unsafe_c_address<c_ptr<const void>>(graph_cpu_values)), 0);
}
