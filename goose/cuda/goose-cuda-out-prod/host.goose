// Apex CUDA out-prod host policy in Goose.
//
// The three branches of ggml_cuda_out_prod, the GEMM parameters each one hands
// to cuBLAS, and the launch geometry of k_compute_out_prod_ptrs. The cuBLAS
// calls themselves are external primitives and stay native; what is ported is
// the decision and everything that feeds it.
//
// src1_T is derived the way ggml_is_transposed derives it: nb[0] != type_size.
//
// Packet k=0 inputs:
//   0 ne0, 1 ne1, 2 ne2, 3 ne3,
//   4 ne00, 5 ne01, 6 ne02, 7 ne03,
//   8 ne10, 9 ne11, 10 ne12, 11 ne13,
//   12 nb01, 13 nb10, 14 nb11,
//   15 nb02, 16 nb03, 17 nb12, 18 nb13,
//   19 nb1, 20 nb2, 21 nb3, 22 type_size
// Packet k=1 outputs:
//   0 branch (0 strided batched, 1 batched ptrs, 2 single),
//   1 lda, 2 ldc, 3 ldb, 4 src1_op (0 N, 1 T), 5 batch,
//   6 s02, 7 s03, 8 s12, 9 s13, 10 s2, 11 s3,
//   12 dps2, 13 dps3, 14 grid_x, 15 grid_y
extern "fg288h_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg288h_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg288h_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run(packet: i64) -> bool {
    let ne0 = get(packet, 0, 0) as i64;
    let ne1 = get(packet, 0, 1) as i64;
    let ne2 = get(packet, 0, 2) as i64;
    let ne3 = get(packet, 0, 3) as i64;
    let ne00 = get(packet, 0, 4) as i64;
    let ne01 = get(packet, 0, 5) as i64;
    let ne02 = get(packet, 0, 6) as i64;
    let ne03 = get(packet, 0, 7) as i64;
    let ne10 = get(packet, 0, 8) as i64;
    let ne11 = get(packet, 0, 9) as i64;
    let ne12 = get(packet, 0, 10) as i64;
    let ne13 = get(packet, 0, 11) as i64;
    let nb01 = get(packet, 0, 12) as i64;
    let nb10 = get(packet, 0, 13) as i64;
    let nb11 = get(packet, 0, 14) as i64;
    let nb02 = get(packet, 0, 15) as i64;
    let nb03 = get(packet, 0, 16) as i64;
    let nb12 = get(packet, 0, 17) as i64;
    let nb13 = get(packet, 0, 18) as i64;
    let nb1 = get(packet, 0, 19) as i64;
    let nb2 = get(packet, 0, 20) as i64;
    let nb3 = get(packet, 0, 21) as i64;
    let tsz = get(packet, 0, 22) as i64;
    ensure(ne01 == ne11 as i64, 40);
    ensure(ne0 == ne00 as i64, 41);
    ensure(ne1 == ne10 as i64, 42);
    ensure(ne02 > 0 as i64 && ne2 % ne02 == (0 as i64), 43);
    ensure(ne03 > 0 as i64 && ne3 % ne03 == (0 as i64), 44);
    ensure(ne2 == ne12 as i64 && ne3 == ne13 as i64, 45);
    ensure(tsz > 0 as i64, 46);
    let src1_t: bool = nb10 != tsz as i64;
    var s1s: i64 = nb10;

    if src1_t {
        s1s = nb11;
    }
    ensure(s1s == tsz as i64, 47);
    let dps2: i64 = ne2 / ne02;
    let dps3: i64 = ne3 / ne03;
    var branch: i64 = 2 as i64;

    if dps2 == (1 as i64) {
        if ne2 > (1 as i64) {
            branch = 0 as i64;
        }
    } else {
        if ne2 > (1 as i64) {
            branch = 1 as i64;
        }
    }

    if branch == (2 as i64) {
        if ne3 > (1 as i64) {
            branch = 1 as i64;
        }
    }
    var batch: i64 = 1 as i64;

    if branch == (0 as i64) {
        batch = ne2;
    }

    if branch == (1 as i64) {
        batch = ne2 * ne3;
    }
    ensure(batch > 0 as i64, 48);
    let lda: i64 = nb01 / tsz;
    let ldc: i64 = nb1 / tsz;
    var ldbraw: i64 = nb11;

    if src1_t {
        ldbraw = nb10;
    }
    let ldb: i64 = ldbraw / tsz;
    var op: i64 = 1 as i64;

    if src1_t {
        op = 0 as i64;
    }
    put(packet, 1, 0, branch as u64);
    put(packet, 1, 1, lda as u64);
    put(packet, 1, 2, ldc as u64);
    put(packet, 1, 3, ldb as u64);
    put(packet, 1, 4, op as u64);
    put(packet, 1, 5, batch as u64);
    put(packet, 1, 6, (nb02 / tsz) as u64);
    put(packet, 1, 7, (nb03 / tsz) as u64);
    put(packet, 1, 8, (nb12 / tsz) as u64);
    put(packet, 1, 9, (nb13 / tsz) as u64);
    put(packet, 1, 10, (nb2 / tsz) as u64);
    put(packet, 1, 11, (nb3 / tsz) as u64);
    put(packet, 1, 12, dps2 as u64);
    put(packet, 1, 13, dps3 as u64);
    put(packet, 1, 14, ( (ne2 + (16 as i64) - (1 as i64)) / (16 as i64)) as u64);
    put(packet, 1, 15, ( (ne3 + (16 as i64) - (1 as i64)) / (16 as i64)) as u64);
    return true;
}

fn main() {
    run(0);
}

