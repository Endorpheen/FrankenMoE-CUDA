// Apex CUDA mul_mat_vec_q host planning (Goose), from ggml-cuda/mmvq.cu:
// calc_nwarps, calc_rows_per_block and calc_launch_params for Q4_K.
//
// Scope: the launch geometry and the eligibility gate for the executed profile.
//   nwarps = 4 and rows_per_block = 1 for ncols_dst = 1 on the SM86+ table
//   blocks_per_iter = vdr*nwarps*warp_size/qi = 2*4*32/32 = 8
//   grid = (ceil(nrows_x/rpb), nchannels_dst, nsamples_or_ntokens) ; block = (32, nwarps)
//
// The Goose body only covers GGML_TYPE_Q4_K with ncols_dst=1 and no fusion, so any
// other type or shape returns false and the parent keeps the native path.
//
// Packet layout written by the bridge:
//   (0,0) vx (0,1) vy (0,2) dst (0,3) ids or 0
//   (1,0) ncols_x (1,1) stride_row_x (1,2) stride_col_y (1,3) stride_col_dst
//   (2,0) stride_channel_x (2,1) stride_channel_y (2,2) stride_channel_dst
//   (3,0) stride_sample_x (3,1) stride_sample_y (3,2) stride_sample_dst
//   (4,0) nrows_x (4,1) nchannels_dst (4,2) nsamples_or_ntokens (4,3) nchannels_y
//   (5,0) warp (5,1) table_nwarps (5,2) use_ids (5,3) ids_stride (5,4) channel_ratio
//   (6,0) eligible (6,1) ncols_dst (6,2) type is Q4_K
extern "fg185h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg185h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg185h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg185h_launch" fn launch(packet: i64) -> i64;
extern "fg185h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn cld(a: u64, b: u64) -> u64 {
    ensure(b != 0, 1);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn run(packet: i64) -> bool {
    ensure(get(packet, 0, 0) != 0 && get(packet, 0, 1) != 0 && get(packet, 0, 2) != 0, 20);

    if get(packet, 6, 0) != 1 {
        return false;
    }
    // Q4_K only; the other 17 instantiations stay native.

    if get(packet, 6, 2) != 1 {
        return false;
    }
    let ncols_x = get(packet, 1, 0);
    ensure(ncols_x > 0, 21);
    // blocks_per_row_x = ncols_x / qk, with qk = 256
    let bpr: i64 = (ncols_x / 256 as u64) as i64;
    ensure(bpr > 0, 22);
    let warp = get(packet, 5, 0) as i64;
    let nwarps = get(packet, 5, 1) as i64;
    ensure(warp == 32 && nwarps == 4, 23);
    // blocks_per_iter = vdr*nwarps*warp/qi with vdr=2 and qi=32
    let bpi: i64 = 2 * nwarps * warp / 32;
    ensure(bpi > 0, 24);
    let nrows = get(packet, 4, 0) as i64;
    let nch = get(packet, 4, 1) as i64;
    let ntok = get(packet, 4, 2) as i64;
    ensure(nrows > 0 && nch > 0 && ntok > 0, 25);
    ensure(nrows <= (2147483647 as i64) && nch <= (2147483647 as i64) && ntok <= (2147483647 as i64), 26);
    let p = plan_ptr(packet);
    put(p, 7, 0, nrows as u64);
    put(p, 7, 1, nch as u64);
    put(p, 7, 2, ntok as u64);
    put(p, 7, 3, bpi as u64);
    put(p, 7, 4, bpr as u64);
    put(p, 7, 5, warp as u64);
    put(p, 7, 6, nwarps as u64);
    ensure(launch(packet) == 0, 27);
    return true;
}

fn main() {
    run(0);
}

