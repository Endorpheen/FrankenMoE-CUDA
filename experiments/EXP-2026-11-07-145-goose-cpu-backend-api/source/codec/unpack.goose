fn unpack_block(
    kind: i64,
    x: const u8[:],
    base: i64,
    panel_index: i64,
    di: i64,
    a: WeightTile &,
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) {
    let f = tile_traits(kind);
    let nb = 256 / f.sub;
    let si = panel_index * nb;
    a.dmin[di] = (0 as f32);
    for j in nb {
        bitmin(a, si + j, 0);
    }

    if kind == 0 || kind == 1 {
        a.d[di] = half(read16(x, base));
        a.dmin[di] = half(read16(x, base + 2));
        for j in 4 {
            let s0 = x[base + 4 + j];
            let m0 = x[base + 8 + j];
            let both = x[base + 12 + j];
            bitscale(a, si + j, (s0 & 63) as i32);
            bitmin(a, si + j, (m0 & 63) as i32);
            bitscale(a, si + j + 4, ( (both & 15) | ( (s0 >> 6) << 4)) as i32);
            bitmin(a, si + j + 4, ( (both >> 4) | ( (m0 >> 6) << 4)) as i32);
        }
        let qbase = base + (if kind == 0 {
            16
        } else {
            48
        }
        );
        for j in 4 {
            let packed = load32(x, qbase + j * 32);
            var lo = packed_simd :: and8(packed, 15);
            var hi = packed_simd :: shr8(packed, 4);

            if kind == 1 {
                let high = load32(x, base + 16);
                for lane in 32 {
                    lo[lane] = lo[lane] | ( ( (high[lane] >> ( (2 * j) as u8)) & 1) << 4);
                    hi[lane] = hi[lane] | ( ( (high[lane] >> ( (2 * j + 1) as u8)) & 1) << 4);
                }
            }
            a.q[panel_index][2 * j] = lo;
            a.q[panel_index][2 * j + 1] = hi;
        }
    } else

    if kind == 2 {
        a.d[di] = half(read16(x, base + 208));
        for s in 16 {
            bitscale(a, si + s, (x[base + 192 + s] as !i8) as i32);
        }
        for h in 2 {
            for lane in 32 {
                let high = x[base + 128 + h * 32 + lane];
                let lo0 = x[base + h * 64 + lane];
                let lo1 = x[base + h * 64 + 32 + lane];
                a.q[panel_index][h * 4][lane] = (lo0 & 15) | ( (high & 3) << 4);
                a.q[panel_index][h * 4 + 1][lane] = (lo1 & 15) | ( (high & 12) << 2);
                a.q[panel_index][h * 4 + 2][lane] = (lo0 >> 4) | (high & 48);
                a.q[panel_index][h * 4 + 3][lane] = (lo1 >> 4) | ( (high & 192) >> 2);
            }
        }
    } else

    if kind == 3 {
        a.d[di] = half(read16(x, base + 108));
        for j in 4 {
            let lo = x[base + 96 + j];
            let hi = x[base + 100 + j];
            let packed = x[base + 104 + j];
            bitscale(a, si + j, ( ( (lo & 15) | ( (packed & 3) << 4)) as i32) - 32);
            bitscale(a, si + 4 + j, ( ( (hi & 15) | ( ( (packed >> 2) & 3) << 4)) as i32) - 32);
            bitscale(a, si + 8 + j, ( ( (lo >> 4) | ( ( (packed >> 4) & 3) << 4)) as i32) - 32);
            bitscale(a, si + 12 + j, ( ( (hi >> 4) | ( ( (packed >> 6) & 3) << 4)) as i32) - 32);
        }
        for h in 2 {
            for group in 4 {
                let raw = load32(x, base + 32 + h * 32);
                let high = load32(x, base);
                var out: u8[32] = default < u8[32] > ();
                for lane in 32 {
                    out[lane] = ( (raw[lane] >> ( (2 * group) as u8)) & 3) | ( ( (high[lane] >> ( (h * 4 + group) as u8)) & 1) << 2);
                }
                a.q[panel_index][h * 4 + group] = out;
            }
        }
    } else

    if kind == 4 {
        a.d[di] = half(read16(x, base + 80));
        a.dmin[di] = half(read16(x, base + 82));
        for s in 16 {
            bitscale(a, si + s, (x[base + s] & 15) as i32);
            bitmin(a, si + s, (x[base + s] >> 4) as i32);
        }
        for h in 2 {
            let raw = load32(x, base + 16 + h * 32);
            for group in 4 {
                a.q[panel_index][h * 4 + group] = packed_simd :: and8(packed_simd :: shr8(raw, (2 * group) as u8), 3);
            }
        }
    } else

    if kind == 5 {
        a.d[di] = half(read16(x, base));
        let high = read16(x, base + 2);
        for s in 8 {
            let packed = ( (x[base + 4 + s / 2] >> ( (4 * (s % 2)) as u8)) & 15) as u16;
            bitscale(a, si + s, ( (packed | ( ( (high >> ( (2 * s) as u16)) & 3) << 4)) as i32) - 32);
        }
        var lookup: u8[32] = default < u8[32] > ();
        for lane in 16 {
            let value = ( (palette[lane] as i16) + 128) as !u8;
            lookup[lane] = value;
            lookup[lane + 16] = value;
        }
        for group in 4 {
            let raw = load32(x, base + 8 + group * 32);
            let lo = packed_simd :: shuffle8(lookup, packed_simd :: and8(raw, 15));
            let hi = packed_simd :: shuffle8(lookup, packed_simd :: shr8(raw, 4));
            for lane in 16 {
                a.q[panel_index][2 * group][lane] = lo[lane];
                a.q[panel_index][2 * group][lane + 16] = hi[lane];
                a.q[panel_index][2 * group + 1][lane] = lo[lane + 16];
                a.q[panel_index][2 * group + 1][lane + 16] = hi[lane + 16];
            }
        }
    } else

    if kind == 6 {
        a.d[di] = half(read16(x, base)) * (0.125 as f32);
        for sub in 8 {
            let info = read32(x, base + 2 + sub * 8 + 4);
            bitscale(a, si + sub, (2 * (info >> 28) + 1) as i32);
            for group in 4 {
                grid8(
                    a,
                    panel_index,
                    sub * 32 + group * 8,
                    gxx[x[base + 2 + sub * 8 + group] as i64],
                    signs[ ( (info >> ( (group * 7) as u32)) & 127) as i64]);
            }
        }
    } else

    if kind == 7 || kind == 8 {
        a.d[di] = half(read16(x, base)) * (0.125 as f32);
        let scales = base + (if kind == 7 {
            66
        } else {
            74
        }
        );
        for sub in 8 {
            bitscale(a, si + 2 * sub, (2 * (x[scales + sub] & 15) + 1) as i32);
            bitscale(a, si + 2 * sub + 1, (2 * (x[scales + sub] >> 4) + 1) as i32);
            for group in 4 {
                if kind == 7 {
                    let info = read16(x, base + 2 + (sub * 4 + group) * 2);
                    grid8(a, panel_index, sub * 32 + group * 8, gxs[ (info & 511) as i64], signs[ (info >> 9) as i64]);
                } else {
                    let index = (x[base + 2 + sub * 4 + group] as u16) | ( ( (x[base + 66 + sub] as u16) << ( (8 - 2 * group) as u16)) & 768);
                    grid8(a, panel_index, sub * 32 + group * 8, gs[index as i64], x[base + 34 + sub * 4 + group]);
                }
            }
        }
    } else

    if kind == 9 {
        a.d[di] = half(read16(x, base)) * (0.25 as f32);
        for sub in 8 {
            let info = read32(x, base + 66 + sub * 4);
            bitscale(a, si + sub, (2 * (info >> 28) + 1) as i32);
            for group in 4 {
                let lo = g3x[x[base + 2 + sub * 8 + group * 2] as i64];
                let hi = g3x[x[base + 2 + sub * 8 + group * 2 + 1] as i64];
                var grid: u8[8] = default < u8[8] > ();
                for lane in 4 {
                    grid[lane] = lo[lane];
                    grid[lane + 4] = hi[lane];
                }
                grid8(
                    a,
                    panel_index,
                    sub * 32 + group * 8,
                    grid,
                    signs[ ( (info >> ( (group * 7) as u32)) & 127) as i64]);
            }
        }
    } else

    if kind == 10 {
        a.d[di] = half(read16(x, base));
        for sub in 8 {
            let packed = x[base + 106 + sub / 2];
            bitscale(a, si + sub, (1 + 2 * ( (packed >> ( (4 * (sub % 2)) as u8)) & 15)) as i32);
            for group in 4 {
                let high = x[base + 66 + sub] as u16;
                let lo = (x[base + 2 + sub * 8 + group * 2] as u16) | ( (high << ( (8 - 2 * group) as u16)) & 256);
                let hi = (x[base + 2 + sub * 8 + group * 2 + 1] as u16) | ( (high << ( (7 - 2 * group) as u16)) & 256);
                let g0 = g3s[lo as i64];
                let g2 = g3s[hi as i64];
                var grid: u8[8] = default < u8[8] > ();
                for lane in 4 {
                    grid[lane] = g0[lane];
                    grid[lane + 4] = g2[lane];
                }
                grid8(a, panel_index, sub * 32 + group * 8, grid, x[base + 74 + sub * 4 + group]);
            }
        }
    } else

    if kind == 11 {
        a.d[di] = half(read16(x, base)) * (0.125 as f32);
        for sub in 8 {
            let high = read16(x, base + 34 + sub * 2);
            bitscale(a, si + sub, (2 * ( (high >> 12) & 7) + 1) as i32);
            let delta: i16 = if (high & 32768) != 0 {
                (-1 as i16)
            } else {
                (1 as i16)
            }
            ;
            for group in 4 {
                let index = (x[base + 2 + sub * 4 + group] as u16) | ( ( (high >> ( (3 * group) as u16)) & 7) << 8);
                tern8(a, panel_index, sub * 32 + group * 8, g1[index as i64], delta);
            }
        }
    } else {
        assert(kind == 12);
        let s0 = read16(x, base + 48);
        let s1 = read16(x, base + 50);
        let s2 = read16(x, base + 52);
        let s3 = read16(x, base + 54);
        let d = (s0 >> 12) | ( (s1 >> 8) & 240) | ( (s2 >> 4) & 3840) | (s3 & 61440);
        a.d[di] = half(d) * (0.125 as f32);
        for sub in 8 {
            let scales = read16(x, base + 48 + (sub / 2) * 2);
            let sh = (6 * (sub % 2)) as u16;
            bitscale(a, si + sub * 2, (2 * ( (scales >> sh) & 7) + 1) as i32);
            bitscale(a, si + sub * 2 + 1, (2 * ( (scales >> (sh + 3)) & 7) + 1) as i32);
            for group in 4 {
                let high = x[base + 32 + sub * 2 + group / 2] as u16;
                let shift = (if group % 2 == 0 {
                    8
                } else {
                    4
                }
                ) as u16;
                let index = (x[base + sub * 4 + group] as u16) | ( (high << shift) & 1792);
                let delta: i16 = if ( (high >> ( (if group % 2 == 0 {
                    3
                } else {
                    7
                }
                ) as u16)) & 1) != 0 {
                    (-1 as i16)
                } else {
                    (1 as i16)
                }
                ;
                tern8(a, panel_index, sub * 32 + group * 8, g1[index as i64], delta);
            }
        }
    }
}

fn unpack_panel(
    p: const Plan &,
    x: const u8[:],
    w: Workspace &,
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) {
    let f = tile_traits(p.kind);
    let nb = 256 / f.sub;
    let a: WeightTile & = & w.a;
    assert(p.first >= 0 && p.stride >= f.bytes && p.start >= 0 && p.count >= 0 && p.count <= 256 && p.nk >= 1 && p.nk <= 16 && round16(p.count) * p.nk <= 256);
    assert(p.nk == 1 || p.count <= 16);
    let ones16 = packed_simd :: splat16(1);
    var ones: i8[32] = default < i8[32] > ();
    for lane in 32 {
        ones[lane] = 1;
    }
    for slab in p.nk {
        for row in round16(p.count) {
            let panel_index = row * p.nk + slab;
            let di = slab * 16 + row;

            if row < p.count {
                unpack_block(
                    p.kind,
                    x,
                    p.origin + (p.first + row) * p.stride + (p.start + slab) * f.bytes,
                    panel_index,
                    di,
                    a,
                    gxx,
                    gxs,
                    gs,
                    g3x,
                    g3s,
                    g1,
                    signs,
                    palette);
            } else {
                a.d[di] = (0 as f32);
                a.dmin[di] = (0 as f32);
                for j in nb {
                    bitscale(a, panel_index * nb + j, 0);
                    bitmin(a, panel_index * nb + j, 0);
                }
                for j in 8 {
                    a.q[panel_index][j] = default < u8[32] > ();
                }
            }

            if f.act != 0 {
                var correction: i32 = 0;
                for j in 8 {
                    let sums = packed_simd :: madd16(ones16, packed_simd :: maddubs(a.q[panel_index][j], ones));

                    if f.sub == 32 {
                        correction += 128 * a.scales[ (panel_index * nb + j) / 8][ (panel_index * nb + j) % 8] * (hsum(sums) - (32 * (f.bias as i32)));
                    } else {
                        let lo = (sums[0] + sums[1]) + (sums[2] + sums[3]);
                        let hi = (sums[4] + sums[5]) + (sums[6] + sums[7]);
                        let s = panel_index * nb + 2 * j;
                        correction += 128 * a.scales[s / 8][s % 8] * (lo - 16 * (f.bias as i32)) + 128 * a.scales[ (s + 1) / 8][ (s + 1) % 8] * (hi - 16 * (f.bias as i32));
                    }
                }
                bitmin(a, panel_index * nb, correction);
            }
        }
    }
    witness(0, p.kind, p.count * p.nk);
}

