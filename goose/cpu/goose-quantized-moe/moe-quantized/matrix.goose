import.packed_simd;
struct Task {
    mode: i64, kind: i64, blocks: i64, rows: i64, weight_stride: i64, first: i64, cols: i64, slots: i64, dst_stride1: i64, dst_stride2: i64, route_first: i64, input_contiguous: i64, input_row_blocks: i64, input_stride1: i64, input_stride2: i64, output_stride: i64
}
struct Route {
    slot: i32, token: i32
}
struct Q2 {
    d: u16, q: u8[16]
}
struct Q5 {
    d: u16, high: u8[4], q: u8[16]
}
struct NL4 {
    d: u16, q: u8[16]
}
struct IQ2X {
    d: u16, q: u8[64]
}
struct IQ2S {
    d: u16, q: u8[64], high: u8[8], scales: u8[8]
}
struct Y0 {
    d: u16, q: i8[32]
}
struct YK {
    d: f32, q: i8[32][8], sums: i16[16]
}
struct Decoded {
    q: u8[32], mask: u8[32], scales: i16[16]
}
extern "fg139_half_float" fn half(x: u16) -> f32;
extern "fmaf" fn fma(a: f32, b: f32, c: f32) -> f32;
extern "fg139_iq4_tail_fused" fn iq4_tail_fused() -> bool;
extern "fg139_math_witness" fn witness(mode: i64, kind: i64, blocks: i64, rows: i64, cols: i64);

fn addf(a: f32[8], b: f32[8]) -> f32[8] {
    var out: f32[8] = default < f32[8] > ();
    for i in 8 {
        out[i] = a[i] + b[i];
    }
    return out;
}

fn hsum(a: f32[8]) -> f32 {
    return ( (a[4] + a[0]) + (a[6] + a[2])) + ( (a[5] + a[1]) + (a[7] + a[3]));
}

fn sub16(a: i16[16], b: i16[16]) -> i16[16] {
    var out: i16[16] = default < i16[16] > ();
    for i in 16 {
        out[i] = a[i] - b[i];
    }
    return out;
}

fn signed_bytes(a: u8[32]) -> i8[32] {
    var out: i8[32] = default < i8[32] > ();
    for i in 32 {
        out[i] = a[i] as !i8;
    }
    return out;
}

fn flip(y: i8[32], mask: u8[32]) -> i8[32] {
    var out: i8[32] = default < i8[32] > ();
    for i in 32 {
        out[i] = ( ( (y[i] as !u8) ^ mask[i]) - mask[i]) as !i8;
    }
    return out;
}

fn signed_product(w: i8[32], y: i8[32]) -> i32[8] {
    var positive: u8[32] = default < u8[32] > ();
    var sy: i8[32] = default < i8[32] > ();
    for i in 32 {
        positive[i] = (if w[i] < 0 {
            -(w[i] as i16)
        } else {
            w[i] as i16
        }
        ) as !u8;
        sy[i] = (if w[i] < 0 {
            -(y[i] as i16)
        } else

        if w[i] == 0 {
            0
        } else {
            y[i] as i16
        }
        ) as !i8;
    }
    return packed_simd :: madd16(packed_simd :: splat16(1), packed_simd :: maddubs(positive, sy));
}

fn pair_scale(low: i16, high: i16) -> i16[16] {
    var out: i16[16] = default < i16[16] > ();
    for i in 8 {
        out[i] = low;
        out[i + 8] = high;
    }
    return out;
}

fn word32(bytes: const u8[64] &, first: i64) -> u32 {
    return (bytes[first] as u32) | ( (bytes[first + 1] as u32) << 8) | ( (bytes[first + 2] as u32) << 16) | ( (bytes[first + 3] as u32) << 24);
}

fn q2_codes(x: const Q2 &, part: i64) -> u8[32] {
    var out: u8[32] = default < u8[32] > ();
    for i in 32 {
        out[i] = (x.q[part * 8 + i / 4] >> ( ( (i % 4) * 2) as u8)) & 3;
    }
    return out;
}

fn q2_acc(x: const Q2 &, y: const Y0[:], first: i64, acc: f32[8]) -> f32[8] {
    var ones: u8[32] = default < u8[32] > ();
    for i in 32 {
        ones[i] = 1;
    }
    let q0 = sub16(packed_simd :: maddubs(q2_codes(x, 0), y[first].q), packed_simd :: maddubs(ones, y[first].q));
    let q1 = sub16(
        packed_simd :: maddubs(q2_codes(x, 1), y[first + 1].q),
        packed_simd :: maddubs(ones, y[first + 1].q));
    let p0 = packed_simd :: madd16(packed_simd :: splat16(1), q0);
    let p1 = packed_simd :: madd16(packed_simd :: splat16(1), q1);
    let d = half(x.d);
    let out = packed_simd :: fma32(d * half(y[first].d), p0, acc);
    return packed_simd :: fma32(d * half(y[first + 1].d), p1, out);
}

fn dot_q2(x: const Q2[:], y: const Y0[:]) -> f32 {
    assert(y.len >= 2 * x.len);
    var a0: f32[8] = default < f32[8] > ();
    var a1: f32[8] = default < f32[8] > ();
    var i: i64 = 0;

    while i + 1 < x.len {
        a0 = q2_acc(x[i], y, 2 * i, a0);
        a1 = q2_acc(x[i + 1], y, 2 * i + 2, a1);
        i += 2;
    }

    if i < x.len {
        a0 = q2_acc(x[i], y, 2 * i, a0);
    }
    return hsum(addf(a0, a1));
}

fn q5_values(x: const Q5 &) -> i8[32] {
    var out: i8[32] = default < i8[32] > ();
    for i in 32 {
        let low = (x.q[i % 16] >> ( ( (i / 16) * 4) as u8)) & 15;
        let high = (x.high[i / 8] >> ( (i % 8) as u8)) & 1;
        out[i] = ( (low as i16) + (high as i16) * 16 - 16) as !i8;
    }
    return out;
}

fn dot_q5(x: const Q5[:], y: const Y0[:]) -> f32 {
    assert(y.len >= x.len);
    var acc: f32[8] = default < f32[8] > ();
    for i in x.len {
        let d = half(x[i].d) * half(y[i].d);
        acc = packed_simd :: fma32(d, signed_product(q5_values(x[i]), y[i].q), acc);
    }
    return hsum(acc);
}

fn nl_values(x: const NL4 &, palette: const i8[:]) -> i8[32] {
    var lookup: u8[32] = default < u8[32] > ();
    var indices: u8[32] = default < u8[32] > ();
    for i in 32 {
        lookup[i] = palette[i % 16] as !u8;
    }
    for i in 16 {
        indices[i] = x.q[i] & 15;
        indices[i + 16] = x.q[i] >> 4;
    }
    return signed_bytes(packed_simd :: shuffle8(lookup, indices));
}

fn dot_nl(x: const NL4[:], y: const Y0[:], palette: const i8[:]) -> f32 {
    assert(y.len >= x.len);
    var a0: f32[8] = default < f32[8] > ();
    var a1: f32[8] = default < f32[8] > ();
    var i: i64 = 0;

    while i + 1 < x.len {
        a0 = packed_simd :: fma32(half(y[i].d) * half(x[i].d), signed_product(nl_values(x[i], palette), y[i].q), a0);
        a1 = packed_simd :: fma32(
            half(y[i + 1].d) * half(x[i + 1].d),
            signed_product(nl_values(x[i + 1], palette), y[i + 1].q),
            a1);
        i += 2;
    }
    var result = hsum(addf(a0, a1));

    if i < x.len {
        var s0: i32 = 0;
        var s1: i32 = 0;
        for j in 16 {
            s0 += (y[i].q[j] as i32) * (palette[ (x[i].q[j] & 15) as i64] as i32);
            s1 += (y[i].q[j + 16] as i32) * (palette[ (x[i].q[j] >> 4) as i64] as i32);
        }
        let scale = half(y[i].d) * half(x[i].d);

        if iq4_tail_fused() {
            result = fma(scale, (s0 + s1) as f32, result);
        } else {
            let product = scale * ( (s0 + s1) as f32);
            result = result + product;
        }
    }
    return result;
}

fn decode_iq(
    kind: i64,
    xx: const IQ2X[:],
    xs: const IQ2S[:],
    weight: i64,
    sub: i64,
    gx: const u8[8][:],
    gs: const u8[8][:],
    signs: const u8[:],
    out: Decoded &) {
    if kind == 3 {
        let x = & xx[weight];
        let info = word32(x.q, sub * 8 + 4);
        out.scales = packed_simd :: splat16( (2 * (info >> 28) + 1) as i16);
        for group in 4 {
            let grid = gx[x.q[sub * 8 + group] as i64];
            let mask = signs[ ( (info >> ( (7 * group) as u32)) & 127) as i64];
            for lane in 8 {
                out.q[group * 8 + lane] = grid[lane];
                out.mask[group * 8 + lane] = (if ( (mask >> (lane as u8)) & 1) != 0 {
                    255
                } else {
                    0
                }
                ) as !u8;
            }
        }
    } else {
        assert(kind == 4);
        let x = & xs[weight];
        out.scales = pair_scale( ( (x.scales[sub] & 15) as i16) * 2 + 1, ( (x.scales[sub] >> 4) as i16) * 2 + 1);
        for group in 4 {
            let index = (x.q[sub * 4 + group] as u16) | ( ( (x.high[sub] as u16) << ( (8 - 2 * group) as u16)) & 768);
            let grid = gs[index as i64];
            let mask = x.q[32 + sub * 4 + group];
            for lane in 8 {
                out.q[group * 8 + lane] = grid[lane];
                out.mask[group * 8 + lane] = (if ( (mask >> (lane as u8)) & 1) != 0 {
                    255
                } else {
                    0
                }
                ) as !u8;
            }
        }
    }
}

fn iq_product(weight: const Decoded &, activation: i8[32]) -> i32[8] {
    return packed_simd :: madd16(weight.scales, packed_simd :: maddubs(weight.q, flip(activation, weight.mask)));
}

fn dot_iq_rows(
    kind: i64,
    xx: const IQ2X[:],
    xs: const IQ2S[:],
    first: i64,
    y: const YK[:],
    blocks: i64,
    columns: i64,
    o0: i64,
    o1: i64,
    o2: i64,
    gx: const u8[8][:],
    gs: const u8[8][:],
    signs: const u8[:]) -> f32[3] {
    assert(columns >= 1 && columns <= 3 && blocks >= 0 && y.len >= o0 + blocks);

    if columns >= 2 {
        assert(y.len >= o1 + blocks);
    }

    if columns >= 3 {
        assert(y.len >= o2 + blocks);
    }
    var a0: f32[8] = default < f32[8] > ();
    var a1: f32[8] = default < f32[8] > ();
    var a2: f32[8] = default < f32[8] > ();
    for ib in blocks {
        var s00: i32[8] = default < i32[8] > ();
        var s01: i32[8] = default < i32[8] > ();
        var s10: i32[8] = default < i32[8] > ();
        var s11: i32[8] = default < i32[8] > ();
        var s20: i32[8] = default < i32[8] > ();
        var s21: i32[8] = default < i32[8] > ();
        var w0: Decoded = default < Decoded > ();
        var w1: Decoded = default < Decoded > ();
        for pair in 4 {
            decode_iq(kind, xx, xs, first + ib, pair * 2, gx, gs, signs, w0);
            decode_iq(kind, xx, xs, first + ib, pair * 2 + 1, gx, gs, signs, w1);
            s00 = packed_simd :: add32(s00, iq_product(w0, y[o0 + ib].q[pair * 2]));
            s01 = packed_simd :: add32(s01, iq_product(w1, y[o0 + ib].q[pair * 2 + 1]));

            if columns >= 2 {
                s10 = packed_simd :: add32(s10, iq_product(w0, y[o1 + ib].q[pair * 2]));
                s11 = packed_simd :: add32(s11, iq_product(w1, y[o1 + ib].q[pair * 2 + 1]));
            }

            if columns >= 3 {
                s20 = packed_simd :: add32(s20, iq_product(w0, y[o2 + ib].q[pair * 2]));
                s21 = packed_simd :: add32(s21, iq_product(w1, y[o2 + ib].q[pair * 2 + 1]));
            }
        }
        let d = if kind == 3 {
            half(xx[first + ib].d)
        } else {
            half(xs[first + ib].d)
        }
        ;
        a0 = packed_simd :: fma32(d * y[o0 + ib].d, packed_simd :: add32(s00, s01), a0);

        if columns >= 2 {
            a1 = packed_simd :: fma32(d * y[o1 + ib].d, packed_simd :: add32(s10, s11), a1);
        }

        if columns >= 3 {
            a2 = packed_simd :: fma32(d * y[o2 + ib].d, packed_simd :: add32(s20, s21), a2);
        }
    }
    var out: f32[3] = default < f32[3] > ();
    out[0] = (0.125 as f32) * hsum(a0);

    if columns >= 2 {
        out[1] = (0.125 as f32) * hsum(a1);
    }

    if columns >= 3 {
        out[2] = (0.125 as f32) * hsum(a2);
    }
    return out;
}

fn dequantize(
    kind: i64,
    blocks: i64,
    x2: const Q2[:],
    x5: const Q5[:],
    xn: const NL4[:],
    xx: const IQ2X[:],
    xs: const IQ2S[:],
    dst: f32[:],
    gx: const u8[8][:],
    gs: const u8[8][:],
    signs: const u8[:],
    palette: const i8[:]) {
    for ib in blocks {
        if kind == 0 {
            let x = & x2[ib];
            let d = half(x.d);
            for lane in 64 {
                let q = ( (x.q[lane / 4] >> ( ( (lane % 4) * 2) as u8)) & 3) as i32;
                dst[ib * 64 + lane] = d * ( (q - 1) as f32);
            }
        } else

        if kind == 1 {
            let x = & x5[ib];
            let d = half(x.d);
            let q = q5_values(x);
            for lane in 32 {
                dst[ib * 32 + lane] = d * (q[lane] as f32);
            }
        } else

        if kind == 2 {
            let x = & xn[ib];
            let d = half(x.d);
            let q = nl_values(x, palette);
            for lane in 32 {
                dst[ib * 32 + lane] = d * (q[lane] as f32);
            }
        } else {
            let d = if kind == 3 {
                half(xx[ib].d)
            } else {
                half(xs[ib].d)
            }
            ;
            var decoded: Decoded = default < Decoded > ();
            for sub in 8 {
                decode_iq(kind, xx, xs, ib, sub, gx, gs, signs, decoded);
                for lane in 32 {
                    let scale = (0.125 as f32) * d * (decoded.scales[lane / 2] as f32);
                    let value = scale * (decoded.q[lane] as f32);
                    dst[ib * 256 + sub * 32 + lane] = if decoded.mask[lane] != 0 {
                        -value
                    } else {
                        value
                    }
                    ;
                }
            }
        }
    }
}

fn row_value(
    p: const Task &,
    first: i64,
    offset: i64,
    x2: const Q2[:],
    x5: const Q5[:],
    xn: const NL4[:],
    xx: const IQ2X[:],
    xs: const IQ2S[:],
    y0: const Y0[:],
    yk: const YK[:],
    gx: const u8[8][:],
    gs: const u8[8][:],
    signs: const u8[:],
    palette: const i8[:]) -> f32 {
    if p.kind == 0 {
        return dot_q2(x2[first .. first + p.blocks], y0[offset .. offset + 2 * p.blocks]);
    }

    if p.kind == 1 {
        return dot_q5(x5[first .. first + p.blocks], y0[offset .. offset + p.blocks]);
    }

    if p.kind == 2 {
        return dot_nl(xn[first .. first + p.blocks], y0[offset .. offset + p.blocks], palette);
    }
    let out = dot_iq_rows(p.kind, xx, xs, first, yk, p.blocks, 1, offset, 0, 0, gx, gs, signs);
    return out[0];
}

fn input_offset(p: const Task &, r: const Route &) -> i64 {
    let slot = (r.slot as i64) % p.slots;
    let token = r.token as i64;

    if p.input_contiguous != 0 {
        return (slot + token * p.slots) * p.input_row_blocks;
    }
    return slot * p.input_stride1 + token * p.input_stride2;
}

fn execute_matrix(
    p: const Task &,
    x2: const Q2[:],
    x5: const Q5[:],
    xn: const NL4[:],
    xx: const IQ2X[:],
    xs: const IQ2S[:],
    y0: const Y0[:],
    yk: const YK[:],
    dst: f32[:],
    gx: const u8[8][:],
    gs: const u8[8][:],
    signs: const u8[:],
    palette: const i8[:],
    routes: const Route[:]) {
    assert(p.kind >= 0 && p.kind <= 4 && p.blocks >= 0);

    if p.mode == 3 && (p.rows <= 0 || p.cols <= 0) {
        witness(3, p.kind, p.blocks, 0, 0);
        return;
    }
    assert(p.rows >= 0);

    if p.mode == 2 {
        dequantize(p.kind, p.blocks, x2, x5, xn, xx, xs, dst, gx, gs, signs, palette);
    } else

    if p.mode == 1 {
        assert(p.kind == 4 && p.cols >= 2 && p.cols <= 3 && p.slots > 0 && routes.len >= p.route_first + p.cols);
        let r0 = routes[p.route_first];
        let r1 = routes[p.route_first + 1];
        let o0 = input_offset(p, r0);
        let o1 = input_offset(p, r1);
        var o2: i64 = 0;

        if p.cols == 3 {
            o2 = input_offset(p, routes[p.route_first + 2]);
        }
        for row in p.rows {
            let values = dot_iq_rows(
                4,
                xx,
                xs,
                (p.first + row) * p.weight_stride,
                yk,
                p.blocks,
                p.cols,
                o0,
                o1,
                o2,
                gx,
                gs,
                signs);
            for col in p.cols {
                let r = routes[p.route_first + col];
                let offset = (r.slot as i64) * p.dst_stride1 + (r.token as i64) * p.dst_stride2 + p.first + row;
                dst[offset] = values[col];
            }
        }
    } else

    if p.mode == 3 {
        assert(p.cols >= 0 && p.slots > 0 && routes.len >= p.route_first + p.cols);
        for col in p.cols {
            let route = routes[p.route_first + col];
            let offset = input_offset(p, route);
            let output = (route.slot as i64) * p.dst_stride1 + (route.token as i64) * p.dst_stride2 + p.first;
            for row in p.rows {
                dst[output + row] = row_value(p, (p.first + row) * p.weight_stride, offset, x2, x5, xn, xx, xs, y0, yk, gx, gs, signs, palette);
            }
        }
    } else {
        assert(p.mode == 0);
        for row in p.rows {
            dst[row * p.output_stride] = row_value(p, (p.first + row) * p.weight_stride, 0, x2, x5, xn, xx, xs, y0, yk, gx, gs, signs, palette);
        }
    }
    witness(p.mode, p.kind, p.blocks, p.rows, p.cols);
}

fn main() {
    var p: Task = default < Task > ();
    p.blocks = 1;
    p.rows = 1;
    p.weight_stride = 1;
    p.cols = 1;
    p.slots = 1;
    p.output_stride = 1;
    let x2: Q2[1] = default < Q2[1] > ();
    let x5: Q5[1] = default < Q5[1] > ();
    let xn: NL4[1] = default < NL4[1] > ();
    let xx: IQ2X[1] = default < IQ2X[1] > ();
    let xs: IQ2S[1] = default < IQ2S[1] > ();
    let y0: Y0[2] = default < Y0[2] > ();
    let yk: YK[1] = default < YK[1] > ();
    var out: f32[256] = default < f32[256] > ();
    let grid: u8[8][1] = default < u8[8][1] > ();
    let signs: u8[128] = default < u8[128] > ();
    let palette: i8[16] = default < i8[16] > ();
    let routes: Route[3] = default < Route[3] > ();
    execute_matrix(p, x2, x5, xn, xx, xs, y0, yk, out, grid, grid, signs, palette, routes);
    print(out[0]);
}

