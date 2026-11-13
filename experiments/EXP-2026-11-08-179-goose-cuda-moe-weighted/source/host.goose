// Apex CUDA MoE weighted reduction host planning (Goose), from
// ggml-cuda/moe-weighted-reduction.cu: launch_moe_weighted_reduction and
// ggml_cuda_op_moe_weighted_reduction.
//
// Scope: the full assert set (all float, contiguous, expert_scale optional), the
// extent derivation (n_embd = experts->ne[0], n_expert_used = experts->ne[1],
// n_tokens = ne[2]*ne[3]) and the launch geometry
//   blocks = (n_tokens, ceil(n_embd/256), 1), threads = 256.
//
// Everything above is Goose. The parent keeps raw tensor reads, the plan arena and
// the launch call. The arithmetic lives in device.goose.
//
// Packet layout written by the bridge:
//   (0,0) experts ptr  (0,1) expert_scale ptr (0 = absent)  (0,2) weights ptr  (0,3) dst ptr
//   (1,0) ne[0] (1,1) ne[1] (1,2) ne[2] (1,3) ne[3] of experts
//   (1,4) contiguous(experts) (1,5) contiguous(weights) (1,6) contiguous(dst)
//   (1,7) expert_scale present ? contiguous(expert_scale) : 1
extern "fg179h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg179h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg179h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg179h_launch" fn launch(packet: i64) -> i64;
extern "fg179h_fail" fn fail(code: i64);

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
    let exp = get(packet, 0, 0);
    let scale = get(packet, 0, 1);
    let w = get(packet, 0, 2);
    let dst = get(packet, 0, 3);
    ensure(exp != 0 && w != 0 && dst != 0, 20);
    // Native: all operands float and contiguous; the optional scale too.
    ensure(get(packet, 1, 4) == 1 && get(packet, 1, 5) == 1 && get(packet, 1, 6) == 1, 21);

    if scale != 0 {
        ensure(get(packet, 1, 7) == 1, 22);
    }
    let ne0 = get(packet, 1, 0) as i64;
    let ne1 = get(packet, 1, 1) as i64;
    let ne2 = get(packet, 1, 2) as i64;
    let ne3 = get(packet, 1, 3) as i64;
    ensure(ne0 > 0 && ne1 > 0, 23);
    ensure(ne2 >= 0 && ne3 >= 0, 24);
    let n_embd: u64 = ne0 as u64;
    let n_used: u64 = ne1 as u64;
    let n_tokens: u64 = mul(ne2 as u64, ne3 as u64);
    ensure(n_tokens > 0, 25);
    let threads: u64 = 256 as u64;
    let gy = cld(n_embd, threads);
    ensure(n_tokens <= 2147483647 as u64 && gy <= 2147483647 as u64, 26);
    let p = plan_ptr(packet);
    put(p, 1, 0, n_tokens);
    put(p, 1, 1, gy);
    put(p, 1, 2, threads);
    put(p, 2, 0, n_embd);
    put(p, 2, 1, n_used);
    ensure(launch(packet) == 0, 27);
    return true;
}

fn main() {
    run(0);
}

