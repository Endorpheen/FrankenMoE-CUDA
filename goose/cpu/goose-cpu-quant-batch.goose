struct FloatBlock {
    values: f32[256]
}
struct Q8Block {
    d: f32, qs: i8[256], bsums: i16[16]
}
extern "fg_q8k_float_bits" fn float_bits(x: f32) -> u32;

fn nearest(x: f32) -> i32 {
    let shifted: f32 = x + (12582912.0 as f32);
    return ( (float_bits(shifted) & 0x007fffff) as i32) - 0x00400000;
}

fn goose_quantize_q8k(x: const FloatBlock[:], y: Q8Block[:]) {
    assert(x.len >= y.len);
    for ib in y.len {
        let out: Q8Block & = & y[ib];
        let input = & x[ib];
        var maximum: f32 = (0.0 as f32);
        var magnitude: f32 = (0.0 as f32);
        for j in 256 {
            let v = input.values[j];
            let absolute = if v < (0.0 as f32) {
                -v
            } else {
                v
            }
            ;

            if absolute > magnitude {
                magnitude = absolute;
                maximum = v;
            }
        }

        if magnitude == (0.0 as f32) {
            out.d = (0.0 as f32);
            for j in 256 {
                out.qs[j] = 0;
            }
        } else {
            let inverse: f32 = (-(127.0 as f32)) / maximum;
            for j in 256 {
                let q = nearest(inverse * input.values[j]);
                out.qs[j] = (if q < 127 {
                    q
                } else {
                    127
                }
                ) as !i8;
            }
            for j in 16 {
                var sum: i32 = 0;
                for k in 16 {
                    sum += out.qs[j * 16 + k] as i32;
                }
                out.bsums[j] = sum as !i16;
            }
            out.d = (1.0 as f32) / inverse;
        }
    }
}
struct Float32Block {
    values: f32[32]
}
struct Q8Zero {
    d: u16, qs: i8[32]
}
extern "fg139_float_half" fn to_half(x: f32) -> u16;
extern "fg139_float_from_bits" fn from_bits(x: u32) -> f32;
extern "fg139_quant_witness" fn witness(kind: i64, blocks: i64);

fn maximum(a: f32, b: f32) -> f32 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}

fn absolute(x: f32) -> f32 {
    return from_bits(float_bits(x) & 0x7fffffff);
}

fn trunc_valid(x: f32) -> i32 {
    let raw = float_bits(x);
    let exponent = ( ( (raw >> 23) & 255) as i32) - 127;

    if exponent < 0 {
        return 0;
    }
    let mantissa = (raw & 0x007fffff) | 0x00800000;
    let value = if exponent >= 23 {
        mantissa << ( (exponent - 23) as u32)
    } else {
        mantissa >> ( (23 - exponent) as u32)
    }
    ;
    return if (raw & 0x80000000) != 0 {
        -(value as i32)
    } else {
        value as i32
    }
    ;
}

fn nearest_saturated(x: f32) -> i8 {
    if (float_bits(x) & 0x7fffffff) >= 0x4f000000 {
        return -128 as i8;
    }
    var whole: i32 = trunc_valid(x);
    let fraction = x - (whole as f32);

    if fraction > (0.5 as f32) || (fraction == (0.5 as f32) && (whole & 1) != 0) {
        whole += 1;
    }

    if fraction < (-(0.5 as f32)) || (fraction == (-(0.5 as f32)) && (whole & 1) != 0) {
        whole -= 1;
    }
    return (if whole > 127 {
        127
    } else

    if whole < (-128) {
        -128
    } else {
        whole
    }
    ) as !i8;
}

fn quantize_zero(x: const Float32Block[:], y: Q8Zero[:]) {
    assert(x.len >= y.len);
    for ib in y.len {
        let input = & x[ib];
        let out: Q8Zero & = & y[ib];
        var lanes: f32[8] = default < f32[8] > ();
        for lane in 8 {
            let a = maximum(absolute(input.values[lane]), absolute(input.values[8 + lane]));
            let b = maximum(a, absolute(input.values[16 + lane]));
            lanes[lane] = maximum(b, absolute(input.values[24 + lane]));
        }
        let a = maximum(maximum(lanes[4], lanes[0]), maximum(lanes[6], lanes[2]));
        let b = maximum(maximum(lanes[5], lanes[1]), maximum(lanes[7], lanes[3]));
        let magnitude = maximum(a, b);
        out.d = to_half(magnitude / (127.0 as f32));
        let inverse: f32 = if magnitude != (0.0 as f32) {
            (127.0 as f32) / magnitude
        } else {
            (0.0 as f32)
        }
        ;
        for lane in 32 {
            out.qs[lane] = nearest_saturated(input.values[lane] * inverse);
        }
    }
}

fn write_half(dst: u8[:], offset: i64, value: f32) {
    let h = to_half(value);
    dst[offset] = h as !u8;
    dst[offset + 1] = (h >> 8) as !u8;
}

fn encode_compact(kind: i64, x: const f32[:], dst: u8[:]) {
    assert(kind >= 0 && kind <= 7);
    let width = if kind == 0 {
        128
    } else

    if kind == 1 {
        64
    } else {
        32
    }
    ;
    let stride = if kind <= 2 {
        18
    } else

    if kind == 3 {
        20
    } else

    if kind == 4 {
        22
    } else

    if kind == 5 {
        24
    } else {
        36
    }
    ;
    assert(x.len % width == 0 && x.len / width <= dst.len / stride);
    for ib in x.len / width {
        let base = ib * width;
        let out = ib * stride;
        var magnitude: f32 = 0.0;
        var selected: f32 = 0.0;
        var minimum: f32 = 3.4028234663852886e38;
        var maximum_value: f32 = -3.4028234663852886e38;
        var total: f32 = 0.0;
        for j in width {
            let value = x[base + j];
            let absolute_value = absolute(value);
            assert( (float_bits(value) & 0x7f800000) != 0x7f800000);

            if magnitude < absolute_value {
                magnitude = absolute_value;
                selected = value;
            }

            if value < minimum {
                minimum = value;
            }

            if value > maximum_value {
                maximum_value = value;
            }
            total += absolute_value;
        }

        if kind == 0 {
            write_half(dst, out, total / (128.0 as f32));
            for j in 16 {
                dst[out + 2 + j] = 0;
            }
            for j in 128 {
                if x[base + j] >= 0.0 {
                    dst[out + 2 + j / 8] |= (1 as u8) << ( (j % 8) as u32);
                }
            }
        } else

        if kind == 1 {
            write_half(dst, out, magnitude);
            let inverse = if magnitude > 0.0 {
                (1.0 as f32) / magnitude
            } else {
                0.0 as f32
            }
            ;
            for j in 16 {
                dst[out + 2 + j] = 0;
            }
            for j in 64 {
                let rounded = trunc_valid(round_away(x[base + j] * inverse)) + 1;
                let q = if rounded < 0 {
                    0
                } else

                if rounded > 3 {
                    3
                } else {
                    rounded
                }
                ;
                dst[out + 2 + j / 4] |= (q as u8) << ( ( (j % 4) * 2) as u32);
            }
        } else

        if kind >= 6 {
            let d = magnitude / (127.0 as f32);
            let inverse = if magnitude != 0.0 {
                (127.0 as f32) / magnitude
            } else {
                0.0 as f32
            }
            ;
            write_half(dst, out, d);
            var sum: i32 = 0;
            for j in 32 {
                let q = nearest_saturated(x[base + j] * inverse);
                dst[out + 4 + j] = q as !u8;
                sum += q as i32;
            }
            write_half(dst, out + 2, d * (sum as f32));
        } else {
            let signed_mode = kind == 2 || kind == 4;
            let five = kind == 4 || kind == 5;
            let denominator = if five {
                31
            } else {
                15
            }
            ;
            let center = if five {
                16
            } else {
                8
            }
            ;
            let d = if signed_mode {
                selected / (-(center as f32))
            } else {
                (maximum_value - minimum) / (denominator as f32)
            }
            ;
            let inverse = if d != 0.0 {
                (1.0 as f32) / d
            } else {
                0.0 as f32
            }
            ;
            write_half(dst, out, d);
            var qs = out + 2;
            var high: u32 = 0;

            if !signed_mode {
                write_half(dst, out + 2, minimum);
                qs += 2;
            }
            let high_offset = qs;

            if five {
                qs += 4;
            }
            for j in 16 {
                let a = if signed_mode {
                    x[base + j] * inverse + ( (center as f32) + (0.5 as f32))
                } else {
                    (x[base + j] - minimum) * inverse + (0.5 as f32)
                }
                ;
                let b = if signed_mode {
                    x[base + 16 + j] * inverse + ( (center as f32) + (0.5 as f32))
                } else {
                    (x[base + 16 + j] - minimum) * inverse + (0.5 as f32)
                }
                ;
                let qa = trunc_valid(a);
                let qb = trunc_valid(b);
                let va = (if qa > denominator {
                    denominator
                } else {
                    qa
                }
                ) as u32;
                let vb = (if qb > denominator {
                    denominator
                } else {
                    qb
                }
                ) as u32;
                dst[qs + j] = ( (va & 15) | ( (vb & 15) << 4)) as !u8;

                if five {
                    high |= ( (va & 16) >> 4) << (j as u32);
                    high |= ( (vb & 16) >> 4) << ( (j + 16) as u32);
                }
            }

            if five {
                for j in 4 {
                    dst[high_offset + j] = (high >> ( (j * 8) as u32)) as !u8;
                }
            }
        }
    }
}
extern "roundf" fn round_away(value: f32) -> f32;

fn encode(
    kind: i64,
    x: const f32[:],
    dst: u8[:],
    x32: const Float32Block[:],
    x256: const FloatBlock[:],
    y0: Q8Zero[:],
    yk: Q8Block[:]) -> bool {
    if kind <= 7 {
        for index in x.len {
            if (float_bits(x[index]) & 0x7f800000) == 0x7f800000 {
                return false;
            }
        }
    }

    if kind == 8 {
        quantize_zero(x32, y0);
        witness(8, y0.len);
    } else

    if kind == 9 {
        goose_quantize_q8k(x256, yk);
        witness(9, yk.len);
    } else {
        encode_compact(kind, x, dst);
        witness(kind, x.len);
    }
    return true;
}

fn main() {
    let x: f32[0] = default < f32[0] > ();
    var dst: u8[0] = default < u8[0] > ();
    let x32: Float32Block[0] = default < Float32Block[0] > ();
    let x256: FloatBlock[0] = default < FloatBlock[0] > ();
    var y0: Q8Zero[0] = default < Q8Zero[0] > ();
    var yk: Q8Block[0] = default < Q8Block[0] > ();
    let ignored = encode(0, x, dst, x32, x256, y0, yk);
}

