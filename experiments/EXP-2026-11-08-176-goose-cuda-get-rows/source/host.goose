// Apex CUDA GET_ROWS host planning and routing (Goose), from ggml-cuda/getrows.cu.
//
// Scope, exactly ggml_cuda_op_get_rows and the float branch of get_rows_cuda:
//   - the native asserts: src1 is I32, ne13==1, and nb[0] equals the type size of
//     src0, src1 and dst,
//   - GGML_ASSERT(ne12 > 0) and ne11 <= UINT32_MAX / ne12,
//   - init_fastdiv_values(ne12) packed as <mp, L, divisor>,
//   - the vectorisation predicate of get_rows_cuda_float: VEC = 16/sizeof(dst_t),
//     enough_blocks, ne00 % VEC == 0, all six stride multiples of 16, and 16-byte
//     alignment of both pointers,
//   - grid/block sizing for both kernels,
//   - the element strides s1..s3, s10..s12 and ne00v.
//
// Everything above is Goose. The C++ bridge keeps only raw tensor-field reads,
// enum queries, the plan arena and the launch call. No original kernel, ggml
// struct or C++ routine is wrapped or called.
//
// Caller-owned bounds: this module proves the shape/type/alignment contract and
// representable offsets. Allocation sizes belong to the graph allocator, as on
// the native path.
extern "fg176h_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg176h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg176h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg176h_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg176h_tc" fn tc(k: i64) -> i64;
extern "fg176h_oc" fn oc(k: i64) -> i64;
extern "fg176h_tsz" fn tsz(ty: i64) -> i64;
extern "fg176h_launch" fn launch(packet: i64) -> i64;
extern "fg176h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn add(a: u64, b: u64) -> u64 {
    ensure(a <= U() - b, 1);
    return a + b;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 2);
    return a * b;
}

fn itg(a: i64) -> u64 {
    ensure(a >= 0, 3);
    return a as u64;
}

fn cld(a: u64, b: u64) -> u64 {
    ensure(b != 0, 4);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn mni(a: u64, b: u64) -> u64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn nev(t: i64, i: i64) -> i64 {
    return ti(t, 3, i);
}

fn nbv(t: i64, i: i64) -> i64 {
    return ti(t, 4, i);
}

fn tsz_of(t: i64) -> i64 {
    return tsz(ti(t, 2, 0));
}
// init_fastdiv_values: L = ceil(log2(d)), mp = (2^32 * (2^L - d))/d + 1.

fn fastdiv(p: i64, d: i64) {
    ensure(d > 0, 10);
    ensure(d <= 4294967295, 11);
    let du: u64 = d as u64;
    var l: u64 = 0;

    while l < 32 && ( (1 as u64) << l) < du {
        l += 1;
    }
    let mp = ( (1 as u64) << 32) * ( ( (1 as u64) << l) - du) / du + 1;
    ensure(mp <= 4294967295, 12);
    put(p, 0, 0, mp);
    put(p, 0, 1, l);
    put(p, 0, 2, du);
}

fn run(packet: i64) -> bool {
    let dst = arg(packet, 0);
    ensure(dst != 0, 20);

    if ti(dst, 5, 0) != oc(0) {
        return false;
    }
    let s0 = ti(dst, 0, 0);
    let s1 = ti(dst, 0, 1);
    ensure(s0 != 0 && s1 != 0, 21);
    // native: src1->type == GGML_TYPE_I32 and ne13 == 1

    if ti(s1, 2, 0) != tc(8) {
        return false;
    }

    if nev(s1, 3) != 1 {
        return false;
    }
    let t0 = ti(s0, 2, 0);
    let td = ti(dst, 2, 0);
    let es0 = tsz_of(s0);
    let es1 = tsz_of(s1);
    let esd = tsz_of(dst);
    // nb[0] must be the type size of each tensor
    ensure(nbv(s0, 0) == es0 && nbv(s1, 0) == es1 && nbv(dst, 0) == esd, 22);
    let ne00 = nev(s0, 0);
    let ne01 = nev(s0, 1);
    let ne02 = nev(s0, 2);
    let ne10 = nev(s1, 0);
    let ne11 = nev(s1, 1);
    let ne12 = nev(s1, 2);
    let ne0 = nev(dst, 0);
    let ne1 = nev(dst, 1);
    let ne2 = nev(dst, 2);
    ensure(ne12 > 0, 23);
    ensure(ne11 <= 4294967295 / ne12, 24);
    let p = plan_ptr(packet);
    fastdiv(p, ne12);
    // Element strides of dst (in dst elements) and src1 (in I32 elements).
    let esdu: u64 = itg(esd);
    let s1e = itg(nbv(dst, 1)) / esdu;
    let s2e = itg(nbv(dst, 2)) / esdu;
    let s3e = itg(nbv(dst, 3)) / esdu;
    ensure(nbv(dst, 1) % esd == 0 && nbv(dst, 2) % esd == 0 && nbv(dst, 3) % esd == 0, 25);
    let es1u: u64 = itg(es1);
    // Native names these nb10/nb11/nb12 = src1->nb[0..2], i.e. the i10/i11/i12
    // steps. Using nb[1..3] here shifted every row index by one dimension, which is
    // why only the first block's rows matched.
    let s10 = itg(nbv(s1, 0)) / es1u;
    let s11 = itg(nbv(s1, 1)) / es1u;
    let s12 = itg(nbv(s1, 2)) / es1u;
    ensure(nbv(s1, 0) % es1 == 0 && nbv(s1, 1) % es1 == 0 && nbv(s1, 2) % es1 == 0, 26);
    put(p, 1, 0, s1e);
    put(p, 1, 1, s2e);
    put(p, 1, 2, s3e);
    put(p, 2, 0, s10);
    put(p, 2, 1, s11);
    put(p, 2, 2, s12);
    put(p, 3, 0, itg(nbv(s0, 1)));
    put(p, 3, 1, itg(nbv(s0, 2)));
    put(p, 3, 2, itg(nbv(s0, 3)));
    // Vector path is compiled only for src0_t == dst_t; the Apex profile is F32->F32.
    let vec: i32 = if t0 == td {
        (16 as i64 / esd) as i32
    } else {
        0
    }
    ;
    var use_vec: bool = false;
    var ne00v: i64 = ne00;

    if vec > 1 {
        let nv: i64 = ne00 / (vec as i64);
        ne00v = nv;
        let bnum_y: i64 = (nv + 63) / 64;
        let enough: bool = bnum_y * ne10 * ne11 * ne12 >= 128;
        let aligned: bool = (ne00 % (vec as i64) == 0) && (nbv(s0, 1) % 16 == 0) && (nbv(s0, 2) % 16 == 0) && (nbv(s0, 3) % 16 == 0) && (nbv(dst, 1) % 16 == 0) && (nbv(dst, 2) % 16 == 0) && (nbv(dst, 3) % 16 == 0);
        // Pointer alignment is a device-address property only the bridge can read, so
        // it answers through the packet field query k=2 (0/1). The decision stays here.
        use_vec = enough && aligned && ti(packet, 9, 0) == 1;
    }

    if use_vec {
        let bnum_y: i64 = (ne00v + 63) / 64;
        let gx = itg(ne10);
        let gy = mni(bnum_y as u64, 65535 as u64);
        let gz = mni(itg(ne11) * itg(ne12), 65535 as u64);
        ensure(gx <= 2147483647 as u64, 30);
        put(p, 6, 0, 1 as u64);
        put(p, 7, 0, ne00v as u64);
        put(p, 7, 1, gx as u64);
        put(p, 7, 2, gy as u64);
        put(p, 7, 3, gz as u64);
        put(p, 7, 4, 64 as u64);
        put(p, 7, 5, ne11 as u64);
        put(p, 7, 6, ne12 as u64);
    } else {
        let bnum_y: i64 = (ne00 + 63) / 64;
        let gx = itg(ne10);
        let gy = mni(bnum_y as u64, 65535 as u64);
        let gz = mni(itg(ne11) * itg(ne12), 65535 as u64);
        ensure(gx <= 2147483647 as u64, 31);
        put(p, 6, 0, 0 as u64);
        put(p, 7, 0, ne00 as u64);
        put(p, 7, 1, gx as u64);
        put(p, 7, 2, gy as u64);
        put(p, 7, 3, gz as u64);
        put(p, 7, 4, 64 as u64);
        put(p, 7, 5, ne11 as u64);
        put(p, 7, 6, ne12 as u64);
        put(p, 7, 7, itg(esd));
    }
    ensure(launch(packet) == 0, 40);
    return true;
}

fn main() {
    run(0);
}

