// Apex CUDA MMQ Q8_1 host planning (Goose), from ggml-cuda/quantize.cu:
// quantize_mmq_q8_1_cuda and quantize_scatter_mmq_q8_1_cuda.
//
// Scope: the assert set (ne00%4==0, ne0%QK8_1_MMQ==0), the layout selection and
// the grid/block geometry for both the direct and the scatter form.
//   direct : blocks = (ne1, ceil(ne0/(4*128)), ne2*ne3), block = 128
//   scatter: blocks = (n_tokens, ceil(ne0/(4*128)), 1), block = 128
//
// Everything above is Goose. The layout enum arrives as an integer:
//   0 = D4, 1 = DS4, 2 = D2S6. Only DS4 is handled by this module; the host
// returns false for the others so the parent keeps them on the native path.
//
// Packet layout written by the bridge:
//   (0,0) x ptr (0,1) ids ptr (0=absent) (0,2) vy ptr (0,3) layout
//   (1,0) ne00 (1,1) s01 (1,2) s02 (1,3) s03
//   (1,4) ne0 (1,5) ne1 (1,6) ne2 (1,7) ne3
//   (2,0) scatter mode (0 direct, 1 scatter) (2,1) n_tokens (2,2) nrows_dst
//   (2,3) n_expert_used
extern "fg180h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg180h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg180h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg180h_launch" fn launch(packet: i64) -> i64;
extern "fg180h_fail" fn fail(code: i64);

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

fn run(packet: i64) -> bool {
    let x = get(packet, 0, 0);
    let vy = get(packet, 0, 2);
    ensure(x != 0 && vy != 0, 20);
    // Only the DS4 layout of this operator is transferred so far.
    let layout = get(packet, 0, 3) as i64;

    if layout != 1 {
        return false;
    }
    let ne00 = get(packet, 1, 0) as i64;
    let ne0 = get(packet, 1, 4) as i64;
    ensure(ne00 > 0 && (ne00 % (4 as i64) == 0), 21);
    ensure(ne0 > 0 && (ne0 % (128 as i64) == 0), 22);
    let scatter = get(packet, 2, 0);
    let threads: u64 = 128 as u64;
    let span: u64 = mul(4 as u64, threads);
    let gy = cld(ne0 as u64, span);
    ensure(gy <= 2147483647 as u64, 23);
    let p = plan_ptr(packet);

    if scatter != 0 {
        let n_tokens = get(packet, 2, 1) as i64;
        let n_used = get(packet, 2, 3) as u64;
        ensure(n_tokens > 0, 24);
        ensure(n_tokens as u64 <= 2147483647 as u64, 25);
        put(p, 1, 0, n_tokens as u64);
        put(p, 1, 1, gy);
        put(p, 1, 2, 1 as u64);
        put(p, 1, 3, threads);
        put(p, 2, 0, 1 as u64);
        // device: scatter flag = 1
        put(p, 2, 1, 0 as u64);
        put(p, 2, 2, 0 as u64);
        put(p, 2, 3, 0 as u64);
        put(p, 3, 0, n_used);
        // n_expert_used
        put(p, 4, 0, get(packet, 6, 2));
        // s02 = stride_token
    } else {
        let ne1 = get(packet, 1, 5) as i64;
        let ne2 = get(packet, 1, 6) as i64;
        let ne3 = get(packet, 1, 7) as i64;
        ensure(ne1 > 0 && ne2 > 0 && ne3 > 0, 26);
        put(p, 1, 0, ne1 as u64);
        put(p, 1, 1, gy);
        put(p, 1, 2, mul(ne2 as u64, ne3 as u64));
        put(p, 1, 3, threads);
        put(p, 2, 0, 0 as u64);
        // scatter flag = 0 (direct)
        put(p, 2, 1, 0 as u64);
        put(p, 2, 2, 0 as u64);
        put(p, 2, 3, 0 as u64);
        put(p, 3, 0, 0 as u64);
        put(p, 4, 0, get(packet, 6, 2));
    }
    put(p, 5, 0, get(packet, 6, 0));
    // ne00 for the device body
    put(p, 5, 1, get(packet, 6, 1));
    // s01
    put(p, 5, 2, get(packet, 6, 3));
    // s03
    put(p, 5, 3, get(packet, 6, 4));
    // ne0
    put(p, 5, 4, get(packet, 6, 5));
    // ne1
    put(p, 5, 5, get(packet, 6, 6));
    // ne2
    ensure(launch(packet) == 0, 27);
    return true;
}

fn main() {
    run(0);
}

