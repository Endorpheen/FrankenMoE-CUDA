extern "fg143_graph_nodes" fn graph_nodes(graph: u64) -> i64;
extern "fg143_graph_node" fn graph_node(graph: u64, index: i64) -> u64;
extern "fg158_prop" fn graph_prop(tensor: u64, field: i64) -> i64;
extern "fg143_graph_inputs" fn graph_inputs(tensor: u64, sources: u64[:]);
extern "ggml_is_empty" fn graph_empty(tensor: u64) -> bool;
extern "fg_cuda_activation_prepare" fn graph_output_prepare(address: u64, bytes: i64, fresh: bool) -> i32;
extern "fg143_graph_describe" fn graph_describe(tensor: u64, packet: i64[:]);
extern "fg143_graph_regular" fn graph_regular(tensor: u64, packet: const i64[:], mode: i64,
    sizes: i64[:], workspace: u64) -> bool;
extern "fg143_graph_gdn" fn graph_gdn(tensor: u64) -> bool;
extern "fg143_graph_scan" fn graph_scan(tensor: u64) -> bool;
extern "fg143_graph_resize" fn graph_resize(workspace: u64, bytes: i64) -> u64;

struct GraphWorkspace { address: u64, capacity: i64 }

fn execute_graph_node(tensor: u64, work: GraphWorkspace &) -> bool {
    if tensor == 0 { return false; }
    let op = graph_prop(tensor, 2);
    if op == OP_NONE || op == OP_RESHAPE || op == OP_VIEW || op == OP_PERMUTE || op == OP_TRANSPOSE {
        return true;
    }
    let address = storage_get(tensor, STORAGE_DATA, 0);
    let bytes = storage_bytes(tensor);
    if address == 0 || bytes > 0x7fffffffffffffff { return false; }
    var sources: u64[10] = [0; 10];
    graph_inputs(tensor, sources[..]);
    for i in sources.len {
        let t = sources[i];
        if t != 0 && storage_get(t, STORAGE_DATA, 0) == 0 && !graph_empty(t) { return false; }
    }
    let fresh = storage_get(tensor, STORAGE_VIEW, 0) == 0;
    if graph_output_prepare(address, bytes as i64, fresh) != 0 { return false; }
    if op == OP_GATED_DELTA_NET { return graph_gdn(tensor); }
    if op == OP_SSM_SCAN { return graph_scan(tensor); }

    var packet: i64[71] = [0; 71];
    var sizes: i64[4] = [0; 4];
    graph_describe(tensor, packet[..]);
    if !graph_regular(tensor, packet[..], 1, sizes[..], 0) { return false; }
    let required = sizes[3];
    if required > work.capacity {
        let next = graph_resize(work.address, required);
        if next == 0 { return false; }
        work.address = next;
        work.capacity = required;
    }
    return graph_regular(tensor, packet[..], 0, sizes[..], work.address);
}

// Return one plus the first failing node index, or zero after the last node.
export fn fg143_graph_run(graph: u64) -> i64 {
    if graph == 0 { return -1; }
    var work = GraphWorkspace { address: 0, capacity: 0 };
    var outcome: i64 = 0;
    for i in graph_nodes(graph) {
        if !execute_graph_node(graph_node(graph, i), work) {
            outcome = i + 1;
            break;
        }
    }
    if work.address != 0 { graph_resize(work.address, 0); }
    return outcome;
}
