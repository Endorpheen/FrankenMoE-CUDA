fn rms_offset(cfg: u64, index: i64, shape: i64) -> i64 {
    var pos = index;
    var offset: i64 = 0;
    for axis in 4 {
        let dim = rms_word(cfg, 2 + axis);
        let coord = pos % dim;
        offset += (coord % rms_word(cfg, shape + axis)) * rms_word(cfg, shape + 4 + axis);
        pos /= dim;
    }
    return offset;
}
struct RmsVec {
    x: f32, y: f32, z: f32, w: f32
}
extern "fg168_load" fn load(p: u64, i: u64) -> f32;
extern "fg168_store" fn store(p: u64, i: u64, v: f32);
extern "fg168_load4" fn load4(p: u64, i: i32) -> RmsVec;
extern "fg168_store4" fn store4(p: u64, i: i32, v: RmsVec);
extern "fg168_shuffle" fn shuffle(v: f32, offset: i32) -> f32;
extern "fg168_barrier" fn barrier();
extern "fg168_rsqrt" fn rsqrt(v: f32) -> f32;
extern "fg168_fma" fn fma(a: f32, b: f32, c: f32) -> f32;
fn warp(v: f32) -> f32 {
    var value = v;
    var shift: i32 = 16;

    while shift > 0 {
        value += shuffle(value, shift);
        shift >>= 1;
    }
    return value;
}
fn block_sum(v: f32, shared: u64, tid: i32, bs: i32) -> f32 {
    var value = warp(v);

    if bs > 32 {
        let lane = tid % 32;

        if lane == 0 {
            rms_shared_store(shared, (tid / 32) as u64, value);
        }
        barrier();
        value = 0.0;

        if lane < bs / 32 {
            value = rms_shared_load(shared, lane as u64);
        }
        value = warp(value);
    }
    return value;
}
fn rms_row(
    x: u64,
    d: u64,
    shared: u64,
    m: u64,
    a: u64,
    n: i32,
    tid: i32,
    bs: i32,
    vpt: i32,
    vec: bool,
    multiply: bool,
    plus: bool,
    scaled: bool,
    mc: i32,
    ac: i32,
    eps: f32,
    scale_out: f32,
    add_zero: bool) {
    var sum: f32 = 0.0;

    if vec {
        var xs: RmsVec[4] = default < RmsVec[4] > ();
        var k: i32 = 0;

        while k < vpt {
            let i = tid + k * bs;

            if i < n / 4 {
                xs[k] = load4(x, i);
                let v = xs[k];
                let xy = fma(v.x, v.x, v.y * v.y);
                let xyz = fma(v.z, v.z, xy);
                sum += fma(v.w, v.w, xyz);
            }
            k += 1;
        }
        sum = block_sum(sum, shared, tid, bs);
        let scale = rsqrt(sum / (n as f32) + eps);
        k = 0;

        while k < vpt {
            let i = tid + k * bs;

            if i < n / 4 {
                let v = xs[k];
                var out = RmsVec {
                    scale * v.x, scale * v.y, scale * v.z, scale * v.w
                }
                ;

                if multiply {
                    let mv = load4(m, i);
                    out.x *= mv.x;
                    out.y *= mv.y;
                    out.z *= mv.z;
                    out.w *= mv.w;
                }

                if plus {
                    let av = load4(a, i);
                    out.x += av.x;
                    out.y += av.y;
                    out.z += av.z;
                    out.w += av.w;
                }

                if scaled {
                    out.x *= scale_out;
                    out.y *= scale_out;
                    out.z *= scale_out;
                    out.w *= scale_out;
                    if add_zero {
                        out.x += 0.0;
                        out.y += 0.0;
                        out.z += 0.0;
                        out.w += 0.0;
                    }
                }
                store4(d, i, out);
            }
            k += 1;
        }
    } else {
        var col: i64 = tid as i64;

        while col < (n as i64) {
            let v = load(x, col as u64);
            sum += v * v;
            col += bs as i64;
        }
        sum = block_sum(sum, shared, tid, bs);
        let scale = rsqrt(sum / (n as f32) + eps);
        col = tid as i64;

        while col < (n as i64) {
            if multiply {
                if plus {
                    store(
                        d,
                        col as u64,
                        fma(scale * load(x, col as u64), load(m, (col % mc) as u64), load(a, (col % ac) as u64)));
                } else {
                    store(d, col as u64, scale * load(x, col as u64) * load(m, (col % mc) as u64));
                }
            } else {
                if scaled {
                    let result = scale_out * (scale * load(x, col as u64));
                    store(d, col as u64, if add_zero { result + 0.0 } else { result });
                } else {
                    store(d, col as u64, scale * load(x, col as u64));
                }
            }
            col += bs as i64;
        }
    }

    if bs > 32 {
        barrier();
    }
}
extern "fg_vector_word" fn rms_word(cfg: u64, index: i64) -> i64;
extern "fg_rms_shared_load" fn rms_shared_load(address: u64, offset: u64) -> f32;
extern "fg_rms_shared_store" fn rms_shared_store(address: u64, offset: u64, value: f32);

export fn fg_reference_rms(x: u64, weights: u64, out: u64, cfg: u64, eps: f32, shared: u64,
    row: i64, thread: i32, lanes: i32, vectors: i32, postscale: f32) {
    let width = rms_word(cfg, 2);
    let pos = row * width;
    let start = rms_offset(cfg, pos, 6);
    let dst = out + ((4 * pos) as u64);
    let weighted = weights != 0;
    let m = if weighted { weights + (rms_offset(cfg, pos, 14) as u64) } else { 0 };
    let cols = if weighted { rms_word(cfg, 14) as i32 } else { 1 };
    let scaled = (rms_word(cfg, 23) & 16) != 0;
    rms_row(x + (start as u64), dst, shared, m, 0, width as i32, thread, lanes,
        vectors, vectors > 0, weighted, false, scaled, cols, 1, eps, postscale, scaled);
}
