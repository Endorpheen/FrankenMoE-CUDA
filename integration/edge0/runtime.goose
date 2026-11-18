// Full routed/shared composition using the accepted EXP915 latent graph.
// weights are ten consecutive [D,U,gate,up,down] handles in route order.
// coefficients are ten scalar F32 tensors; shared is [gate,up,down,router].
// The projected input is an existing D(x)*inverse from this same layer/token.
fn e0_from_projected(c: i64, projected: i64, weights: i64[:], work: i64[:], nodes: i64[:]) -> i64 {
    let du_type = prop(weights[0], 0);
    if (du_type != 2 && du_type != 8) ||
       !el_matrix(weights[0], du_type, 2560, 1280) || !el_matrix(weights[1], du_type, 1280, 2560) ||
       !el_matrix(weights[2], 11, 1280, 640) || !el_matrix(weights[3], 11, 1280, 640) ||
       !el_matrix(weights[4], 20, 640, 1280) { return 0; }
    nodes[2] = el_mm(c, weights[2], projected, work);
    nodes[3] = el_mm(c, weights[3], projected, work);
    el_begin(work, 3, c, nodes[2], 0, 10);
    nodes[4] = gb_common_args(work);
    nodes[5] = el_mul(c, nodes[4], nodes[3], work);
    nodes[6] = el_mm(c, weights[4], nodes[5], work);
    nodes[7] = el_mm(c, weights[1], nodes[6], work);
    return nodes[7];
}

export fn e0_routed_shared(c: i64, x: i64, inverse: i64, weights: i64[:],
                            coefficients: i64[:], shared: i64[:], groups: i64[:],
                            work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || weights.len != 50 || coefficients.len != 10 || shared.len != 4 ||
       groups.len != 10 || work.len < 16 || nodes.len < 109 { return 0; }
    var i: i64 = 0;
    while i < 10 {
        if groups[i] < 0 || groups[i] > 9 || !el_matrix(coefficients[i], 0, 1, 1) { return 0; }
        i += 1;
    }
    if shared[0] == 0 || shared[1] == 0 || shared[2] == 0 || shared[3] == 0 { return 0; }
    i = 0;
    var sum: i64 = 0;
    while i < 10 {
        var earlier: i64 = -1;
        var j: i64 = 0;
        while j < i {
            if groups[j] == groups[i] && earlier < 0 { earlier = j; }
            j += 1;
        }
        var branch: i64 = 0;
        if earlier < 0 {
            branch = e0_latent_expert(c, x, inverse, weights[i*5 .. i*5+5], work, nodes[i*8 .. i*8+8]);
        } else {
            nodes[i*8] = nodes[earlier*8];
            nodes[i*8+1] = nodes[earlier*8+1];
            branch = e0_from_projected(c, nodes[i*8+1], weights[i*5 .. i*5+5], work, nodes[i*8 .. i*8+8]);
        }
        if branch == 0 { return 0; }
        nodes[80+i] = el_mul(c, branch, coefficients[i], work);
        if i == 0 { sum = nodes[80]; } else {
            el_begin(work, 1, c, sum, nodes[80+i], 2);
            nodes[89+i] = gb_common_args(work);
            sum = nodes[89+i];
        }
        i += 1;
    }
    nodes[99] = el_mm(c, shared[0], x, work);
    nodes[100] = el_mm(c, shared[1], x, work);
    el_begin(work, 3, c, nodes[99], 0, 10);
    nodes[101] = gb_common_args(work);
    nodes[102] = el_mul(c, nodes[101], nodes[100], work);
    nodes[103] = el_mm(c, shared[2], nodes[102], work);
    nodes[104] = el_mm(c, shared[3], x, work);
    el_begin(work, 3, c, nodes[104], 0, 7);
    nodes[105] = gb_common_args(work);
    nodes[106] = el_mul(c, nodes[103], nodes[105], work);
    nodes[107] = sum;
    el_begin(work, 1, c, sum, nodes[106], 2);
    nodes[108] = gb_common_args(work);
    return nodes[108];
}
