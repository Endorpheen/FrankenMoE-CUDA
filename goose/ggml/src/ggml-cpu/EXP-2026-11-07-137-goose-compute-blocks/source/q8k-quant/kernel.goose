struct Q8Block {
    d: f32, qs: i8[256], bsums: i16[16]
}
extern "fg_q8k_float_bits" fn float_bits(x: f32) -> u32;

fn nearest(x: f32) -> i32 {
    let shifted: f32 = x + 12582912.0;
    return ( (float_bits(shifted) & 0x007fffff) as i32) - 0x00400000;
}

fn goose_quantize_q8k(x: const f32[:], y: Q8Block[:]) {
    assert(x.len >= y.len * 256);
    for ib in y.len {
        let out: Q8Block & = & y[ib];
        var maximum: f32 = 0.0;
        var magnitude: f32 = 0.0;
        for j in 256 {
            let v = x[ib * 256 + j];
            let absolute = if v < 0.0 {
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

        if magnitude == 0.0 {
            out.d = 0.0;
            for j in 256 {
                out.qs[j] = 0;
            }
        } else {
            let inverse: f32 = (-127.0) / maximum;
            for j in 256 {
                let q = nearest(inverse * x[ib * 256 + j]);
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
            out.d = 1.0 / inverse;
        }
    }
}

fn main() {
    let x: f32[256] = default < f32[256] > ();
    var y: Q8Block[1] = default < Q8Block[1] > ();
    goose_quantize_q8k(x, y);
    print(y[0].d);
}

