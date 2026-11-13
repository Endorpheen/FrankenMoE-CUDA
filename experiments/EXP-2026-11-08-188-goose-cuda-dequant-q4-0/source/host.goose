// Apex CUDA Q4_0 row dequantization host planning (Goose), from
// ggml-cuda/convert.cu: dequantize_row_q4_0_cuda.
//
// Scope: the F16 element-count contract and the launch geometry.
//   nb32 = k / 32 ; nb = ceil(k / 256) ; grid = (nb, 1, 1) ; block = 32
// The native function does not validate k, so a k that is not a multiple of 32 leaves
// the tail block partially defined there. This module rejects it instead of copying
// that behaviour.
//
// Packet layout written by the bridge:
//   (0,0) source pointer (0,1) block count ceil(k/256) (0,2) destination pointer
//   (0,3) nb32 = k/32
extern "fg188h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg188h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg188h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg188h_launch" fn launch(packet: i64) -> i64;
extern "fg188h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn cld(a: u64, b: u64) -> u64 {
    ensure(b != 0, 1);
    var q: u64 = a / b;

    if a % b != 0 {
        q = q + 1 as u64;
    }
    return q;
}

fn run(packet: i64) -> bool {
    let src = get(packet, 0, 0);
    let dst = get(packet, 0, 2);
    let nb = get(packet, 0, 1);
    let nb32 = get(packet, 0, 3);
    ensure(src != 0 && dst != 0, 20);
    ensure(nb > 0, 21);
    ensure(nb32 > 0, 22);
    ensure(nb <= (2147483647 as u64), 23);
    let p = plan_ptr(packet);
    put(p, 1, 0, nb);
    put(p, 1, 1, 32 as u64);
    ensure(launch(packet) == 0, 24);
    return true;
}

fn main() {
    run(0);
}

