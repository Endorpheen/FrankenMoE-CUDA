struct I2Q8Vec { x: f32, y: f32, z: f32, w: f32 }
extern "q8_vector" fn vector(base: u64, index: i64) -> I2Q8Vec;
extern "q8_load_f32" fn load(base: u64, index: i64) -> f32;
extern "inkling_q8_value_g" fn quant(base: u64, lane: u32) -> f32;
extern "q8_job_word" fn job_word(base: u64, index: i64) -> u64;
extern "q8_store_f32" fn store(base: u64, value: f32);
extern "madd" fn fma(a: f32, b: f32, c: f32) -> f32;
extern fn butterfly(value: f32, distance: i32) -> f32;

fn reduce(value: f32) -> f32 {
    var v = value;
    v += butterfly(v, 16);
    v += butterfly(v, 8);
    v += butterfly(v, 4);
    v += butterfly(v, 2);
    v += butterfly(v, 1);
    return v;
}

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
    width: i64, warps: i64, block_row: i64, tile: i64, thread: i64, fp32: bool, wide: bool) {
    let lane = thread % 32;
    let row = block_row * warps + thread / 32;
    let base = (if wide { 4 } else { 2 }) * tile * width + lane;
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
        if wide {
            c = update(c, val, vector(packed, index + 2 * width));
            d = update(d, val, vector(packed, index + 3 * width));
        }
        at += 32;
    }
    a = reduce_vector(a);
    b = reduce_vector(b);
    if wide { c = reduce_vector(c); d = reduce_vector(d); }
    if lane != 0 { return; }
    let target = output + ((4 * row) as! u64);
    let first = tile * (if wide { 32 } else { 16 }) + 1;
    scatter(target, jobs, first, a);
    scatter(target, jobs, first + 8, b);
    if wide { scatter(target, jobs, first + 16, c); scatter(target, jobs, first + 24, d); }
}

export fn i2_q8_fp32_pair(weights: u64, packed: u64, output: u64, jobs: u64,
    width: i64, warps: i64, block_row: i64, tile: i64, thread: i64) {
    let lane = thread % 32;
    let row = 2 * (block_row * warps + thread / 32);
    let base = 4 * tile * width + lane;
    let weight = row * width + lane;
    let zero = I2Q8Vec {x:0.0,y:0.0,z:0.0,w:0.0};
    var a0 = zero;
    var b0 = zero;
    var c0 = zero;
    var d0 = zero;
    var a1 = zero;
    var b1 = zero;
    var c1 = zero;
    var d1 = zero;
    var at: i64 = 0;
    while at < width {
        let w0 = load(weights, weight + at);
        let w1 = load(weights, weight + width + at);
        let index = base + at;
        let a = vector(packed, index);
        a0 = update(a0, w0, a);
        a1 = update(a1, w1, a);
        let b = vector(packed, index + width);
        b0 = update(b0, w0, b);
        b1 = update(b1, w1, b);
        let c = vector(packed, index + 2 * width);
        c0 = update(c0, w0, c);
        c1 = update(c1, w1, c);
        let d = vector(packed, index + 3 * width);
        d0 = update(d0, w0, d);
        d1 = update(d1, w1, d);
        at += 32;
    }
    a0 = reduce_vector(a0);
    b0 = reduce_vector(b0);
    c0 = reduce_vector(c0);
    d0 = reduce_vector(d0);
    a1 = reduce_vector(a1);
    b1 = reduce_vector(b1);
    c1 = reduce_vector(c1);
    d1 = reduce_vector(d1);
    if lane != 0 { return; }
    let target = output + ((4 * row) as! u64);
    let first = tile * 32 + 1;
    scatter(target, jobs, first, a0);
    scatter(target, jobs, first + 8, b0);
    scatter(target, jobs, first + 16, c0);
    scatter(target, jobs, first + 24, d0);
    scatter(target + 4, jobs, first, a1);
    scatter(target + 4, jobs, first + 8, b1);
    scatter(target + 4, jobs, first + 16, c1);
    scatter(target + 4, jobs, first + 24, d1);
}

extern "q8_store_vector" fn store_vector(base: u64, index: i64, value: I2Q8Vec);

fn pack_vector(input: u64, jobs: u64, first: i64, channel: i64) -> I2Q8Vec {
    return I2Q8Vec {x:load(input + job_word(jobs, first), channel),
        y:load(input + job_word(jobs, first + 2), channel),
        z:load(input + job_word(jobs, first + 4), channel),
        w:load(input + job_word(jobs, first + 6), channel)};
}

export fn i2_q8_pack8(input: u64, jobs: u64, packed: u64, width: i64, group: i64, tile: i64, lane: i64) {
    let channel = group * 128 + lane;
    if channel >= width { return; }
    let first = tile * 16;
    let lo = pack_vector(input, jobs, first, channel);
    let hi = pack_vector(input, jobs, first + 8, channel);
    let output = 2 * tile * width + channel;
    store_vector(packed, output, lo);
    store_vector(packed, output + width, hi);
}

export fn i2_q8_expand(weights: u64, output: u64, stride: u64, width: i64, group: i64, row: i64, lane: i64) {
    let channel = group * 128 + lane;
    if channel < width {
        let weight = weights + (row as! u64) * stride + ((channel / 32 * 34) as! u64);
        store(output + ((4 * (row * width + channel)) as! u64), quant(weight, (channel % 32) as! u32));
    }
}
