// CUDA transposed CPY tile in Goose (F32), from ggml-cuda/cpy.cu
// cpy_scalar_transpose<float>.
//
// A 32x32 tile is loaded from a (ne00 x ne01) matrix, staged through a
// double-buffered shared float[32][33] array, then written transposed. The
// wrapper owns the shared array; Goose owns every index and the loop over eight
// matrices per block.
extern "fg434c_ld_f32" fn ld_f32(p: u64, o: u64) -> f32;
extern "fg434c_st_f32" fn st_f32(p: u64, o: u64, v: f32);
extern "fg434c_sync" fn sync();
extern "fg434c_fail" fn fail(c: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn transpose(
    cx: u64,
    cdst: u64,
    ne00: i64,
    ne01: i64,
    ne: i64,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    ty: i64,
    shared: u64) -> i64 {
    if ne00 <= (0 as i64) || ne01 <= (0 as i64) || ne <= (0 as i64) {
        return 0 as i64;
    }
    let n: i64 = ne00 * ne01;
    let nmat: i64 = ne / n;
    let sx: i64 = bx * (32 as i64) + tx;
    let sy: i64 = by * (32 as i64) + ty;
    let dcol: i64 = by * (32 as i64) + tx;
    let drow: i64 = bx * (32 as i64) + ty;
    var cur: i64 = 0 as i64;
    var i: i64 = 0 as i64;

    while i < (8 as i64) {
        let imat: i64 = bz * (8 as i64) + i;

        if imat >= nmat {
            i = (8 as i64);
        } else {
            var j: i64 = 0 as i64;

            while j < (32 as i64) {
                if sx < ne01 && sy + j < ne00 {
                    let toff: i64 = cur * (1056 as i64) + (ty + j) * (33 as i64) + tx;
                    let soff: i64 = imat * n + (sy + j) * ne01 + sx;
                    st_f32(shared, (toff * (4 as i64)) as u64, ld_f32(cx, (soff * (4 as i64)) as u64));
                }
                j = j + (8 as i64);
            }
            sync();
            j = 0 as i64;

            while j < (32 as i64) {
                if drow + j < ne01 && dcol < ne00 {
                    let toff: i64 = cur * (1056 as i64) + tx * (33 as i64) + (ty + j);
                    let doff: i64 = imat * n + (drow + j) * ne00 + dcol;
                    st_f32(cdst, (doff * (4 as i64)) as u64, ld_f32(shared, (toff * (4 as i64)) as u64));
                }
                j = j + (8 as i64);
            }
            sync();
            cur = (1 as i64) - cur;
            i = i + (1 as i64);
        }
    }
    return 0 as i64;
}

fn main() {
    transpose(
        0 as u64,
        0 as u64,
        1 as i64,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as u64);
}

