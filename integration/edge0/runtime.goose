// Full routed/shared composition using the accepted EXP915 latent graph.
// weights are ten consecutive [D,U,gate,up,down] handles in route order.
// coefficients are ten scalar F32 tensors; shared is [gate,up,down,router].
export fn e0_routed_shared(c: i64, x: i64, inverse: i64, weights: i64[:],
                            coefficients: i64[:], shared: i64[:],
                            work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || weights.len != 50 || coefficients.len != 10 || shared.len != 4 ||
       work.len < 16 || nodes.len < 109 { return 0; }
    var i: i64 = 0;
    while i < 10 {
        if !el_matrix(coefficients[i], 0, 1, 1) { return 0; }
        i += 1;
    }
    if shared[0] == 0 || shared[1] == 0 || shared[2] == 0 || shared[3] == 0 { return 0; }
    i = 0;
    var sum: i64 = 0;
    while i < 10 {
        let branch = e0_latent_expert(c, x, inverse, weights[i*5 .. i*5+5], work, nodes[i*8 .. i*8+8]);
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
