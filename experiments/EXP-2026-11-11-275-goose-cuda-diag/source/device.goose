// Apex CUDA diag in Goose, from ggml-cuda/diag.cu.
//
// Device side: the multi-dim index decomposition, the destination index, the
// i0==i1 selection, the batch/source index and the write (copy or zero).
// Host side: the block count and the shape/product contract.
//
// All index arithmetic is Goose. Externs are bit-preserving 32-bit and 16-bit
// loads/stores, so a diagonal half value passes through unchanged.
extern "fg275d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg275d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg275d_ld_u16" fn ld_u16(x: u64, o: u64) -> u32;
extern "fg275d_st_u16" fn st_u16(dst: u64, o: u64, v: u32);
extern "fg275d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn idx(gidx: i64, ne0: i64, ne1: i64, ne2: i64, ne3: i64) -> i64 {
    ensure(ne0 > 0 as i64 && ne1 > 0 as i64 && ne2 > 0 as i64 && ne3 > 0 as i64, 20);
    let i0: i64 = gidx % ne0;
    let i1: i64 = (gidx / ne0) % ne1;
    let i2: i64 = (gidx / (ne0 * ne1)) % ne2;
    let i3: i64 = gidx / (ne0 * ne1 * ne2);
    ( (i3 * ne2 + i2) * ne1 + i1) * ne0 + i0
}

fn diag_f32(dst: u64, src: u64, ne0: i64, ne1: i64, ne2: i64, ne3: i64, gidx: i64, total: i64) {
    if gidx >= total as i64 {
        return;
    }
    let i0: i64 = gidx % ne0;
    let i1: i64 = (gidx / ne0) % ne1;
    let i3: i64 = gidx / (ne0 * ne1 * ne2);
    let i2: i64 = (gidx / (ne0 * ne1)) % ne2;
    let di: i64 = idx(gidx, ne0, ne1, ne2, ne3);

    if i0 == i1 {
        let bi: i64 = i3 * ne2 + i2;
        let si: i64 = bi * ne0 + i0;
        st_f32(dst, ( (di * 4 as i64) as u64), ld_f32(src, ( (si * 4 as i64) as u64)));
    } else {
        st_f32(dst, ( (di * 4 as i64) as u64), 0.0 as f32);
    }
}

fn diag_f16(dst: u64, src: u64, ne0: i64, ne1: i64, ne2: i64, ne3: i64, gidx: i64, total: i64) {
    if gidx >= total as i64 {
        return;
    }
    let i0: i64 = gidx % ne0;
    let i1: i64 = (gidx / ne0) % ne1;
    let i3: i64 = gidx / (ne0 * ne1 * ne2);
    let i2: i64 = (gidx / (ne0 * ne1)) % ne2;
    let di: i64 = idx(gidx, ne0, ne1, ne2, ne3);

    if i0 == i1 {
        let bi: i64 = i3 * ne2 + i2;
        let si: i64 = bi * ne0 + i0;
        st_u16(dst, ( (di * 2 as i64) as u64), ld_u16(src, ( (si * 2 as i64) as u64)));
    } else {
        st_u16(dst, ( (di * 2 as i64) as u64), 0 as u32);
    }
}

fn main() {
    diag_f32(0, 0, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
    diag_f16(0, 0, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

