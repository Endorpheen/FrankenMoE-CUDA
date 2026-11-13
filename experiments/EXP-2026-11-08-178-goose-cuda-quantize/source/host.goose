// Apex CUDA Q8_1 activation quantization host planning (Goose), from
// ggml-cuda/quantize.cu: quantize_row_q8_1_cuda (non-MMQ, ids == nullptr).
//
// The native entry takes raw pointers and scalars, not ggml tensors, so the
// bridge marshals those scalars into the packet and every decision stays here:
// the ids-absent rule, the ne0 multiple-of-32 rule, init_fastdiv_values(ne2), the
// grid/block sizing and the source geometry handed to the device body.
//
// Packet layout written by the bridge before the call (slot, index):
//   (0,0) x pointer        (0,1) vy pointer      (0,2) ids pointer (0 = absent)
//   (1,0) ne00             (1,1) s01  (1,2) s02  (1,3) s03
//   (1,4) ne0 (1,5) ne1 (1,6) ne2 (1,7) ne3
// Output plan:
//   (1,*) the launch geometry, (2,*) the device geometry
extern "fg178h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg178h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg178h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg178h_launch" fn launch(packet: i64) -> i64;
extern "fg178h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 1);
    return a * b;
}

fn cld(a: u64, b: u64) -> u64 {
    ensure(b != 0, 2);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}
// init_fastdiv_values(ne2): L = ceil(log2(d)), mp = (2^32*(2^L - d))/d + 1.

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
    put(p, 6, 0, mp);
    put(p, 6, 1, l);
    put(p, 6, 2, du);
}

fn run(packet: i64) -> bool {
    let x = get(packet, 0, 0);
    let vy = get(packet, 0, 1);
    let ids = get(packet, 0, 2);
    ensure(x != 0 && vy != 0, 20);
    // This quantizer is the non-MMQ variant; the native entry asserts ids == null.

    if ids != 0 {
        return false;
    }
    let ne00 = get(packet, 1, 0) as i64;
    let s01 = get(packet, 1, 1) as i64;
    let s02 = get(packet, 1, 2) as i64;
    let s03 = get(packet, 1, 3) as i64;
    let ne0 = get(packet, 1, 4) as i64;
    let ne1 = get(packet, 1, 5) as i64;
    let ne2 = get(packet, 1, 6) as i64;
    let ne3 = get(packet, 1, 7) as i64;
    // Native: GGML_ASSERT(ne0 % QK8_1 == 0) with QK8_1 == 32.
    ensure(ne0 > 0 && (ne0 % (32 as i64) == 0), 21);
    ensure(ne1 > 0 && ne2 > 0 && ne3 > 0 && ne00 > 0, 22);
    let p = plan_ptr(packet);
    fastdiv(p, ne2);
    let bs: u64 = 256 as u64;
    let gx = cld(ne0 as u64, bs);
    ensure(gx <= 2147483647 as u64, 23);
    put(p, 1, 0, gx);
    put(p, 1, 1, ne1 as u64);
    put(p, 1, 2, mul(ne2 as u64, ne3 as u64));
    put(p, 1, 3, bs);
    put(p, 2, 0, ne00 as u64);
    put(p, 2, 1, s01 as u64);
    put(p, 2, 2, s02 as u64);
    put(p, 2, 3, s03 as u64);
    put(p, 2, 4, ne0 as u64);
    put(p, 2, 5, ne1 as u64);
    put(p, 2, 6, ne2 as u64);
    ensure(launch(packet) == 0, 24);
    return true;
}

fn main() {
    run(0);
}

