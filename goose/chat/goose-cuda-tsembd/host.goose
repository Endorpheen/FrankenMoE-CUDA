// Apex CUDA timestep embedding host plan in Goose, from ggml-cuda/tsembd.cu.
//
// grid = (ceil(((dim+1)/2) / 256), ne00, 1), block = 256.
// Validation: non-null src/dst, ne00 > 0, dim > 0, max_period > 0.
//
// Packet: (0,0) dst, (0,1) timesteps, (0,2) ne00, (0,3) nb1,
//         (0,4) dim, (0,5) max_period.
extern "fg276h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg276h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg276h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg276h_launch" fn launch(packet: i64) -> i64;
extern "fg276h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let src = get(packet, 0, 1);
    let ne00 = get(packet, 0, 2) as i64;
    let dim = get(packet, 0, 4) as i64;
    let maxp = get(packet, 0, 5) as i64;
    let nb1 = get(packet, 0, 3) as i64;
    ensure(dst != (0 as u64) && src != (0 as u64), 20);
    ensure(ne00 > 0 as i64 && dim > 0 as i64 && maxp > 0 as i64, 21);
    ensure(nb1 >= (dim * 4 as i64), 22);
    let half_ceil: i64 = (dim + (1 as i64)) / (2 as i64);
    let nb: i64 = (half_ceil + (256 as i64) - (1 as i64)) / (256 as i64);
    let p = plan_ptr(packet);
    put(p, 1, 0, nb as u64);
    put(p, 1, 1, 256 as u64);
    put(p, 1, 2, ne00 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

