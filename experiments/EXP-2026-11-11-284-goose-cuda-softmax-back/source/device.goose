// Apex CUDA softmax backward in Goose, from ggml-cuda/softmax.cu:
// soft_max_back_f32. One row per 32-thread block, no shared memory.
//
//   dgf_dot = sum over col of dstf[col]*grad[col]   (WARP_SIZE stride)
//   dst[col] = scale * (grad[col] - dgf_dot) * dstf[col]
//
// The reduction is the 16,8,4,2,1 shuffle ladder; every index and arithmetic
// operation is Goose. Externs are 32-bit float loads and a store.
extern "fg284d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg284d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg284d_shfl_xor" fn shfl_xor(v: f32, m: i32) -> f32;
extern "fg284d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn warp_sum(x: f32) -> f32 {
    var v: f32 = x;
    var off: i64 = 16 as i64;

    while off >= 1 as i64 {
        let t: f32 = shfl_xor(v, off as i32);
        v = v + t;
        off = off / 2 as i64;
    }
    v
}

fn softback(grad: u64, dstf: u64, dst: u64, ncols: i64, scale: f32, tid: i64, row: i64) {
    ensure(ncols > 0 as i64, 20);
    let base: i64 = row * ncols;
    var dot: f32 = 0.0 as f32;
    var col: i64 = tid;

    while col < ncols as i64 {
        dot = dot + ld_f32(
            dstf,
            ( ( (base + col) * 4 as i64) as u64)) * ld_f32(grad,
            ( ( (base + col) * 4 as i64) as u64));
        col = col + 32 as i64;
    }
    dot = warp_sum(dot);
    col = tid;

    while col < ncols as i64 {
        let g: f32 = ld_f32(grad, ( ( (base + col) * 4 as i64) as u64));
        let f: f32 = ld_f32(dstf, ( ( (base + col) * 4 as i64) as u64));
        st_f32(dst, ( ( (base + col) * 4 as i64) as u64), scale * (g - dot) * f);
        col = col + 32 as i64;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    softback(0, 0, 0, 1 as i64, 0.0 as f32, 0 as i64, 0 as i64);
}

