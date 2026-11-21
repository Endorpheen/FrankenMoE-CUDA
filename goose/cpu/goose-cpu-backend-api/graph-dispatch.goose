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
extern "fg_cuda_full_ffn_enabled" fn full_ffn_enabled() -> bool;
extern "fg_cuda_direct_ssm_enabled" fn direct_ssm_enabled() -> bool;
extern "fg_cuda_direct_ssm" fn direct_ssm(plan: const i64[:], step: i64, deadline: i64) -> i64;
extern "fg_cuda_glu_pack_offer" fn offer_glu_pack(plan: const i64[:]) -> bool;
extern "fg_cuda_glu_pack_close" fn close_glu_pack() -> bool;
extern "fg_cuda_rms_scale" fn rms_scale(plan: const i64[:], input: u64, input_bytes: i64,
    output: u64, bytes: i64, step: i64, deadline: i64, eps: f32, scale: f32) -> i64;
extern "fg143_read_contiguous_id" fn graph_read_id(address: u64, index: i64, wide: bool) -> i64;
extern "fg_cuda_fwht_q4_enabled" fn fwht_q4_enabled() -> bool;
extern "fg_cuda_fwht_q4_rows" fn fwht_q4_rows(input: u64, target: u64, width: i64, rows: i64,
    rotation: i32, scale: f32) -> i64;
extern "fg_cuda_routed_graph" fn routed_graph(plan: const i64[:], glu: const i64[:], norm: const i64[:],
    shared: const i64[:], step: i64, deadline: i64) -> i64;

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

fn graph_vector_plan(p: const i64[:], op: i64, config: i64[:]) {
    config[0] = op;
    config[1] = p[30] * p[31];
    for axis in 4 {
        config[2 + axis] = p[30 + axis];
        config[6 + axis] = p[axis];
        config[10 + axis] = p[4 + axis];
        config[14 + axis] = p[10 + axis];
        config[18 + axis] = p[14 + axis];
    }
    config[24] = (config[1] + 127) / 128;
    config[25] = 128;
}

fn shared_projection(p: const i64[:], width: i64, rows: i64, kind: i64) -> bool {
    let w = packet_tensor(p, 0);
    let block_bytes = if kind == 12 { 144 } else { 210 };
    let row_bytes = width / 256 * block_bytes;
    return w.kind == kind && w.ne[0] == width && w.ne[1] == rows && w.ne[2] == 1 && w.ne[3] == 1 &&
        w.nb[0] == block_bytes && w.nb[1] == row_bytes && w.bytes == rows * row_bytes &&
        routed_dense(packet_tensor(p, 10), width, 1, 1, 0) &&
        routed_dense(packet_tensor(p, 30), rows, 1, 1, 0);
}

// Match the complete shared expert and residual before extending the routed graph.
// Each matched intermediate must have its final consumer inside this group.
fn shared_ffn_plan(graph: u64, step: i64, deadlines: const i64[:], input: u64, routed: u64,
                   width: i64, shared: i64[:]) -> bool {
    if step + 14 >= deadlines.len || deadlines[step + 5] != step + 13 ||
       deadlines[step + 6] != step + 8 || deadlines[step + 7] != step + 8 ||
       deadlines[step + 8] != step + 9 || deadlines[step + 9] != step + 12 ||
       deadlines[step + 10] != step + 11 || deadlines[step + 11] != step + 12 ||
       deadlines[step + 12] != step + 13 || deadlines[step + 13] != step + 14 { return false; }
    let a = graph_node(graph, step + 6);
    let b = graph_node(graph, step + 7);
    let glu = graph_node(graph, step + 8);
    let down = graph_node(graph, step + 9);
    let router = graph_node(graph, step + 10);
    let sigmoid = graph_node(graph, step + 11);
    let product = graph_node(graph, step + 12);
    let added = graph_node(graph, step + 13);
    let output = graph_node(graph, step + 14);
    if graph_prop(a, 2) != OP_MUL_MAT || graph_prop(b, 2) != OP_MUL_MAT ||
       graph_prop(glu, 2) != OP_GLU || graph_prop(down, 2) != OP_MUL_MAT ||
       graph_prop(router, 2) != OP_MUL_MAT || graph_prop(sigmoid, 2) != OP_UNARY ||
       graph_prop(product, 2) != OP_MUL || graph_prop(added, 2) != OP_ADD ||
       graph_prop(output, 2) != OP_ADD || storage_get(output, STORAGE_VIEW, 0) != 0 { return false; }

    var s: u64[10] = [0; 10];
    var packet: i64[71] = [0; 71];
    graph_inputs(glu, s[..]);
    let gate = s[0]; let up = s[1];
    if !((gate == a && up == b) || (gate == b && up == a)) { return false; }
    graph_describe(up, packet[..]);
    let hidden = packet[1];
    if hidden <= 0 || hidden > 8192 || hidden % 256 != 0 ||
       !shared_projection(packet[..], width, hidden, 12) { return false; }
    graph_inputs(up, s[..]);
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    let up_weight = s[0];
    graph_inputs(gate, s[..]);
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    let gate_weight = s[0];
    graph_describe(gate, packet[..]);
    if !shared_projection(packet[..], width, hidden, 12) { return false; }
    graph_inputs(down, s[..]);
    let down_weight = s[0];
    if s[1] != glu { return false; }
    graph_describe(down, packet[..]);
    let kind = packet[8];
    if (kind != 12 && kind != 14) || !shared_projection(packet[..], hidden, width, kind) { return false; }
    graph_describe(glu, packet[..]);
    if packet[41] != GLU_OP_SWIGLU || packet[42] != 0 || packet[53] != 0 ||
       !routed_dense(packet_tensor(packet[..], 30), hidden, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_SWIGLU, shared[8 .. 34]);

    graph_inputs(router, s[..]);
    let router_weight = s[0];
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    graph_describe(router, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 0), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 10), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 30), 1, 1, 1, 0) { return false; }
    graph_inputs(sigmoid, s[..]);
    if s[0] != router { return false; }
    graph_describe(sigmoid, packet[..]);
    if packet[41] != UNARY_OP_SIGMOID || !routed_dense(packet_tensor(packet[..], 30), 1, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_SIGMOID, shared[34 .. 60]);
    graph_inputs(product, s[..]);
    if s[0] != down || s[1] != sigmoid { return false; }
    graph_describe(product, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_MUL, shared[60 .. 86]);
    graph_inputs(added, s[..]);
    if s[0] != routed || s[1] != product { return false; }
    graph_describe(added, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_ADD, shared[86 .. 112]);
    graph_inputs(output, s[..]);
    if s[0] != added { return false; }
    let residual = s[1];
    graph_describe(output, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 10), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_ADD, shared[112 .. 138]);

    let tensors: u64[6] = [up_weight, gate_weight, down_weight, router_weight, residual, output];
    for i in tensors.len {
        let address = storage_get(tensors[i], STORAGE_DATA, 0);
        if address == 0 { return false; }
        shared[i] = address as! i64;
    }
    shared[6] = hidden; shared[7] = kind;
    return true;
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
    graph_vector_plan(matrix[..], CV_SWIGLU, glu[..]);

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
    if full_ffn_enabled() {
        var shared: i64[138] = [0; 138];
        if shared_ffn_plan(graph, step, deadlines, plan[3] as! u64, output, width, shared[..]) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[4] as! u64, (experts * 4) as u64) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[5] as! u64, 32) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, shared[4] as! u64, (width * 4) as u64) &&
           (shared[5] == plan[3] || !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[3] as! u64, (width * 4) as u64)) {
            let grouped = routed_graph(plan[..], glu[..], norm[..], shared[..], step, deadlines[step + 14]);
            if grouped != 0 { return if grouped > 0 { 15 } else { grouped }; }
        }
    }
    let destination = plan[6] as! u64;
    if activation_spans_overlap(destination, (width * 4) as u64, plan[3] as! u64, (width * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[4] as! u64, (experts * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[5] as! u64, 32) { return 0; }
    var empty: i64[0] = [];
    let grouped = routed_graph(plan[..], glu[..], norm[..], empty[..], step, deadlines[step + 5]);
    return if grouped > 0 { 6 } else { grouped };
}

fn try_direct_ssm(graph: u64, step: i64, deadlines: const i64[:], concat: u64) -> i64 {
    if step + 3 >= deadlines.len || deadlines[step] != step + 3 || deadlines[step + 1] > step + 2 ||
       deadlines[step + 2] > step + 3 { return 0; }
    let tail = graph_node(graph, step + 1);
    let copy = graph_node(graph, step + 2);
    let conv = graph_node(graph, step + 3);
    if graph_prop(tail, 2) != OP_VIEW || graph_prop(copy, 2) != OP_CPY ||
       graph_prop(conv, 2) != OP_SSM_CONV || storage_get(tail, STORAGE_VIEW, 0) != concat ||
       storage_get(conv, STORAGE_VIEW, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    graph_inputs(concat, inputs[..]);
    let history = inputs[0];
    let transposed = inputs[1];
    if history == 0 || transposed == 0 || graph_prop(transposed, 2) != OP_TRANSPOSE { return 0; }
    graph_inputs(transposed, inputs[..]);
    let mixed = inputs[0];
    graph_inputs(copy, inputs[..]);
    if mixed == 0 || inputs[0] != tail || inputs[1] != history { return 0; }
    graph_inputs(conv, inputs[..]);
    let weights = inputs[1];
    if inputs[0] != concat || weights == 0 { return 0; }
    var p: i64[71] = [0; 71];
    graph_describe(conv, p[..]);
    let channels = p[30]; let tokens = p[31]; let silu = p[41];
    if channels < 128 || channels > 65535 * 128 || tokens <= 32 || tokens > 65535 * 32 ||
       (silu != 0 && silu != 1) || !routed_dense(packet_tensor(p[..], 30), channels, tokens, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 10), 4, channels, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 0), tokens + 3, channels, 1, 0) ||
       storage_get(tail, STORAGE_VIEW_OFFSET, 0) != (4 * tokens) as u64 { return 0; }
    graph_describe(copy, p[..]);
    let v = packet_tensor(p[..], 0);
    if v.kind != 0 || v.ne[0] != 3 || v.ne[1] != channels || v.ne[2] != 1 || v.ne[3] != 1 ||
       v.nb[0] != 4 || v.nb[1] != 4 * (tokens + 3) ||
       !routed_dense(packet_tensor(p[..], 30), 3, channels, 1, 0) { return 0; }
    graph_describe(concat, p[..]);
    let t = packet_tensor(p[..], 10);
    if p[41] != 0 || !routed_dense(packet_tensor(p[..], 0), 3, channels, 1, 0) ||
       t.kind != 0 || t.ne[0] != tokens || t.ne[1] != channels || t.ne[2] != 1 || t.ne[3] != 1 ||
       t.nb[0] != 4 * channels || t.nb[1] != 4 { return 0; }
    let x = storage_get(mixed, STORAGE_DATA, 0);
    let h = storage_get(history, STORAGE_DATA, 0);
    let w = storage_get(weights, STORAGE_DATA, 0);
    let y = storage_get(conv, STORAGE_DATA, 0);
    if x == 0 || h == 0 || w == 0 || y == 0 || x != storage_get(transposed, STORAGE_DATA, 0) ||
       h != storage_get(copy, STORAGE_DATA, 0) { return 0; }
    let plan: i64[7] = [h as! i64, x as! i64, w as! i64, y as! i64, channels, tokens, silu];
    // Use the earlier step to keep the projection live until both launches.
    return direct_ssm(plan[..], step, deadlines[step + 3]);
}

fn try_glu_pack(graph: u64, step: i64, deadlines: const i64[:], glu: u64) -> bool {
    if step + 1 >= deadlines.len || deadlines[step] != step + 1 ||
       storage_get(glu, STORAGE_VIEW, 0) != 0 { return false; }
    let down = graph_node(graph, step + 1);
    if graph_prop(down, 2) != OP_MUL_MAT_ID { return false; }
    var p: i64[71] = [0; 71];
    graph_describe(down, p[..]);
    let tokens = p[12]; let kind = p[8];
    if tokens < 512 || tokens > 4096 || (kind != 12 && kind != 14) ||
       !routed_dense(packet_tensor(p[..], 10), 512, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 30), 2048, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 20), 8, tokens, 1, 26) ||
       p[0] != 512 || p[1] != 2048 || p[2] != 256 || p[3] != 1 { return false; }
    var s: u64[10] = [0; 10];
    graph_inputs(down, s[..]);
    let down_w = s[0]; let ids = s[2];
    if down_w == 0 || ids == 0 || s[1] != glu { return false; }
    let out = storage_get(down, STORAGE_DATA, 0);
    graph_describe(glu, p[..]);
    if p[41] != GLU_OP_SWIGLU || p[42] != 0 || p[53] != 0 ||
       !routed_dense(packet_tensor(p[..], 0), 512, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 10), 512, 8, tokens, 0) { return false; }
    graph_inputs(glu, s[..]);
    let gate = s[0]; let up = s[1];
    if gate == 0 || up == 0 || graph_prop(gate, 2) != OP_MUL_MAT_ID ||
       graph_prop(up, 2) != OP_MUL_MAT_ID { return false; }
    if step < 2 || deadlines[step - 2] != step || deadlines[step - 1] != step { return false; }
    let first = graph_node(graph, step - 2); let second = graph_node(graph, step - 1);
    if !((first == gate && second == up) || (first == up && second == gate)) { return false; }
    let a = storage_get(gate, STORAGE_DATA, 0);
    let b = storage_get(up, STORAGE_DATA, 0);
    let input = storage_get(glu, STORAGE_DATA, 0);
    let size = 4 * 512 * 8 * tokens;
    if out == 0 || a == 0 || b == 0 || input == 0 ||
       activation_spans_overlap(out, (4 * 2048 * 8 * tokens) as u64, a, size as u64) ||
       activation_spans_overlap(out, (4 * 2048 * 8 * tokens) as u64, b, size as u64) { return false; }
    graph_inputs(gate, s[..]);
    let gate_w = s[0];
    if gate_w == 0 || s[2] != ids { return false; }
    graph_inputs(up, s[..]);
    let up_w = s[0];
    if up_w == 0 || s[2] != ids { return false; }
    let plan: i64[8] = [storage_get(up_w, STORAGE_DATA, 0) as! i64,
        storage_get(gate_w, STORAGE_DATA, 0) as! i64, storage_get(down_w, STORAGE_DATA, 0) as! i64,
        a as! i64, b as! i64, input as! i64, size, kind];
    return offer_glu_pack(plan[..]);
}

fn try_rms_scale(graph: u64, step: i64, deadlines: const i64[:], rms: u64) -> i64 {
    if step + 1 >= deadlines.len || deadlines[step] != step + 1 ||
       storage_get(rms, STORAGE_VIEW, 0) != 0 { return 0; }
    let scale = graph_node(graph, step + 1);
    if graph_prop(scale, 2) != OP_SCALE || storage_get(scale, STORAGE_VIEW, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    graph_inputs(scale, inputs[..]);
    if inputs[0] != rms { return 0; }
    var p: i64[71] = [0; 71];
    graph_describe(scale, p[..]);
    let width = p[30]; let heads = p[31]; let tokens = p[32];
    let factor = from_bits(p[41] as! u32);
    if p[42] != 0 || !(factor > 0.0 && factor <= 1.0) ||
       width != 128 || heads < 1 || heads > 64 || tokens < 64 || tokens > 4096 ||
       !routed_dense(packet_tensor(p[..], 30), width, heads, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 0), width, heads, tokens, 0) { return 0; }
    let out = storage_get(scale, STORAGE_DATA, 0);
    graph_describe(rms, p[..]);
    let inp = p[57] as! u64;
    let x = packet_tensor(p[..], 0);
    let eps = from_bits(p[41] as! u32);
    let words = width * heads * tokens;
    if p[55] != 0 || inp == 0 || out == 0 || (inp | out) % 4 != 0 ||
       !(eps >= 0.0 && eps <= 1.0) ||
       x.ne[0] != width || x.ne[1] != heads || x.ne[2] != tokens || x.ne[3] != 1 ||
       x.nb[0] != 4 || !cv_layout(x, x.bytes) ||
       activation_spans_overlap(inp, x.bytes as u64, out, (words * 4) as u64) { return 0; }
    var cfg: i64[26] = [0; 26];
    cfg[0] = CV_RMS; cfg[1] = words;
    for i in 4 {
        cfg[2 + i] = x.ne[i]; cfg[6 + i] = x.ne[i]; cfg[10 + i] = x.nb[i];
    }
    cfg[23] = 16 | (if x.bytes > words * 4 { 1 } else { 0 });
    cfg[24] = heads * tokens; cfg[25] = 32;
    return rms_scale(cfg[..], inp, x.bytes, out, words * 4, step, deadlines[step + 1], eps, factor);
}

fn try_fwht_q4(graph: u64, step: i64, deadlines: const i64[:], fwht: u64) -> i64 {
    if step + 2 >= deadlines.len || deadlines[step] != step + 2 ||
       deadlines[step + 1] > step + 2 || storage_get(fwht, STORAGE_VIEW, 0) != 0 { return 0; }
    let view = graph_node(graph, step + 1);
    let update = graph_node(graph, step + 2);
    if graph_prop(view, 2) != OP_VIEW || graph_prop(update, 2) != OP_SET_ROWS ||
       storage_get(view, STORAGE_VIEW, 0) != fwht || storage_get(view, STORAGE_VIEW_OFFSET, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    graph_inputs(update, inputs[..]);
    if inputs[0] != view || inputs[1] == 0 { return 0; }
    let ids_tensor = inputs[1];
    let target = storage_get(update, STORAGE_DATA, 0);
    var p: i64[71] = [0; 71];
    graph_describe(update, p[..]);
    let width = p[0]; let rows = p[1]; let capacity = p[31];
    if width <= 0 || width > 65536 || width % 32 != 0 || rows < 64 || rows > 4096 ||
       !routed_dense(packet_tensor(p[..], 0), width, rows, 1, 0) ||
       p[38] != 2 || p[30] != width || p[32] != 1 || p[33] != 1 ||
       p[34] != 18 || p[35] != width / 32 * 18 || capacity < rows || target == 0 { return 0; }
    let stride = p[35];
    let target_bytes = p[39];
    let id_shape = packet_tensor(p[..], 10);
    if (id_shape.kind != 26 && id_shape.kind != 27) || id_shape.ne[0] != rows ||
       id_shape.ne[1] != 1 || id_shape.ne[2] != 1 || id_shape.ne[3] != 1 ||
       !dense_scalar(id_shape, if id_shape.kind == 27 { 8 } else { 4 }) { return 0; }
    graph_describe(fwht, p[..]);
    let rotation = p[42];
    let x = packet_tensor(p[..], 0);
    let input = p[57] as! u64;
    if p[41] != 0x46574854 || (rotation != 64 && rotation != 256) || width % rotation != 0 ||
       x.kind != 0 || x.ne[0] % rotation != 0 || !dense_scalar(x, 4) ||
       x.bytes != 4 * width * rows || p[39] != x.bytes || input == 0 { return 0; }
    let ids = storage_get(ids_tensor, STORAGE_DATA, 0);
    if ids == 0 || id_shape.bytes != rows * (if id_shape.kind == 27 { 8 } else { 4 }) { return 0; }
    if activation_read_address(ids, id_shape.bytes) != 0 { return -1; }
    let first = graph_read_id(ids, 0, id_shape.kind == 27);
    if first < 0 || first > capacity - rows || (first + rows) > target_bytes / stride { return 0; }
    for i in rows {
        if graph_read_id(ids, i, id_shape.kind == 27) != first + i { return 0; }
    }
    return fwht_q4_rows(input, target + ((first * stride) as u64), width, rows, rotation as i32, 1.0 / sqrt32(rotation as f32));
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
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == OP_CUSTOM && fwht_q4_enabled() {
            let fused = try_fwht_q4(graph, i, deadlines, tensor);
            if fused > 0 { i += 3; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == OP_RMS_NORM {
            let fused = try_rms_scale(graph, i, deadlines, tensor);
            if fused > 0 { i += 2; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == OP_GLU &&
           try_glu_pack(graph, i, deadlines, tensor) {
            // Keep both operands alive at the SwiGLU step until packing finishes.
            let ok = execute_graph_node(graph_node(graph, i + 1), work, selective, i, deadlines[i + 1]);
            let used = close_glu_pack();
            if !ok || !used { outcome = i + 1; break; }
            i += 2;
            continue;
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == OP_CONCAT && direct_ssm_enabled() {
            let grouped = try_direct_ssm(graph, i, deadlines, tensor);
            if grouped > 0 { i += 4; continue; }
            if grouped < 0 { outcome = i + 1; break; }
        }
        if use_routed_graph && graph_prop(tensor, 2) == OP_MUL_MAT_ID {
            let grouped = try_routed_graph(graph, i, deadlines);
            if grouped > 0 { i += grouped; continue; }
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
