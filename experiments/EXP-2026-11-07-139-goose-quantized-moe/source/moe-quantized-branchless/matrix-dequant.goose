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
                    let scale = 0.125 * d * (decoded.scales[lane / 2] as f32);
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

