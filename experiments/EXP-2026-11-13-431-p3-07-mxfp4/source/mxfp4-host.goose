// MXFP4 MMQ activation quantizer host plan in Goose.
// direct : grid (ne1, ceil(ne0/512), ne2*ne3), block (32,8)
// scatter: grid (n_tokens, ceil(ne0/512), 1), block (32,8)
extern "fg431h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg431h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg431h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg431h_launch" fn launch(packet: i64) -> i64;
extern "fg431h_fail" fn fail(c: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let x = get(packet, 0, 0);
    let ids = get(packet, 0, 1);
    let vy = get(packet, 0, 2);
    let ne00 = get(packet, 1, 0) as i64;
    let s01 = get(packet, 1, 1) as i64;
    let s02 = get(packet, 1, 2) as i64;
    let s03 = get(packet, 1, 3) as i64;
    let ne0 = get(packet, 1, 4) as i64;
    let ne1 = get(packet, 1, 5) as i64;
    let ne2 = get(packet, 1, 6) as i64;
    let ne3 = get(packet, 1, 7) as i64;
    let scatter = get(packet, 2, 0);
    ensure(x != (0 as u64) && vy != (0 as u64), 20);
    ensure(ne0 > (0 as i64) && ne0 % (64 as i64) == (0 as i64), 21);
    let gy: i64 = (ne0 + (512 as i64) - (1 as i64)) / (512 as i64);
    ensure(gy > (0 as i64), 22);
    let p = plan_ptr(packet);

    if scatter != (0 as u64) {
        let n_tokens = get(packet, 2, 1) as i64;
        let nrows = get(packet, 2, 2) as i64;
        ensure(n_tokens > (0 as i64) && nrows > (0 as i64), 23);
        put(p, 1, 0, n_tokens as u64);
        put(p, 1, 1, gy as u64);
        put(p, 1, 2, 1 as u64);
        put(p, 1, 3, 32 as u64);
        put(p, 1, 4, 8 as u64);
        put(p, 1, 5, 1 as u64);
        put(p, 2, 0, 1 as u64);
        put(p, 3, 0, 0 as u64);
        put(p, 3, 1, get(packet, 2, 4));
        put(p, 3, 2, 0 as u64);
        put(p, 3, 3, nrows as u64);
        put(p, 3, 4, 1 as u64);
    } else {
        ensure(ne1 > (0 as i64) && ne2 > (0 as i64) && ne3 > (0 as i64), 24);
        put(p, 1, 0, ne1 as u64);
        put(p, 1, 1, gy as u64);
        put(p, 1, 2, (ne2 * ne3) as u64);
        put(p, 1, 3, 32 as u64);
        put(p, 1, 4, 8 as u64);
        put(p, 1, 5, 1 as u64);
        put(p, 2, 0, 0 as u64);
        put(p, 3, 0, s01 as u64);
        put(p, 3, 1, s02 as u64);
        put(p, 3, 2, s03 as u64);
        put(p, 3, 3, ne1 as u64);
        put(p, 3, 4, ne2 as u64);
    }
    put(p, 4, 0, ne00 as u64);
    put(p, 4, 1, ne0 as u64);
    put(p, 4, 2, get(packet, 2, 3));
    ensure(launch(packet) == (0 as i64), 25);
    return true;
}

fn main() {
    run(0);
}

