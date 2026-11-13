// Apex CUDA mul_mat_vec_f host planning (Goose), from ggml-cuda/mmvf.cu:
// launch_mul_mat_vec_f_cuda.
//
// Scope: the launch geometry for the executed profile and the guards that decide
// whether the Goose body may handle this call at all.
//   block_size_best: start at warp_size, then try 2*warp, 3*warp, ... up to
//   max_block_size taking whichever minimises niter = ceil(ncols2/block_size)
//   grid = (nrows, nchannels_dst, nsamples_or_ntokens) ; block = (block_size, 1, 1)
//
// The Goose body only covers T=float, ncols_dst=1, no fusion, single-token ids, so
// any other shape returns false and the parent keeps the native path.
//
// Packet layout written by the bridge:
//   (0,0) x (0,1) y (0,2) dst (0,3) ids or 0
//   (1,0) ncols2 (1,1) stride_row (1,2) stride_col_y2 (1,3) stride_col_dst
//   (2,0) stride_channel_x (2,1) stride_channel_y (2,2) stride_channel_dst
//   (3,0) stride_sample_x (3,1) stride_sample_y (3,2) stride_sample_dst
//   (4,0) nrows (4,1) nchannels_dst (4,2) nsamples_or_ntokens (4,3) nchannels_y
//   (5,0) warp (5,1) max_block (5,2) use_ids (5,3) ids_stride
//   (6,0) eligible (6,1) ncols_dst
extern "fg184h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg184h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg184h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg184h_launch" fn launch(packet: i64) -> i64;
extern "fg184h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn ceil_div(a: i64, b: i64) -> i64 {
    ensure(b > 0, 1);
    return a / b + if a % b != 0 {
        1 as i64
    } else {
        0 as i64
    }
    ;
}

fn run(packet: i64) -> bool {
    ensure(get(packet, 0, 0) != 0 && get(packet, 0, 1) != 0 && get(packet, 0, 2) != 0, 20);
    // Only the profile the Goose body implements: F32 source, one destination column,
    // no fusion, at most one token id per call.

    if get(packet, 6, 0) != 1 {
        return false;
    }
    let warp = get(packet, 5, 0) as i64;
    let maxb = get(packet, 5, 1) as i64;
    ensure(warp == 32, 21);
    // Slot (1,0) carries the FULL column count; native searches block sizes on ncols,
    // not on the pair count, and only the kernel body works in float2 pairs.
    let ncols = get(packet, 1, 0) as i64;
    ensure(ncols > 0 && ncols % 2 == 0, 22);
    var best: i64 = warp;
    var best_iter: i64 = ceil_div(ncols, 2 * warp);
    var b: i64 = 2 * warp;

    while b <= maxb {
        let it: i64 = ceil_div(ncols, 2 * b);

        if it < best_iter {
            best_iter = it;
            best = b;
        }
        b = b + warp;
    }
    let nrows = get(packet, 4, 0) as i64;
    let nch = get(packet, 4, 1) as i64;
    let ntok = get(packet, 4, 2) as i64;
    ensure(nrows > 0 && nch > 0 && ntok > 0, 23);
    ensure(nrows <= (2147483647 as i64) && nch <= (2147483647 as i64) && ntok <= (2147483647 as i64), 24);
    let p = plan_ptr(packet);
    put(p, 7, 0, nrows as u64);
    put(p, 7, 1, nch as u64);
    put(p, 7, 2, ntok as u64);
    put(p, 7, 3, best as u64);
    put(p, 7, 4, warp as u64);
    ensure(launch(packet) == 0, 25);
    return true;
}

fn main() {
    run(0);
}

