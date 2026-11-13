fn read_index(t: const Tensor &, a: const u8[:], i: i64) -> i64 {
    let at = offset(t, i);

    if t.kind == 26 {
        return (read32(a, at) as !i32) as i64;
    }
    assert(t.kind == 27 && at >= 0 && at <= a.len - 8);
    var u: u64 = 0;
    for b in 8 {
        u = u | ( (a[at + b] as u64) << ( (8 * b) as u64));
    }
    return u as !i64;
}

fn indexed(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != OP_SET_ROWS && p.op != OP_GET_ROWS_BACK && p.op != OP_REPEAT_BACK && p.op != OP_ADD_ID && p.op != OP_ACC && p.op != OP_SET && p.op != OP_TRI {
        return false;
    }

    if p.op == OP_TRI {
        var index = thread_first(p);

        while index < elements(p.d) {
            let col = index % p.d.ne[0];
            let row = (index / p.d.ne[0]) % p.d.ne[1];
            let keep = if p.ip[0] != 0 {
                col >= row + p.ip[1]
            } else {
                col <= row + p.ip[1]
            }
            ;
            store(p.d, dst, index, if keep {
                load(p.x, x, index)
            } else {
                (0 as f32)
            }
            );
            index += p.nth;
        }
        return true;
    }

    if p.op == OP_REPEAT_BACK {
        if p.ith != 0 {
            return true;
        }
        for i in elements(p.d) {
            store(p.d, dst, i, (0 as f32));
        }
        for i in elements(p.x) {
            var n = i;
            var target: i64 = 0;
            var stride: i64 = 1;
            for axis in 4 {
                let at = n % p.x.ne[axis];
                n /= p.x.ne[axis];
                target += (at % p.d.ne[axis]) * stride;
                stride *= p.d.ne[axis];
            }
            store(p.d, dst, target, load(p.d, dst, target) + load(p.x, x, i));
        }
        return true;
    }

    if p.op == OP_GET_ROWS_BACK {
        if p.ith != 0 {
            return true;
        }
        for i in elements(p.d) {
            store(p.d, dst, i, (0 as f32));
        }
        let width = p.x.ne[0];
        for row in rows(p.x) {
            let id = read_index(p.y, y, row);
            assert(id >= 0 && id < p.d.ne[1]);
            for col in width {
                let index = col + id * width;
                store(p.d, dst, index, load(p.d, dst, index) + load(p.x, x, row * width + col));
            }
        }
        return true;
    }

    if p.op == OP_SET_ROWS {
        if p.ith != 0 {
            return true;
        }
        let width = p.x.ne[0];
        assert(width == p.d.ne[0]);
        for row in rows(p.x) {
            let i1 = row % p.x.ne[1];
            let i2 = (row / p.x.ne[1]) % p.x.ne[2];
            let i3 = row / (p.x.ne[1] * p.x.ne[2]);
            let id = read_index(p.y, y, i1 + p.y.ne[0] * ( (i2 % p.y.ne[1]) + p.y.ne[1] * (i3 % p.y.ne[2])));
            assert(id >= 0 && id < p.d.ne[1]);
            for col in width {
                let target = col * p.d.nb[0] + id * p.d.nb[1] + i2 * p.d.nb[2] + i3 * p.d.nb[3];
                store_at(p.d, dst, target, load(p.x, x, row * width + col));
            }
        }
        return true;
    }

    if p.op == OP_ADD_ID {
        return false;
    }

    if p.ith != 0 {
        return true;
    }
    assert(elements(p.x) == elements(p.d));

    if p.ip[4] == 0 {
        for i in elements(p.x) {
            let a = offset(p.x, i);
            let b = offset(p.d, i);
            for byte in scalar_bytes(p.x.kind) {
                dst[b + byte] = x[a + byte];
            }
        }
    }
    for i in elements(p.y) {
        var n = i;
        var target = p.ip[3];
        for axis in 4 {
            let at = n % p.y.ne[axis];
            n /= p.y.ne[axis];
            let stride = if axis == 0 {
                p.d.nb[0]
            } else {
                p.ip[axis - 1]
            }
            ;
            target = add_size(target, mul_size(at, stride));
        }
        let value = load(p.y, y, i);
        store_at(p.d, dst, target, if p.op == OP_ACC {
            load_at(p.d, dst, target) + value
        } else {
            value
        }
        );
    }
    return true;
}

