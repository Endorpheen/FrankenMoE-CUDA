extern "fg143_codec_from_bits" fn from_bits(x: u32) -> f32;
extern "ldexpf" fn ldexp32(x: f32, p: i32) -> f32;

fn super_kind(t: i64) -> i64 {
    if t == 12 {
        return 0;
    }

    if t == 13 {
        return 1;
    }

    if t == 14 {
        return 2;
    }

    if t == 11 {
        return 3;
    }

    if t == 10 {
        return 4;
    }

    if t == 23 {
        return 5;
    }

    if t == 16 {
        return 6;
    }

    if t == 17 {
        return 7;
    }

    if t == 22 {
        return 8;
    }

    if t == 18 {
        return 9;
    }

    if t == 21 {
        return 10;
    }

    if t == 19 {
        return 11;
    }

    if t == 29 {
        return 12;
    }
    return -1;
}

fn compact_width(t: i64) -> i64 {
    if t == 41 {
        return 128;
    }

    if t == 42 || t == 40 {
        return 64;
    }

    if t == 15 {
        return 256;
    }
    return 32;
}

fn compact_bytes(t: i64) -> i64 {
    if t == 2 || t == 20 || t == 41 || t == 42 {
        return 18;
    }

    if t == 3 {
        return 20;
    }

    if t == 6 {
        return 22;
    }

    if t == 7 {
        return 24;
    }

    if t == 8 {
        return 34;
    }

    if t == 9 || t == 40 {
        return 36;
    }

    if t == 39 {
        return 17;
    }

    if t == 15 {
        return 292;
    }
    return 0;
}

fn fp4(index: u8) -> f32 {
    let v = index & 7;
    let magnitude = if v <= 4 {
        v as i32
    } else

    if v == 5 {
        6
    } else

    if v == 6 {
        8
    } else {
        12
    }
    ;
    return (if index < 8 {
        magnitude
    } else {
        -magnitude
    }
    ) as f32;
}

fn ue4m3(x: u8) -> f32 {
    if x == 0 || x == 127 {
        return (0 as f32);
    }
    let exp = ( (x >> 3) & 15) as i32;
    let mantissa = (x & 7) as f32;

    if exp == 0 {
        return ldexp32(mantissa, -9) * (0.5 as f32);
    }
    return ldexp32( (1 as f32) + mantissa / (8 as f32), exp - 7) * (0.5 as f32);
}

fn compact_value(t: i64, x: const u8[:], base: i64, lane: i64, palette: const i8[:]) -> f32 {
    if t == 15 {
        return from_bits(read32(x, base)) * ( (x[base + 4 + lane] as !i8) as f32);
    }

    if t == 39 {
        let exponent = x[base];
        let d = from_bits(if exponent < 2 {
            (0x00200000 as u32) << (exponent as u32)
        } else {
            ( (exponent - 1) as u32) << 23
        }
        );
        let bits = x[base + 1 + lane % 16];
        let value = if lane < 16 {
            bits & 15
        } else {
            bits >> 4
        }
        ;
        return fp4(value) * d;
    }

    if t == 40 {
        let sub = lane / 16;
        let i = lane % 16;
        let bits = x[base + 4 + sub * 8 + i % 8];
        let value = if i < 8 {
            bits & 15
        } else {
            bits >> 4
        }
        ;
        return fp4(value) * ue4m3(x[base + sub]);
    }
    let d = half(read16(x, base));

    if t == 41 {
        let bit = (x[base + 2 + lane / 8] >> ( (lane % 8) as u8)) & 1;
        return if bit != 0 {
            d
        } else {
            -d
        }
        ;
    }

    if t == 42 {
        let q = (x[base + 2 + lane / 4] >> ( (2 * (lane % 4)) as u8)) & 3;
        return ( (q as i32) - 1) as f32 * d;
    }

    if t == 8 {
        return ( (x[base + 2 + lane] as !i8) as f32) * d;
    }

    if t == 9 {
        return ( (x[base + 4 + lane] as !i8) as f32) * d;
    }

    if t == 2 || t == 3 || t == 20 {
        let qbase = base + (if t == 3 {
            4
        } else {
            2
        }
        );
        let bits = x[qbase + lane % 16];
        let q = if lane < 16 {
            bits & 15
        } else {
            bits >> 4
        }
        ;

        if t == 20 {
            return (palette[q as i64] as f32) * d;
        }

        if t == 2 {
            return ( ( (q as i32) - 8) as f32) * d;
        }
        return (q as f32) * d + half(read16(x, base + 2));
    }
    assert(t == 6 || t == 7);
    let high = read32(x, base + (if t == 7 {
        4
    } else {
        2
    }
    ));
    let bits = x[base + (if t == 7 {
        8
    } else {
        6
    }
    ) + lane % 16];
    let q = ( (if lane < 16 {
        bits & 15
    } else {
        bits >> 4
    }
    ) as u32) | ( ( (high >> (lane as u32)) & 1) << 4);

    if t == 6 {
        return ( ( (q as i32) - 16) as f32) * d;
    }
    return (q as f32) * d + half(read16(x, base + 2));
}

fn row_entry(
    weight_type: i64,
    raw: const u8[:],
    dst: f32[:],
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) -> bool {
    let kind = super_kind(weight_type);

    if kind < 0 {
        let width = compact_width(weight_type);
        let bytes = compact_bytes(weight_type);

        if bytes == 0 {
            return false;
        }
        assert(dst.len % width == 0 && raw.len >= (dst.len / width) * bytes);
        for ib in dst.len / width {
            for lane in width {
                dst[ib * width + lane] = compact_value(weight_type, raw, ib * bytes, lane, palette);
            }
        }
        return true;
    }
    let f = tile_traits(kind);
    assert(dst.len % 256 == 0 && raw.len >= (dst.len / 256) * f.bytes);
    var decoded: WeightTile = default < WeightTile > ();
    for ib in dst.len / 256 {
        unpack_block(kind, raw, ib * f.bytes, 0, 0, decoded, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
        for lane in 256 {
            let sub = lane / f.sub;
            let scale = decoded.scales[sub / 8][sub % 8] as f32;
            let minimum = decoded.mins[sub / 8][sub % 8] as f32;
            let q = (decoded.q[0][lane / 32][lane % 32] as i32) - f.bias as i32;
            dst[ib * 256 + lane] = (decoded.d[0] * scale) * (q as f32) - decoded.dmin[0] * minimum;
        }
    }
    return true;
}

fn main() {
    let x: u8[18] = default < u8[18] > ();
    var out: f32[32] = default < f32[32] > ();
    let g8: u8[8][1] = default < u8[8][1] > ();
    let g4: u8[4][1] = default < u8[4][1] > ();
    let g1: i8[8][1] = default < i8[8][1] > ();
    let signs: u8[128] = default < u8[128] > ();
    let palette: i8[16] = default < i8[16] > ();
    let ok = row_entry(2, x, out, g8, g8, g8, g4, g4, g1, signs, palette);
    print(ok);
}

