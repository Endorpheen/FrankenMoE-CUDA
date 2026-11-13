// CUDA pad_reflect_1d device path in Goose, from ggml-cuda/pad_reflect_1d.cu.
//
// Per destination element: decode the (i3, i2, i1, i0) position from the
// launch geometry, map i0 to a source column by reflection, then load and
// store one float through the typed primitives.
//
// The native launcher packs i1 and the i0 tile into blockIdx.x with
// fast_div_modulo(blockIdx.x, ne01): tile0 = blockIdx.x / ne01 and
// i1 = blockIdx.x % ne01. Both operands are non-negative there, so Goose's
// Euclidean division agrees with C. That is checked here rather than assumed.
extern "fg413d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg413d_st_f32" fn st_f32(x: u64, o: u64, v: f32);
extern "fg413d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// Native reflection: left padding mirrors at zero, the middle copies, right
// padding mirrors around the last source column.

fn reflect_index(rel: i64, ne00: i64) -> i64 {
    if rel < (0 as i64) {
        return (0 as i64) - rel;
    }

    if rel < ne00 {
        return rel;
    }
    return (2 as i64) * ne00 - (2 as i64) - rel;
}

fn pad_elem(
    src: u64,
    dst: u64,
    ne0: i64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    p0: i64,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    bdx: i64) -> i64 {
    ensure(ne00 > 0 as i64 && ne01 > 0 as i64 && ne02 > 0 as i64 && ne03 > 0 as i64, 20);
    let i3: i64 = bz;
    let i2: i64 = by;
    let i1: i64 = bx % ne01;
    let tile0: i64 = bx / ne01;
    let i0: i64 = tx + tile0 * bdx;

    if i0 >= ne0 || i1 >= ne01 || i2 >= ne02 || i3 >= ne03 {
        return 0 as i64;
    }
    let rel: i64 = i0 - p0;
    let sidx: i64 = reflect_index(rel, ne00);
    let so: u64 = (i3 * nb03 + i2 * nb02 + i1 * nb01 + sidx * nb00) as u64;
    let dofs: u64 = (i3 * nb3 + i2 * nb2 + i1 * nb1 + i0 * nb0) as u64;
    let v: f32 = ld_f32(src, so);
    st_f32(dst, dofs, v);
    return 1 as i64;
}

fn main() {
    pad_elem(
        0 as u64,
        0 as u64,
        1 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        1 as i64);
}

