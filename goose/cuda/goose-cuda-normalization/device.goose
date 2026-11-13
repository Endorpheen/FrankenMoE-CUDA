// Full normalization arithmetic/state;imports are storage,ISA and synchronization effects.
struct F4 {
    x: f32, y: f32, z: f32, w: f32
}
struct F2 {
    x: f32, y: f32
}
extern "fg168_di" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg168_df" fn fl(p: i64, i: i64) -> f32;
extern "fg168_dm" fn coeff(p: i64, i: i64, k: i64) -> u32;
extern "fg168_umulhi" fn mulhi(a: u32, b: u32) -> u32;
extern "fg168_load" fn load(p: u64, i: u64) -> f32;
extern "fg168_store" fn store(p: u64, i: u64, v: f32);
extern "fg168_load4" fn load4(p: u64, i: i32) -> F4;
extern "fg168_store4" fn store4(p: u64, i: i32, v: F4);
extern "fg168_shuffle" fn shuffle(v: f32, offset: i32) -> f32;
extern "fg168_barrier" fn barrier();
extern "fg168_rsqrt" fn rsqrt(v: f32) -> f32;
extern "fg168_fma" fn fma(a: f32, b: f32, c: f32) -> f32;
extern "fg168_max" fn maximum(a: f32, b: f32) -> f32;
extern "fg168_lc" fn lc();
extern "fg168_sync" fn sync();

fn modulo(p: i64, n: u32, key: i64) -> u32 {
    let hi = mulhi(n, coeff(p, key, 0));
    let q = ( ( (hi as u64) + (n as u64)) >> (coeff(p, key, 1) as u64)) as !u32;
    return n - q * coeff(p, key, 2);
}

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
            store(shared, (tid / 32) as u64, value);
        }
        barrier();
        value = 0.0;

        if lane < bs / 32 {
            value = load(shared, lane as u64);
        }
        value = warp(value);
    }
    return value;
}

fn block_pair(v: F2, shared: u64, tid: i32, bs: i32) -> F2 {
    var out = F2 {
        warp(v.x), warp(v.y)
    }
    ;

    if bs > 32 {
        let lane = tid % 32;

        if lane == 0 {
            store(shared, ( (tid / 32) * 2) as u64, out.x);
            store(shared, ( (tid / 32) * 2 + 1) as u64, out.y);
        }
        barrier();
        out = F2 {
            0.0, 0.0
        }
        ;

        if lane < bs / 32 {
            out.x = load(shared, (lane * 2) as u64);
            out.y = load(shared, (lane * 2 + 1) as u64);
        }
        out.x = warp(out.x);
        out.y = warp(out.y);
    }
    return out;
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
    scale_out: f32) {
    var sum: f32 = 0.0;

    if vec {
        var xs: F4[4] = default < F4[4] > ();
        var k: i32 = 0;

        while k < vpt {
            let i = tid + k * bs;

            if i < n / 4 {
                xs[k] = load4(x, i);
                let v = xs[k];
                sum += v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
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
                var out = F4 {
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
                    store(d, col as u64, scale_out * (scale * load(x, col as u64)));
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

fn offset(p: i64, which: i64, row: u64, channel: u64, sample: u64) -> u64 {
    return row * get(p, 2, which * 4 + 1) + channel * get(p, 2, which * 4 + 2) + sample * get(p, 2, which * 4 + 3);
}

fn broadcast_offset(p: i64, which: i64, row: u64, channel: u64, sample: u64) -> u64 {
    let key = (which - 2) * 4;
    let r = modulo(p, row as u32, key + 1) as u64;
    let c = modulo(p, channel as u32, key + 2) as u64;
    let s = modulo(p, sample as u32, key + 3) as u64;
    return offset(p, which, r, c, s);
}

fn lane(
    p: i64,
    mode: i64,
    bs: i32,
    vpt: i32,
    vec: bool,
    multiply: bool,
    plus: bool,
    scaled: bool,
    tid: i32,
    row: u64,
    channel0: u64,
    sample0: u64,
    gridy: u64,
    gridz: u64,
    shared: u64) {
    let x = get(p, 0, 0);
    let dst = get(p, 0, 1);
    let n = get(p, 3, 1) as i32;
    let eps = fl(p, 0);

    if mode == 1 {
        let size = get(p, 3, 15);
        let start = row * size + (tid as u64);
        let end = if (row + 1) * size < get(p, 3, 16) {
            (row + 1) * size
        } else {
            get(p, 3, 16)
        }
        ;
        var sum: f32 = 0.0;
        sync();
        var i = start;

        while i < end {
            sum += load(x, i);
            i += bs as u64;
        }
        sum = block_sum(sum, shared, tid, bs);
        let mean = sum / (size as f32);
        sum = 0.0;
        i = start;

        while i < end {
            let v = load(x, i) - mean;
            store(dst, i, v);
            sum += v * v;
            i += bs as u64;
        }
        sum = block_sum(sum, shared + 128, tid, bs);
        let scale = rsqrt(sum / (size as f32) + eps);
        i = start;

        while i < end {
            store(dst, i, load(dst, i) * scale);
            i += bs as u64;
        }
        return;
    }

    if mode == 6 {
        let nr = get(p, 3, 2);
        let nc = get(p, 3, 3);
        let r = row % nr;
        let c = (row / nr) % nc;
        let s = row / (nr * nc);
        let xr = x + offset(p, 0, r, c, s) * 4;
        let grad = get(p, 0, 4) + offset(p, 4, r, c, s) * 4;
        let d = dst + offset(p, 1, r, c, s) * 4;
        var pair = F2 {
            0.0, 0.0
        }
        ;
        var col: i64 = tid as i64;
        sync();

        while col < (n as i64) {
            let v = load(xr, col as u64);
            pair.x += v * v;
            pair.y += v * load(grad, col as u64);
            col += bs as i64;
        }
        pair = block_pair(pair, shared, tid, bs);
        let mean_eps = pair.x / (n as f32) + eps;
        let sum_eps = pair.x + (n as f32) * eps;
        let sg = rsqrt(mean_eps);
        let sx = -sg * pair.y / sum_eps;
        col = tid as i64;

        while col < (n as i64) {
            store(d, col as u64, sg * load(grad, col as u64) + sx * load(xr, col as u64));
            col += bs as i64;
        }
        return;
    }

    if mode >= 2 && mode <= 5 {
        lc();
    }
    sync();
    var sample = sample0;

    while sample < get(p, 3, 4) {
        var channel = channel0;

        while channel < get(p, 3, 3) {
            let xr = x + offset(p, 0, row, channel, sample) * 4;
            let d = dst + offset(p, 1, row, channel, sample) * 4;

            if mode >= 2 && mode <= 5 {
                var m: u64 = 0;
                var a: u64 = 0;

                if multiply {
                    m = get(p, 0, 2) + broadcast_offset(p, 2, row, channel, sample) * 4;
                }

                if plus {
                    a = get(p, 0, 3) + broadcast_offset(p, 3, row, channel, sample) * 4;
                }
                rms_row(
                    xr,
                    d,
                    shared,
                    m,
                    a,
                    n,
                    tid,
                    bs,
                    vpt,
                    vec,
                    multiply,
                    plus,
                    scaled,
                    get(p, 1, 8) as i32,
                    get(p, 1, 12) as i32,
                    eps,
                    fl(p, 1));
            } else {
                var pair = F2 {
                    0.0, 0.0
                }
                ;
                var col: i64 = tid as i64;

                while col < (n as i64) {
                    let v = load(xr, col as u64);

                    if mode == 0 {
                        pair.x += v;
                    }
                    pair.y += v * v;
                    col += bs as i64;
                }
                var mean: f32 = 0.0;
                var scale: f32 = 0.0;

                if mode == 0 {
                    pair = block_pair(pair, shared, tid, bs);
                    mean = pair.x / (n as f32);
                    let variance = pair.y / (n as f32) - mean * mean;
                    scale = rsqrt(variance + eps);
                } else {
                    pair.y = block_sum(pair.y, shared, tid, bs);
                    scale = rsqrt(maximum(pair.y, eps * eps));
                }
                col = tid as i64;

                while col < (n as i64) {
                    store(d, col as u64, if mode == 0 {
                        (load(xr, col as u64) - mean) * scale
                    } else {
                        scale * load(xr, col as u64)
                    }
                    );
                    col += bs as i64;
                }

                if bs > 32 {
                    barrier();
                }
            }
            channel += gridy;
        }
        sample += gridz;
    }
}

fn main() {
    lane(0, 0, 32, 1, false, false, false, false, 0, 0 as u64, 0 as u64, 0 as u64, 1 as u64, 1 as u64, 0 as u64);
}

