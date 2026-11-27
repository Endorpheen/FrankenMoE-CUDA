const CV_ADD: i64 = 1;
const CV_SUB: i64 = 2;
const CV_MUL: i64 = 3;
const CV_DIV: i64 = 4;
const CV_SCALE: i64 = 5;
const CV_CLAMP: i64 = 6;
const CV_SIGMOID: i64 = 7;
const CV_SILU: i64 = 8;
const CV_SOFTPLUS: i64 = 9;
const CV_SWIGLU: i64 = 10;
const CV_RMS: i64 = 11;
const CV_L2: i64 = 12;
const CV_SIGMOID_REFERENCE: i64 = 13;
const CV_SILU_REFERENCE: i64 = 14;
const CV_SWIGLU_REFERENCE: i64 = 15;
const CV_FLAT_PLAN: i64 = 8;

extern "fg_vector_word" fn cv_word(cfg: u64, i: i64) -> i64;
extern "fg_device_global_load" fn cv_load(address: u64) -> f32;
extern "fg_device_global_store" fn cv_store(address: u64, value: f32);
extern "fg_attn_exp" fn exp32(x: f32) -> f32;
extern "fg_device_reciprocal" fn cv_reciprocal(x: f32) -> f32;
extern "fg_vector_log" fn log32(x: f32) -> f32;
extern "fg_vector_sqrt" fn cv_sqrt(x: f32) -> f32;
extern "fg_vector_min" fn cv_min(a: f32, b: f32) -> f32;
extern "fg_vector_max" fn cv_max(a: f32, b: f32) -> f32;
extern "fg_vector_shuffle_f64" fn cv_shuffle_f64(value: f64, delta: i32) -> f64;

struct CVVec4 { x: f32, y: f32, z: f32, w: f32 }

extern "fg_vector_load4" fn cv_load4(address: u64) -> CVVec4;
extern "fg_vector_store4" fn cv_store4(address: u64, value: CVVec4);

// The standalone plan carries output extents, input extents and byte
// strides. Broadcasting and the resulting address calculation are Goose.
fn cv_offset(cfg: u64, index: i64, shape: i64) -> i64 {
    var pos = index;
    var offset: i64 = 0;
    for axis in 4 {
        let dim = cv_word(cfg, 2 + axis);
        let coord = pos % dim;
        offset += (coord % cv_word(cfg, shape + axis)) * cv_word(cfg, shape + 4 + axis);
        pos /= dim;
    }
    return offset;
}

fn cv_warp_sum(value: f64) -> f64 {
    var sum = value;
    for step in 5 {
        sum += cv_shuffle_f64(sum, (16 >> step) as i32);
    }
    return sum;
}

fn cv_norm(x: u64, y: u64, dst: u64, cfg: u64, op: i64, row: i64, lane: i64, eps: f32) {
    let width = cv_word(cfg, 2);
    let start = row * width;
    let xrow = x + (cv_offset(cfg, start, 6) as u64);
    let column_stride = cv_word(cfg, 10);
    var sum: f64 = 0.0;
    var i = lane;
    while i < width {
        let v = cv_load(xrow + ((i * column_stride) as u64));
        sum += (v * v) as f64;
        i += 32;
    }
    sum = cv_warp_sum(sum);
    let denominator = if op == CV_L2 {
        cv_max(cv_sqrt(sum as f32), eps)
    } else {
        cv_sqrt(((sum / (width as f64)) as f32) + eps)
    };
    let scale: f32 = 1.0 / denominator;
    i = lane;
    while i < width {
        let index = start + i;
        let value = cv_load(xrow + ((i * column_stride) as u64)) * scale;
        let result = if y == 0 { value } else { value * cv_load(y + (cv_offset(cfg, index, 14) as u64)) };
        cv_store(dst + ((4 * index) as u64), result);
        i += 32;
    }
}

fn cv_value(op: i64, a: f32, b: f32, param0: f32, param1: f32) -> f32 {
    if op == CV_SIGMOID_REFERENCE || op == CV_SILU_REFERENCE || op == CV_SWIGLU_REFERENCE {
        let s = cv_reciprocal(1.0 + exp32(-a));
        return if op == CV_SIGMOID_REFERENCE { s }
            else if op == CV_SILU_REFERENCE { a * s }
            else { (a * s) * b };
    }
    return if op == CV_ADD { a + b }
        else if op == CV_SUB { a - b }
        else if op == CV_MUL { a * b }
        else if op == CV_DIV { a / b }
        else if op == CV_SCALE { a * param0 + param1 }
        else if op == CV_CLAMP { cv_min(cv_max(a, param0), param1) }
        else if op == CV_SIGMOID { sigmoid(a) }
        else if op == CV_SILU { silu(a) }
        else if op == CV_SOFTPLUS { softplus(a) }
        else { silu(a) * b };
}

export fn fg_vector_swiglu_reference(gate: f32, up: f32) -> f32 {
    return cv_value(CV_SWIGLU_REFERENCE, gate, up, 0.0, 0.0);
}

export fn fg_vector_flat(x: u64, y: u64, dst: u64, op: i64, count: i64,
    param0: f32, param1: f32, index: i64) {
    if index >= count { return; }
    let offset = (4 * index) as u64;
    let a = cv_load(x + offset);
    let b = if y != 0 { cv_load(y + offset) } else { 0.0 };
    cv_store(dst + offset, cv_value(op, a, b, param0, param1));
}

export fn fg_vector_flat4(x: u64, y: u64, dst: u64, op: i64, vectors: i64,
    param0: f32, param1: f32, index: i64) {
    if index >= vectors { return; }
    let offset = (16 * index) as u64;
    let a = cv_load4(x + offset);
    let b = if y != 0 { cv_load4(y + offset) } else { CVVec4 { 0.0, 0.0, 0.0, 0.0 } };
    cv_store4(dst + offset, CVVec4 {
        cv_value(op, a.x, b.x, param0, param1),
        cv_value(op, a.y, b.y, param0, param1),
        cv_value(op, a.z, b.z, param0, param1),
        cv_value(op, a.w, b.w, param0, param1)
    });
}

export fn fg_vector_ops(x: u64, y: u64, dst: u64, cfg: u64, param0: f32, param1: f32,
                        group: i64, lane: i64) {
    let op = cv_word(cfg, 0);
    if op == CV_RMS || op == CV_L2 {
        cv_norm(x, y, dst, cfg, op, group, lane, param0);
        return;
    }
    let index = group * cv_word(cfg, 25) + lane;
    if index >= cv_word(cfg, 1) { return; }
    let width = cv_word(cfg, 2);
    var xa = cv_offset(cfg, index, 6);
    var xb = xa;
    let gl = cv_word(cfg, 22);
    if gl != 0 {
        // For packed GLU, the input's first extent is twice the output.
        let half_offset = width * cv_word(cfg, 10);
        if gl == 1 { xb += half_offset; } else { xa += half_offset; }
    }
    let a = cv_load(x + (xa as u64));
    let b = if y != 0 { cv_load(y + (cv_offset(cfg, index, 14) as u64)) }
        else if gl != 0 { cv_load(x + (xb as u64)) } else { 0.0 };
    cv_store(dst + ((4 * index) as u64), cv_value(op, a, b, param0, param1));
}
fn silu(x: f32) -> f32 {
    return x / ( (1 as f32) + exp32(-x));
}
fn sigmoid(x: f32) -> f32 {
    return (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
}
fn softplus(x: f32) -> f32 {
    return if x > (20.0 as f32) { x } else { log32( (1.0 as f32) + exp32(x)) };
}

extern "fg_device_mul_rn" fn cv_mul_rn(a: f32, b: f32) -> f32;

export fn fg_moe_add_shared_residual(routed: u64, shared: u64, gate: u64,
                                      residual: u64, output: u64, index: u64) {
    let offset = 4 * index;
    let product = cv_mul_rn(cv_load(shared + offset), cv_load(gate));
    let mixture = cv_load(routed + offset) + product;
    cv_store(output + offset, mixture + cv_load(residual + offset));
}
