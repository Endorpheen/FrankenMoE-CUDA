extern "fg_cuda_blit" fn cuda_blit(config: const i64[:], x: const u8[:], y: const u8[:], dst: u8[:]) -> i64;

fn blit_disjoint(a: u64, a_bytes: i64, b: u64, b_bytes: i64) -> bool {
    return if a <= b { b - a >= a_bytes as u64 } else { a - b >= b_bytes as u64 };
}

fn blit_cuda(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], addresses: const i64[3] &) -> i64 {
    let op = p.op;
    let mode = if op == OP_CONCAT { 0 } else if op == OP_CONT || op == OP_DUP { 1 }
        else if op == OP_CPY { 2 } else { -1 };
    if mode < 0 || p.nth != 1 || !cv_layout(p.x, x.len) || !cv_layout(p.d, dst.len) ||
       !dense_scalar(p.d, 4) || addresses[0] == 0 || addresses[2] == 0 { return 0; }
    let target = addresses[2] as! u64;
    if !blit_disjoint(addresses[0] as! u64, p.x.bytes, target, p.d.bytes) { return 0; }
    let count = elements(p.d);
    let groups = 1 + (count - 1) / 128;
    if groups > 2147483647 { return 0; }
    var axis: i64 = 0;
    var left: i64 = 0;
    if mode == 0 {
        axis = p.ip[0];
        if axis < 0 || axis >= 4 || addresses[1] == 0 || !cv_layout(p.y, y.len) ||
           !blit_disjoint(addresses[1] as! u64, p.y.bytes, target, p.d.bytes) { return 0; }
        left = p.x.ne[axis];
        for i in 4 {
            if i == axis {
                if p.d.ne[i] != add_size(left, p.y.ne[i]) { return 0; }
            } else if p.d.ne[i] != p.x.ne[i] || p.d.ne[i] != p.y.ne[i] { return 0; }
        }
    } else if count != elements(p.x) { return 0; }
    var config: i64[17] = [0; 17];
    config[0] = mode;
    config[1] = groups;
    config[2] = count;
    config[3] = axis;
    config[4] = left;
    for i in 4 {
        config[5 + i] = if mode == 0 { p.d.ne[i] } else { p.x.ne[i] };
        config[9 + i] = p.x.nb[i];
        config[13 + i] = p.y.nb[i];
    }
    let y_bytes = if mode == 0 { p.y.bytes } else { 0 };
    return cuda_blit(config, x[0 .. p.x.bytes], y[0 .. y_bytes], dst[0 .. p.d.bytes]);
}
