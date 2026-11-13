struct F4 {
    x: f32, y: f32, z: f32, w: f32
}
extern "fg_rms_load4" fn load4(x: const f32[:], i: i32) -> F4;
extern "fg_rms_store4" fn store4(x: f32[:], i: i32, v: F4);
extern "fg_rms_shuffle" fn shuffle(x: f32, offset: i32) -> f32;
extern "fg_rms_barrier" fn barrier();
extern "fg_rms_rsqrt" fn rsqrt(x: f32) -> f32;
extern "fg_rms_fma" fn fma(x: f32, y: f32, z: f32) -> f32;

fn warp_sum(x: f32) -> f32 {
    var value = x;
    var offset: i32 = 16;

    while offset > 0 {
        value += shuffle(value, offset);
        offset >>= 1;
    }
    return value;
}

fn block_sum(x: f32, shared: f32[:], tid: i32, bs: i32) -> f32 {
    var value = warp_sum(x);

    if bs > 32 {
        let lane = tid % 32;

        if lane == 0 {
            shared[tid / 32] = value;
        }
        barrier();
        value = 0.0;

        if lane < bs / 32 {
            value = shared[lane];
        }
        value = warp_sum(value);
    }
    return value;
}

fn goose_rms_row(
    x: const f32[:],
    dst: f32[:],
    shared: f32[:],
    mul: const f32[:],
    add: const f32[:],
    ncols: i32,
    tid: i32,
    bs: i32,
    vpt: i32,
    vec: bool,
    multiply: bool,
    plus: bool,
    scaled: bool,
    mul_cols: i32,
    add_cols: i32,
    eps: f32,
    scale_out: f32) {
    var sum: f32 = 0.0;

    if vec {
        var xs: F4[4] = default < F4[4] > ();
        var k: i32 = 0;

        while k < vpt {
            let i = tid + k * bs;

            if i < ncols / 4 {
                xs[k] = load4(x, i);
                let v = xs[k];
                sum += v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
            }
            k += 1;
        }
        sum = block_sum(sum, shared, tid, bs);
        let scale = rsqrt(sum / (ncols as f32) + eps);
        k = 0;

        while k < vpt {
            let i = tid + k * bs;

            if i < ncols / 4 {
                let v = xs[k];
                var value = F4 {
                    scale * v.x, scale * v.y, scale * v.z, scale * v.w
                }
                ;

                if multiply {
                    let m = load4(mul, i);
                    value.x *= m.x;
                    value.y *= m.y;
                    value.z *= m.z;
                    value.w *= m.w;
                }

                if plus {
                    let a = load4(add, i);
                    value.x += a.x;
                    value.y += a.y;
                    value.z += a.z;
                    value.w += a.w;
                }

                if scaled {
                    value.x *= scale_out;
                    value.y *= scale_out;
                    value.z *= scale_out;
                    value.w *= scale_out;
                }
                store4(dst, i, value);
            }
            k += 1;
        }
    } else {
        var col = tid;

        while col < ncols {
            let v = x[col];
            sum += v * v;
            col += bs;
        }
        sum = block_sum(sum, shared, tid, bs);
        let scale = rsqrt(sum / (ncols as f32) + eps);
        col = tid;

        while col < ncols {
            if multiply {
                if plus {
                    dst[col] = fma(scale * x[col], mul[col % mul_cols], add[col % add_cols]);
                } else {
                    dst[col] = scale * x[col] * mul[col % mul_cols];
                }
            } else

            if scaled {
                dst[col] = scale_out * (scale * x[col]);
            } else {
                dst[col] = scale * x[col];
            }
            col += bs;
        }
    }

    if bs > 32 {
        barrier();
    }
}

fn main() {
    var x: f32[1] = default < f32[1] > ();
    var shared: f32[32] = default < f32[32] > ();
    goose_rms_row(x, x, shared, x, x, 1, 0, 32, 1, false, false, false, false, 1, 1, 0.000001, 1.0);
    print(x[0]);
}

