struct Shape {
    ne: i64[4], stride: i64[4], kind: i64
}
struct Plan {
    a: Shape, b: Shape, d: Shape, ith: i64, nth: i64, mode: i64, op: i64, params: f32[4]
}
extern "fg_el_bits" fn bits(x: f32) -> u32;
extern "fg_el_from_bits" fn from_bits(x: u32) -> f32;
extern "fg_el_half" fn half(x: u16) -> f32;
extern "fg_el_to_half" fn to_half(x: f32) -> u16;
extern "fg_element_witness" fn witness(mode: i64, op: i64, values: i64);
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

fn bf16(x: u16) -> f32 {
    return from_bits( (x as u32) << 16);
}

fn to_bf16(x: f32) -> u16 {
    let u = bits(x);

    if (u & 2147483647) > 2139095040 {
        return ( (u >> 16) | 64) as !u16;
    }
    return ( (u + (32767 + ( (u >> 16) & 1))) >> 16) as !u16;
}

fn minimum(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn row_u_abs(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = abs32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = abs32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = abs32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = abs32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = abs32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_sgn(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else

            if x < (0.0 as f32) {
                -(1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else

            if x < (0.0 as f32) {
                -(1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else

            if x < (0.0 as f32) {
                -(1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else

            if x < (0.0 as f32) {
                -(1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else

            if x < (0.0 as f32) {
                -(1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_neg(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = -x;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = -x;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = -x;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = -x;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = -x;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_step(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                (1.0 as f32)
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_tanh(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = tanh32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = tanh32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = tanh32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = tanh32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = tanh32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_elu(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                expm132(x)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                expm132(x)
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                expm132(x)
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                expm132(x)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                expm132(x)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_relu(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                (0.0 as f32)
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                x
            } else {
                (0.0 as f32)
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_sigmoid(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_hardsigmoid(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_exp(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = exp32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = exp32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = exp32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = exp32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = exp32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_hardswish(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_sqr(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = x * x;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = x * x;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = x * x;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = x * x;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = x * x;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_sqrt(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = sqrt32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = sqrt32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = sqrt32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = sqrt32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = sqrt32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_sin(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = sin32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = sin32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = sin32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = sin32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = sin32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_cos(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = cos32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = cos32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = cos32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = cos32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = cos32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_log(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = log32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = log32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = log32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = log32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = log32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_expm1(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = exp32(x) - (1.0 as f32);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = exp32(x) - (1.0 as f32);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = exp32(x) - (1.0 as f32);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = exp32(x) - (1.0 as f32);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = exp32(x) - (1.0 as f32);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_softplus(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (20.0 as f32) {
                x
            } else {
                log32( (1.0 as f32) + exp32(x))
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (20.0 as f32) {
                x
            } else {
                log32( (1.0 as f32) + exp32(x))
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (20.0 as f32) {
                x
            } else {
                log32( (1.0 as f32) + exp32(x))
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (20.0 as f32) {
                x
            } else {
                log32( (1.0 as f32) + exp32(x))
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (20.0 as f32) {
                x
            } else {
                log32( (1.0 as f32) + exp32(x))
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_floor(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = floor32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = floor32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = floor32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = floor32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = floor32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_ceil(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = ceil32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = ceil32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = ceil32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = ceil32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = ceil32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_round(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = round32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = round32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = round32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = round32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = round32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_trunc(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = trunc32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = trunc32(x);
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = trunc32(x);
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = trunc32(x);
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = trunc32(x);
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_u_xielu(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = a32[a0 + lane];
            let value: f32 = if x > (0.0 as f32) {
                p.params[1] * x * x + p.params[2] * x
            } else {
                (expm132(min32(x, p.params[3])) - x) * p.params[0] + p.params[2] * x
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 1 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                p.params[1] * x * x + p.params[2] * x
            } else {
                (expm132(min32(x, p.params[3])) - x) * p.params[0] + p.params[2] * x
            }
            ;
            d16[d0 + lane] = to_half(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 2 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                p.params[1] * x * x + p.params[2] * x
            } else {
                (expm132(min32(x, p.params[3])) - x) * p.params[0] + p.params[2] * x
            }
            ;
            d16[d0 + lane] = to_bf16(value);
        }
    } else

    if p.a.kind == 2 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = bf16(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                p.params[1] * x * x + p.params[2] * x
            } else {
                (expm132(min32(x, p.params[3])) - x) * p.params[0] + p.params[2] * x
            }
            ;
            d32[d0 + lane] = value;
        }
    } else

    if p.a.kind == 1 && p.d.kind == 0 {
        for lane in p.a.ne[0] {
            let x = half(a16[a0 + lane]);
            let value: f32 = if x > (0.0 as f32) {
                p.params[1] * x * x + p.params[2] * x
            } else {
                (expm132(min32(x, p.params[3])) - x) * p.params[0] + p.params[2] * x
            }
            ;
            d32[d0 + lane] = value;
        }
    } else {
        assert(false);
    }
}

fn row_b_add(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = a32[at] + b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 1 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) + half(b16[bt]);
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 2 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) + bf16(b16[bt]);
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) + b32[bt];
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) + b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) + b32[bt];
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) + b32[bt];
                d32[dt] = value;
            }
        }
    } else {
        assert(false);
    }
}

fn row_b_sub(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = a32[at] - b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 1 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) - half(b16[bt]);
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 2 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) - bf16(b16[bt]);
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) - b32[bt];
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) - b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) - b32[bt];
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) - b32[bt];
                d32[dt] = value;
            }
        }
    } else {
        assert(false);
    }
}

fn row_b_mul(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = a32[at] * b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 1 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) * half(b16[bt]);
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 2 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) * bf16(b16[bt]);
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) * b32[bt];
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) * b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) * b32[bt];
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) * b32[bt];
                d32[dt] = value;
            }
        }
    } else {
        assert(false);
    }
}

fn row_b_div(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:],
    a0: i64,
    b0: i64,
    d0: i64) {
    if p.a.kind == 0 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = a32[at] / b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 1 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) / half(b16[bt]);
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 2 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) / bf16(b16[bt]);
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 2 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) / b32[bt];
                d16[dt] = to_bf16(value);
            }
        }
    } else

    if p.a.kind == 2 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = bf16(a16[at]) / b32[bt];
                d32[dt] = value;
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 1 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) / b32[bt];
                d16[dt] = to_half(value);
            }
        }
    } else

    if p.a.kind == 1 && p.b.kind == 0 && p.d.kind == 0 {
        for repeat in p.a.ne[0] / p.b.ne[0] {
            for lane in p.b.ne[0] {
                let at = a0 + repeat * p.b.ne[0] + lane;
                let bt = b0 + lane * p.b.stride[0];
                let dt = d0 + repeat * p.b.ne[0] + lane;
                let value: f32 = half(a16[at]) / b32[bt];
                d32[dt] = value;
            }
        }
    } else {
        assert(false);
    }
}

fn execute(
    p: const Plan &,
    a32: const f32[:],
    a16: const u16[:],
    b32: const f32[:],
    b16: const u16[:],
    d32: f32[:],
    d16: u16[:]) {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    let nr = p.a.ne[1] * p.a.ne[2] * p.a.ne[3];
    let step = (nr + p.nth - 1) / p.nth;
    let first = step * p.ith;
    let last = minimum(first + step, nr);
    var done: i64 = 0;
    var row = first;

    while row < last {
        let z = row / (p.a.ne[2] * p.a.ne[1]);
        let y = (row - z * p.a.ne[2] * p.a.ne[1]) / p.a.ne[1];
        let x = row - z * p.a.ne[2] * p.a.ne[1] - y * p.a.ne[1];
        let a0 = z * p.a.stride[3] + y * p.a.stride[2] + x * p.a.stride[1];
        let d0 = z * p.d.stride[3] + y * p.d.stride[2] + x * p.d.stride[1];
        var b0: i64 = 0;

        if p.mode == 1 {
            b0 = (z % p.b.ne[3]) * p.b.stride[3] + (y % p.b.ne[2]) * p.b.stride[2] + (x % p.b.ne[1]) * p.b.stride[1];
        }

        if p.mode == 0 {
            if p.op == 0 {
                row_u_abs(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 1 {
                row_u_sgn(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 2 {
                row_u_neg(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 3 {
                row_u_step(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 4 {
                row_u_tanh(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 5 {
                row_u_elu(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 6 {
                row_u_relu(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 7 {
                row_u_sigmoid(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 8 {
                row_u_hardsigmoid(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 9 {
                row_u_exp(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 10 {
                row_u_hardswish(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 11 {
                row_u_sqr(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 12 {
                row_u_sqrt(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 13 {
                row_u_sin(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 14 {
                row_u_cos(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 15 {
                row_u_log(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 16 {
                row_u_expm1(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 17 {
                row_u_softplus(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 18 {
                row_u_floor(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 19 {
                row_u_ceil(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 20 {
                row_u_round(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 21 {
                row_u_trunc(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 22 {
                row_u_xielu(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else {
                assert(false);
            }
        } else {
            if p.op == 0 {
                row_b_add(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 1 {
                row_b_sub(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 2 {
                row_b_mul(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else

            if p.op == 3 {
                row_b_div(p, a32, a16, b32, b16, d32, d16, a0, b0, d0);
            } else {
                assert(false);
            }
        }
        done += p.a.ne[0];
        row += 1;
    }

    if done > 0 {
        witness(p.mode, p.op, done);
    }
}

fn main() {
    let p: Plan = default < Plan > ();
    var f: f32[1] = default < f32[1] > ();
    var h: u16[1] = default < u16[1] > ();
    execute(p, f, h, f, h, f, h);
}

