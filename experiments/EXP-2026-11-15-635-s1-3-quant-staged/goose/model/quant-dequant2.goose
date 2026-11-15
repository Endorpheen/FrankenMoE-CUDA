// Row dequantizers for the remaining block types (P7-04 S5b), from
// ggml/src/ggml-quants.c (llama.cpp-flint-cuda):
//   - dequantize_row_q1_0, dequantize_row_q2_0
//   - dequantize_row_q2_K, dequantize_row_q3_K, dequantize_row_q4_K,
//     dequantize_row_q5_K, dequantize_row_q6_K, dequantize_row_q8_K
//   - dequantize_row_tq1_0, dequantize_row_tq2_0
//   - dequantize_row_mxfp4, dequantize_row_nvfp4
//   - dequantize_row_iq4_nl, dequantize_row_iq4_xs, dequantize_row_iq3_xxs,
//     dequantize_row_iq3_s, dequantize_row_iq2_s, dequantize_row_iq1_m
// and the different-type copy primitives of ggml/src/ggml-cpu/ops.cpp:
//   - ggml_compute_forward_dup_from_q  (quantized -> f32, type traits to_float)
//   - ggml_compute_forward_dup_flt     (f16/bf16 <-> f32 conversions)
//   - ggml_compute_forward_dup_to_q    (f32 -> quantized, type traits from_float)
// for the destination types f16, bf16 and q8_0.
//
// Every scalar expression keeps the native shape and evaluation order: the
// same nibble/bit layout, the same 6-bit k-scale unpacking, the same
// (scale * quant - min) association, the same float d of q8_K, the same
// ternary base-3 decode of tq1_0, the same E8M0 / UE4M3 sub-block scales and
// the same grid-plus-sign decode of the IQ families. Floats are compared bit
// for bit by the gate; the only folded case is the sign of zero, which Goose
// does not carry (documented in README.md).
//
// Host primitives: float bit reinterpretation and the IQ grid tables. The
// grids are built by the native ggml_quantize_init constructors; the port
// owns the decode, not a hand copy of the tables.

extern "qd_bits" fn bits(v: f32) -> i64;
extern "qd_of_bits" fn of_bits(b: i64) -> f32;
extern "qd_iq2_grid" fn iq2_grid(g: i64, i: i64) -> i64;
extern "qd_iq3_grid" fn iq3_grid(g: i64, i: i64) -> i64;

// ---- ggml type ids --------------------------------------------------------

const T_F16: i64 = 1;
const T_Q8_0: i64 = 8;
const T_Q2_K: i64 = 10;
const T_Q3_K: i64 = 11;
const T_Q4_K: i64 = 12;
const T_Q5_K: i64 = 13;
const T_Q6_K: i64 = 14;
const T_Q8_K: i64 = 15;
const T_IQ3_XXS: i64 = 18;
const T_IQ4_NL: i64 = 20;
const T_IQ3_S: i64 = 21;
const T_IQ2_S: i64 = 22;
const T_IQ4_XS: i64 = 23;
const T_IQ1_M: i64 = 29;
const T_BF16: i64 = 30;
const T_TQ1_0: i64 = 34;
const T_TQ2_0: i64 = 35;
const T_MXFP4: i64 = 39;
const T_NVFP4: i64 = 40;
const T_Q1_0: i64 = 41;
const T_Q2_0: i64 = 42;

// ---- geometry -------------------------------------------------------------

const QK_K: i64 = 256;
const QK1_0: i64 = 128;
const QK2_0: i64 = 64;

fn blk_elems(t: i64) -> i64 {
    if t == T_Q1_0 { return 128; }
    if t == T_Q2_0 { return 64; }
    if t == T_MXFP4 { return 32; }
    if t == T_IQ4_NL { return 32; }
    if t == T_NVFP4 { return 64; }
    if t == T_Q2_K { return 256; }
    if t == T_Q3_K { return 256; }
    if t == T_Q4_K { return 256; }
    if t == T_Q5_K { return 256; }
    if t == T_Q6_K { return 256; }
    if t == T_Q8_K { return 256; }
    if t == T_TQ1_0 { return 256; }
    if t == T_TQ2_0 { return 256; }
    if t == T_IQ3_XXS { return 256; }
    if t == T_IQ3_S { return 256; }
    if t == T_IQ2_S { return 256; }
    if t == T_IQ4_XS { return 256; }
    if t == T_IQ1_M { return 256; }
    return 0;
}

fn blk_bytes(t: i64) -> i64 {
    if t == T_Q1_0 { return 18; }
    if t == T_Q2_0 { return 18; }
    if t == T_Q2_K { return 84; }
    if t == T_Q3_K { return 110; }
    if t == T_Q4_K { return 144; }
    if t == T_Q5_K { return 176; }
    if t == T_Q6_K { return 210; }
    if t == T_Q8_K { return 292; }
    if t == T_TQ1_0 { return 54; }
    if t == T_TQ2_0 { return 66; }
    if t == T_MXFP4 { return 17; }
    if t == T_NVFP4 { return 36; }
    if t == T_IQ4_NL { return 18; }
    if t == T_IQ4_XS { return 136; }
    if t == T_IQ3_XXS { return 98; }
    if t == T_IQ3_S { return 110; }
    if t == T_IQ2_S { return 82; }
    if t == T_IQ1_M { return 56; }
    return 0;
}

// ---- byte loads -----------------------------------------------------------

fn u8at(s: const u8[:], at: i64) -> i64 {
    return s[at] as i64;
}

fn u16at(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8);
}

fn u32at(s: const u8[:], at: i64) -> i64 {
    return (s[at] as i64) | ((s[at + 1] as i64) << 8) | ((s[at + 2] as i64) << 16) | ((s[at + 3] as i64) << 24);
}

fn i8of(v: i64) -> i64 {
    if v >= 128 { return v - 256; }
    return v;
}

// IEEE half -> float, the native ggml_compute_fp16_to_fp32 bit trick, so
// subnormals, signed zero and inf/NaN carry the same bits as the oracle.
fn half(h: i64) -> f32 {
    let w = (h << 16) & 0xffffffff;
    let sign = w & 0x80000000;
    let two_w = (w + w) & 0xffffffff;

    let norm = of_bits(((two_w >> 4) + 0x70000000) & 0xffffffff) * pow2_neg(112);
    let denorm = of_bits(((two_w >> 17) | 0x3f000000) & 0xffffffff) - 0.5;

    var bitsv = bits(denorm) & 0xffffffff;
    if two_w >= (1 << 27) {
        bitsv = bits(norm) & 0xffffffff;
    }
    return of_bits((sign | bitsv) & 0xffffffff);
}

fn pow2_neg(n: i64) -> f32 {
    var v: f32 = 1.0;
    var k = 0;
    while k < n {
        v = v * 0.5;
        k = k + 1;
    }
    return v;
}

// ---- small numeric tables -------------------------------------------------

// kvalues_mxfp4: E2M1 values doubled, 0..7 positive, 8..15 negative.
fn kv_mxfp4(i: i64) -> i64 {
    let m = i & 7;
    var v: i64 = m;
    if m == 5 { v = 6; }
    if m == 6 { v = 8; }
    if m == 7 { v = 12; }
    if i >= 8 {
        return 0 - v;
    }
    return v;
}

// kvalues_iq4nl, 16 non-linear values.
fn kv_iq4nl(i: i64) -> i64 {
    if i == 0 { return 0 - 127; }
    if i == 1 { return 0 - 104; }
    if i == 2 { return 0 - 83; }
    if i == 3 { return 0 - 65; }
    if i == 4 { return 0 - 49; }
    if i == 5 { return 0 - 35; }
    if i == 6 { return 0 - 22; }
    if i == 7 { return 0 - 10; }
    if i == 8 { return 1; }
    if i == 9 { return 13; }
    if i == 10 { return 25; }
    if i == 11 { return 38; }
    if i == 12 { return 53; }
    if i == 13 { return 69; }
    if i == 14 { return 89; }
    return 113;
}

// ksigns_iq2xs[i] for i in 0..127: bit j is the sign of element j (j = 0..6,
// element 7 carries the parity bit of the index).
fn ksign(i: i64) -> i64 {
    var v = i & 127;
    var k = 0;
    var p = 0;
    while k < 7 {
        p = p ^ ((i >> k) & 1);
        k = k + 1;
    }
    if p != 0 {
        v = v | 128;
    }
    return v;
}

fn sgn(bit_set: bool) -> f32 {
    if bit_set {
        return 0.0 - 1.0;
    }
    return 1.0;
}

// e8m0_to_fp32 / 2, the MXFP4 block scale.
fn e8m0_half(e: i64) -> f32 {
    var b: i64 = 0;
    if e < 2 {
        b = 2097152 << e;
    } else {
        b = (e - 1) << 23;
    }
    return of_bits(b);
}

// ue4m3 (unsigned E4M3, bias 7) scaled by 0.5, the NVFP4 sub-block scale.
fn ue4m3(x: i64) -> f32 {
    if x == 0 || x == 127 {
        return 0.0;
    }
    let e = (x >> 3) & 15;
    let m = x & 7;
    var raw: f32 = 0.0;
    if e == 0 {
        raw = m as f32;
        var k = 0;
        while k < 9 {
            raw = raw * 0.5;
            k = k + 1;
        }
    } else {
        raw = 1.0 + (m as f32) / 8.0;
        var k = e - 7;
        while k > 0 {
            raw = raw * 2.0;
            k = k - 1;
        }
        while k < 0 {
            raw = raw * 0.5;
            k = k + 1;
        }
    }
    return raw * 0.5;
}

fn pow3(i: i64) -> i64 {
    if i == 0 { return 1; }
    if i == 1 { return 3; }
    if i == 2 { return 9; }
    if i == 3 { return 27; }
    if i == 4 { return 81; }
    return 243;
}

fn grid_byte(gv: i64, j: i64) -> i64 {
    return (gv >> (8 * j)) & 255;
}

// 6-bit k-scale unpacking, sc in bits 0..7 and m in bits 16..23.
fn scale_min_k4(src: const u8[:], base: i64, j: i64) -> i64 {
    if j < 4 {
        let sc = u8at(src, base + j) & 63;
        let m = u8at(src, base + j + 4) & 63;
        return sc | (m << 16);
    }
    let sc = (u8at(src, base + j + 4) & 15) | ((u8at(src, base + j - 4) >> 6) << 4);
    let m = (u8at(src, base + j + 4) >> 4) | ((u8at(src, base + j) >> 6) << 4);
    return sc | (m << 16);
}

// unpacked q3_K scale is in bits 0..7, signed.
fn scal8(s0: i64, s1: i64, s2: i64, s3: i64, is: i64) -> i64 {
    let w = is / 4;
    let sh = (is & 3) * 8;
    var v = s0;
    if w == 1 { v = s1; }
    if w == 2 { v = s2; }
    if w == 3 { v = s3; }
    return i8of((v >> sh) & 255);
}

// ---- per-type decoders ----------------------------------------------------

fn dq_q1_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK1_0;
    var i: i64 = 0;
    while i < nb {
        let o = i * 18;
        let d = half(u16at(src, o));
        let nd = 0.0 - d;
        var j: i64 = 0;
        while j < QK1_0 {
            let b = u8at(src, o + 2 + j / 8);
            let bit = (b >> (j & 7)) & 1;
            if bit != 0 {
                out[i * QK1_0 + j] = d;
            } else {
                out[i * QK1_0 + j] = nd;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_q2_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK2_0;
    var i: i64 = 0;
    while i < nb {
        let o = i * 18;
        let d = half(u16at(src, o));
        var j: i64 = 0;
        while j < QK2_0 {
            let q = (u8at(src, o + 2 + j / 4) >> ((j & 3) * 2)) & 3;
            out[i * QK2_0 + j] = ((q - 1) as f32) * d;
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_q2_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 84;
        let d = half(u16at(src, o + 80));
        let mn = half(u16at(src, o + 82));
        let scales = o;
        let qs = o + 16;
        var is = 0;
        var y = i * QK_K;
        var nn: i64 = 0;
        while nn < QK_K {
            let qb = qs + (nn / 128) * 32;
            var j: i64 = 0;
            while j < 4 {
                let shift = j * 2;
                let sc0 = u8at(src, scales + is);
                is = is + 1;
                let dl0 = d * ((sc0 & 15) as f32);
                let ml0 = mn * ((sc0 >> 4) as f32);
                var l: i64 = 0;
                while l < 16 {
                    out[y + l] = dl0 * (((u8at(src, qb + l) >> shift) & 3) as f32) - ml0;
                    l = l + 1;
                }
                y = y + 16;

                let sc1 = u8at(src, scales + is);
                is = is + 1;
                let dl1 = d * ((sc1 & 15) as f32);
                let ml1 = mn * ((sc1 >> 4) as f32);
                l = 0;
                while l < 16 {
                    out[y + l] = dl1 * (((u8at(src, qb + 16 + l) >> shift) & 3) as f32) - ml1;
                    l = l + 1;
                }
                y = y + 16;
                j = j + 1;
            }
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q3_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 110;
        let d_all = half(u16at(src, o + 108));

        let a0 = u32at(src, o + 96);
        let a1 = u32at(src, o + 100);
        let tmp = u32at(src, o + 104);
        let s0 = (a0 & 0x0f0f0f0f) | (((tmp >> 0) & 0x03030303) << 4);
        let s1 = (a1 & 0x0f0f0f0f) | (((tmp >> 2) & 0x03030303) << 4);
        let s2 = ((a0 >> 4) & 0x0f0f0f0f) | (((tmp >> 4) & 0x03030303) << 4);
        let s3 = ((a1 >> 4) & 0x0f0f0f0f) | (((tmp >> 6) & 0x03030303) << 4);

        var y = i * QK_K;
        var is = 0;
        var nn: i64 = 0;
        while nn < QK_K {
            let qb = o + 32 + (nn / 128) * 32;
            let hb = o;
            var j: i64 = 0;
            while j < 4 {
                let mbit = 1 << ((nn / 128) * 4 + j);
                let dl0 = d_all * ((scal8(s0, s1, s2, s3, is) - 32) as f32);
                is = is + 1;
                var l: i64 = 0;
                while l < 16 {
                    let q = (u8at(src, qb + l) >> (j * 2)) & 3;
                    var hv = 0;
                    if (u8at(src, hb + l) & mbit) == 0 { hv = 4; }
                    out[y + l] = dl0 * ((q - hv) as f32);
                    l = l + 1;
                }
                y = y + 16;

                let dl1 = d_all * ((scal8(s0, s1, s2, s3, is) - 32) as f32);
                is = is + 1;
                l = 0;
                while l < 16 {
                    let q = (u8at(src, qb + 16 + l) >> (j * 2)) & 3;
                    var hv = 0;
                    if (u8at(src, hb + 16 + l) & mbit) == 0 { hv = 4; }
                    out[y + l] = dl1 * ((q - hv) as f32);
                    l = l + 1;
                }
                y = y + 16;
                j = j + 1;
            }
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q4_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 144;
        let d = half(u16at(src, o));
        let mn = half(u16at(src, o + 2));
        let scales = o + 4;
        let qs = o + 16;
        var is = 0;
        var y = i * QK_K;
        var j: i64 = 0;
        while j < QK_K {
            let p0 = scale_min_k4(src, scales, is + 0);
            let p1 = scale_min_k4(src, scales, is + 1);
            let d1 = d * ((p0 & 255) as f32);
            let m1 = mn * (((p0 >> 16) & 255) as f32);
            let d2 = d * ((p1 & 255) as f32);
            let m2 = mn * (((p1 >> 16) & 255) as f32);
            let qb = qs + j / 2;
            var l: i64 = 0;
            while l < 32 {
                out[y + l] = d1 * ((u8at(src, qb + l) & 15) as f32) - m1;
                l = l + 1;
            }
            l = 0;
            while l < 32 {
                out[y + 32 + l] = d2 * ((u8at(src, qb + l) >> 4) as f32) - m2;
                l = l + 1;
            }
            y = y + 64;
            is = is + 2;
            j = j + 64;
        }
        i = i + 1;
    }
}

fn dq_q5_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 176;
        let d = half(u16at(src, o));
        let mn = half(u16at(src, o + 2));
        let scales = o + 4;
        let qh = o + 16;
        let qs = o + 48;
        var is = 0;
        var y = i * QK_K;
        var j: i64 = 0;
        while j < QK_K {
            let p0 = scale_min_k4(src, scales, is + 0);
            let p1 = scale_min_k4(src, scales, is + 1);
            let d1 = d * ((p0 & 255) as f32);
            let m1 = mn * (((p0 >> 16) & 255) as f32);
            let d2 = d * ((p1 & 255) as f32);
            let m2 = mn * (((p1 >> 16) & 255) as f32);
            let u1 = 1 << (j / 64 * 2);
            let u2 = 2 << (j / 64 * 2);
            let qb = qs + j / 2;
            var l: i64 = 0;
            while l < 32 {
                var extra = 0;
                if (u8at(src, qh + l) & u1) != 0 { extra = 16; }
                out[y + l] = d1 * (((u8at(src, qb + l) & 15) + extra) as f32) - m1;
                l = l + 1;
            }
            l = 0;
            while l < 32 {
                var extra = 0;
                if (u8at(src, qh + l) & u2) != 0 { extra = 16; }
                out[y + 32 + l] = d2 * (((u8at(src, qb + l) >> 4) + extra) as f32) - m2;
                l = l + 1;
            }
            y = y + 64;
            is = is + 2;
            j = j + 64;
        }
        i = i + 1;
    }
}

fn dq_q6_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 210;
        let d = half(u16at(src, o + 208));
        var y = i * QK_K;
        var nn: i64 = 0;
        while nn < QK_K {
            let ql = o + (nn / 128) * 64;
            let qh = o + 128 + (nn / 128) * 32;
            let sc = o + 192 + (nn / 128) * 8;
            var l: i64 = 0;
            while l < 32 {
                let is = l / 16;
                let q1 = (((u8at(src, ql + l) & 15) | (((u8at(src, qh + l) >> 0) & 3) << 4)) - 32) as f32;
                let q2 = (((u8at(src, ql + l + 32) & 15) | (((u8at(src, qh + l) >> 2) & 3) << 4)) - 32) as f32;
                let q3 = (((u8at(src, ql + l) >> 4) | (((u8at(src, qh + l) >> 4) & 3) << 4)) - 32) as f32;
                let q4 = (((u8at(src, ql + l + 32) >> 4) | (((u8at(src, qh + l) >> 6) & 3) << 4)) - 32) as f32;
                out[y + l] = d * (i8of(u8at(src, sc + is)) as f32) * q1;
                out[y + l + 32] = d * (i8of(u8at(src, sc + is + 2)) as f32) * q2;
                out[y + l + 64] = d * (i8of(u8at(src, sc + is + 4)) as f32) * q3;
                out[y + l + 96] = d * (i8of(u8at(src, sc + is + 6)) as f32) * q4;
                l = l + 1;
            }
            y = y + 128;
            nn = nn + 128;
        }
        i = i + 1;
    }
}

fn dq_q8_K(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 292;
        let d = of_bits(u32at(src, o));
        var j: i64 = 0;
        while j < QK_K {
            out[i * QK_K + j] = d * (i8of(u8at(src, o + 4 + j)) as f32);
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_tq1_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 54;
        let d = half(u16at(src, o + 52));
        var y = i * QK_K;
        // qs is 48 bytes: one 32-byte group of 5 base-3 planes, then one
        // 16-byte group (sizeof(qs) - sizeof(qs) % 32 == 32).
        var j: i64 = 0;
        while j < 32 {
            var nn: i64 = 0;
            while nn < 5 {
                var m: i64 = 0;
                while m < 32 {
                    let q = (u8at(src, o + j + m) * pow3(nn)) & 255;
                    let xi = (q * 3) >> 8;
                    out[y] = ((xi - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                nn = nn + 1;
            }
            j = j + 32;
        }
        j = 32;
        while j < 48 {
            var nn: i64 = 0;
            while nn < 5 {
                var m: i64 = 0;
                while m < 16 {
                    let q = (u8at(src, o + j + m) * pow3(nn)) & 255;
                    let xi = (q * 3) >> 8;
                    out[y] = ((xi - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                nn = nn + 1;
            }
            j = j + 16;
        }
        var nn: i64 = 0;
        while nn < 4 {
            var m: i64 = 0;
            while m < 4 {
                let q = (u8at(src, o + 48 + m) * pow3(nn)) & 255;
                let xi = (q * 3) >> 8;
                out[y] = ((xi - 1) as f32) * d;
                y = y + 1;
                m = m + 1;
            }
            nn = nn + 1;
        }
        i = i + 1;
    }
}

fn dq_tq2_0(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 66;
        let d = half(u16at(src, o + 64));
        var y = i * QK_K;
        var j: i64 = 0;
        while j < 64 {
            var l: i64 = 0;
            while l < 4 {
                var m: i64 = 0;
                while m < 32 {
                    let q = (u8at(src, o + j + m) >> (l * 2)) & 3;
                    out[y] = ((q - 1) as f32) * d;
                    y = y + 1;
                    m = m + 1;
                }
                l = l + 1;
            }
            j = j + 32;
        }
        i = i + 1;
    }
}

fn dq_mxfp4(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 32;
    var i: i64 = 0;
    while i < nb {
        let o = i * 17;
        let d = e8m0_half(u8at(src, o));
        var j: i64 = 0;
        while j < 16 {
            let x0 = kv_mxfp4(u8at(src, o + 1 + j) & 15);
            let x1 = kv_mxfp4(u8at(src, o + 1 + j) >> 4);
            out[i * 32 + j] = (x0 as f32) * d;
            out[i * 32 + j + 16] = (x1 as f32) * d;
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_nvfp4(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 64;
    var i: i64 = 0;
    while i < nb {
        let o = i * 36;
        var s: i64 = 0;
        while s < 4 {
            let d = ue4m3(u8at(src, o + s));
            var j: i64 = 0;
            while j < 8 {
                let qb = u8at(src, o + 4 + s * 8 + j);
                let x0 = kv_mxfp4(qb & 15);
                let x1 = kv_mxfp4(qb >> 4);
                out[i * 64 + s * 16 + j] = (x0 as f32) * d;
                out[i * 64 + s * 16 + j + 8] = (x1 as f32) * d;
                j = j + 1;
            }
            s = s + 1;
        }
        i = i + 1;
    }
}

fn dq_iq4_nl(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / 32;
    var i: i64 = 0;
    while i < nb {
        let o = i * 18;
        let d = half(u16at(src, o));
        var j: i64 = 0;
        while j < 16 {
            let q = u8at(src, o + 2 + j);
            out[i * 32 + j] = d * (kv_iq4nl(q & 15) as f32);
            out[i * 32 + j + 16] = d * (kv_iq4nl(q >> 4) as f32);
            j = j + 1;
        }
        i = i + 1;
    }
}

fn dq_iq4_xs(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 136;
        let d = half(u16at(src, o));
        let sh = u16at(src, o + 2);
        var ib: i64 = 0;
        while ib < 8 {
            let ls = ((u8at(src, o + 4 + ib / 2) >> (4 * (ib & 1))) & 15) | (((sh >> (2 * ib)) & 3) << 4);
            let dl = d * ((ls - 32) as f32);
            var j: i64 = 0;
            while j < 16 {
                let qb = u8at(src, o + 8 + ib * 16 + j);
                out[i * QK_K + ib * 32 + j] = dl * (kv_iq4nl(qb & 15) as f32);
                out[i * QK_K + ib * 32 + j + 16] = dl * (kv_iq4nl(qb >> 4) as f32);
                j = j + 1;
            }
            ib = ib + 1;
        }
        i = i + 1;
    }
}

fn dq_iq3_xxs(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 98;
        let d = half(u16at(src, o));
        var ib32: i64 = 0;
        while ib32 < 8 {
            // grid codes: 8 bytes per 32 elements; scales and signs: one word
            // per 32 elements, stored after QK_K/4 code bytes.
            let a1 = u32at(src, o + 2 + 64 + 4 * ib32);
            let db = d * ((0.5 + ((a1 >> 28) as f32)) * 0.5);
            var l: i64 = 0;
            while l < 4 {
                let g1 = iq3_grid(0, u8at(src, o + 2 + 8 * ib32 + 2 * l));
                let g2 = iq3_grid(0, u8at(src, o + 2 + 8 * ib32 + 2 * l + 1));
                let signs = ksign((a1 >> (7 * l)) & 127);
                var j: i64 = 0;
                while j < 4 {
                    let base = i * QK_K + ib32 * 32 + l * 8;
                    out[base + j] = (db * (grid_byte(g1, j) as f32)) * sgn((signs & (1 << j)) != 0);
                    out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((signs & (1 << (j + 4))) != 0);
                    j = j + 1;
                }
                l = l + 1;
            }
            ib32 = ib32 + 1;
        }
        i = i + 1;
    }
}

fn dq_iq3_s(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 110;
        let d = half(u16at(src, o));
        let qs = o + 2;
        let qh = o + 66;
        let sg = o + 74;
        let sc = o + 106;
        var step: i64 = 0;
        while step < 4 {
            let sbyte = u8at(src, sc + step);
            let db1 = d * ((1 + 2 * (sbyte & 15)) as f32);
            let db2 = d * ((1 + 2 * (sbyte >> 4)) as f32);
            var half_at: i64 = 0;
            while half_at < 2 {
                var db = db1;
                if half_at == 1 { db = db2; }
                let qb = qs + step * 16 + half_at * 8;
                let hb = qh + step * 2 + half_at;
                let sb = sg + step * 8 + half_at * 4;
                var l: i64 = 0;
                while l < 4 {
                    let g1 = iq3_grid(1, u8at(src, qb + 2 * l) | ((u8at(src, hb) << (8 - 2 * l)) & 256));
                    let g2 = iq3_grid(1, u8at(src, qb + 2 * l + 1) | ((u8at(src, hb) << (7 - 2 * l)) & 256));
                    let sbyte2 = u8at(src, sb + l);
                    var j: i64 = 0;
                    while j < 4 {
                        let base = i * QK_K + step * 64 + half_at * 32 + l * 8;
                        out[base + j] = (db * (grid_byte(g1, j) as f32)) * sgn((sbyte2 & (1 << j)) != 0);
                        out[base + j + 4] = (db * (grid_byte(g2, j) as f32)) * sgn((sbyte2 & (1 << (j + 4))) != 0);
                        j = j + 1;
                    }
                    l = l + 1;
                }
                half_at = half_at + 1;
            }
            step = step + 1;
        }
        i = i + 1;
    }
}

fn dq_iq2_s(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 82;
        let d = half(u16at(src, o));
        let qs = o + 2;
        let qh = o + 66;
        let sg = o + 2 + 32;
        let sc = o + 74;
        var ib32: i64 = 0;
        while ib32 < 8 {
            let sbyte = u8at(src, sc + ib32);
            let db0 = d * ((0.5 + ((sbyte & 15) as f32)) * 0.25);
            let db1 = d * ((0.5 + ((sbyte >> 4) as f32)) * 0.25);
            var l: i64 = 0;
            while l < 4 {
                var db = db0;
                if l >= 2 { db = db1; }
                let gv = iq2_grid(3, u8at(src, qs + ib32 * 4 + l) | ((u8at(src, qh + ib32) << (8 - 2 * l)) & 0x300));
                let sbyte2 = u8at(src, sg + ib32 * 4 + l);
                var j: i64 = 0;
                while j < 8 {
                    let g = grid_byte(gv, j) as f32;
                    out[i * QK_K + ib32 * 32 + l * 8 + j] = (db * g) * sgn((sbyte2 & (1 << j)) != 0);
                    j = j + 1;
                }
                l = l + 1;
            }
            ib32 = ib32 + 1;
        }
        i = i + 1;
    }
}

fn dq_iq1_m(src: const u8[:], n: i64, out: f32[:]) {
    let nb = n / QK_K;
    var i: i64 = 0;
    while i < nb {
        let o = i * 56;
        let qs = o;
        let qh = o + 32;
        let sc = o + 48;
        let s0 = u16at(src, sc);
        let s1 = u16at(src, sc + 2);
        let s2 = u16at(src, sc + 4);
        let s3 = u16at(src, sc + 6);
        let su = (s0 >> 12) | ((s1 >> 8) & 240) | ((s2 >> 4) & 3840) | (s3 & 61440);
        let d = half(su);
        var ib: i64 = 0;
        while ib < 8 {
            let sb = u16at(src, sc + (ib / 2) * 2);
            let dl1 = d * ((2 * ((sb >> (6 * (ib & 1) + 0)) & 7) + 1) as f32);
            let dl2 = d * ((2 * ((sb >> (6 * (ib & 1) + 3)) & 7) + 1) as f32);
            let qb = qs + ib * 4;
            let hb = qh + ib * 2;
            let h0 = u8at(src, hb);
            let h1 = u8at(src, hb + 1);
            let i0 = u8at(src, qb + 0) | ((h0 << 8) & 0x700);
            let i1 = u8at(src, qb + 1) | ((h0 << 4) & 0x700);
            let i2 = u8at(src, qb + 2) | ((h1 << 8) & 0x700);
            let i3 = u8at(src, qb + 3) | ((h1 << 4) & 0x700);
            var j: i64 = 0;
            while j < 32 {
                var gv = iq2_grid(2, i0);
                var dl = dl1;
                var dbit = (h0 & 8) != 0;
                if j >= 8 {
                    gv = iq2_grid(2, i1);
                    dbit = (h0 & 128) != 0;
                }
                if j >= 16 {
                    gv = iq2_grid(2, i2);
                    dl = dl2;
                    dbit = (h1 & 8) != 0;
                }
                if j >= 24 {
                    gv = iq2_grid(2, i3);
                    dl = dl2;
                    dbit = (h1 & 128) != 0;
                }
                var delta: f32 = 0.125;
                if dbit { delta = 0.0 - 0.125; }
                let gs = i8of(grid_byte(gv, j & 7));
                out[i * QK_K + ib * 32 + j] = dl * ((gs as f32) + delta);
                j = j + 1;
            }
            ib = ib + 1;
        }
        i = i + 1;
    }
}

// ---- dispatcher -----------------------------------------------------------

fn impl(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    let be = blk_elems(t);
    if be == 0 {
        return 0 - 1;
    }
    if n % be != 0 {
        return 0 - 1;
    }
    let bb = blk_bytes(t);
    if src.len < (n / be) * bb {
        return 0 - 1;
    }
    if out.len < n {
        return 0 - 1;
    }

    if t == T_Q1_0 { dq_q1_0(src, n, out); }
    if t == T_Q2_0 { dq_q2_0(src, n, out); }
    if t == T_Q2_K { dq_q2_K(src, n, out); }
    if t == T_Q3_K { dq_q3_K(src, n, out); }
    if t == T_Q4_K { dq_q4_K(src, n, out); }
    if t == T_Q5_K { dq_q5_K(src, n, out); }
    if t == T_Q6_K { dq_q6_K(src, n, out); }
    if t == T_Q8_K { dq_q8_K(src, n, out); }
    if t == T_TQ1_0 { dq_tq1_0(src, n, out); }
    if t == T_TQ2_0 { dq_tq2_0(src, n, out); }
    if t == T_MXFP4 { dq_mxfp4(src, n, out); }
    if t == T_NVFP4 { dq_nvfp4(src, n, out); }
    if t == T_IQ4_NL { dq_iq4_nl(src, n, out); }
    if t == T_IQ4_XS { dq_iq4_xs(src, n, out); }
    if t == T_IQ3_XXS { dq_iq3_xxs(src, n, out); }
    if t == T_IQ3_S { dq_iq3_s(src, n, out); }
    if t == T_IQ2_S { dq_iq2_s(src, n, out); }
    if t == T_IQ1_M { dq_iq1_m(src, n, out); }
    return n;
}

export fn qd_dequant(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    return impl(t, src, n, out);
}

export fn qd_dequant_block_size(t: i64) -> i64 {
    return blk_bytes(t);
}

export fn qd_dequant_elems(t: i64) -> i64 {
    return blk_elems(t);
}

// ---- different-type copy --------------------------------------------------

// quantized -> f32, the to_float leg of ggml_compute_forward_dup_from_q.
export fn qd_copy_to_f32(t: i64, src: const u8[:], n: i64, out: f32[:]) -> i64 {
    if t == T_F16 {
        if src.len < 2 * n || out.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            out[i] = half(u16at(src, 2 * i));
            i = i + 1;
        }
        return n;
    }
    if t == T_BF16 {
        if src.len < 2 * n || out.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            out[i] = of_bits(u16at(src, 2 * i) << 16);
            i = i + 1;
        }
        return n;
    }
    return impl(t, src, n, out);
}

// f32 -> f16 bits, the exact native bit-trick rounding (round to nearest even,
// overflow to infinity, NaN to 0x7E00).
fn f32_to_f16(f: f32) -> i64 {
    var af = f;
    if af < 0.0 {
        af = 0.0 - af;
    }
    var si: f32 = 1.0;
    var k = 0;
    while k < 112 {
        si = si * 2.0;
        k = k + 1;
    }
    var sz: f32 = 1.0;
    k = 0;
    while k < 110 {
        sz = sz * 0.5;
        k = k + 1;
    }
    var base = (af * si) * sz;

    let w = bits(f) & 0xffffffff;
    let shl1_w = (w + w) & 0xffffffff;
    let sign = w & 0x80000000;
    var bias = shl1_w & 0xff000000;
    if bias < 0x71000000 {
        bias = 0x71000000;
    }

    base = of_bits((bias >> 1) + 0x07800000) + base;
    let bb = bits(base) & 0xffffffff;
    let exp_bits = (bb >> 13) & 0x7c00;
    let mant_bits = bb & 0x0fff;
    let nonsign = exp_bits + mant_bits;
    var outv = nonsign;
    if shl1_w > 0xff000000 {
        outv = 0x7e00;
    }
    return ((sign >> 16) | outv) & 0xffff;
}

// f32 -> bf16 bits, round to nearest even (ggml_compute_fp32_to_bf16).
fn f32_to_bf16(f: f32) -> i64 {
    let u = bits(f) & 0xffffffff;
    if (u & 0x7fffffff) > 0x7f800000 {
        return ((u >> 16) | 64) & 0xffff;
    }
    let r = (u + (0x7fff + ((u >> 16) & 1))) & 0xffffffff;
    return (r >> 16) & 0xffff;
}

fn store_u16(dst: u8[:], at: i64, v: i64) {
    dst[at] = (v & 255) as u8;
    dst[at + 1] = ((v >> 8) & 255) as u8;
}

// round to nearest even, as _mm256_round_ps(x, _MM_ROUND_NEAREST).
fn rne(x: f32) -> i64 {
    var a = x;
    var neg = 0;
    if a < 0.0 {
        a = 0.0 - a;
        neg = 1;
    }
    let t = a as i64;
    let frac = a - (t as f32);
    var r = t;
    if frac > 0.5 {
        r = t + 1;
    } else if frac == 0.5 {
        if (t & 1) != 0 {
            r = t + 1;
        }
    }
    if neg != 0 {
        return 0 - r;
    }
    return r;
}

// f32 -> quantized, the from_float leg of ggml_compute_forward_dup_to_q.
// Only f16, bf16 and q8_0 are in scope; q8_0 follows the AVX2 from_float:
// d = amax/127, id = 127/amax, round to nearest even.
export fn qd_copy_from_f32(t: i64, src: const f32[:], n: i64, dst: u8[:]) -> i64 {
    if t == T_F16 {
        if dst.len < 2 * n || src.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            store_u16(dst, 2 * i, f32_to_f16(src[i]));
            i = i + 1;
        }
        return n;
    }
    if t == T_BF16 {
        if dst.len < 2 * n || src.len < n { return 0 - 1; }
        var i: i64 = 0;
        while i < n {
            store_u16(dst, 2 * i, f32_to_bf16(src[i]));
            i = i + 1;
        }
        return n;
    }
    if t == T_Q8_0 {
        if n % 32 != 0 || src.len < n || dst.len < (n / 32) * 34 { return 0 - 1; }
        let nb = n / 32;
        var b: i64 = 0;
        while b < nb {
            var amax: f32 = 0.0;
            var j: i64 = 0;
            while j < 32 {
                var v = src[b * 32 + j];
                if v < 0.0 {
                    v = 0.0 - v;
                }
                if v > amax {
                    amax = v;
                }
                j = j + 1;
            }
            let d = amax / 127.0;
            var id: f32 = 0.0;
            if amax != 0.0 {
                id = 127.0 / amax;
            }
            store_u16(dst, b * 34, f32_to_f16(d));
            j = 0;
            while j < 32 {
                let q = rne(src[b * 32 + j] * id);
                dst[b * 34 + 2 + j] = (q & 255) as u8;
                j = j + 1;
            }
            b = b + 1;
        }
        return n;
    }
    return 0 - 1;
}
