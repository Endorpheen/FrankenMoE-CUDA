// Row dequantizers for the basic block types (P7-04 S5a), from
// ggml/src/ggml-quants.c: dequantize_row_q4_0/q4_1/q5_0/q5_1/q8_0.
//
// The input is the packed block image (little-endian, as ggml writes it); the
// output is one f32 per element. The half-precision scale is decoded in Goose so
// the module needs no libc shim.

const T_Q4_0: i64 = 2;
const T_Q4_1: i64 = 3;
const T_Q5_0: i64 = 6;
const T_Q5_1: i64 = 7;
const T_Q8_0: i64 = 8;

const QK: i64 = 32;

fn dq_u8(s: const u8[:], at: i64) -> i64 {
    return s[at] as i64;
}

fn dq_u16(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8);
}

fn dq_u32(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8) | ((s[at + 2] as i64) << 16) | ((s[at + 3] as i64) << 24);
}

// IEEE half -> float, including subnormals and signed zero.
fn dq_half(u: i64) -> f32 {
    let sign = (u >> 15) & 1;
    let exp = (u >> 10) & 31;
    let mant = u & 1023;
    var v: f32 = 0.0;

    if exp == 0 {
        v = (mant as f32) * (1.0 / 16777216.0);
    } else {
        var e = exp - 15;
        var f: f32 = 1.0 + (mant as f32) / 1024.0;
        while e > 0 {
            f = f * 2.0;
            e = e - 1;
        }
        while e < 0 {
            f = f * 0.5;
            e = e + 1;
        }
        v = f;
    }
    if sign != 0 {
        // 0.0 - v flips the sign of a non-zero value but turns -0.0 into
        // +0.0; negation keeps the sign bit, as GGML_FP16_TO_FP32 does.
        return -v;
    }
    return v;
}

fn dq_block_size(t: i64) -> i64 {
    if t == T_Q4_0 { return 18; }
    if t == T_Q4_1 { return 20; }
    if t == T_Q5_0 { return 22; }
    if t == T_Q5_1 { return 24; }
    if t == T_Q8_0 { return 34; }
    return 0;
}

fn dq_impl(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    let bs = dq_block_size(t);
    if bs == 0 || n % QK != 0 || src.len < (n / QK) * bs {
        return 0 - 1;
    }
    let nb = n / QK;
    var i: i64 = 0;

    while i < nb {
        let o = i * bs;

        if t == T_Q8_0 {
            let d = dq_half(dq_u16(src, o));
            var j: i64 = 0;
            while j < QK {
                let q = dq_u8(src, o + 2 + j);
                let v = (q - (q & 128) * 2) as f32;
                out[i * QK + j] = v * d;
                j = j + 1;
            }
        } else if t == T_Q4_0 || t == T_Q4_1 {
            let d = dq_half(dq_u16(src, o));
            var m: f32 = 0.0;
            if t == T_Q4_1 {
                m = dq_half(dq_u16(src, o + 2));
            }
            var j: i64 = 0;
            while j < QK / 2 {
                let b = if t == T_Q4_0 { o + 2 } else { o + 4 };
                let q = dq_u8(src, b + j);
                var x0 = q & 15;
                var x1 = (q >> 4) & 15;
                if t == T_Q4_0 {
                    x0 = x0 - 8;
                    x1 = x1 - 8;
                }
                // q4_0 has no min: adding m (0.0) would turn a -0.0 product
                // into +0.0, which the native dequantizer does not do.
                if t == T_Q5_0 {
                    out[i * QK + j] = (x0 as f32) * d;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d;
                } else {
                    out[i * QK + j] = (x0 as f32) * d + m;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d + m;
                }
                j = j + 1;
            }
        } else {
            let d = dq_half(dq_u16(src, o));
            var m: f32 = 0.0;
            var qh_at = o + 2;
            var qs_at = o + 6;
            if t == T_Q5_1 {
                m = dq_half(dq_u16(src, o + 2));
                qh_at = o + 4;
                qs_at = o + 8;
            }
            let qh = dq_u32(src, qh_at);
            var j: i64 = 0;
            while j < QK / 2 {
                let q = dq_u8(src, qs_at + j);
                let xh0 = ((qh >> j) << 4) & 16;
                let xh1 = (qh >> (j + 12)) & 16;
                var x0 = (q & 15) | xh0;
                var x1 = ((q >> 4) & 15) | xh1;
                if t == T_Q5_0 {
                    x0 = x0 - 16;
                    x1 = x1 - 16;
                }
                // q5_0 has no min either; see the q4_0 note above.
                if t == T_Q5_0 {
                    out[i * QK + j] = (x0 as f32) * d;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d;
                } else {
                    out[i * QK + j] = (x0 as f32) * d + m;
                    out[i * QK + j + QK / 2] = (x1 as f32) * d + m;
                }
                j = j + 1;
            }
        }
        i = i + 1;
    }
    return n;
}

export fn qz_dequant(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    return dq_impl(t, src, n, out);
}

export fn qz_dequant_block_size(t: i64) -> i64 {
    return dq_block_size(t);
}
