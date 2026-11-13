// Apex CUDA mul_mat_vec_f host plan in Goose, from mmvf.cu.
//
// block_size_best = warp; niter_best = ceil(ncols / (2*warp));
// for bs = 2*warp .. max_block step warp: niter = ceil(ncols/(2*bs)); keep the
// smallest niter (first wins). shared = warp*4 bytes (no fusion);
// grid = (nrows, nchannels_dst, nsamples_or_ntokens).
//
// Packet: (0,0) dst,(0,1) x,(0,2) ncols,(0,3) nrows,(0,4) nchannels_dst,
//         (0,5) nsamples_dst,(0,6) warp,(0,7) max_block.
extern "fg287h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg287h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg287h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg287h_launch" fn launch(packet: i64) -> i64;
extern "fg287h_fail" fn fail(code: i64);

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
    let nch = get(packet, 0, 4) as i64;
    let ns = get(packet, 0, 5) as i64;
    let warp = get(packet, 0, 6) as i64;
    let maxb = get(packet, 0, 7) as i64;
    ensure(dst != (0 as u64) && x != (0 as u64), 20);
    ensure(ncols > 0 as i64 && nrows > 0 as i64 && nch > 0 as i64 && ns > 0 as i64, 21);
    ensure(warp > 0 as i64 && maxb >= 2 * warp as i64, 22);
    var best: i64 = warp;
    var niter_best: i64 = (ncols + 2 * warp - (1 as i64)) / (2 * warp);
    var bs: i64 = 2 * warp;

    while bs <= maxb as i64 {
        let niter: i64 = (ncols + 2 * bs - (1 as i64)) / (2 * bs);

        if niter < niter_best as i64 {
            niter_best = niter;
            best = bs;
        }
        bs = bs + warp;
    }
    let shared: i64 = warp * (4 as i64);
    let p = plan_ptr(packet);
    put(p, 1, 0, best as u64);
    put(p, 1, 1, shared as u64);
    put(p, 1, 2, nrows as u64);
    put(p, 1, 3, nch as u64);
    put(p, 1, 4, ns as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

