// Apex CUDA CONCAT device path (Goose), from ggml-cuda/concat.cu.
//
// Scope, exactly the two compute kernels of concat.cu:
//   fg175c_cont_<w>_<dim>  <- concat_cont<T, dim> for dim 0..2
//   fg175c_nc_<w>_<dim>    <- concat_non_cont<T, dim> for dim 0..3
// with T the element copy unit: 1/2/4/8 bytes for a non-quantized type whose
// ggml_type_size is that value, or 1 byte for a quantized type, because the
// native entry rejects a quantized type whose ne[0] is not a block multiple and
// then treats both operands as byte tensors.
//
// Everything algorithmic is Goose: grid/thread mapping, index decode, the
// src0-inside test, the dim-dependent source selection, the destination address
// sum and the copy. Extern symbols are only raw scalar loads/stores of the copy
// width and the PDL primitives. No original kernel, GGML struct or C++ routine
// is called.
//
// Raw ABI: every u64 is a tensor base byte address and every offset below is a
// byte offset from it. Strides are native nb[] values already in bytes, so no
// element-size division is needed. Extents are element counts.
//
// Caller-owned bounds: ggml_cuda_op_concat asserts equal types, quantized
// alignment and the contiguity each branch needs. This module checks that the
// computed offsets are non-negative but cannot see allocation sizes, which the
// graph allocator owns.
extern "fg175c_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;
extern "fg175c_ld_u16" fn ld_u16(src: u64, byte: u64) -> u32;
extern "fg175c_ld_u32" fn ld_u32(src: u64, byte: u64) -> u32;
extern "fg175c_ld_u64" fn ld_u64(src: u64, byte: u64) -> u64;
extern "fg175c_st_u8" fn st_u8(dst: u64, byte: u64, v: u32);
extern "fg175c_st_u16" fn st_u16(dst: u64, byte: u64, v: u32);
extern "fg175c_st_u32" fn st_u32(dst: u64, byte: u64, v: u32);
extern "fg175c_st_u64" fn st_u64(dst: u64, byte: u64, v: u64);
extern "fg175c_pdl_sync" fn pdl_sync();
extern "fg175c_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// Signed byte offset from an index sum; a negative result violates the shape
// contract on the native path as well, since it would form a bogus pointer.

fn off(o: i64) -> u64 {
    ensure(o >= 0, 10);
    return o as u64;
}

fn wbytes(i: i64, w: i32) -> u64 {
    return off(i * (w as i64));
}
// Copy one element of width w bytes. Load and store widths match, so the copy is
// byte-exact for every T the native entry selects.

fn fg175c_copy(src: u64, soff: u64, dst: u64, doff: u64, w: i32) {
    if w == 1 {
        st_u8(dst, doff, ld_u8(src, soff));
    } else {
        if w == 2 {
            st_u16(dst, doff, ld_u16(src, soff));
        } else {
            if w == 4 {
                st_u32(dst, doff, ld_u32(src, soff));
            } else {
                if w == 8 {
                    st_u64(dst, doff, ld_u64(src, soff));
                } else {
                    ensure(false, 11);
                }
            }
        }
    }
}
// concat_cont<T,dim>: contiguous-to-3 operands, one i3 slice per launch.
//   n = ne0*ne1*ne2
//   for (i = blockIdx.x*blockDim.x + threadIdx.x; i < n; i += blockDim.x*gridDim.x)
//     dim 0: row = i/ne0; i0 = i - row*ne0
//            i0 < ne00 ? x[row*ne00 + i0] : y[row*(ne0-ne00) + (i0-ne00)]
//     dim 1: p = ne0*ne1; s0p = ne0*ne01; s1p = p - s0p; i2 = i/p; i01 = i - i2*p
//            i01 < s0p ? x[i2*s0p + i01] : y[i2*s1p + (i01-s0p)]
//     dim 2: s0sz = ne0*ne1*ne02
//            i < s0sz ? x[i] : y[i - s0sz]

fn fg175c_cont(
    x: u64,
    y: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne0: i64,
    ne1: i64,
    ne2: i64,
    w: i32,
    dim: i32,
    bx: i32,
    tx: i32,
    gx: i32,
    nt: i32) {
    pdl_sync();
    ensure(nt > 0 && gx > 0 && bx >= 0 && tx >= 0, 12);
    let n: i64 = ne0 * ne1 * ne2;
    var i: i64 = (bx as i64) * (nt as i64) + (tx as i64);
    let stride: i64 = (gx as i64) * (nt as i64);

    while i < n {
        var soff: i64 = 0;
        var frm: i32 = 0;

        if dim == 0 {
            let row: i64 = i / ne0;
            let i0: i64 = i - row * ne0;

            if i0 < ne00 {
                soff = row * ne00 + i0;
                frm = 0;
            } else {
                soff = row * (ne0 - ne00) + (i0 - ne00);
                frm = 1;
            }
        } else {
            if dim == 1 {
                let p: i64 = ne0 * ne1;
                let s0p: i64 = ne0 * ne01;
                let s1p: i64 = p - s0p;
                let i2: i64 = i / p;
                let i01: i64 = i - i2 * p;

                if i01 < s0p {
                    soff = i2 * s0p + i01;
                    frm = 0;
                } else {
                    soff = i2 * s1p + (i01 - s0p);
                    frm = 1;
                }
            } else {
                if dim == 2 {
                    let s0sz: i64 = ne0 * ne1 * ne02;

                    if i < s0sz {
                        soff = i;
                        frm = 0;
                    } else {
                        soff = i - s0sz;
                        frm = 1;
                    }
                } else {
                    ensure(false, 13);
                }
            }
        }
        let dbo: u64 = wbytes(i, w);

        if frm == 0 {
            fg175c_copy(x, wbytes(soff, w), d, dbo, w);
        } else {
            fg175c_copy(y, wbytes(soff, w), d, dbo, w);
        }
        i += stride;
    }
}
// concat_non_cont<T,dim>: one destination element per (i0,i1,i2,i3).
//   i1 = blockIdx.x, i2 = blockIdx.y, i3 = blockIdx.z
//   for (i0 = threadIdx.x; i0 < ne0; i0 += blockDim.x)
//     inside = i0<ne00 && i1<ne01 && i2<ne02 && i3<ne03
//     inside ? src0 + i3*nb03 + i2*nb02 + i1*nb01 + i0*nb00
//     dim 0  : src1 + i3*nb13 + i2*nb12 + i1*nb11 + (i0-ne00)*nb10
//     dim 1  : src1 + i3*nb13 + i2*nb12 + (i1-ne01)*nb11 + i0*nb10
//     dim 2  : src1 + i3*nb13 + (i2-ne02)*nb12 + i1*nb11 + i0*nb10
//     dim 3  : src1 + (i3-ne03)*nb13 + i2*nb12 + i1*nb11 + i0*nb10
//     dst + i3*nb3 + i2*nb2 + i1*nb1 + i0*nb0

fn fg175c_nc(
    s0: u64,
    s1: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    ne0: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    w: i32,
    dim: i32,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    nt: i64) {
    pdl_sync();
    ensure(nt > 0 && tx >= 0 && bx >= 0 && by >= 0 && bz >= 0, 14);
    let i1: i64 = bx;
    let i2: i64 = by;
    let i3: i64 = bz;
    var i0: i64 = tx;

    while i0 < ne0 {
        let doff: i64 = i3 * nb3 + i2 * nb2 + i1 * nb1 + i0 * nb0;

        if i0 < ne00 && i1 < ne01 && i2 < ne02 && i3 < ne03 {
            let soff: i64 = i3 * nb03 + i2 * nb02 + i1 * nb01 + i0 * nb00;
            fg175c_copy(s0, off(soff), d, off(doff), w);
        } else {
            var soff: i64 = 0;

            if dim == 0 {
                soff = i3 * nb13 + i2 * nb12 + i1 * nb11 + (i0 - ne00) * nb10;
            } else {
                if dim == 1 {
                    soff = i3 * nb13 + i2 * nb12 + (i1 - ne01) * nb11 + i0 * nb10;
                } else {
                    if dim == 2 {
                        soff = i3 * nb13 + (i2 - ne02) * nb12 + i1 * nb11 + i0 * nb10;
                    } else {
                        if dim == 3 {
                            soff = (i3 - ne03) * nb13 + i2 * nb12 + i1 * nb11 + i0 * nb10;
                        } else {
                            ensure(false, 15);
                        }
                    }
                }
            }
            fg175c_copy(s1, off(soff), d, off(doff), w);
        }
        i0 += nt;
    }
}
// Per-dim entries. dim is a compile-time constant at each parent call site,
// exactly like the native template instantiation; these wrappers keep the two
// reviewed bodies single and make the parent dispatch table explicit.

fn fg175c_cont_0(
    x: u64,
    y: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne0: i64,
    ne1: i64,
    ne2: i64,
    w: i32,
    bx: i32,
    tx: i32,
    gx: i32,
    nt: i32) {
    fg175c_cont(x, y, d, ne00, ne01, ne02, ne0, ne1, ne2, w, 0, bx, tx, gx, nt);
}

fn fg175c_cont_1(
    x: u64,
    y: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne0: i64,
    ne1: i64,
    ne2: i64,
    w: i32,
    bx: i32,
    tx: i32,
    gx: i32,
    nt: i32) {
    fg175c_cont(x, y, d, ne00, ne01, ne02, ne0, ne1, ne2, w, 1, bx, tx, gx, nt);
}

fn fg175c_cont_2(
    x: u64,
    y: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne0: i64,
    ne1: i64,
    ne2: i64,
    w: i32,
    bx: i32,
    tx: i32,
    gx: i32,
    nt: i32) {
    fg175c_cont(x, y, d, ne00, ne01, ne02, ne0, ne1, ne2, w, 2, bx, tx, gx, nt);
}

fn fg175c_nc_0(
    s0: u64,
    s1: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    ne0: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    w: i32,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    nt: i64) {
    fg175c_nc(
        s0,
        s1,
        d,
        ne00,
        ne01,
        ne02,
        ne03,
        nb00,
        nb01,
        nb02,
        nb03,
        nb10,
        nb11,
        nb12,
        nb13,
        ne0,
        nb0,
        nb1,
        nb2,
        nb3,
        w,
        0,
        bx,
        by,
        bz,
        tx,
        nt);
}

fn fg175c_nc_1(
    s0: u64,
    s1: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    ne0: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    w: i32,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    nt: i64) {
    fg175c_nc(
        s0,
        s1,
        d,
        ne00,
        ne01,
        ne02,
        ne03,
        nb00,
        nb01,
        nb02,
        nb03,
        nb10,
        nb11,
        nb12,
        nb13,
        ne0,
        nb0,
        nb1,
        nb2,
        nb3,
        w,
        1,
        bx,
        by,
        bz,
        tx,
        nt);
}

fn fg175c_nc_2(
    s0: u64,
    s1: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    ne0: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    w: i32,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    nt: i64) {
    fg175c_nc(
        s0,
        s1,
        d,
        ne00,
        ne01,
        ne02,
        ne03,
        nb00,
        nb01,
        nb02,
        nb03,
        nb10,
        nb11,
        nb12,
        nb13,
        ne0,
        nb0,
        nb1,
        nb2,
        nb3,
        w,
        2,
        bx,
        by,
        bz,
        tx,
        nt);
}

fn fg175c_nc_3(
    s0: u64,
    s1: u64,
    d: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    ne0: i64,
    nb0: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    w: i32,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    nt: i64) {
    fg175c_nc(
        s0,
        s1,
        d,
        ne00,
        ne01,
        ne02,
        ne03,
        nb00,
        nb01,
        nb02,
        nb03,
        nb10,
        nb11,
        nb12,
        nb13,
        ne0,
        nb0,
        nb1,
        nb2,
        nb3,
        w,
        3,
        bx,
        by,
        bz,
        tx,
        nt);
}
// Retained entry; the parent never executes main. It names every exported
// wrapper so the parent's mechanical extraction maps emitted symbols to the
// eight device entry points without relying on names the C backend does not
// preserve. Each call uses a distinct trailing argument so equal-argument calls
// are not folded together, and every extent is zero so nothing is read or
// written.

fn main() {
    let z: u64 = 0;
    fg175c_cont_0(z, z, z, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1);
    fg175c_cont_1(z, z, z, 0, 0, 0, 0, 0, 1, 2, 1, 1, 1, 2);
    fg175c_cont_2(z, z, z, 0, 0, 0, 0, 0, 2, 3, 2, 2, 1, 3);
    fg175c_nc_0(z, z, z, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 0, 0, 0, 1);
    fg175c_nc_1(z, z, z, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 0, 0, 0, 1);
    fg175c_nc_2(z, z, z, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 1);
    fg175c_nc_3(z, z, z, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0, 1);
}

