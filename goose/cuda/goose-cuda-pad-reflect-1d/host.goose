// CUDA pad_reflect_1d host plan in Goose, from ggml-cuda/pad_reflect_1d.cu.
//
// Packet layout: (0,0) src, (0,1) dst, (0,2) dst ne0, (0,3) src ne00,
// (0,4) ne01, (0,5) ne02, (0,6) ne03, (0,7) p0, (0,8) p1.
//
// The native op reads p0/p1 from op_params, asserts that the padded length
// matches, and launches grid (ne01 * ceil(ne0/256), ne02, ne03) with a
// 256-thread block. The assertion becomes an explicit refusal here.
extern "fg413h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg413h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg413h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg413h_launch" fn launch(packet: i64) -> i64;
extern "fg413h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let src = get(packet, 0, 0);
    let dst = get(packet, 0, 1);
    let ne0 = get(packet, 0, 2) as i64;
    let ne00 = get(packet, 0, 3) as i64;
    let ne01 = get(packet, 0, 4) as i64;
    let ne02 = get(packet, 0, 5) as i64;
    let ne03 = get(packet, 0, 6) as i64;
    let p0 = get(packet, 0, 7) as i64;
    let p1 = get(packet, 0, 8) as i64;
    ensure(src != (0 as u64) && dst != (0 as u64), 20);
    ensure(ne00 > 0 as i64 && ne01 > 0 as i64 && ne02 > 0 as i64 && ne03 > 0 as i64, 21);
    ensure(p0 >= (0 as i64) && p1 >= (0 as i64), 22);
    ensure(ne0 == ne00 + p0 + p1, 23);
    let bsize: i64 = 256 as i64;
    let tiles0: i64 = (ne0 + bsize - (1 as i64)) / bsize;
    ensure(tiles0 > 0 as i64, 24);
    let p = plan_ptr(packet);
    put(p, 1, 0, (ne01 * tiles0) as u64);
    put(p, 1, 1, ne02 as u64);
    put(p, 1, 2, ne03 as u64);
    put(p, 1, 3, bsize as u64);
    put(p, 1, 4, 1 as u64);
    put(p, 1, 5, 1 as u64);
    ensure(launch(packet) == (0 as i64), 25);
    return true;
}

fn main() {
    run(0);
}

