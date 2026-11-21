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

// Stage one 32-by-32 tile with a padded shared stride for the transpose.
fn transpose_tile(src: u64, dst: u64, columns: i64, rows: i64,
    src_pitch: i64, dst_pitch: i64, bx: i64, by: i64, tx: i64, ty: i64, scratch: u64) {
    let sx = bx * 32 + tx;
    let sy = by * 32 + ty;
    let dx = by * 32 + tx;
    let dy = bx * 32 + ty;
    for group in 4 {
        let row = sy + 8 * group;
        if sx < columns && row < rows {
            let tile = ((ty + 8 * group) * 33 + tx) as u64;
            let offset = (4 * (row * src_pitch + sx)) as u64;
            st_f32(scratch, 4 * tile, ld_f32(src, offset));
        }
    }
    sync();
    for group in 4 {
        let row = dy + 8 * group;
        if dx < rows && row < columns {
            let tile = (tx * 33 + ty + 8 * group) as u64;
            let offset = (4 * (row * dst_pitch + dx)) as u64;
            st_f32(dst, offset, ld_f32(scratch, 4 * tile));
        }
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
    var cur: i64 = 0;
    var i: i64 = 0;
    while i < 8 {
        let imat = bz * 8 + i;
        if imat >= nmat { return 0; }
        let offset = (4 * imat * n) as u64;
        transpose_tile(cx + offset, cdst + offset, ne01, ne00,
            ne01, ne00, bx, by, tx, ty, shared + ((4 * 1056 * cur) as u64));
        sync();
        cur = 1 - cur;
        i += 1;
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

