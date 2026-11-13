// Apex CUDA softmax host plan in Goose, from ggml-cuda/softmax.cu.
//
// nth = 32; while (nth < ncols && nth < 1024) nth *= 2;
// grid = (ne01, ne02, ne03); nbytes_shared = (pad(ncols,32) + 32) * 4.
// use_shared = nbytes_shared <= smpbo. In the low path the native may take the
// cooperative parallelize_cols kernel; that path is NOT ported, so it is rejected
// here with code 24 instead of launching a different algorithm silently.
//
// Packet: (0,0) dst,(0,1) src,(0,2) ncols,(0,3) ne01,(0,4) ne02,(0,5) ne03,
//         (0,6) smpbo,(0,7) coop,(0,8) scale bits,(0,9) max_bias bits,
//         (1,0) nrows.
extern "fg281h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg281h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg281h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg281h_launch" fn launch(packet: i64) -> i64;
extern "fg281h_u32f" fn u32f(b: u32) -> f32;
extern "fg281h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let src = get(packet, 0, 1);
    let ncols = get(packet, 0, 2) as i64;
    let ne01 = get(packet, 0, 3) as i64;
    let ne02 = get(packet, 0, 4) as i64;
    let ne03 = get(packet, 0, 5) as i64;
    let smpbo = get(packet, 0, 6) as i64;
    let coop = get(packet, 0, 7) as i64;
    let scale: f32 = u32f(get(packet, 0, 8) as u32);
    let maxb: f32 = u32f(get(packet, 0, 9) as u32);
    let nrows = get(packet, 1, 0) as i64;
    ensure(dst != (0 as u64) && src != (0 as u64), 20);
    ensure(ncols > 0 as i64 && ne01 > 0 as i64 && ne02 > 0 as i64 && ne03 > 0 as i64 && nrows > 0 as i64, 21);
    var nth: i64 = 32 as i64;

    while nth < ncols as i64 && nth < 1024 as i64 {
        nth = nth * 2 as i64;
    }
    let padn: i64 = ( (ncols + (32 as i64) - (1 as i64)) / (32 as i64)) * (32 as i64);
    let nbs: i64 = (padn + (32 as i64)) * (4 as i64);
    var use_shared: i64 = 0 as i64;
    var shared: i64 = 32 as i64 * (4 as i64);

    if nbs <= smpbo as i64 {
        use_shared = 1 as i64;
        shared = nbs;
    } else {
        if coop != 0 as i64 {
            if (ncols / nrows) > 8192 as i64 {
                if scale == (1.0 as f32) {
                    if maxb == (0.0 as f32) {
                        fail(24);
                    }
                }
            }
        }
    }
    let p = plan_ptr(packet);
    put(p, 1, 1, nth as u64);
    put(p, 1, 2, shared as u64);
    put(p, 1, 3, use_shared as u64);
    put(p, 1, 4, ne01 as u64);
    put(p, 1, 5, ne02 as u64);
    put(p, 1, 6, ne03 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

