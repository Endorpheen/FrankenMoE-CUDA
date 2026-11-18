// Minimal numeric composition witness: the caller supplies only tensor handles
// and one workspace; all node ordering and argument preparation happen in Goose.
fn chain_op(work: i64[:], mode: i64, c: i64, a: i64, b: i64, op: i64, param: i64) -> i64 {
    var i: i64 = 0;
    while i < 16 {
        work[i] = 0;
        i = i + 1;
    }
    work[0] = mode;
    work[1] = c;
    work[2] = a;
    work[3] = b;
    work[5] = op;
    work[7] = param;
    return gb_common_args(work);
}

// Inputs: c, x, norm weight, first/second matrix, RMS_NORM/MUL/ADD enum IDs,
// epsilon bits. Output records norm/scale/projection/final/residual handles.
export fn ggt_chain(c: i64, x: i64, norm: i64, w0: i64, w1: i64,
                    rms_op: i64, mul_op: i64, add_op: i64, eps: i64,
                    work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || norm == 0 || w0 == 0 || w1 == 0 || work.len < 16 || nodes.len < 5 {
        return 0;
    }
    let h0 = chain_op(work, 4, c, x, 0, rms_op, eps);
    let h1 = chain_op(work, 0, c, h0, norm, mul_op, 0);
    let h2 = chain_op(work, 11, c, w0, h1, gb_op(1), 0);
    let h3 = chain_op(work, 11, c, w1, h2, gb_op(1), 0);
    let h4 = chain_op(work, 0, c, h3, x, add_op, 0);
    nodes[0] = h0;
    nodes[1] = h1;
    nodes[2] = h2;
    nodes[3] = h3;
    nodes[4] = h4;
    return h4;
}
