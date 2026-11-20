struct DenseDot { a: f32, b: f32, c: f32, d: f32 }
struct DenseParts { hi: u32, lo: u32 }

extern "fg_dense_load" fn dense_load(address: u64) -> f32;
extern "fg_dense_job" fn dense_job(address: u64) -> u64;
extern "fg_dense_tf32" fn dense_tf32(value: f32) -> u32;
extern "fg_dense_f32" fn dense_f32(bits: u32) -> f32;
extern "fg_dense_store" fn dense_store(address: u64, value: f32);
extern "fg_dense_mma8" fn dense_mma8(a0: u32, a1: u32, a2: u32, a3: u32,
    b0: u32, b1: u32, acc: DenseDot) -> DenseDot;

fn dense_parts(base: u64, k: i64, extent: i64) -> DenseParts {
    if base == 0 || k >= extent { return DenseParts { hi: 0, lo: 0 }; }
    let value = dense_load(base + ((k * 4) as u64));
    let hi = dense_tf32(value);
    return DenseParts { hi: hi, lo: dense_tf32(value - dense_f32(hi)) };
}

// PTX m16n8k8 fragments. Preserve F32 input precision on model-range
// values with a TF32 leading term and the two residual cross-products.
export fn fg_device_dense_mma(weights: u64, input: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, row: i64, rows: i64, count: i64, lane: i64) {
    let group_id = lane / 4;
    let thread_id = lane % 4;
    let row0 = row + group_id;
    let row1 = row0 + 8;
    let w0 = if row0 < rows { weights + ((row0 * stride) as u64) } else { 0 };
    let w1 = if row1 < rows { weights + ((row1 * stride) as u64) } else { 0 };
    let source = if group_id < count { input + dense_job(jobs + ((group_id * 16) as u64)) } else { 0 };
    let zero = DenseDot { a: 0.0, b: 0.0, c: 0.0, d: 0.0 };
    var main = zero;
    var correction = zero;
    var k = thread_id;
    while k - thread_id < extent {
        let a0 = dense_parts(w0, k, extent);
        let a1 = dense_parts(w1, k, extent);
        let a2 = dense_parts(w0, k + 4, extent);
        let a3 = dense_parts(w1, k + 4, extent);
        let b0 = dense_parts(source, k, extent);
        let b1 = dense_parts(source, k + 4, extent);
        main = dense_mma8(a0.hi, a1.hi, a2.hi, a3.hi, b0.hi, b1.hi, main);
        correction = dense_mma8(a0.hi, a1.hi, a2.hi, a3.hi, b0.lo, b1.lo, correction);
        correction = dense_mma8(a0.lo, a1.lo, a2.lo, a3.lo, b0.hi, b1.hi, correction);
        k += 8;
    }
    let col = 2 * thread_id;
    if col < count {
        let dst = output + dense_job(jobs + ((col * 16 + 8) as u64));
        if row0 < rows { dense_store(dst + ((row0 * 4) as u64), main.a + correction.a); }
        if row1 < rows { dense_store(dst + ((row1 * 4) as u64), main.c + correction.c); }
    }
    if col + 1 < count {
        let dst = output + dense_job(jobs + ((col * 16 + 24) as u64));
        if row0 < rows { dense_store(dst + ((row0 * 4) as u64), main.b + correction.b); }
        if row1 < rows { dense_store(dst + ((row1 * 4) as u64), main.d + correction.d); }
    }
}
