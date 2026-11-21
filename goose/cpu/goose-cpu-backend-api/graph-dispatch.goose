extern "fg143_graph_nodes" fn graph_nodes(graph: u64) -> i64;
extern "fg143_graph_node" fn graph_node(graph: u64, index: i64) -> u64;
extern "fg158_prop" fn graph_prop(tensor: u64, field: i64) -> i64;
extern "fg143_graph_inputs" fn graph_inputs(tensor: u64, sources: u64[:]);
extern "ggml_is_empty" fn graph_empty(tensor: u64) -> bool;
extern "ggml_is_contiguous" fn graph_dense(tensor: u64) -> bool;
extern "fg_cuda_activation_prepare" fn graph_output_prepare(address: u64, bytes: i64, fresh: bool,
    overwrite: bool, step: i64, deadline: i64) -> i32;
extern "fg143_graph_describe" fn graph_describe(tensor: u64, packet: i64[:]);
extern "fg143_graph_regular" fn graph_regular(tensor: u64, packet: const i64[:], mode: i64,
    sizes: i64[:], workspace: u64) -> bool;
extern "fg143_graph_gdn" fn graph_gdn(tensor: u64) -> bool;
extern "fg143_graph_scan" fn graph_scan(tensor: u64) -> bool;
extern "fg143_graph_resize" fn graph_resize(workspace: u64, bytes: i64) -> u64;
extern "fg143_graph_observe" fn graph_observe(tensor: u64) -> bool;
extern "fg_cuda_routed_graph_enabled" fn routed_graph_enabled() -> bool;
extern "fg_cuda_routed_graph" fn routed_graph(plan: const i64[:], glu: const i64[:], norm: const i64[:],
    step: i64, deadline: i64) -> i64;

export fn fg143_routed_layer_matches(layers: const i64[:], plan: const i64[:]) -> bool {
    for i in layers.len / 4 {
        let j = i * 4;
        if layers[j] >= 0 && layers[j + 1] == plan[0] && layers[j + 2] == plan[1] && layers[j + 3] == plan[2] {
            return true;
        }
    }
    return false;
}

struct GraphWorkspace { address: u64, capacity: i64 }

fn routed_dense(t: Tensor, n0: i64, n1: i64, n2: i64, kind: i64) -> bool {
    return t.kind == kind && t.ne[0] == n0 && t.ne[1] == n1 && t.ne[2] == n2 && t.ne[3] == 1 &&
        dense_scalar(t, 4) && t.bytes == n0 * n1 * n2 * 4;
}

fn routed_shape(p: const i64[:], width: i64, rows: i64, experts: i64, inputs: i64, kind: i64) -> bool {
    let w = packet_tensor(p, 0);
    let row = width / 256 * (if kind == 12 { 144 } else { 210 });
    let plane_bytes = row * rows;
    return w.kind == kind && w.ne[0] == width && w.ne[1] == rows && w.ne[2] == experts && w.ne[3] == 1 &&
        w.nb[0] == (if kind == 12 { 144 } else { 210 }) && w.nb[1] == row && w.nb[2] == plane_bytes &&
        w.bytes == plane_bytes * experts &&
        routed_dense(packet_tensor(p, 10), width, inputs, 1, 0) &&
        routed_dense(packet_tensor(p, 20), 8, 1, 1, 26) &&
        routed_dense(packet_tensor(p, 30), rows, 8, 1, 0);
}

fn try_routed_graph(graph: u64, step: i64, deadlines: const i64[:]) -> i64 {
    if step + 5 >= deadlines.len || deadlines[step] != step + 2 || deadlines[step + 1] != step + 2 ||
       deadlines[step + 2] != step + 3 || deadlines[step + 3] != step + 5 || deadlines[step + 4] != step + 5 {
        return 0;
    }
    let first = graph_node(graph, step);
    let second = graph_node(graph, step + 1);
    let glu_node = graph_node(graph, step + 2);
    let down_node = graph_node(graph, step + 3);
    let norm_node = graph_node(graph, step + 4);
    let output = graph_node(graph, step + 5);
    if graph_prop(first, 2) != OP_MUL_MAT_ID || graph_prop(second, 2) != OP_MUL_MAT_ID ||
       graph_prop(glu_node, 2) != OP_GLU || graph_prop(down_node, 2) != OP_MUL_MAT_ID ||
       graph_prop(norm_node, 2) != OP_CUSTOM || graph_prop(output, 2) != OP_CUSTOM { return 0; }
    var s: u64[10] = [0; 10];
    graph_inputs(glu_node, s[..]);
    let gate_node = s[0];
    let up_node = s[1];
    if !((gate_node == first && up_node == second) || (gate_node == second && up_node == first)) { return 0; }
    graph_inputs(up_node, s[..]);
    let up = s[0]; let input = s[1]; let ids = s[2];
    graph_inputs(gate_node, s[..]);
    let gate = s[0];
    if s[1] != input || s[2] != ids { return 0; }
    graph_inputs(down_node, s[..]);
    let down = s[0];
    if s[1] != glu_node || s[2] != ids { return 0; }
    graph_inputs(norm_node, s[..]);
    let probs = s[0];
    if s[1] != ids { return 0; }
    graph_inputs(output, s[..]);
    if s[0] != down_node || s[1] != norm_node { return 0; }

    var matrix: i64[71] = [0; 71];
    graph_describe(up_node, matrix[..]);
    let width = matrix[0]; let hidden = matrix[1]; let experts = matrix[2];
    if width <= 0 || width > 8192 || width % 256 != 0 ||
       hidden <= 0 || hidden > 8192 || hidden % 256 != 0 || experts != 256 ||
       !routed_shape(matrix[..], width, hidden, experts, 1, 12) { return 0; }
    graph_describe(gate_node, matrix[..]);
    if !routed_shape(matrix[..], width, hidden, experts, 1, 12) { return 0; }
    graph_describe(down_node, matrix[..]);
    let down_kind = matrix[8];
    if (down_kind != 12 && down_kind != 14) || !routed_shape(matrix[..], hidden, width, experts, 8, down_kind) { return 0; }

    graph_describe(glu_node, matrix[..]);
    if matrix[41] != GLU_OP_SWIGLU || matrix[42] != 0 || matrix[53] != 0 ||
       !routed_dense(packet_tensor(matrix[..], 30), hidden, 8, 1, 0) { return 0; }
    var glu: i64[26] = [0; 26];
    glu[0] = CV_SWIGLU; glu[1] = hidden * 8;
    for axis in 4 {
        glu[2 + axis] = matrix[30 + axis];
        glu[6 + axis] = matrix[axis];
        glu[10 + axis] = matrix[4 + axis];
        glu[14 + axis] = matrix[10 + axis];
        glu[18 + axis] = matrix[14 + axis];
    }
    glu[24] = (glu[1] + 127) / 128; glu[25] = 128;

    graph_describe(norm_node, matrix[..]);
    if matrix[41] != 0x4d4f454e || !routed_dense(packet_tensor(matrix[..], 0), experts, 1, 1, 0) ||
       !routed_dense(packet_tensor(matrix[..], 30), 1, 8, 1, 0) { return 0; }
    let scale = from_bits(matrix[42] as! u32);
    if !(scale >= 0.0 && scale <= 3.4028234663852886e38) { return 0; }
    var norm: i64[22] = [0; 22];
    norm[0] = 4; norm[1] = 1; norm[2] = 32; norm[4] = experts; norm[12] = 8; norm[21] = matrix[42];

    graph_describe(output, matrix[..]);
    if matrix[41] != 0x4d4f4557 || !routed_dense(packet_tensor(matrix[..], 30), width, 1, 1, 0) ||
       storage_get(output, STORAGE_VIEW, 0) != 0 { return 0; }
    let tensors: u64[7] = [up, gate, down, input, probs, ids, output];
    var plan: i64[10] = [0; 10];
    for j in tensors.len {
        let address = storage_get(tensors[j], STORAGE_DATA, 0);
        if address == 0 { return 0; }
        plan[j] = address as! i64;
    }
    plan[7] = width; plan[8] = hidden; plan[9] = down_kind;
    let destination = plan[6] as! u64;
    if activation_spans_overlap(destination, (width * 4) as u64, plan[3] as! u64, (width * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[4] as! u64, (experts * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[5] as! u64, 32) { return 0; }
    return routed_graph(plan[..], glu[..], norm[..], step, deadlines[step + 5]);
}

fn execute_graph_node(tensor: u64, work: GraphWorkspace &, selective: bool, step: i64, deadline: i64) -> bool {
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
    let fresh = storage_get(tensor, STORAGE_VIEW, 0) == 0;
    var overwrite = selective && fresh && graph_dense(tensor) &&
        (op == OP_MUL_MAT || op == OP_MUL_MAT_ID || op == OP_RMS_NORM || op == OP_MUL ||
         op == OP_ADD || op == OP_DIV || op == OP_CONT || op == OP_SCALE || op == OP_UNARY || op == OP_ROPE);
    for i in sources.len {
        let t = sources[i];
        if t == 0 { continue; }
        let src = storage_get(t, STORAGE_DATA, 0);
        if src == 0 && !graph_empty(t) { return false; }
        if overwrite && activation_spans_overlap(address, bytes, src, storage_bytes(t)) { overwrite = false; }
    }
    if graph_output_prepare(address, bytes as i64, fresh, overwrite, step, deadline) != 0 { return false; }
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
export fn fg143_graph_run(graph: u64, selective: bool, deadlines: const i64[:], observe: bool) -> i64 {
    if graph == 0 { return -1; }
    let count = graph_nodes(graph);
    if deadlines.len != 0 && (deadlines.len != count || !selective) { return -1; }
    var work = GraphWorkspace { address: 0, capacity: 0 };
    var outcome: i64 = 0;
    let use_routed_graph = !observe && deadlines.len != 0 && routed_graph_enabled();
    var i: i64 = 0;
    while i < count {
        let step = if deadlines.len == 0 { -1 } else { i };
        let deadline = if deadlines.len == 0 { -1 } else { deadlines[i] };
        let tensor = graph_node(graph, i);
        if use_routed_graph && graph_prop(tensor, 2) == OP_MUL_MAT_ID {
            let grouped = try_routed_graph(graph, i, deadlines);
            if grouped > 0 { i += 6; continue; }
            if grouped < 0 { outcome = i + 1; break; }
        }
        if !execute_graph_node(tensor, work, selective, step, deadline) || (observe && !graph_observe(tensor)) {
            outcome = i + 1;
            break;
        }
        i += 1;
    }
    if work.address != 0 { graph_resize(work.address, 0); }
    return outcome;
}
