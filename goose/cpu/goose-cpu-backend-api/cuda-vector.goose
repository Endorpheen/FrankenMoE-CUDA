extern "fg_cuda_vector" fn cuda_vector(config: const i64[:], x: const u8[:], y: const u8[:],
    dst: u8[:], param0: f32, param1: f32) -> i64;

fn cv_operation(p: const Node &) -> i64 {
    if p.op == OP_ADD || p.op == OP_ADD1 { return CV_ADD; }
    if p.op == OP_SUB { return CV_SUB; }
    if p.op == OP_MUL { return CV_MUL; }
    if p.op == OP_DIV { return CV_DIV; }
    if p.op == OP_SCALE { return CV_SCALE; }
    if p.op == OP_CLAMP { return CV_CLAMP; }
    if p.op == OP_RMS_NORM { return CV_RMS; }
    if p.op == OP_L2_NORM { return CV_L2; }
    if p.op == OP_GLU && p.ip[0] == GLU_OP_SWIGLU && p.ip[12] == 0 { return CV_SWIGLU; }
    if p.op == OP_UNARY {
        if p.ip[0] == UNARY_OP_SIGMOID { return CV_SIGMOID; }
        if p.ip[0] == UNARY_OP_SILU { return CV_SILU; }
        if p.ip[0] == UNARY_OP_SOFTPLUS { return CV_SOFTPLUS; }
    }
    return 0;
}

fn cv_layout(t: const Tensor &, available: i64) -> bool {
    if t.kind != 0 || t.bytes <= 0 || t.bytes > available { return false; }
    for axis in 4 {
        if t.ne[axis] <= 0 || t.nb[axis] < 0 || t.nb[axis] % 4 != 0 { return false; }
    }
    validate(t, available);
    return true;
}

fn cv_multiply_windows(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], plan: i64[:]) -> i64 {
    let capacity = high(1, (16 * 1024 * 1024) / p.d.nb[2]);
    plan[23] = 4;
    var offset: i64 = 0;
    while offset < p.d.ne[2] {
        let count = low(capacity, p.d.ne[2] - offset);
        let first_x = offset * p.x.nb[2];
        let first_y = offset * p.y.nb[2];
        let first_d = offset * p.d.nb[2];
        plan[1] = count * (p.d.nb[2] / 4);
        plan[4] = count;
        plan[8] = count;
        plan[16] = count;
        plan[24] = 1 + (plan[1] - 1) / 128;
        let result = cuda_vector(plan, x[first_x .. first_x + count * p.x.nb[2]],
            y[first_y .. first_y + count * p.y.nb[2]], dst[first_d .. first_d + count * p.d.nb[2]],
            p.fp[0], p.fp[1]);
        if result <= 0 { return if offset == 0 { result } else { -1 }; }
        offset += count;
    }
    return 1;
}

fn cv_try(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], addresses: const i64[3] &) -> i64 {
    let op = cv_operation(p);
    if op == 0 || p.nth != 1 || !cv_layout(p.d, dst.len) || !dense_scalar(p.d, 4) ||
       !cv_layout(p.x, x.len) {
        return 0;
    }
    let norm = op == CV_RMS || op == CV_L2;
    let packed = op == CV_SWIGLU && p.ip[2] == 0;
    let pair = op <= CV_DIV || (op == CV_SWIGLU && !packed) || (op == CV_RMS && p.ip[14] != 0);
    if pair && !cv_layout(p.y, y.len) { return 0; }
    if norm && !same_shape(p.x, p.d) { return 0; }
    if op == CV_SWIGLU && !packed && (!same_shape(p.x, p.d) || !same_shape(p.y, p.d)) { return 0; }
    for axis in 4 {
        if packed {
            let extent = if axis == 0 { mul_size(p.d.ne[axis], 2) } else { p.d.ne[axis] };
            if p.x.ne[axis] != extent { return 0; }
        } else if p.d.ne[axis] % p.x.ne[axis] != 0 { return 0; }
        if pair && p.d.ne[axis] % p.y.ne[axis] != 0 { return 0; }
    }
    let count = elements(p.d);
    let groups = if norm { rows(p.d) } else { 1 + (count - 1) / 128 };
    if groups > 2147483647 { return 0; }
    var config: i64[26] = [0; 26];
    config[0] = op;
    config[1] = count;
    for axis in 4 {
        config[2 + axis] = p.d.ne[axis];
        config[6 + axis] = p.x.ne[axis];
        config[10 + axis] = p.x.nb[axis];
        config[14 + axis] = p.y.ne[axis];
        config[18 + axis] = p.y.nb[axis];
    }
    config[22] = if !packed { 0 } else if p.ip[1] != 0 { 2 } else { 1 };
    let pack_x = p.x.nb[0] == 4 && p.x.bytes > mul_size(elements(p.x), 4);
    let pack_y = pair && p.y.nb[0] == 4 && p.y.bytes > mul_size(elements(p.y), 4);
    config[23] = (if pack_x { 1 } else { 0 }) | (if pack_y { 2 } else { 0 });
    config[24] = groups;
    config[25] = if norm { 32 } else { 128 };
    let y_bytes = if pair { p.y.bytes } else { 0 };
    // The full MoE product can exceed the available staging memory. Its
    // token planes are independent and can be published directly to RAM.
    if op == CV_MUL && p.d.bytes > 32 * 1024 * 1024 &&
       p.d.ne[2] > 1 && p.d.ne[3] == 1 && same_shape(p.x, p.d) &&
       p.y.ne[2] == p.d.ne[2] && p.y.ne[3] == 1 &&
       dense_scalar(p.x, 4) && dense_scalar(p.y, 4) &&
       addresses[0] != 0 && addresses[1] != 0 && addresses[2] != 0 &&
       blit_disjoint(addresses[0] as! u64, p.x.bytes, addresses[2] as! u64, p.d.bytes) &&
       blit_disjoint(addresses[1] as! u64, p.y.bytes, addresses[2] as! u64, p.d.bytes) {
        return cv_multiply_windows(p, x, y, dst, config[..]);
    }
    return cuda_vector(config, x[0 .. p.x.bytes], y[0 .. y_bytes], dst[0 .. p.d.bytes], p.fp[0], p.fp[1]);
}
