// Shared fusion and parameter decisions;imports are typed projections or accepted Goose components.
extern "fg160_arg" fn arg(p: i64, i: i64) -> u64;
extern "fg160_graph" fn graph(g: i64, k: i64) -> i64;
extern "fg160_node" fn node(g: i64, i: i64) -> i64;
extern "fg160_tensor" fn tensor(t: i64, k: i64, i: i64) -> u64;
extern "fg160_i32" fn read_i32(p: i64, i: i64) -> i64;
extern "fg160_usage" fn usage(buffer: i64) -> i64;
extern "fg160_hash_find" fn hash_find(g: i64, t: i64) -> u64;
extern "fg160_hash_used" fn hash_used(g: i64, slot: u64) -> i64;
extern "fg160_count" fn count_read(g: i64, slot: u64) -> i64;
extern "fg160_param_read" fn raw_param(t: i64, offset: u64) -> u64;
extern "fg160_param_write" fn put_param(t: i64, offset: u64, value: u64);
extern "fg160_param_copy" fn copy_param(t: i64, data: i64, n: u64);
extern "fg160_glu" fn make_glu(c: i64, a: i64, b: i64, op: i64) -> i64;
extern "fg160_argsort" fn sort(c: i64, a: i64) -> i64;
extern "fg160_view" fn view(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64, b1: u64, b2: u64, b3: u64) -> i64;
extern "fg160_new" fn fresh(c: i64, ty: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64;
extern "fg160_op_src" fn op_src(t: i64, op: i64, src: i64);
extern "fg160_fail" fn fail(code: i64);

fn flag_output() -> u64 {
    return 2 as u64;
}

fn flag_param() -> u64 {
    return 4 as u64;
}

fn flag_compute() -> u64 {
    return 16 as u64;
}

fn max_src() -> u64 {
    return 10 as u64;
}

fn usage_weights() -> u64 {
    return 1 as u64;
}

fn param_bytes() -> u64 {
    return 64 as u64;
}

fn op_rope() -> u64 {
    return 48 as u64;
}

fn op_rope_back() -> u64 {
    return 49 as u64;
}

fn op_pad() -> u64 {
    return 64 as u64;
}

fn glu_oai() -> u64 {
    return 3 as u64;
}

fn glu_clamp() -> u64 {
    return 6 as u64;
}

fn rope_vision() -> u64 {
    return 24 as u64;
}

fn ip(x: u64) -> i64 {
    return x as !i64;
}

fn ensure(v: bool, code: i64) {
    if !v {
        fail(code);
    }
}

fn valid(g: i64, i: i64) -> bool {
    return i >= 0 && i < graph(g, 0);
}

fn index(p: i64, seq: i64, i: i64) -> i64 {
    return if p == 0 {
        seq + i
    } else {
        read_i32(p, i)
    }
    ;
}

fn use_count(g: i64, i: i64) -> i64 {
    ensure(valid(g, i), 1);
    let slot = hash_find(g, node(g, i));

    if slot == 18446744073709551615 as u64 || hash_used(g, slot) == 0 {
        return 0;
    }
    return count_read(g, slot);
}

fn has_uses(g: i64, i: i64, n: i64) -> bool {
    if !valid(g, i) {
        return false;
    }
    let t = node(g, i);
    return use_count(g, i) == n && tensor(t, 2, 0) == 0 && (tensor(t, 1, 0) & flag_output()) == 0;
}

fn shape(a: i64, b: i64) -> bool {
    var i: i64 = 0;

    while i < 4 {
        if tensor(a, 3, i) != tensor(b, 3, i) {
            return false;
        }
        i += 1;
    }
    return true;
}

fn constant(t: i64) -> bool {
    let buffer = ip(tensor(t, 4, 0));
    return buffer != 0 && usage(buffer) == (usage_weights() as i64) && (tensor(t, 1, 0) & flag_param()) == 0;
}

fn member(g: i64, indices: i64, count: i64, t: i64, seq: i64) -> i64 {
    var i: i64 = 0;

    while i < count {
        let k = index(indices, seq, i);

        if !valid(g, k) {
            return -1;
        }

        if node(g, k) == t {
            return i;
        }
        i += 1;
    }
    return -1;
}

fn linear(g: i64, indices: i64, seq: i64, ops: i64, count: i64) -> bool {
    if count < 0 {
        return false;
    }
    var i: i64 = 0;

    while i < count {
        let k = index(indices, seq, i);

        if !valid(g, k) {
            return false;
        }
        let t = node(g, k);

        if tensor(t, 0, 0) != (read_i32(ops, i) as u64) || (tensor(t, 1, 0) & flag_compute()) == 0 {
            return false;
        }

        if i < count - 1 && !has_uses(g, k, 1) {
            return false;
        }

        if i > 0 {
            let prev = node(g, index(indices, seq, i - 1));

            if (tensor(t, 5, 0) != (prev as u64) && tensor(t, 5, 1) != (prev as u64)) || !shape(t, prev) {
                return false;
            }
        }
        i += 1;
    }
    return true;
}

fn subgraph(g: i64, indices: i64, seq: i64, ops: i64, count: i64, outputs: i64, nout: i64) -> bool {
    ensure(outputs != 0 && nout > 0, 2);

    if count < 0 {
        return false;
    }
    var i: i64 = 0;

    while i < count {
        let k = index(indices, seq, i);

        if !valid(g, k) {
            return false;
        }
        let t = node(g, k);

        if tensor(t, 0, 0) != (read_i32(ops, i) as u64) || (tensor(t, 1, 0) & flag_compute()) == 0 {
            return false;
        }

        if member(g, outputs, nout, t, 0) != -1 {
            i += 1;
            continue;
        }

        if (tensor(t, 1, 0) & flag_output()) != 0 {
            return false;
        }
        var uses: i64 = 0;
        var j = i + 1;

        while j < count {
            let nextidx = index(indices, seq, j);

            if !valid(g, nextidx) {
                return false;
            }
            let next = node(g, nextidx);
            var s: i64 = 0;

            while s < (max_src() as i64) {
                if tensor(next, 5, s) == (t as u64) {
                    uses += 1;
                }
                s += 1;
            }
            j += 1;
        }

        if uses != use_count(g, k) {
            return false;
        }
        var base = ip(tensor(t, 2, 0));
        var slow = base;
        var fast = base;

        while base != 0 {
            if member(g, indices, count, base, seq) == -1 && !constant(base) {
                return false;
            }
            base = ip(tensor(base, 2, 0));

            if slow != 0 {
                slow = ip(tensor(slow, 2, 0));
            }

            if fast != 0 {
                fast = ip(tensor(fast, 2, 0));
            }

            if fast != 0 {
                fast = ip(tensor(fast, 2, 0));
            }

            if slow != 0 && slow == fast {
                return false;
            }
        }
        i += 1;
    }
    return true;
}

fn checked_add(a: i64, b: i64) -> i64 {
    if b > 0 {
        ensure(a <= 9223372036854775807 - b, 3);
    }

    if b < 0 {
        ensure(a >= (-9223372036854775807 - 1) - b, 3);
    }
    return a + b;
}

fn dispatch(mode: i64, p: i64) -> u64 {
    let a = ip(arg(p, 0));
    let b = ip(arg(p, 1));
    let c = ip(arg(p, 2));
    let d = ip(arg(p, 3));
    let e = ip(arg(p, 4));
    let f = ip(arg(p, 5));
    let h = ip(arg(p, 6));

    if mode == 0 {
        return use_count(a, b) as !u64;
    }

    if mode == 1 {
        return if has_uses(a, b, c) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 2 {
        return if linear(a, b, 0, c, d) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 3 {
        ensure(d < 32, 4);

        if b < 0 || d < 0 || b + d > graph(a, 0) {
            return 0 as u64;
        }
        return if linear(a, 0, b, c, d) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 4 {
        return if subgraph(a, b, 0, d, c, e, f) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 5 {
        ensure(c < 32, 5);

        if b < 0 || c < 0 || b + c > graph(a, 0) {
            return 0 as u64;
        }
        return if subgraph(a, 0, b, d, c, e, f) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 6 {
        if d < 0 {
            return 0 as u64;
        }
        var i: i64 = 0;

        while i < d {
            let dst = read_i32(c, i * 3);
            let srcidx = read_i32(c, i * 3 + 1);
            let from = read_i32(c, i * 3 + 2);
            let di = b + dst;
            let si = b + from;

            if !valid(a, di) || !valid(a, si) || srcidx < 0 || srcidx >= (max_src() as i64) {
                return 0 as u64;
            }

            if tensor(node(a, di), 5, srcidx) != (node(a, si) as u64) {
                return 0 as u64;
            }
            i += 1;
        }
        return 1 as u64;
    }

    if mode == 7 {
        ensure(a != 0 && b != 0, 6);
        return member(a, b, c, d, 0) as !u64;
    }

    if mode == 8 {
        return if constant(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 9 {
        ensure(a != 0 && arg(p, 2) <= param_bytes() && (arg(p, 2) == 0 || b != 0), 7);

        if arg(p, 2) > 0 {
            copy_param(a, b, arg(p, 2));
        }
        return 0 as u64;
    }

    if mode == 10 || mode == 11 {
        ensure(a != 0 && arg(p, 1) < param_bytes() / 4, 8);
        return raw_param(a, arg(p, 1) * 4);
    }

    if mode == 12 || mode == 13 {
        ensure(a != 0 && arg(p, 1) < param_bytes() / 4, 9);
        put_param(a, arg(p, 1) * 4, arg(p, 2));
        return 0 as u64;
    }

    if mode == 14 {
        let op = tensor(a, 0, 0);
        ensure( (op == op_rope() || op == op_rope_back()) && b >= 0 && raw_param(a, 8 as u64) != rope_vision(), 10);
        put_param(a, 60 as u64, arg(p, 1));
        return a as u64;
    }

    if mode == 15 {
        let r = make_glu(a, b, c, if h == 0 {
            glu_oai() as i64
        } else {
            glu_clamp() as i64
        }
        );

        if h == 0 {
            put_param(r, 8 as u64, arg(p, 3));
        }
        put_param(r, 12 as u64, arg(p, 4));
        return r as u64;
    }

    if mode == 16 {
        ensure(c >= 0 && (c as u64) <= tensor(b, 3, 0), 11);
        let t = sort(a, b);
        return view(
            a,
            t,
            c,
            ip(tensor(t, 3, 1)),
            ip(tensor(t, 3, 2)),
            ip(tensor(t, 3, 3)),
            tensor(t, 6, 1),
            tensor(t, 6, 2),
            tensor(t, 6, 3)) as u64;
    }

    if mode == 17 {
        // Padding values are signed int32;validate final extents and every signed add.
        let n0 = checked_add(checked_add(ip(tensor(b, 3, 0)), read_i32(c, 0)), read_i32(c, 1));
        let n1 = checked_add(checked_add(ip(tensor(b, 3, 1)), read_i32(c, 2)), read_i32(c, 3));
        let n2 = checked_add(checked_add(ip(tensor(b, 3, 2)), read_i32(c, 4)), read_i32(c, 5));
        let n3 = checked_add(checked_add(ip(tensor(b, 3, 3)), read_i32(c, 6)), read_i32(c, 7));
        ensure(n0 >= 0 && n1 >= 0 && n2 >= 0 && n3 >= 0, 12);
        let r = fresh(a, ip(tensor(b, 7, 0)), n0, n1, n2, n3);
        var i: i64 = 0;

        while i < 8 {
            put_param(r, (i * 4) as u64, read_i32(c, i) as !u64);
            i += 1;
        }
        put_param(r, 32 as u64, if d != 0 {
            1 as u64
        } else {
            0 as u64
        }
        );
        op_src(r, op_pad() as i64, b);
        return r as u64;
    }
    fail(99);
    return 0 as u64;
}

fn main() {
    print(dispatch(99, 0));
}

