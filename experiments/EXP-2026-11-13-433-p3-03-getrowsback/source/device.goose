// CUDA GET_ROWS_BACK in Goose, from ggml-cuda/getrows.cu.
// For each output column, sum grad[i] over the gradient rows whose index equals
// the destination row. The native kernel strides over destination rows by
// gridDim.y, so the wrapper passes gy explicitly.
extern "fg433b_ld_f32" fn ld_f32(p: u64, o: u64) -> f32;
extern "fg433b_ld_i32" fn ld_i32(p: u64, o: u64) -> i64;
extern "fg433b_st_f32" fn st_f32(p: u64, o: u64, v: f32);
extern "fg433b_fail" fn fail(c: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn rows_back(
    grad: u64,
    rows: u64,
    dst: u64,
    ncols: i64,
    nrows_grad: i64,
    nrows_dst: i64,
    col: i64,
    by: i64,
    gy: i64) -> i64 {
    if col >= ncols {
        return 0 as i64;
    }
    var dr: i64 = by;

    while dr < nrows_dst {
        var sum: f32 = 0.0;
        var i: i64 = 0 as i64;

        while i < nrows_grad {
            let r: i64 = ld_i32(rows, (i * (4 as i64)) as u64);

            if r == dr {
                sum = sum + ld_f32(grad, ( (i * ncols + col) * (4 as i64)) as u64);
            }
            i = i + (1 as i64);
        }
        st_f32(dst, ( (dr * ncols + col) * (4 as i64)) as u64, sum);
        dr = dr + gy;
    }
    return 0 as i64;
}

fn main() {
    rows_back(0 as u64, 0 as u64, 0 as u64, 1 as i64, 1 as i64, 1 as i64, 0 as i64, 0 as i64, 1 as i64);
}

