// Apex CUDA acc host plan in Goose, from ggml-cuda/acc.cu.
//
// num_blocks = ceil(ne / CUDA_ACC_BLOCK_SIZE), block = 256.
// Checks: non-null x/y/dst, ne > 0, and s11/s12/s13 > 0 (the native kernel
// divides by them without validation; zero would be undefined behavior).
//
// Packet: (0,0) dst,(0,1) x,(0,2) y,(0,3) ne,(0,4) s11,(0,5) s12,(0,6) s13.
extern "fg278h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg278h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg278h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg278h_launch" fn launch(packet: i64) -> i64;
extern "fg278h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let x = get(packet, 0, 1);
    let y = get(packet, 0, 2);
    let ne = get(packet, 0, 3) as i64;
    let s11 = get(packet, 0, 4) as i64;
    let s12 = get(packet, 0, 5) as i64;
    let s13 = get(packet, 0, 6) as i64;
    ensure(dst != (0 as u64) && x != (0 as u64) && y != (0 as u64), 20);
    ensure(ne > 0 as i64, 21);
    ensure(s11 > 0 as i64 && s12 > 0 as i64 && s13 > 0 as i64, 22);
    let nb: i64 = (ne + (256 as i64) - (1 as i64)) / (256 as i64);
    let p = plan_ptr(packet);
    put(p, 1, 0, nb as u64);
    put(p, 1, 1, 256 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

