struct I2Q8Vec { x: f32, y: f32, z: f32, w: f32 }
extern "q8_vector" fn vector(base: u64, index: i64) -> I2Q8Vec;
extern "q8_load_f32" fn load(base: u64, index: i64) -> f32;
extern "q8_load_quant" fn quant(base: u64, lane: u32) -> f32;
extern "q8_job_word" fn job_word(base: u64, index: i64) -> u64;
extern "q8_store_f32" fn store(base: u64, value: f32);
extern "madd" fn fma(a: f32, b: f32, c: f32) -> f32;
extern "reduce" fn reduce(value: f32) -> f32;

fn update(acc: I2Q8Vec, val: f32, row: I2Q8Vec) -> I2Q8Vec {
    return I2Q8Vec {x:fma(val,row.x,acc.x), y:fma(val,row.y,acc.y), z:fma(val,row.z,acc.z), w:fma(val,row.w,acc.w)};
}

fn reduce_vector(value: I2Q8Vec) -> I2Q8Vec {
    return I2Q8Vec {x:reduce(value.x), y:reduce(value.y), z:reduce(value.z), w:reduce(value.w)};
}

fn scatter(output: u64, jobs: u64, index: i64, value: I2Q8Vec) {
    store(output + job_word(jobs, index), value.x);
    store(output + job_word(jobs, index + 2), value.y);
    store(output + job_word(jobs, index + 4), value.z);
    store(output + job_word(jobs, index + 6), value.w);
}

export fn i2_q8_rows16(weights: u64, packed: u64, output: u64, jobs: u64, stride: u64,
    width: i64, warps: i64, block_row: i64, tile: i64, thread: i64, fp32: bool) {
    let lane = thread % 32;
    let row = block_row * warps + thread / 32;
    let base = 4 * tile * width + lane;
    let weight = weights + (row as! u64) * stride;
    let zero = I2Q8Vec {x:0.0,y:0.0,z:0.0,w:0.0};
    var a = zero;
    var b = zero;
    var c = zero;
    var d = zero;
    var at: i64 = 0;
    while at < width {
        let val = if fp32 { load(weights, row * width + lane + at) }
            else { quant(weight + ((at / 32 * 34) as! u64), lane as! u32) };
        let index = base + at;
        a = update(a, val, vector(packed, index));
        b = update(b, val, vector(packed, index + width));
        c = update(c, val, vector(packed, index + 2 * width));
        d = update(d, val, vector(packed, index + 3 * width));
        at += 32;
    }
    a = reduce_vector(a);
    b = reduce_vector(b);
    c = reduce_vector(c);
    d = reduce_vector(d);
    if lane != 0 { return; }
    let target = output + ((4 * row) as! u64);
    let first = tile * 32 + 1;
    scatter(target, jobs, first, a);
    scatter(target, jobs, first + 8, b);
    scatter(target, jobs, first + 16, c);
    scatter(target, jobs, first + 24, d);
}
