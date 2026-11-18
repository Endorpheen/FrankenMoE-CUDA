// One selected latent expert; routing weights, reduction and shared expert are
// outside this boundary. Tensor/context/workspace lifetime belongs to the caller.
fn el_begin(w: i64[:], mode: i64, c: i64, a: i64, b: i64, op: i64) {
    var i: i64 = 0;
    while i < 16 { w[i] = 0; i += 1; }
    w[0] = mode;
    w[1] = c;
    w[2] = a;
    w[3] = b;
    w[5] = op;
}

fn el_matrix(t: i64, ty: i64, n0: i64, n1: i64) -> bool {
    return t != 0 && prop(t, 0) == ty && ne(t, 0) == n0 && ne(t, 1) == n1 &&
           ne(t, 2) == 1 && ne(t, 3) == 1 && pred(0, t, 0) != 0;
}

fn el_mm(c: i64, a: i64, b: i64, w: i64[:]) -> i64 {
    el_begin(w, 11, c, a, b, gb_op(1));
    return gb_common_args(w);
}

fn el_mul(c: i64, a: i64, b: i64, w: i64[:]) -> i64 {
    el_begin(w, 1, c, a, b, 7);
    return gb_common_args(w);
}

// The pinned scales are positive finite F32. Validate the entire input before
// writing anything, including a reciprocal-overflow check for subnormal scales.
export fn e0_inverse_scales(scales: f32[:], inverse: f32[:]) -> i64 {
    if scales.len != 1280 || inverse.len < 1280 { return -1; }
    var i: i64 = 0;
    while i < 1280 {
        let s: f32 = scales[i];
        if !(s > 0.0) || !(s <= 3.4028234663852886e38) { return -2; }
        let inv: f32 = 1.0 / s;
        if !(inv > 0.0) || !(inv <= 3.4028234663852886e38) { return -2; }
        i += 1;
    }
    i = 0;
    while i < 1280 { inverse[i] = 1.0 / scales[i]; i += 1; }
    return 1280;
}

// weights: group D, group U, expert gate, expert up, expert down.
// nodes: D, scaled D, gate, up, SiLU, product, down, U.
export fn e0_latent_expert(c: i64, x: i64, inverse: i64, weights: i64[:],
                           work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || inverse == 0 || weights.len < 5 || work.len < 16 || nodes.len < 8 {
        return 0;
    }
    if prop(x, 0) != 0 || ne(x, 0) != 2560 || ne(x, 1) < 1 || ne(x, 1) > 4096 ||
       ne(x, 2) != 1 || ne(x, 3) != 1 || pred(0, x, 0) == 0 { return 0; }
    if !el_matrix(inverse, 0, 1280, 1) { return 0; }
    if weights[0] == 0 { return 0; }
    let du_type = prop(weights[0], 0);
    if du_type != 2 && du_type != 8 { return 0; }
    if !el_matrix(weights[0], du_type, 2560, 1280) || !el_matrix(weights[1], du_type, 1280, 2560) ||
       !el_matrix(weights[2], 11, 1280, 640) || !el_matrix(weights[3], 11, 1280, 640) ||
       !el_matrix(weights[4], 20, 640, 1280) { return 0; }
    nodes[0] = el_mm(c, weights[0], x, work);
    nodes[1] = el_mul(c, nodes[0], inverse, work);
    nodes[2] = el_mm(c, weights[2], nodes[1], work);
    nodes[3] = el_mm(c, weights[3], nodes[1], work);
    el_begin(work, 3, c, nodes[2], 0, 10);
    nodes[4] = gb_common_args(work);
    nodes[5] = el_mul(c, nodes[4], nodes[3], work);
    nodes[6] = el_mm(c, weights[4], nodes[5], work);
    nodes[7] = el_mm(c, weights[1], nodes[6], work);
    return nodes[7];
}
