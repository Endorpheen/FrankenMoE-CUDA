// Apex CUDA add_id host plan in Goose, from ggml-cuda/add-id.cu.
//
// threads = min(ne00, 768), grid = (ne01, ne02).
// Contract: F32 row strides must be 4 bytes (nb00/nb10/nb20), all extents
// positive, all four pointers present.
//
// Packet: (0,0) dst,(0,1) src0,(0,2) src1,(0,3) src2,
//         (0,4) ne0,(0,5) ne1,(0,6) ne2,(0,7) nb00,(0,8) nb10,(0,9) nb20.
extern "fg277h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg277h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg277h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg277h_launch" fn launch(packet: i64) -> i64;
extern "fg277h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let dst = get(packet, 0, 0);
    let s0 = get(packet, 0, 1);
    let s1 = get(packet, 0, 2);
    let s2 = get(packet, 0, 3);
    let ne0 = get(packet, 0, 4) as i64;
    let ne1 = get(packet, 0, 5) as i64;
    let ne2 = get(packet, 0, 6) as i64;
    let nb00 = get(packet, 0, 7) as i64;
    let nb10 = get(packet, 0, 8) as i64;
    let nb20 = get(packet, 0, 9) as i64;
    ensure(dst != (0 as u64) && s0 != (0 as u64) && s1 != (0 as u64) && s2 != (0 as u64), 20);
    ensure(ne0 > 0 as i64 && ne1 > 0 as i64 && ne2 > 0 as i64, 21);
    ensure(nb00 == (4 as i64) && nb10 == (4 as i64) && nb20 == (4 as i64), 22);
    var thr: i64 = ne0;

    if thr > 768 as i64 {
        thr = 768 as i64;
    }
    let p = plan_ptr(packet);
    put(p, 1, 0, thr as u64);
    put(p, 1, 1, ne1 as u64);
    put(p, 1, 2, ne2 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

