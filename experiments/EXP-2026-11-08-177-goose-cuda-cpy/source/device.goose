// Apex CUDA CPY device path (Goose), from ggml-cuda/cpy.cu.
//
// Scope: the element paths of ggml_cuda_cpy that the fixed Apex profile actually
// executes (GPU census: cpy_scalar<cpy_1_scalar<float,float>> 440 launches):
//   fg177c_scalar     <- cpy_scalar<cpy_1_scalar<src_t,dst_t>>
//   fg177c_contiguous <- cpy_scalar_contiguous<src_t,dst_t>
//
// Everything algorithmic is Goose: the flattened index, the four-dimension decode
// against both the source and destination extents, both byte-offset sums and the
// element copy. Extern symbols are only raw element loads/stores of the copy
// width and the PDL primitives. No original kernel, GGML struct or C++ routine is
// called.
//
// The native entry never reaches these kernels when the two tensors are the same
// type and both contiguous (that branch is a single cudaMemcpyAsync), nor when the
// copy reduces to cudaMemcpy2DAsync. Those remain transport in the parent.
//
// Raw ABI: every u64 is a tensor base byte address; the offsets below are byte
// offsets from it. nb00..nb03 are src0 byte strides and nb10..nb13 are src1 byte
// strides, exactly the native locals. ne00..ne02 and ne10..ne12 are the extents
// used by the native decode; dst->ne[3] is implied by ne.
//
// Caller-owned bounds: ggml_cuda_cpy asserts equal element counts, and the graph
// allocator owns allocation sizes. This module checks that the computed offsets
// are non-negative and that the arms agree on element count.
extern "fg177c_ld_f32" fn ld_f32(src: u64, byte: u64) -> f32;
extern "fg177c_st_f32" fn st_f32(dst: u64, byte: u64, v: f32);
extern "fg177c_pdl_sync" fn pdl_sync();
extern "fg177c_pdl_lc" fn pdl_lc();
extern "fg177c_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn off(o: i64) -> u64 {
    ensure(o >= 0, 10);
    return o as u64;
}
// cpy_scalar<cpy_1_scalar>: one element per GPU thread, flattened index i.
//   i03 = i/(ne00*ne01*ne02); i02 = (i - i03*..)/(ne00*ne01)
//   i01 = (i - i03*.. - i02*ne01*ne00)/ne00; i00 = remainder
//   x_off = i00*nb00 + i01*nb01 + i02*nb02 + i03*nb03
//   i13 = i/(ne10*ne11*ne12); i12 = (i - i13*..)/(ne10*ne11)
//   i11 = (i - i13*.. - i12*ne10*ne11)/ne10; i10 = remainder
//   d_off = i10*nb10 + i11*nb11 + i12*nb12 + i13*nb13
//   *dst = cast(*src)

fn fg177c_scalar(
    src: u64,
    dst: u64,
    ne: i64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    nb00: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    ne10: i64,
    ne11: i64,
    ne12: i64,
    nb10: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    bx: i32,
    tx: i32,
    nt: i32) {
    pdl_lc();
    ensure(nt > 0 && bx >= 0 && tx >= 0, 11);
    ensure(ne00 > 0 && ne01 > 0 && ne02 > 0 && ne10 > 0 && ne11 > 0 && ne12 > 0, 12);
    let i: i64 = (nt as i64) * (bx as i64) + (tx as i64);

    if i < ne {
        let p3: i64 = ne00 * ne01 * ne02;
        let i03: i64 = i / p3;
        let r3: i64 = i - i03 * p3;
        let p2: i64 = ne00 * ne01;
        let i02: i64 = r3 / p2;
        let r2: i64 = r3 - i02 * p2;
        let i01: i64 = r2 / ne00;
        let i00: i64 = r2 - i01 * ne00;
        let xo: i64 = i00 * nb00 + i01 * nb01 + i02 * nb02 + i03 * nb03;
        let q3: i64 = ne10 * ne11 * ne12;
        let i13: i64 = i / q3;
        let s3: i64 = i - i13 * q3;
        let q2: i64 = ne10 * ne11;
        let i12: i64 = s3 / q2;
        let s2: i64 = s3 - i12 * q2;
        let i11: i64 = s2 / ne10;
        let i10: i64 = s2 - i11 * ne10;
        let doff: i64 = i10 * nb10 + i11 * nb11 + i12 * nb12 + i13 * nb13;
        pdl_sync();
        st_f32(dst, off(doff), ld_f32(src, off(xo)));
    }
}
// cpy_scalar_contiguous: same type on both sides, so the copy is a flat walk.
//   dst[i] = cast(src[i]) with i = blockDim.x*blockIdx.x + threadIdx.x

fn fg177c_contiguous(src: u64, dst: u64, ne: i64, bx: i32, tx: i32, nt: i32) {
    ensure(nt > 0 && bx >= 0 && tx >= 0, 20);
    let i: i64 = (nt as i64) * (bx as i64) + (tx as i64);

    if i < ne {
        pdl_sync();
        let e: u64 = (i * (4 as i64)) as u64;
        st_f32(dst, e, ld_f32(src, e));
    }
}
// Retained entries; the parent never executes main. Distinct leading extents keep
// the emitter from folding the two calls together, and ne=0 makes both bodies
// return before any memory effect.

fn main() {
    let z: u64 = 0;
    fg177c_scalar(
        z,
        z,
        0 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0,
        0,
        1 as i32);
    fg177c_contiguous(z, z, 0 as i64, 0, 0, 1 as i32);
}

