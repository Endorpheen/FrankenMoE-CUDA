// Apex CUDA IQ3_S row dequantization host planning (Goose), from
// ggml-cuda/convert.cu: dequantize_row_iq3_s_cuda.
//
// Contract: nb = ceil(k / QK_K), grid = (nb,1,1), block = 32.
// The native function has no tail guard and always processes whole 256-element
// blocks, so a k that is not a multiple of QK_K would read past the last block
// and write past the destination. This module rejects it instead.
//
// Packet layout: (0,0) source pointer (0,1) element count k (0,2) destination.
extern "fg259h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg259h_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg259h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg259h_launch" fn launch(packet: i64) -> i64;
extern "fg259h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn cld(a: u64, b: u64) -> u64 {
    ensure( (b != (0 as u64)), 1);
    var q: u64 = a / b;

    if (a % b) != (0 as u64) {
        q = q + (1 as u64);
    }
    return q;
}

fn run(packet: i64) -> bool {
    let src = get(packet, 0, 0);
    let k = get(packet, 0, 1);
    let dst = get(packet, 0, 2);
    ensure( (src != (0 as u64)) && (dst != (0 as u64)), 20);
    ensure(k > (0 as u64), 21);
    ensure( (k % (256 as u64)) == (0 as u64), 30);
    ensure(k <= (4294967295 as u64), 22);
    let nb: i64 = (cld(k, 256 as u64)) as i64;
    let p = plan_ptr(packet);
    put(p, 1, 0, nb as u64);
    put(p, 1, 1, 32 as u64);
    ensure(launch(packet) == (0 as i64), 23);
    return true;
}

fn main() {
    run(0);
}

