fn image_ops(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != OP_IM2COL && p.op != OP_POOL_1D && p.op != OP_POOL_2D && p.op != OP_WIN_PART && p.op != OP_WIN_UNPART {
        return false;
    }
    var i = thread_first(p);
    let count = elements(p.d);

    while i < count {
        let o0 = i % p.d.ne[0];
        let o1 = (i / p.d.ne[0]) % p.d.ne[1];
        let o2 = (i / (p.d.ne[0] * p.d.ne[1])) % p.d.ne[2];
        let o3 = i / (p.d.ne[0] * p.d.ne[1] * p.d.ne[2]);
        var value: f32 = (0 as f32);

        if p.op == OP_IM2COL {
            let is2 = p.ip[6] != 0;
            let kw = p.x.ne[0];
            let kh = if is2 {
                p.x.ne[1]
            } else {
                1
            }
            ;
            let channel = o0 / (kw * kh);
            let tap = o0 % (kw * kh);
            let xx = o1 * p.ip[0] + (tap % kw) * p.ip[4] - p.ip[2];
            let yy = if is2 {
                o2 * p.ip[1] + (tap / kw) * p.ip[5] - p.ip[3]
            } else {
                0
            }
            ;
            let input_h = if is2 {
                p.y.ne[1]
            } else {
                1
            }
            ;
            let batch = if is2 {
                o3
            } else {
                o2
            }
            ;

            if xx >= 0 && xx < p.y.ne[0] && yy >= 0 && yy < input_h {
                let off = xx * p.y.nb[0] + (if is2 {
                    yy * p.y.nb[1] + channel * p.y.nb[2] + batch * p.y.nb[3]
                } else {
                    channel * p.y.nb[1] + batch * p.y.nb[2]
                }
                );
                value = load_at(p.y, y, off);
            }
        } else

        if p.op == OP_POOL_1D || p.op == OP_POOL_2D {
            let is2 = p.op == OP_POOL_2D;
            let k0 = p.ip[1];
            let k1 = if is2 {
                p.ip[2]
            } else {
                1
            }
            ;
            let s0 = if is2 {
                p.ip[3]
            } else {
                p.ip[2]
            }
            ;
            let s1 = if is2 {
                p.ip[4]
            } else {
                1
            }
            ;
            let pad0 = if is2 {
                p.ip[5]
            } else {
                p.ip[3]
            }
            ;
            let pad1 = if is2 {
                p.ip[6]
            } else {
                0
            }
            ;
            assert(k0 > 0 && k1 > 0);
            let average = p.ip[0] == 0;
            value = if average {
                (0 as f32)
            } else {
                from_bits(0xff7fffff)
            }
            ;
            for ky in k1 {
                for kx in k0 {
                    let xx = o0 * s0 - pad0 + kx;
                    let yy = if is2 {
                        o1 * s1 - pad1 + ky
                    } else {
                        o1
                    }
                    ;

                    if xx >= 0 && xx < p.x.ne[0] && yy >= 0 && yy < p.x.ne[1] {
                        let v = load_at(p.x, x, xx * p.x.nb[0] + yy * p.x.nb[1] + o2 * p.x.nb[2] + o3 * p.x.nb[3]);
                        value = if average {
                            value + v
                        } else {
                            max32(v, value)
                        }
                        ;
                    }
                }
            }

            if average {
                value /= (k0 * k1) as f32;
            }
        } else

        if p.op == OP_WIN_PART {
            let window = p.ip[0];
            let nx = p.ip[1];
            assert(window > 0 && nx > 0);
            let wx = o3 % nx;
            let wy = o3 / nx;
            let xx = wx * window + o1;
            let yy = wy * window + o2;

            if xx < p.x.ne[1] && yy < p.x.ne[2] {
                value = load_at(p.x, x, o0 * p.x.nb[0] + xx * p.x.nb[1] + yy * p.x.nb[2]);
            }
        } else {
            let window = p.ip[0];
            let nx = (p.d.ne[1] + window - 1) / window;
            let which = (o2 / window) * nx + (o1 / window);
            value = load_at(
                p.x,
                x,
                o0 * p.x.nb[0] + (o1 % window) * p.x.nb[1] + (o2 % window) * p.x.nb[2] + which * p.x.nb[3]);
        }
        store(p.d, dst, i, value);
        i += p.nth;
    }
    return true;
}

