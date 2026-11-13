// CUDA row-reduction host plan in Goose, from ggml-cuda/sumrows.cu and mean.cu.
//
// Packet: (0,0) src, (0,1) dst, (0,2) ncols, (0,3) nrows, (0,4) nsm,
// (0,5) strided flag, (0,6) norm flag, (0,7) ne1, (0,8) ne2,
// (0,9) nb1, (0,10) nb2, (0,11) nb3.
//
// Both native entry points use the same occupancy heuristic: a block of 512
// threads while nrows/nsm < 2, otherwise 32 threads for ncols < 1024 and 128
// threads above that. Grid is one block per row.
extern "fg414h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg414h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg414h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg414h_launch" fn launch(packet: i64) -> i64;
extern "fg414h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let src = get(packet, 0, 0);
    let dst = get(packet, 0, 1);
    let ncols = get(packet, 0, 2) as i64;
    let nrows = get(packet, 0, 3) as i64;
    let nsm = get(packet, 0, 4) as i64;
    let strided = get(packet, 0, 5) as i64;
    let ne1 = get(packet, 0, 7) as i64;
    let ne2 = get(packet, 0, 8) as i64;
    ensure(src != (0 as u64) && dst != (0 as u64), 20);
    ensure(ncols > (0 as i64) && nrows > (0 as i64) && nsm > (0 as i64), 21);
    ensure(ne1 > (0 as i64) && ne2 > (0 as i64), 22);
    var bdx: i64 = 128 as i64;

    if (nrows / nsm) < (2 as i64) {
        bdx = 512 as i64;
    } else {
        if ncols < (1024 as i64) {
            bdx = 32 as i64;
        }
    }
    let p = plan_ptr(packet);
    put(p, 1, 0, nrows as u64);
    put(p, 1, 1, 1 as u64);
    put(p, 1, 2, 1 as u64);
    put(p, 1, 3, bdx as u64);
    put(p, 1, 4, 1 as u64);
    put(p, 1, 5, 1 as u64);
    put(p, 1, 6, strided as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

