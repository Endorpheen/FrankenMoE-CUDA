fn packet_tensor(packet: const i64[:], at: i64) -> Tensor {
    var t: Tensor = default <Tensor>();
    for axis in 4 {
        t.ne[axis] = packet[at + axis];
        t.nb[axis] = packet[at + 4 + axis];
    }
    t.kind = packet[at + 8];
    t.bytes = packet[at + 9];
    return t;
}

export fn fg143_node_run(packet: const i64[:], mode: i64,
    x: const u8[:], y: const u8[:], z: const u8[:], mask: const u8[:], dst: u8[:],
    scratch: f32[:], indices: i64[:],
    gxx: const u8[8][:], gxs: const u8[8][:], gs: const u8[8][:],
    g3x: const u8[4][:], g3s: const u8[4][:], g1: const i8[8][:],
    signs: const u8[:], palette: const i8[:]) -> bool {
    if packet.len != 71 { return false; }
    var p: Node = default <Node>();
    p.x = packet_tensor(packet, 0);
    p.y = packet_tensor(packet, 10);
    p.z = packet_tensor(packet, 20);
    p.d = packet_tensor(packet, 30);
    p.op = packet[40];
    p.nth = 1;
    for i in 16 { p.ip[i] = packet[41 + i]; }
    for i in 8 { p.fp[i] = from_bits(p.ip[i] as! u32); }
    // GGML stores the original kernel parameters, while the CPU kernels take
    // normalized dimensions, operand flags and floating-point parameters.
    if p.op == OP_ROPE || p.op == OP_ROPE_BACK {
        p.ip[0] = packet[42];
        p.ip[1] = packet[43];
        p.ip[2] = packet[45];
        p.ip[3] = packet[56];
        for i in 4 { p.ip[4 + i] = packet[52 + i]; }
        p.ip[8] = if p.z.bytes > 0 { 1 } else { 0 };
        for i in 6 { p.fp[i] = from_bits(packet[46 + i] as! u32); }
    } else if p.op == OP_SOFT_MAX {
        p.ip[0] = if p.y.bytes > 0 { 1 } else { 0 };
        p.ip[1] = if p.z.bytes > 0 { 1 } else { 0 };
    } else if p.op == OP_GLU {
        p.ip[2] = if p.y.bytes > 0 { 1 } else { 0 };
        p.fp[0] = from_bits(packet[43] as! u32);
        p.fp[1] = from_bits(packet[44] as! u32);
    } else if p.op == OP_UNARY {
        for i in 4 { p.fp[i] = from_bits(packet[42 + i] as! u32); }
    }
    if mode == 1 { p.ip[15] = 1464816203; }
    if mode == 0 && p.op == OP_FLASH_ATTN_EXT {
        if packet[69] != 0 { return false; }
        let mask_shape = packet_tensor(packet, 59);
        return flash_attention(p, mask_shape, x, y, z, mask, dst, palette);
    }
    if mode == 0 && p.op == OP_SET_ROWS && p.d.kind == 2 {
        return set_quantized_rows(p, packet[57] as! u64, packet[58] as! u64, y);
    }
    let addresses: i64[3] = [packet[57], packet[70], packet[58]];
    return entry(p, x, y, z, dst, scratch, indices, addresses, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
}

fn state_plan(packet: const i64[:]) -> StatePlan {
    var p: StatePlan = default <StatePlan>();
    for i in 7 { p.source[i] = packet_tensor(packet, i * 10); }
    p.dst = packet_tensor(packet, 70);
    p.nth = 1;
    p.op = packet[80];
    p.snapshots = packet[81];
    return p;
}

export fn fg143_gdn_run(packet: const i64[:],
    q: const f32[:], k: const f32[:], v: const f32[:], gate: const f32[:],
    beta: const f32[:], initial: const f32[:], dst: f32[:]) -> bool {
    if packet.len != 82 || packet[80] != OP_GATED_DELTA_NET { return false; }
    let p = state_plan(packet);
    return gdn(p, q, k, v, gate, beta, initial, dst);
}

export fn fg143_state_run(packet: const i64[:],
    a0: const u8[:], a1: const u8[:], a2: const u8[:], a3: const u8[:],
    a4: const u8[:], a5: const u8[:], a6: const u8[:], dst: u8[:]) -> bool {
    if packet.len != 82 { return false; }
    let p = state_plan(packet);
    return state_entry(p, a0, a1, a2, a3, a4, a5, a6, dst);
}

extern "fg143_graph_nodes" fn graph_nodes(graph: u64) -> i64;
extern "fg143_graph_exec_node" fn graph_exec_node(graph: u64, workspace: u64, index: i64) -> bool;

// Return one plus the first failing node index, or zero after the last node.
export fn fg143_graph_run(graph: u64, workspace: u64) -> i64 {
    if graph == 0 || workspace == 0 { return -1; }
    let n = graph_nodes(graph);
    for i in n {
        if !graph_exec_node(graph, workspace, i) { return i + 1; }
    }
    return 0;
}

extern "qz_quantize_row" fn quantize_row(kind: i64, source: u64, destination: u64, width: i64) -> i64;

fn set_quantized_rows(p: const Node &, source: u64, destination: u64, ids: const u8[:]) -> bool {
    let width = p.x.ne[0];
    if p.d.kind != 2 || p.x.kind != 0 || width <= 0 || width % 32 != 0 ||
       p.d.ne[0] != width || p.x.nb[0] != 4 || p.d.nb[0] != 18 ||
       p.x.ne[2] != p.d.ne[2] || p.x.ne[3] != p.d.ne[3] ||
       (p.y.kind != 26 && p.y.kind != 27) || p.y.ne[0] != p.x.ne[1] ||
       p.y.ne[1] <= 0 || p.y.ne[2] <= 0 || source == 0 || destination == 0 {
        return false;
    }
    validate(p.y, ids.len);
    let row_bytes = (width / 32) * 18;
    for row in rows(p.x) {
        let r = row % p.x.ne[1];
        let head = (row / p.x.ne[1]) % p.x.ne[2];
        let batch = row / (p.x.ne[1] * p.x.ne[2]);
        let id = read_index(p.y, ids, r + p.y.ne[0] * ((head % p.y.ne[1]) + p.y.ne[1] * (batch % p.y.ne[2])));
        if id < 0 || id >= p.d.ne[1] { return false; }
        let from = r * p.x.nb[1] + head * p.x.nb[2] + batch * p.x.nb[3];
        let to = id * p.d.nb[1] + head * p.d.nb[2] + batch * p.d.nb[3];
        if from < 0 || width * 4 > p.x.bytes - from || to < 0 || row_bytes > p.d.bytes - to {
            return false;
        }
        if quantize_row(p.d.kind, source + (from as u64), destination + (to as u64), width) != row_bytes {
            return false;
        }
    }
    return true;
}
