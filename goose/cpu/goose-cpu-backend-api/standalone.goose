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

extern "fg_cuda_dsa_score" fn cuda_dsa_score(config: i64[:], q: const u8[:], k: const u8[:],
    weights: const u8[:], masks: const u8[:], dst: u8[:]) -> i64;

fn dispatch_dsa_score(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
                      weights: const u8[:], masks: const u8[:], dst: u8[:]) -> bool {
    let queries = p.x.ne[2];
    let cells = p.y.ne[2];
    if queries < 1 || queries > 4096 || cells < 1 || cells > 1048576 ||
       p.x.kind != 0 || p.y.kind != 1 || p.z.kind != 0 || mask.kind != 1 || p.d.kind != 0 ||
       p.x.ne[0] != 128 || p.x.ne[1] != 64 || p.x.ne[3] != 1 ||
       p.x.nb[0] != 4 || p.x.nb[1] != 512 || p.x.nb[2] != 32768 ||
       p.y.ne[0] != 128 || p.y.ne[1] != 1 || p.y.ne[3] != 1 ||
       p.y.nb[0] != 2 || p.y.nb[2] < 256 || p.y.nb[2] % 2 != 0 ||
       p.z.ne[0] != 64 || p.z.ne[1] != queries || p.z.ne[2] != 1 || p.z.ne[3] != 1 ||
       p.z.nb[0] != 4 || p.z.nb[1] != 256 ||
       mask.ne[0] != cells || mask.ne[1] != queries || mask.ne[2] != 1 || mask.ne[3] != 1 ||
       mask.nb[0] != 2 || mask.nb[1] < 2 * cells || mask.nb[1] % 2 != 0 ||
       p.d.ne[0] != cells || p.d.ne[1] != queries || p.d.bytes != 4 * cells * queries ||
       p.x.bytes > q.len || p.y.bytes > k.len || p.z.bytes > weights.len ||
       mask.bytes > masks.len || p.d.bytes > dst.len { return false; }
    var config: i64[6] = [cells, queries, p.y.nb[2], p.x.nb[2], p.z.nb[1], mask.nb[1]];
    return cuda_dsa_score(config, q[0 .. p.x.bytes], k[0 .. p.y.bytes],
        weights[0 .. p.z.bytes], masks[0 .. mask.bytes], dst[0 .. p.d.bytes]) == 1;
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
    } else if p.op == OP_TOP_K {
        p.ip[0] = 1;
    } else if p.op == OP_GLU {
        p.ip[2] = if p.y.bytes > 0 { 1 } else { 0 };
        p.fp[0] = from_bits(packet[43] as! u32);
        p.fp[1] = from_bits(packet[44] as! u32);
    } else if p.op == OP_UNARY {
        for i in 4 { p.fp[i] = from_bits(packet[42 + i] as! u32); }
    }
    if mode == 1 { p.ip[15] = 1464816203; }
    if p.op == OP_CUSTOM && p.ip[0] == 0x46574854 {
        return mode == 1 || kv_fwht(p, x, dst);
    }
    if p.op == OP_CUSTOM && p.ip[0] == 0x4d4f4557 {
        return mode == 1 || moe_weighted(p, x, y, dst);
    }
    if p.op == OP_CUSTOM && p.ip[0] == 0x4d4f454e {
        return mode == 1 || moe_normalize(p, x, y, dst);
    }
    if mode == 0 && p.op == OP_LIGHTNING_INDEXER {
        let mask_shape = packet_tensor(packet, 59);
        return dispatch_dsa_score(p, mask_shape, x, y, z, mask, dst);
    }
    if mode == 0 && p.op == OP_FLASH_ATTN_EXT {
        if packet[69] != 0 { return false; }
        let mask_shape = packet_tensor(packet, 59);
        return flash_attention(p, mask_shape, x, y, z, mask, dst, palette);
    }
    if mode == 0 && p.op == OP_SET_ROWS && p.d.kind == 2 {
        return set_quantized_rows(p, packet[57] as! u64, packet[58] as! u64, y, indices);
    }
    let addresses: i64[3] = [packet[57], packet[70], packet[58]];
    return entry(p, x, y, z, dst, scratch, indices, addresses, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
}

extern "fg_cuda_fwht" fn cuda_fwht(src: const u8[:], dst: u8[:], width: i32, rows: i64, scale: f32) -> i64;

fn kv_fwht(p: const Node &, src: const u8[:], dst: u8[:]) -> bool {
    let width = p.ip[1];
    if width < 64 || width > 0x7fffffff || width > p.x.ne[0] || (width & (width - 1)) != 0 || p.x.ne[0] % width != 0 ||
       p.x.kind != 0 || p.d.kind != 0 || !dense_scalar(p.x, 4) || !dense_scalar(p.d, 4) ||
       src.len != p.x.bytes || src.len != dst.len || src.len <= 0 || src.len % (4 * width) != 0 { return false; }
    let count = src.len / (4 * width);
    let scale = 1.0 / sqrt32(width as f32);
    let device = cuda_fwht(src, dst, width as i32, count, scale);
    if device != 0 { return device > 0; }
    if activation_read(src) != 0 { return false; }
    for i in src.len / 4 {
        let byte = i * 4;
        write32(dst, byte, bits(from_bits(read32(src, byte)) * scale));
    }
    for row in count {
        let start = row * width * 4;
        var step: i64 = 1;
        while step < width {
            for group in width / (2 * step) {
                for lane in step {
                    let a = start + (group * 2 * step + lane) * 4;
                    let b = a + step * 4;
                    let av = from_bits(read32(dst, a));
                    let bv = from_bits(read32(dst, b));
                    write32(dst, a, bits(av + bv));
                    write32(dst, b, bits(av - bv));
                }
            }
            step *= 2;
        }
    }
    return true;
}

extern "fg_cuda_moe_weighted" fn cuda_moe_weighted(x: const u8[:], weights: const u8[:], dst: u8[:],
    width: i64, used: i64, count: i64, tile: i64) -> i64;

fn moe_weighted(p: const Node &, x: const u8[:], w: const u8[:], dst: u8[:]) -> bool {
    let width = p.x.ne[0];
    let used = p.x.ne[1];
    let count = p.x.ne[2];
    if width <= 0 || width > 65535 * 256 || used <= 0 || used > 64 || count <= 0 || count > 0x7fffffff ||
       p.x.ne[3] != 1 || p.y.ne[0] != 1 || p.y.ne[1] != used || p.y.ne[2] != count || p.y.ne[3] != 1 ||
       p.d.ne[0] != width || p.d.ne[1] != count || p.d.ne[2] != 1 || p.d.ne[3] != 1 ||
       p.x.kind != 0 || p.y.kind != 0 || p.d.kind != 0 || !dense_scalar(p.x, 4) || !dense_scalar(p.y, 4) ||
       !dense_scalar(p.d, 4) || x.len != p.x.bytes || w.len != p.y.bytes || dst.len != p.d.bytes { return false; }
    let input_row = width * used * 4;
    let tile = high(1, (16 * 1024 * 1024) / input_row);
    let device = cuda_moe_weighted(x, w, dst, width, used, count, low(count, tile));
    if device != 0 { return device > 0; }
    if activation_read(x) != 0 || activation_read(w) != 0 { return false; }
    for row in count {
        let xbase = row * input_row;
        let wbase = row * used * 4;
        for col in width {
            let byte = col * 4;
            var sum = from_bits(read32(x, xbase + byte)) * from_bits(read32(w, wbase));
            for e in 1 .. used {
                let product = from_bits(read32(x, xbase + e * width * 4 + byte)) * from_bits(read32(w, wbase + e * 4));
                sum += product;
            }
            write32(dst, row * width * 4 + byte, bits(sum));
        }
    }
    return true;
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
    if (packet.len != 82 && packet.len != 83) || packet[80] != OP_GATED_DELTA_NET { return false; }
    var p = state_plan(packet);
    p.publish = packet.len == 83 && packet[82] == 1;
    return gdn(p, q, k, v, gate, beta, initial, dst);
}

export fn fg143_state_run(packet: const i64[:],
    a0: const u8[:], a1: const u8[:], a2: const u8[:], a3: const u8[:],
    a4: const u8[:], a5: const u8[:], a6: const u8[:], dst: u8[:]) -> bool {
    if packet.len != 82 { return false; }
    if activation_read(a0) != 0 || activation_read(a1) != 0 || activation_read(a2) != 0 ||
       activation_read(a3) != 0 || activation_read(a4) != 0 || activation_read(a5) != 0 ||
       activation_read(a6) != 0 { return false; }
    let p = state_plan(packet);
    return state_entry(p, a0, a1, a2, a3, a4, a5, a6, dst);
}

extern "qz_quantize_row" fn quantize_row(kind: i64, source: u64, destination: u64, width: i64) -> i64;
extern "fg_cuda_set_q4_rows" fn cuda_set_q4_rows(source: u64, source_bytes: i64, stride: i64,
    destination: u64, destination_bytes: i64, width: i64, count: i64) -> i64;

fn set_quantized_rows(p: const Node &, source: u64, destination: u64, ids: const u8[:], work: i64[:]) -> bool {
    let width = p.x.ne[0];
    if p.d.kind != 2 || p.x.kind != 0 || width <= 0 || width % 32 != 0 ||
       p.d.ne[0] != width || p.x.nb[0] != 4 || p.d.nb[0] != 18 ||
       p.x.ne[2] != p.d.ne[2] || p.x.ne[3] != p.d.ne[3] ||
       (p.y.kind != 26 && p.y.kind != 27) || p.y.ne[0] != p.x.ne[1] ||
       p.y.ne[1] <= 0 || p.y.ne[2] <= 0 || source == 0 || destination == 0 {
        return false;
    }
    validate(p.y, ids.len);
    if activation_read(ids) != 0 { return false; }
    let row_bytes = (width / 32) * 18;
    let count = rows(p.x);
    if work.len / 2 < count { return false; }
    if count == 0 { return true; }
    var packed = p.x.ne[2] == 1 && p.x.ne[3] == 1 && p.x.nb[1] >= width * 4 && p.x.nb[1] % 4 == 0;
    var first = p.d.bytes;
    var last: i64 = 0;
    for row in count {
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
        if row != 0 && to != last { packed = false; }
        first = low(first, to);
        last = high(last, to + row_bytes);
        work[2 * row] = from;
        work[2 * row + 1] = to;
    }
    let target = destination + (first as u64);
    let touched = last - first;
    if packed {
        let source_bytes = work[2 * (count - 1)] + width * 4;
        let device = cuda_set_q4_rows(source, source_bytes, p.x.nb[1], target, touched, width, count);
        if device != 0 { return device > 0; }
    } else if cuda_state_read(target, touched) != 0 {
        // Preserve the previous GPU bytes in the holes of a sparse update.
        return false;
    }
    if activation_read_address(source, p.x.bytes) != 0 { return false; }
    for row in count {
        let from = source + (work[2 * row] as u64);
        let to = destination + (work[2 * row + 1] as u64);
        if quantize_row(p.d.kind, from, to, width) != row_bytes { return false; }
    }
    return cuda_state_write(target, touched) == 0;
}
