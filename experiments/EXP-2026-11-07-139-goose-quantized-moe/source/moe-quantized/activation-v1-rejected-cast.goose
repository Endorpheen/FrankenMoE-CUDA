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

fn nearest_saturated(x: f32) -> i8 {
    if (float_bits(x) & 0x7fffffff) >= 0x4f000000 {
        return -128 as i8;
    }
    var whole: i32 = x as i32;
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

fn activation(kind: i64, x32: const Float32Block[:], x256: const FloatBlock[:], y0: Q8Zero[:], yk: Q8Block[:]) {
    if kind == 0 {
        quantize_zero(x32, y0);
        witness(0, y0.len);
    } else {
        assert(kind == 1);
        goose_quantize_q8k(x256, yk);
        witness(1, yk.len);
    }
}

fn main() {
    let x32: Float32Block[1] = default < Float32Block[1] > ();
    let x256: FloatBlock[1] = default < FloatBlock[1] > ();
    var y0: Q8Zero[1] = default < Q8Zero[1] > ();
    var yk: Q8Block[1] = default < Q8Block[1] > ();
    activation(0, x32, x256, y0, yk);
    print(y0[0].d);
}

