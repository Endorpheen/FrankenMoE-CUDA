struct Tensor {
    ne: i64[4], nb: i64[4], kind: i64, bytes: i64
}
struct Node {
    x: Tensor, y: Tensor, z: Tensor, d: Tensor, op: i64, ith: i64, nth: i64, ip: i64[16], fp: f32[8]
}
extern "fg143_bits" fn bits(x: f32) -> u32;
extern "fg143_from_bits" fn from_bits(x: u32) -> f32;
extern "fg143_half" fn half(x: u16) -> f32;
extern "fg143_to_half" fn to_half(x: f32) -> u16;
extern "fabsf" fn abs32(x: f32) -> f32;
extern "expf" fn exp32(x: f32) -> f32;
extern "expm1f" fn expm132(x: f32) -> f32;
extern "logf" fn log32(x: f32) -> f32;
extern "tanhf" fn tanh32(x: f32) -> f32;
extern "sqrtf" fn sqrt32(x: f32) -> f32;
extern "sinf" fn sin32(x: f32) -> f32;
extern "cosf" fn cos32(x: f32) -> f32;
extern "floorf" fn floor32(x: f32) -> f32;
extern "ceilf" fn ceil32(x: f32) -> f32;
extern "roundf" fn round32(x: f32) -> f32;
extern "truncf" fn trunc32(x: f32) -> f32;
extern "fminf" fn min32(a: f32, b: f32) -> f32;
extern "fmaxf" fn max32(a: f32, b: f32) -> f32;
extern "fmaf" fn fma32(a: f32, b: f32, c: f32) -> f32;
extern "erff" fn erf32(x: f32) -> f32;
extern "fg143_witness" fn witness(op: i64, values: i64);

fn low(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn high(a: i64, b: i64) -> i64 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}

fn add_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0 && a <= 0x7fffffffffffffff - b);
    return a + b;
}

fn mul_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0);
    assert(a == 0 || b <= 0x7fffffffffffffff / a);
    return a * b;
}

fn scalar_bytes(kind: i64) -> i64 {
    if kind == 0 || kind == 26 {
        return 4;
    }

    if kind == 1 || kind == 30 || kind == 25 {
        return 2;
    }

    if kind == 24 {
        return 1;
    }

    if kind == 27 || kind == 28 {
        return 8;
    }
    return 0;
}

fn scalar_type(t: const Tensor &) -> bool {
    return scalar_bytes(t.kind) > 0;
}

fn elements(t: const Tensor &) -> i64 {
    var n: i64 = 1;
    for axis in 4 {
        n = mul_size(n, t.ne[axis]);
    }
    return n;
}

fn rows(t: const Tensor &) -> i64 {
    return mul_size(mul_size(t.ne[1], t.ne[2]), t.ne[3]);
}

fn validate(t: const Tensor &, available: i64) {
    assert(t.bytes >= 0 && t.bytes <= available);
    let bytes = scalar_bytes(t.kind);
    assert(bytes > 0);
    var last: i64 = bytes;
    var empty: bool = false;
    for axis in 4 {
        assert(t.ne[axis] >= 0 && t.nb[axis] >= 0);

        if t.ne[axis] == 0 {
            empty = true;
        } else {
            last = add_size(last, mul_size(t.ne[axis] - 1, t.nb[axis]));
        }
    }
    assert(empty || last <= t.bytes);
}

fn offset(t: const Tensor &, index: i64) -> i64 {
    assert(index >= 0 && index < elements(t));
    var n = index;
    var result: i64 = 0;
    for axis in 4 {
        assert(t.ne[axis] > 0);
        result = add_size(result, mul_size(n % t.ne[axis], t.nb[axis]));
        n /= t.ne[axis];
    }
    return result;
}

fn broadcast(t: const Tensor &, d: const Tensor &, index: i64) -> i64 {
    var n = index;
    var result: i64 = 0;
    for axis in 4 {
        assert(t.ne[axis] > 0 && d.ne[axis] > 0 && d.ne[axis] % t.ne[axis] == 0);
        let c = n % d.ne[axis];
        n /= d.ne[axis];
        result = add_size(result, mul_size(c % t.ne[axis], t.nb[axis]));
    }
    return result;
}

fn read16(a: const u8[:], i: i64) -> u16 {
    assert(i >= 0 && i <= a.len - 2);
    return (a[i] as u16) | ( (a[i + 1] as u16) << 8);
}

fn read32(a: const u8[:], i: i64) -> u32 {
    assert(i >= 0 && i <= a.len - 4);
    return (a[i] as u32) | ( (a[i + 1] as u32) << 8) | ( (a[i + 2] as u32) << 16) | ( (a[i + 3] as u32) << 24);
}

fn write16(a: u8[:], i: i64, v: u16) {
    assert(i >= 0 && i <= a.len - 2);
    a[i] = v as !u8;
    a[i + 1] = (v >> 8) as !u8;
}

fn write32(a: u8[:], i: i64, v: u32) {
    assert(i >= 0 && i <= a.len - 4);
    a[i] = v as !u8;
    a[i + 1] = (v >> 8) as !u8;
    a[i + 2] = (v >> 16) as !u8;
    a[i + 3] = (v >> 24) as !u8;
}

fn load_at(t: const Tensor &, a: const u8[:], i: i64) -> f32 {
    if t.kind == 0 {
        return from_bits(read32(a, i));
    }

    if t.kind == 1 {
        return half(read16(a, i));
    }

    if t.kind == 30 {
        return from_bits( (read16(a, i) as u32) << 16);
    }

    if t.kind == 26 {
        return (read32(a, i) as !i32) as f32;
    }

    if t.kind == 25 {
        return (read16(a, i) as !i16) as f32;
    }
    assert(t.kind == 24 && i >= 0 && i < a.len);
    return (a[i] as !i8) as f32;
}

fn trunc_i32(x: f32) -> i32 {
    let u = bits(x);
    let magnitude = u & 0x7fffffff;
    let exponent = ( (magnitude >> 23) & 255) as i64;

    if exponent < 127 {
        return 0;
    }
    assert(exponent <= 158);
    var v: u32 = (magnitude & 0x7fffff) | 0x800000;

    if exponent >= 150 {
        v = v << ( (exponent - 150) as u32);
    } else {
        v = v >> ( (150 - exponent) as u32);
    }

    if u >> 31 != 0 {
        assert(v <= 0x80000000);
        return ( (0 as u32) - v) as !i32;
    }
    assert(v <= 0x7fffffff);
    return v as i32;
}

fn store_at(t: const Tensor &, a: u8[:], i: i64, x: f32) {
    if t.kind == 0 {
        write32(a, i, bits(x));
    } else

    if t.kind == 1 {
        write16(a, i, to_half(x));
    } else

    if t.kind == 30 {
        let u = bits(x);
        let v = if (u & 0x7fffffff) > 0x7f800000 {
            ( (u >> 16) | 64) as !u16
        } else {
            ( (u + (32767 + ( (u >> 16) & 1))) >> 16) as !u16
        }
        ;
        write16(a, i, v);
    } else

    if t.kind == 26 {
        write32(a, i, trunc_i32(x) as !u32);
    } else

    if t.kind == 25 {
        write16(a, i, trunc_i32(x) as !u16);
    } else {
        assert(t.kind == 24 && i >= 0 && i < a.len);
        a[i] = trunc_i32(x) as !u8;
    }
}

fn load(t: const Tensor &, a: const u8[:], index: i64) -> f32 {
    return load_at(t, a, offset(t, index));
}

fn store(t: const Tensor &, a: u8[:], index: i64, value: f32) {
    store_at(t, a, offset(t, index), value);
}

fn thread_first(p: const Node &) -> i64 {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    return p.ith;
}

fn same_shape(a: const Tensor &, b: const Tensor &) -> bool {
    for i in 4 {
        if a.ne[i] != b.ne[i] {
            return false;
        }
    }
    return true;
}

fn gelu(x: f32) -> f32 {
    return (0.5 as f32) * x * ( (1 as f32) + tanh32( (0.7978845608028654 as f32) * x * ( (1 as f32) + (0.044715 as f32) * x * x)));
}

fn gelu_erf(x: f32) -> f32 {
    return (0.5 as f32) * x * ( (1 as f32) + erf32(x * (0.7071067811865475 as f32)));
}

fn gelu_quick(x: f32) -> f32 {
    return x / ( (1 as f32) + exp32(-(1.702 as f32) * x));
}

fn silu(x: f32) -> f32 {
    return x / ( (1 as f32) + exp32(-x));
}

fn unary_value(p: const Node &, op: i64, x: f32) -> f32 {
    if op == 0 {
        return abs32(x);
    }

    if op == 1 {
        return if x > (0.0 as f32) {
            (1.0 as f32)
        } else

        if x < (0.0 as f32) {
            -(1.0 as f32)
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 2 {
        return -x;
    }

    if op == 3 {
        return if x > (0.0 as f32) {
            (1.0 as f32)
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 4 {
        return tanh32(x);
    }

    if op == 5 {
        return if x > (0.0 as f32) {
            x
        } else {
            expm132(x)
        }
        ;
    }

    if op == 6 {
        return if x > (0.0 as f32) {
            x
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 7 {
        return (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
    }

    if op == 12 {
        return min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
    }

    if op == 13 {
        return exp32(x);
    }

    if op == 11 {
        return x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
    }

    if op == 14 {
        return exp32(x) - (1.0 as f32);
    }

    if op == 15 {
        return if x > (20.0 as f32) {
            x
        } else {
            log32( (1.0 as f32) + exp32(x))
        }
        ;
    }

    if op == 18 {
        return floor32(x);
    }

    if op == 19 {
        return ceil32(x);
    }

    if op == 20 {
        return round32(x);
    }

    if op == 21 {
        return trunc32(x);
    }

    if op == 17 {
        return if x > (0.0 as f32) {
            fma32(p.fp[1] * x, x, p.fp[2] * x)
        } else {
            fma32(expm132(min32(x, p.fp[3])) - x, p.fp[0], p.fp[2] * x)
        }
        ;
    }

    if op == 8 {
        return gelu(x);
    }

    if op == 16 {
        return gelu_erf(x);
    }

    if op == 9 {
        return gelu_quick(x);
    }
    assert(op == 10);
    return silu(x);
}

fn pointwise(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 2 && p.op != 4 && p.op != 6 && p.op != 7 && p.op != 8 && p.op != 32 && p.op != 9 && p.op != 10 && p.op != 12 && p.op != 13 && p.op != 11 && p.op != 50 && p.op != 71 && p.op != 91 && p.op != 23 {
        return false;
    }
    var i = thread_first(p);
    let count = elements(p.d);

    while i < count {
        let a = load_at(p.x, x, broadcast(p.x, p.d, i));
        var value: f32 = 0;

        if p.op == 2 || p.op == 4 || p.op == 6 || p.op == 7 || p.op == 8 || p.op == 23 {
            let b = load_at(p.y, y, broadcast(p.y, p.d, i));

            if p.op == 2 || p.op == 4 {
                value = a + b;
            } else

            if p.op == 6 {
                value = a - b;
            } else

            if p.op == 7 {
                value = a * b;
            } else

            if p.op == 8 {
                value = a / b;
            } else {
                let sig = (1 as f32) / ( (1 as f32) + exp32(-a));
                value = b * sig * ( (1 as f32) + a * ( (1 as f32) - sig));
            }
        } else

        if p.op == 32 {
            value = a * p.fp[0] + p.fp[1];
        } else

        if p.op == 9 {
            value = a * a;
        } else

        if p.op == 10 {
            value = sqrt32(a);
        } else

        if p.op == 12 {
            value = sin32(a);
        } else

        if p.op == 13 {
            value = cos32(a);
        } else

        if p.op == 11 {
            value = log32(a);
        } else

        if p.op == 50 {
            value = min32(max32(a, p.fp[0]), p.fp[1]);
        } else

        if p.op == 71 {
            value = if a > (0 as f32) {
                a
            } else {
                a * p.fp[0]
            }
            ;
        } else {
            value = unary_value(p, p.ip[0], a);
        }
        store(p.d, dst, i, value);
        i += p.nth;
    }
    return true;
}

fn glu(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 100 {
        return false;
    }
    let width = p.d.ne[0];
    assert(width > 0 && rows(p.x) == rows(p.d));
    let separate = p.ip[2] != 0;
    let swapped = p.ip[1] != 0;

    if separate {
        assert(same_shape(p.x, p.y) && same_shape(p.x, p.d));
    } else {
        assert(p.x.ne[0] == 2 * width);
    }
    var row = thread_first(p);

    while row < rows(p.d) {
        for col in width {
            let a = load(p.x, x, row * p.x.ne[0] + col + (if separate || !swapped {
                0
            } else {
                width
            }
            ));
            let b = if separate {
                load(p.y, y, row * p.y.ne[0] + col)
            } else {
                load(p.x, x, row * p.x.ne[0] + col + (if swapped {
                    0
                } else {
                    width
                }
                ))
            }
            ;
            let op = p.ip[0];
            var value: f32 = 0;

            if op == GLU_REGLU {
                value = (if a > (0 as f32) {
                    a
                } else {
                    (0 as f32)
                }
                ) * b;
            } else

            if op == GLU_GEGLU {
                value = gelu(a) * b;
            } else

            if op == GLU_GEGLU_ERF {
                value = gelu_erf(a) * b;
            } else

            if op == GLU_GEGLU_QUICK {
                value = gelu_quick(a) * b;
            } else

            if op == GLU_SWIGLU {
                value = silu(a) * b;
            } else

            if op == GLU_SWIGLU_OAI {
                let aa = min32(a, p.fp[1]);
                let bb = max32(min32(b, p.fp[1]), -p.fp[1]);
                value = (aa / ( (1 as f32) + exp32(-p.fp[0] * aa))) * ( (1 as f32) + bb);
            } else {
                assert(op == GLU_SWIGLU_CLAMP);
                let aa = min32(a, p.fp[0]);
                let bb = min32(max32(b, -p.fp[0]), p.fp[0]);
                value = silu(aa) * bb;
            }
            store(p.d, dst, row * width + col, value);
        }
        row += p.nth;
    }
    return true;
}

fn reduction(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 14 && p.op != 15 && p.op != 17 && p.op != 16 && p.op != 18 && p.op != 19 {
        return false;
    }

    if p.op == 14 {
        if p.ith == 0 {
            var total: f64 = 0;
            for row in rows(p.x) {
                var value: f64 = 0;
                for col in p.x.ne[0] {
                    value += load(p.x, x, row * p.x.ne[0] + col) as f64;
                }
                total += value;
            }
            store(p.d, dst, 0, total as f32);
        }
        return true;
    }

    if p.op == 19 {
        assert(p.x.kind == 26 && p.y.kind == 26 && same_shape(p.x, p.y));

        if p.ith == 0 {
            var total: i64 = 0;
            for i in elements(p.x) {
                if read32(x, offset(p.x, i)) == read32(y, offset(p.y, i)) {
                    total += 1;
                }
            }
            assert(p.d.bytes >= 8);
            let u = total as u64;
            for i in 8 {
                dst[i] = (u >> ( (8 * i) as u64)) as !u8;
            }
        }
        return true;
    }
    var row = thread_first(p);
    let width = p.x.ne[0];

    while row < rows(p.x) {
        if p.op == 18 {
            assert(width > 0);
            var at: i64 = 0;
            var best = load(p.x, x, row * width);
            for col in width {
                let v = load(p.x, x, row * width + col);

                if v > best {
                    best = v;
                    at = col;
                }
            }
            assert(p.d.kind == 26 && at <= 0x7fffffff);
            write32(dst, offset(p.d, row), at as u32);
        } else

        if p.op == 16 {
            var value: f32 = 0;
            for col in width {
                value += load(p.x, x, row * width + col);
                store(p.d, dst, row * width + col, value);
            }
        } else {
            var total: f64 = 0;
            for col in width {
                total += load(p.x, x, row * width + col) as f64;
            }
            let value = if p.op == 17 {
                (total / (width as f64)) as f32
            } else {
                total as f32
            }
            ;
            store(p.d, dst, row, value);
        }
        row += p.nth;
    }
    return true;
}

fn normalize(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 24 && p.op != 25 && p.op != 28 && p.op != 26 {
        return false;
    }
    assert(same_shape(p.x, p.d));
    let width = p.x.ne[0];
    assert(width > 0);
    var row = thread_first(p);

    while row < rows(p.x) {
        var sum: f64 = 0;
        var sq: f64 = 0;
        var cross: f64 = 0;
        for col in width {
            let v = load(p.x, x, row * width + col);
            sum += v as f64;

            if p.op == 26 {
                let a = load(p.y, y, row * width + col);
                sq += (a * a) as f64;
                cross += (a * v) as f64;
            }
        }
        let mean = if p.op == 24 {
            (sum / (width as f64)) as f32
        } else {
            (0 as f32)
        }
        ;

        if p.op != 26 {
            for col in width {
                let v = load(p.x, x, row * width + col) - mean;
                sq += (v * v) as f64;
            }
        }
        let variance = if p.op == 28 {
            sq as f32
        } else {
            (sq as f32) / (width as f32)
        }
        ;
        let scale = if p.op == 28 {
            (1 as f32) / max32(sqrt32(variance), p.fp[0])
        } else {
            (1 as f32) / sqrt32(variance + p.fp[0])
        }
        ;
        for col in width {
            let v = load(p.x, x, row * width + col);
            let value = if p.op == 26 {
                (v - load(p.y, y, row * width + col) * (cross as f32) / ( (sq as f32) + p.fp[0] * (width as f32))) * scale
            } else {
                (v - mean) * scale
            }
            ;
            store(p.d, dst, row * width + col, value);
        }
        row += p.nth;
    }
    return true;
}

fn sort_rows(p: const Node &, x: const u8[:], dst: u8[:], indices: i64[:]) -> bool {
    if p.op != 69 && p.op != 70 {
        return false;
    }
    let n = p.x.ne[0];
    assert(p.d.kind == 26 && p.d.ne[0] <= n);
    assert(indices.len >= mul_size(n, p.nth));
    var row = thread_first(p);
    let base = p.ith * n;

    while row < rows(p.x) {
        for i in n {
            indices[base + i] = i;
        }
        for i in n {
            var j = i;

            while j > 0 {
                let ia = indices[base + j - 1];
                let ib = indices[base + j];
                let a = load(p.x, x, row * n + ia);
                let b = load(p.x, x, row * n + ib);
                let ordered = if p.ip[0] == 0 {
                    a <= b
                } else {
                    a >= b
                }
                ;

                if ordered {
                    break;
                }
                indices[base + j - 1] = ib;
                indices[base + j] = ia;
                j -= 1;
            }
        }
        for i in p.d.ne[0] {
            assert(indices[base + i] <= 0x7fffffff);
            write32(dst, offset(p.d, row * p.d.ne[0] + i), indices[base + i] as u32);
        }
        row += p.nth;
    }
    return true;
}
extern "powf" fn pow32(a: f32, b: f32) -> f32;

fn group_normalize(p: const Node &, x: const u8[:], dst: u8[:]) -> bool {
    if p.op != 27 {
        return false;
    }
    assert(same_shape(p.x, p.d));
    let groups = p.ip[0];
    assert(groups > 0);
    let channels = p.x.ne[2];
    let per = (channels + groups - 1) / groups;
    var group = thread_first(p);

    while group < groups {
        let first = group * per;
        let last = low(first + per, channels);
        let count = (last - first) * p.x.ne[0] * p.x.ne[1];
        assert(count > 0);
        for batch in p.x.ne[3] {
            var sum: f64 = 0;
            for ch in last - first {
                for row in p.x.ne[1] {
                    var part: f64 = 0;
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        part += load(p.x, x, index) as f64;
                    }
                    sum += part;
                }
            }
            let mean = (sum / (count as f64)) as f32;
            var sq: f64 = 0;
            for ch in last - first {
                for row in p.x.ne[1] {
                    var part: f64 = 0;
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        let v = load(p.x, x, index) - mean;
                        part += (v * v) as f64;
                        store(p.d, dst, index, v);
                    }
                    sq += part;
                }
            }
            let scale = (1 as f32) / sqrt32( (sq / (count as f64)) as f32 + p.fp[0]);
            for ch in last - first {
                for row in p.x.ne[1] {
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        store(p.d, dst, index, load(p.d, dst, index) * scale);
                    }
                }
            }
        }
        group += p.nth;
    }
    return true;
}

fn softmax(
    p: const Node &,
    x: const u8[:],
    mask: const u8[:],
    sinks: const u8[:],
    dst: u8[:],
    scratch: f32[:]) -> bool {
    if p.op != 46 && p.op != 47 {
        return false;
    }
    let width = p.x.ne[0];
    assert(width > 0 && same_shape(p.x, p.d));
    assert(scratch.len >= mul_size(width, p.nth));
    let base = p.ith * width;
    var row = thread_first(p);

    while row < rows(p.x) {
        if p.op == 47 {
            assert(same_shape(p.x, p.y));
            var dot: f64 = 0;
            for col in width {
                let index = row * width + col;
                dot += (load(p.x, x, index) * load(p.y, mask, index)) as f64;
            }
            for col in width {
                let index = row * width + col;
                let grad = load(p.x, x, index);
                let prob = load(p.y, mask, index);
                store(p.d, dst, index, p.fp[0] * prob * (grad - dot as f32));
            }
        } else {
            let r1 = row % p.x.ne[1];
            let head = (row / p.x.ne[1]) % p.x.ne[2];
            let batch = row / (p.x.ne[1] * p.x.ne[2]);
            var head_power: i64 = 1;

            while head_power <= p.x.ne[2] / 2 {
                head_power *= 2;
            }
            var slope: f32 = 1;

            if p.fp[1] > (0 as f32) {
                let m = pow32(2, -p.fp[1] / ( (if head < head_power {
                    head_power
                } else {
                    2 * head_power
                }
                ) as f32));
                slope = pow32(m, (if head < head_power {
                    head + 1
                } else {
                    2 * (head - head_power) + 1
                }
                ) as f32);
            }
            var maximum = from_bits(0xff800000);
            for col in width {
                var v = load(p.x, x, row * width + col) * p.fp[0];

                if p.ip[0] != 0 {
                    assert(p.y.ne[0] >= width && p.y.ne[1] > r1);
                    let off = col * p.y.nb[0] + r1 * p.y.nb[1] + (head % p.y.ne[2]) * p.y.nb[2] + (batch % p.y.ne[3]) * p.y.nb[3];
                    v += slope * load_at(p.y, mask, off);
                }
                scratch[base + col] = v;

                if v > maximum {
                    maximum = v;
                }
            }

            if p.ip[1] != 0 {
                let v = load(p.z, sinks, head);

                if v > maximum {
                    maximum = v;
                }
            }
            var sum: f64 = 0;
            for col in width {
                let v = exp32(scratch[base + col] - maximum);
                scratch[base + col] = v;
                sum += v as f64;
            }

            if p.ip[1] != 0 {
                sum += exp32(load(p.z, sinks, head) - maximum) as f64;
            }
            assert(sum > (0 as f64));
            let scale = ( (1 as f64) / sum) as f32;
            for col in width {
                store(p.d, dst, row * width + col, scratch[base + col] * scale);
            }
        }
        row += p.nth;
    }
    return true;
}

fn transforms(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    let op = p.op;

    if op != 1 && op != 34 && op != 35 && op != 20 && op != 22 && op != 64 && op != 65 && op != 66 && op != 67 && op != 73 && op != 43 && op != 44 && op != 45 && op != 40 && op != 42 {
        return false;
    }
    var index = thread_first(p);
    let count = elements(p.d);

    if (op == 1 || op == 34 || op == 35) && p.x.kind == p.d.kind {
        assert(count == elements(p.x));
        let bytes = scalar_bytes(p.x.kind);

        while index < count {
            let source = offset(p.x, index);
            let target = offset(p.d, index);
            for byte in bytes {
                dst[target + byte] = x[source + byte];
            }
            index += p.nth;
        }
        return true;
    }

    while index < count {
        var n = index;
        var c: i64[4] = default < i64[4] > ();
        for axis in 4 {
            c[axis] = n % p.d.ne[axis];
            n /= p.d.ne[axis];
        }
        var value: f32 = 0;
        var target = index;

        if op == 73 {
            value = p.fp[0];
        } else

        if op == 67 {
            value = p.fp[0] + p.fp[2] * (index as f32);
        } else

        if op == 20 {
            value = load_at(p.x, x, broadcast(p.x, p.d, index));
        } else

        if op == 22 {
            let axis = p.ip[0];
            assert(axis >= 0 && axis < 4);
            var source = c;

            if c[axis] < p.x.ne[axis] {
                var offset: i64 = 0;
                for k in 4 {
                    offset = add_size(offset, mul_size(source[k], p.x.nb[k]));
                }
                value = load_at(p.x, x, offset);
            } else {
                source[axis] -= p.x.ne[axis];
                var offset: i64 = 0;
                for k in 4 {
                    offset = add_size(offset, mul_size(source[k], p.y.nb[k]));
                }
                value = load_at(p.y, y, offset);
            }
        } else

        if op == 64 {
            var valid: bool = true;
            var offset: i64 = 0;
            for axis in 4 {
                if c[axis] >= p.x.ne[axis] {
                    valid = false;
                } else {
                    offset = add_size(offset, mul_size(c[axis], p.x.nb[axis]));
                }
            }

            if valid {
                value = load_at(p.x, x, offset);
            }
        } else

        if op == 65 {
            let left = p.ip[0];
            let at = c[0] - left;
            let reflected = if at < 0 {
                -at
            } else

            if at >= p.x.ne[0] {
                2 * (p.x.ne[0] - 1) - at
            } else {
                at
            }
            ;
            assert(reflected >= 0 && reflected < p.x.ne[0]);
            value = load_at(p.x, x, reflected * p.x.nb[0] + c[1] * p.x.nb[1] + c[2] * p.x.nb[2] + c[3] * p.x.nb[3]);
        } else

        if op == 66 {
            var offset: i64 = 0;
            for axis in 4 {
                let size = p.x.ne[axis];
                assert(size > 0);
                let at = ( (c[axis] - p.ip[axis]) % size + size) % size;
                offset = add_size(offset, mul_size(at, p.x.nb[axis]));
            }
            value = load_at(p.x, x, offset);
        } else

        if op == 43 {
            if c[0] == c[1] {
                value = load(p.x, x, c[0] + p.x.ne[0] * (c[2] + p.x.ne[2] * c[3]));
            }
        } else

        if op == 44 || op == 45 {
            value = load(p.x, x, index);

            if c[0] > p.ip[0] + c[1] {
                value = if op == 44 {
                    from_bits(0xff800000)
                } else {
                    (0 as f32)
                }
                ;
            }
        } else

        if op == 40 {
            let row = index / p.d.ne[0];
            let id = read32(y, offset(p.y, row)) as !i32;
            assert(id >= 0 && (id as i64) < p.x.ne[1]);
            let ix = c[0] * p.x.nb[0] + (id as i64) * p.x.nb[1] + c[2] * p.x.nb[2] + c[3] * p.x.nb[3];
            value = load_at(p.x, x, ix);
        } else

        if op == 42 {
            return false;
        } else {
            assert(count == elements(p.x));
            value = load(p.x, x, index);
        }
        store(p.d, dst, target, value);
        index += p.nth;
    }
    return true;
}

fn entry(
    p: const Node &,
    x: const u8[:],
    y: const u8[:],
    z: const u8[:],
    dst: u8[:],
    scratch: f32[:],
    indices: i64[:]) -> bool {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    validate(p.d, dst.len);
    validate(p.x, x.len);

    if p.y.bytes > 0 {
        validate(p.y, y.len);
    }

    if p.z.bytes > 0 {
        validate(p.z, z.len);
    }

    if transforms(p, x, y, dst) {
        witness(p.op, elements(p.d));
        return true;
    }

    if p.x.kind == 27 || p.x.kind == 28 || p.d.kind == 27 || p.d.kind == 28 {
        if p.op != 19 {
            return false;
        }
    }
    let handled = pointwise(
        p,
        x,
        y,
        dst) || glu(p,
        x,
        y,
        dst) || reduction(p,
        x,
        y,
        dst) || normalize(p,
        x,
        y,
        dst) || group_normalize(p,
        x,
        dst) || softmax(p,
        x,
        y,
        z,
        dst,
        scratch) || sort_rows(p,
        x,
        dst,
        indices);

    if handled {
        witness(p.op, elements(p.d));
    }
    return handled;
}

fn main() {
    var p: Node = default < Node > ();
    p.nth = 1;
    let x: u8[4] = default < u8[4] > ();
    var d: u8[4] = default < u8[4] > ();
    var f: f32[1] = default < f32[1] > ();
    var i: i64[1] = default < i64[1] > ();
    let ok = entry(p, x, x, x, d, f, i);
    print(ok);
}

