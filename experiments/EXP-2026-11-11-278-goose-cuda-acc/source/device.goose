// Apex CUDA acc in Goose, from ggml-cuda/acc.cu.
//
// Device side: the flat index bound, src1_idx = i - offset, the stride
// decomposition (i13,i12,i11,i10) and the conditional accumulate
// dst[i] = x[i] + y[src1_idx mapped through the strides].
// Host side: num_blocks = ceil(ne/256), block 256, pointer/extent checks and the
// positive-stride guard (the native kernel divides by them without a check).
//
// Divisions are only evaluated for src1_idx >= 0, where C truncation and Goose's
// floor agree. Externs are 32-bit float loads and a store.
extern "fg278d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg278d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg278d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn acc(
    x: u64,
    y: u64,
    dst: u64,
    ne: i64,
    ne10: i64,
    ne11: i64,
    ne12: i64,
    ne13: i64,
    s11: i64,
    s12: i64,
    s13: i64,
    offset: i64,
    i: i64) {
    if i >= ne as i64 {
        return;
    }
    let si: i64 = i - offset;
    var val: f32 = ld_f32(x, ( (i * 4 as i64) as u64));

    if si >= 0 as i64 {
        ensure(s11 > 0 as i64 && s12 > 0 as i64 && s13 > 0 as i64, 20);
        let i13: i64 = si / s13;
        let t1: i64 = si - i13 * s13;
        let i12: i64 = t1 / s12;
        let t2: i64 = t1 - i12 * s12;
        let i11: i64 = t2 / s11;
        let i10: i64 = t2 - i11 * s11;

        if i10 < ne10 as i64 {
            if i11 < ne11 as i64 {
                if i12 < ne12 as i64 {
                    if i13 < ne13 as i64 {
                        let off: i64 = ( (i13 * ne12 + i12) * ne11 + i11) * ne10 + i10;
                        val = val + ld_f32(y, ( (off * 4 as i64) as u64));
                    }
                }
            }
        }
    }
    st_f32(dst, ( (i * 4 as i64) as u64), val);
}
// Retained entry; the parent never executes main.

fn main() {
    acc(0, 0, 0, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

