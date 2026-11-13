// Apex CUDA argsort host plan in Goose, from ggml-cuda/argsort.cu.
//
// ncols_pad = next_power_of_2(ncols); block = min(ncols_pad, 1024);
// grid = (nrows,1,1); shared = ncols_pad * 4 bytes;
// shared must fit the device limit (native asserts this).
//
// Packet: (0,0) dst,(0,1) x,(0,2) ncols,(0,3) nrows,(0,4) smpb (bytes).
extern "fg283h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg283h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg283h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg283h_launch" fn launch(packet: i64) -> i64;
extern "fg283h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let x = get(packet, 0, 1);
    let ncols = get(packet, 0, 2) as i64;
    let nrows = get(packet, 0, 3) as i64;
    let smpb = get(packet, 0, 4) as i64;
    ensure(dst != (0 as u64) && x != (0 as u64), 20);
    ensure(ncols > 0 as i64 && nrows > 0 as i64, 21);
    var np2: i64 = 1 as i64;

    while np2 < ncols as i64 {
        np2 = np2 * 2 as i64;
    }
    var blk: i64 = 1024 as i64;

    if np2 < 1024 as i64 {
        blk = np2;
    }
    let shared: i64 = np2 * (4 as i64);
    ensure(shared <= smpb as i64, 24);
    let p = plan_ptr(packet);
    put(p, 1, 0, blk as u64);
    put(p, 1, 1, shared as u64);
    put(p, 1, 2, nrows as u64);
    put(p, 1, 3, np2 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

