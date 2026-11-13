// Apex CUDA diag host plan in Goose, from ggml-cuda/diag.cu.
//
// Contract: num_blocks = ceil(n_elems / CUDA_DIAG_BLOCK_SIZE), block = 256.
// The shape contract (diagonal of the last two dims copied per batch) requires
// every extent positive and n_elems == ne0*ne1*ne2*ne3.
//
// Packet: (0,0) dst, (0,1) src, (0,2..5) ne0..ne3, (0,6) n_elems.
extern "fg275h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg275h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg275h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg275h_launch" fn launch(packet: i64) -> i64;
extern "fg275h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let src = get(packet, 0, 1);
    let ne0 = get(packet, 0, 2) as i64;
    let ne1 = get(packet, 0, 3) as i64;
    let ne2 = get(packet, 0, 4) as i64;
    let ne3 = get(packet, 0, 5) as i64;
    let ne = get(packet, 0, 6) as i64;
    ensure(dst != (0 as u64) && src != (0 as u64), 20);
    ensure(ne0 > 0 as i64 && ne1 > 0 as i64 && ne2 > 0 as i64 && ne3 > 0 as i64 && ne1 == ne0, 21);
    ensure(ne == ne0 * ne1 * ne2 * ne3, 22);
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

