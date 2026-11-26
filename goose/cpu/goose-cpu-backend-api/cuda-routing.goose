extern "fg_cuda_routing" fn cuda_routing(config: const i64[:], x: const u8[:], y: const u8[:], dst: u8[:]) -> i64;

fn moe_normalize(p: const Node &, probs: const u8[:], ids: const u8[:], dst: u8[:]) -> bool {
    let experts = p.x.ne[0]; let used = p.y.ne[0]; let tokens = p.x.ne[1];
    if experts <= 0 || used <= 0 || used > experts || used > 64 || tokens <= 0 || tokens > 2147483647 ||
       p.x.kind != 0 || p.y.kind != 26 || p.d.kind != 0 || p.x.ne[2] != 1 || p.x.ne[3] != 1 ||
       p.y.ne[1] != tokens || p.y.ne[2] != 1 || p.y.ne[3] != 1 ||
       p.d.ne[0] != 1 || p.d.ne[1] != used || p.d.ne[2] != tokens || p.d.ne[3] != 1 ||
       !dense_scalar(p.x, 4) || !dense_scalar(p.y, 4) || !dense_scalar(p.d, 4) ||
       p.x.bytes > probs.len || p.y.bytes > ids.len || p.d.bytes > dst.len { return false; }
    let scale = p.fp[1];
    if !(scale >= 0.0 && scale <= 3.4028234663852886e38) { return false; }
    var cfg: i64[22] = [0; 22];
    cfg[0] = 4; cfg[1] = tokens; cfg[2] = 32;
    cfg[4] = experts; cfg[12] = used; cfg[21] = p.ip[1];
    let cuda = cuda_routing(cfg, probs[0 .. p.x.bytes], ids[0 .. p.y.bytes], dst[0 .. p.d.bytes]);
    if cuda != 0 { return cuda > 0; }
    if activation_read(probs) != 0 || activation_read(ids) != 0 { return false; }
    for row in tokens {
        var picked: f32[64] = default <f32[64]>();
        var sum: f64 = 0.0;
        for slot in used {
            let id = read32(ids, 4 * (row * used + slot)) as! i32;
            if id < 0 || (id as i64) >= experts { return false; }
            picked[slot] = from_bits(read32(probs, 4 * (row * experts + (id as i64))));
            sum += picked[slot] as f64;
        }
        let denominator = max32(sum as f32, 0.00006103515625);
        for slot in used {
            let weight = picked[slot] / denominator;
            let result = if scale == 0.0 || scale == 1.0 { weight } else { weight * scale + 0.0 };
            write32(dst, 4 * (row * used + slot), bits(result));
        }
    }
    return true;
}

extern "fg_cuda_giga_dsa_top_wide" fn cuda_giga_dsa_top_wide(x: const u8[:], dst: u8[:], cells: i64, rows: i64) -> i64;

fn routing_cuda(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> i64 {
    if p.op == OP_TOP_K && p.ip[13] == 1 && p.nth == 1 && p.x.kind == 0 && p.d.kind == 26 &&
        p.x.ne[0] > 4096 && p.d.ne[0] == 2048 && p.x.ne[1] == p.d.ne[1] &&
        p.x.ne[2] == 1 && p.x.ne[3] == 1 && p.d.ne[2] == 1 && p.d.ne[3] == 1 &&
        dense_scalar(p.x, 4) && dense_scalar(p.d, 4) && p.x.bytes <= x.len && p.d.bytes <= dst.len {
        let wide = cuda_giga_dsa_top_wide(x[0..p.x.bytes],dst[0..p.d.bytes],p.x.ne[0],p.x.ne[1]);
        if wide != 0 { return wide; }
    }
    let kind = if p.op == OP_SOFT_MAX { 0 } else if p.op == OP_ARGSORT || p.op == OP_TOP_K { 1 }
        else if p.op == OP_GET_ROWS { 2 } else if p.op == OP_SUM_ROWS { 3 } else { -1 };
    if kind < 0 || p.nth != 1 || !cv_layout(p.x, x.len) || p.d.bytes <= 0 ||
       (if kind == 1 { p.d.kind != 26 } else { p.d.kind != 0 }) || !dense_scalar(p.d, 4) { return 0; }
    validate(p.d, dst.len);
    let width = p.x.ne[0];
    let mutates_rows = kind == 1 || kind == 3;
    if mutates_rows {
        for axis in 3 {
            if p.d.ne[axis + 1] != p.x.ne[axis + 1] { return 0; }
        }
    }
    var detail: i64 = 0;
    var param: i64 = 0;
    var groups = rows(p.x);
    var lanes: i64 = 32;
    let compact_topk = p.op == OP_TOP_K && p.ip[14] == 1 && width == 256 && p.d.ne[0] == 8 &&
        p.x.ne[2] == 1 && p.x.ne[3] == 1;
    if kind == 0 {
        if width > 4096 || !same_shape(p.x, p.d) || p.ip[0] != 0 || p.ip[1] != 0 ||
           !(p.fp[0] > 0.0 && p.fp[0] <= 1.0) { return 0; }
        param = bits(p.fp[0]) as i64;
    } else if kind == 1 {
        if width > 4096 || p.x.nb[0] != 4 || p.d.ne[0] > width ||
           (p.ip[0] != 0 && p.ip[0] != 1) { return 0; }
        detail = 1;
        while detail < width { detail *= 2; }
        lanes = if detail <= 32 { 32 } else { 128 };
        param = p.ip[0];
    } else if kind == 2 {
        if p.y.kind != 26 || p.y.ne[3] != 1 || p.d.ne[0] != width ||
           p.y.ne[0] != p.d.ne[1] || p.y.ne[1] != p.d.ne[2] || p.y.ne[2] != p.d.ne[3] ||
           p.y.ne[1] != p.x.ne[2] || p.y.ne[2] != p.x.ne[3] { return 0; }
        for axis in 3 {
            if p.y.nb[axis] < 0 || p.y.nb[axis] % 4 != 0 { return 0; }
        }
        validate(p.y, y.len);
        detail = elements(p.d);
        lanes = 128;
        groups = 1 + (detail - 1) / lanes;
    } else if p.d.ne[0] != 1 || width > 4096 { return 0; }
    if compact_topk { groups = 1 + (groups - 1) / 4; }
    if groups <= 0 || groups > 2147483647 { return 0; }
    var config: i64[22] = [0; 22];
    config[0] = kind;
    config[1] = groups;
    config[2] = lanes;
    config[3] = if compact_topk { 1 } else { 0 };
    for axis in 4 {
        config[4 + axis] = p.x.ne[axis];
        config[8 + axis] = p.x.nb[axis];
        config[12 + axis] = p.d.ne[axis];
        config[16 + axis] = p.y.nb[axis];
    }
    config[20] = detail;
    config[21] = param;
    let ids = if kind == 2 { p.y.bytes } else { 0 };
    return cuda_routing(config, x[0 .. p.x.bytes], y[0 .. ids], dst[0 .. p.d.bytes]);
}
