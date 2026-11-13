// Full scheduler decisions/state transitions;imports project fields or execute effects.
extern "fg164_dep_slot" fn dep_slot(dep: i64, t: i64) -> u64;
extern "fg164_arg" fn arg(p: i64, i: i64) -> u64;
extern "fg164_get" fn get(p: i64, kind: i64, k: i64, i: i64) -> u64;
extern "fg164_put" fn put(p: i64, kind: i64, k: i64, i: i64, v: u64);
extern "fg164_constant" fn constant(k: i64) -> u64;
extern "fg164_record" fn record(s: i64, kind: i64, i: i64) -> i64;
extern "fg164_ptr" fn ptr(p: i64, i: u64) -> u64;
extern "fg164_ptr_put" fn ptr_put(p: i64, i: u64, v: u64);
extern "fg164_i32" fn word(p: i64, i: u64) -> i64;
extern "fg164_i32_put" fn word_put(p: i64, i: u64, v: i64);
extern "fg164_heap" fn heap(kind: i64, n: u64, clear: i64) -> i64;
extern "fg164_drop" fn drop(kind: i64, p: i64);
extern "fg164_mem" fn mem(dst: i64, src: i64, n: u64, fill: i64);
extern "fg164_hash_slot" fn slot(s: i64, t: i64) -> u64;
extern "fg164_component" fn component(op: i64, a: i64, b: i64, c: i64, d: u64, e: u64) -> u64;
extern "fg164_view" fn graph_view(out: i64, g: i64, lo: i64, hi: i64);
extern "fg164_name" fn name(t: i64, backend: i64, src: i64, copy: i64);
extern "fg164_optimize" fn optimize(backend: i64, graph: i64, deps: i64);
extern "fg164_callback" fn callback(which: i64, s: i64, a: i64, b: i64, c: i64, d: i64) -> u64;
extern "fg164_expert" fn expert(s: i64, b: i64, op: i64, src: i64, dst: i64, value: i64) -> u64;
extern "fg164_expert_proc" fn expert_proc(backend: i64) -> u64;
extern "fg164_async_present" fn async_present(backend: i64) -> i64;
extern "fg164_try_async" fn try_async(src_backend: i64, dst_backend: i64, src: i64, dst: i64) -> u64;
extern "fg164_env" fn env(k: i64) -> i64;
extern "fg164_atoi" fn atoi(p: i64) -> i64;
extern "fg164_debug" fn debug(which: i64, s: i64, g: i64);
extern "fg164_fail" fn fail(code: i64);
extern "fg164_mark" fn mark(mode: i64);

fn op_view() -> i64 {
    return 37;
}

fn op_reshape() -> i64 {
    return 36;
}

fn op_permute() -> i64 {
    return 38;
}

fn op_transpose() -> i64 {
    return 39;
}

fn op_rope() -> i64 {
    return 48;
}

fn op_flash() -> i64 {
    return 74;
}

fn op_mmid() -> i64 {
    return 30;
}

fn flag_input() -> u64 {
    return 1 as u64;
}

fn ip(x: u64) -> i64 {
    return x as !i64;
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn ensure(v: bool, code: i64) {
    if !v {
        fail(code);
    }
}

fn add(a: u64, b: u64) -> u64 {
    ensure(a <= U() - b, 1);
    return a + b;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 2);
    return a * b;
}

fn s(p: i64, k: i64, i: i64) -> u64 {
    return get(p, 0, k, i);
}

fn w(p: i64, k: i64, i: i64, v: u64) {
    put(p, 0, k, i, v);
}

fn sp(p: i64, k: i64) -> u64 {
    return get(p, 1, k, 0);
}

fn sw(p: i64, k: i64, v: u64) {
    put(p, 1, k, 0, v);
}

fn t(p: i64, k: i64, i: i64) -> u64 {
    return get(p, 3, k, i);
}

fn gr(p: i64, k: i64, i: i64) -> u64 {
    return get(p, 2, k, i);
}

fn gw(p: i64, k: i64, i: i64, v: u64) {
    put(p, 2, k, i, v);
}

fn backend(sched: i64, b: i64) -> i64 {
    ensure(b >= 0 && b < ip(s(sched, 2, 0)), 3);
    return ip(s(sched, 3, b));
}

fn bid(sched: i64, tensor: i64) -> i64 {
    return word(ip(s(sched, 7, 0)), slot(sched, tensor));
}

fn set_bid(sched: i64, tensor: i64, b: i64) {
    word_put(ip(s(sched, 7, 0)), slot(sched, tensor), b);
}

fn copy_index(sched: i64, id: u64, b: i64, c: i64) -> u64 {
    ensure(b >= 0 && b < ip(s(sched, 2, 0)) && c >= 0 && c < ip(s(sched, 17, 0)), 4);
    return add(mul(id, mul(s(sched, 2, 0), s(sched, 17, 0))), add(mul(b as u64, s(sched, 17, 0)), c as u64));
}

fn copy_get(sched: i64, id: u64, b: i64, c: i64) -> i64 {
    return ip(ptr(ip(s(sched, 8, 0)), copy_index(sched, id, b, c)));
}

fn copy_put(sched: i64, id: u64, b: i64, c: i64, tensor: i64) {
    ptr_put(ip(s(sched, 8, 0)), copy_index(sched, id, b, c), tensor as u64);
}

fn split(sched: i64, i: i64) -> i64 {
    ensure(i >= 0 && i < ip(s(sched, 16, 0)), 5);
    return record(sched, 0, i);
}

fn view_op(op: i64) -> bool {
    return op == op_view() || op == op_reshape() || op == op_permute() || op == op_transpose();
}

fn supports(sched: i64, b: i64, node: i64) -> bool {
    return component(0, backend(sched, b), node, 0, 0 as u64, 0 as u64) != 0;
}

fn supports_kind(sched: i64, b: i64, kind: i64) -> bool {
    return component(1, backend(sched, b), kind, 0, 0 as u64, 0 as u64) != 0;
}

fn buffer_of(tensor: i64) -> i64 {
    let view = ip(t(tensor, 3, 0));
    return ip(t(if view != 0 {
        view
    } else {
        tensor
    }
    , 5, 0));
}

fn backend_index(sched: i64, p: i64) -> i64 {
    var i: i64 = 0;

    while i < ip(s(sched, 2, 0)) {
        if s(sched, 3, i) == (p as u64) {
            return i;
        }
        i += 1;
    }
    return -1;
}

fn from_buffer(sched: i64, tensor: i64, node: i64) -> i64 {
    let buf = buffer_of(tensor);

    if buf == 0 {
        return -1;
    }
    let kind = ip(get(buf, 4, 0, 0));
    var i: i64 = 0;

    while i < ip(s(sched, 2, 0)) {
        if supports_kind(sched, i, kind) && supports(sched, i, node) {
            return i;
        }
        i += 1;
    }
    return -1;
}

fn current(sched: i64, tensor: i64) -> i64 {
    let existing = from_buffer(sched, tensor, tensor);

    if existing >= 0 {
        return existing;
    }
    let view = ip(t(tensor, 3, 0));

    if view != 0 {
        let v = from_buffer(sched, view, tensor);

        if v >= 0 {
            return v;
        }
    }
    ensure(t(tensor, 5, 0) == 0 && (view == 0 || t(view, 5, 0) == 0), 6);

    if (t(tensor, 2, 0) & flag_input()) != 0 {
        return ip(s(sched, 2, 0)) - 1;
    }

    if ip(t(tensor, 1, 0)) != op_rope() && ip(t(tensor, 1, 0)) != op_flash() {
        var j: i64 = 0;

        while (j as u64) < constant(4) {
            let src = ip(t(tensor, 4, j));

            if src != 0 {
                let buf = ip(t(src, 5, 0));

                if buf != 0 && get(buf, 4, 1, 0) == constant(5) {
                    let b = from_buffer(sched, src, tensor);

                    if s(
                        sched,
                        35,
                        0) != 0 && b == ip(s(sched, 2, 0)) - 1 && component(2,
                        buf,
                        0,
                        0,
                        0 as u64,
                        0 as u64) != 0 {
                        var up: i64 = 0;

                        while up < b {
                            if supports(
                                sched,
                                up,
                                tensor) && component(3,
                                backend(sched, up),
                                tensor,
                                0,
                                0 as u64,
                                0 as u64) != 0 {
                                return up;
                            }
                            up += 1;
                        }
                    }
                    return b;
                }
            }
            j += 1;
        }
    }
    return -1;
}

fn compatible(sched: i64, tensor: i64, b: i64) -> bool {
    let buf = buffer_of(tensor);
    var kind: i64 = 0;

    if buf != 0 {
        kind = ip(get(buf, 4, 0, 0));
    } else {
        var id = bid(sched, tensor);
        let view = ip(t(tensor, 3, 0));

        if id < 0 && view != 0 {
            id = bid(sched, view);
        }

        if id >= 0 {
            backend(sched, id);
            kind = ip(s(sched, 4, id));
        }
    }
    return kind != 0 && supports_kind(sched, b, kind);
}

fn set_supported(sched: i64, node: i64, b: i64) {
    if supports(sched, b, node) {
        set_bid(sched, node, b);
    }
}

fn grow_inputs(p: i64, graph: bool) {
    let cap = if graph {
        s(p, 23, 0)
    } else {
        sp(p, 5)
    }
    ;
    var wanted = constant(6);

    if cap > 0 {
        wanted = mul(cap, 2 as u64);
    }
    ensure(wanted <= 2147483647, 7);
    let next = heap(2, mul(wanted, 8 as u64), 0);
    ensure(next != 0, 8);
    let old = ip(if graph {
        s(p, 21, 0)
    } else {
        sp(p, 3)
    }
    );

    if cap > 0 {
        mem(next, old, cap * 8, 0);
    }
    drop(2, old);

    if graph {
        w(p, 21, 0, next as u64);
        w(p, 23, 0, wanted);
    } else {
        sw(p, 3, next as u64);
        sw(p, 5, wanted);
    }
}

fn grow_splits(sched: i64, needed: i64) {
    if needed < ip(s(sched, 16, 0)) {
        return;
    }
    let old = s(sched, 16, 0);
    let cap = mul(old, 2 as u64);
    ensure(cap <= 2147483647 && (needed as u64) < cap, 9);
    let next = heap(1, cap, 1);
    ensure(next != 0, 10);
    mem(next, ip(s(sched, 14, 0)), mul(old, constant(1)), 0);
    drop(1, ip(s(sched, 14, 0)));
    w(sched, 14, 0, next as u64);
    w(sched, 16, 0, cap);
}

fn duplicate(sched: i64, src: i64, b: i64, c: i64) -> i64 {
    let tensor = ip(component(4, ip(s(sched, 24, 0)), src, 0, 0 as u64, 0 as u64));
    var i: i64 = 0;

    while i < 4 {
        put(tensor, 3, 6, i, t(src, 6, i));
        i += 1;
    }
    name(tensor, backend(sched, b), src, c);
    return tensor;
}

fn deps_free(sched: i64) {
    let dep = ip(s(sched, 41, 0));

    if dep == 0 {
        return;
    }
    drop(5, ip(get(dep, 5, 1, 0)));
    var k: i64 = 5;

    while k <= 9 {
        drop(2, ip(get(dep, 5, k, 0)));
        k += 1;
    }
    drop(4, dep);
    w(sched, 41, 0, 0 as u64);
}

fn candidates_free(sched: i64) {
    drop(6, ip(s(sched, 42, 0)));
    w(sched, 42, 0, 0 as u64);
    w(sched, 43, 0, 0 as u64);
    w(sched, 44, 0, 0 as u64);
}

fn deps_new(sched: i64, n: u64) -> i64 {
    let dep = heap(4, 1 as u64, 1);
    ensure(dep != 0, 11);
    put(dep, 5, 0, 0, sched as u64);
    w(sched, 41, 0, dep as u64);
    let size = component(25, 0, 0, 0, add(mul(n, 2 as u64), 1 as u64), 0 as u64);
    put(dep, 5, 4, 0, size);
    var k: i64 = 5;

    while k <= 9 {
        let bytes = if k == 5 {
            mul(size / 32 + if size % 32 != 0 {
                1 as u64
            } else {
                0 as u64
            }
            , 4 as u64)
        } else {
            mul(size, 8 as u64)
        }
        ;
        let p = heap(2, bytes, if k == 6 {
            0
        } else {
            1
        }
        );
        ensure(p != 0, 12);
        put(dep, 5, k, 0, p as u64);
        k += 1;
    }
    return dep;
}

fn dep_add(dep: i64, tensor: i64, until: i64) {
    ensure(tensor != 0 && until != 0, 13);
    let key = dep_slot(dep, until);
    let heads = ip(get(dep, 5, 7, 0));
    let tails = ip(get(dep, 5, 8, 0));
    let counts = ip(get(dep, 5, 9, 0));
    let n = get(dep, 5, 2, 0);
    let cap = get(dep, 5, 3, 0);
    let pairs = ip(get(dep, 5, 1, 0));
    var chain = ptr(heads, key);

    while chain != 0 {
        if get(pairs, 6, 0, (chain - 1) as i64) == (tensor as u64) {
            return;
        }
        chain = get(pairs, 6, 2, (chain - 1) as i64);
    }

    if n == cap {
        let count = if cap == 0 {
            16 as u64
        } else {
            mul(cap, 2 as u64)
        }
        ;
        let next = heap(5, count, 0);
        ensure(next != 0, 14);

        if n != 0 {
            mem(next, pairs, mul(n, constant(2)), 0);
        }
        drop(5, pairs);
        put(dep, 5, 1, 0, next as u64);
        put(dep, 5, 3, 0, count);
    }
    let p = ip(get(dep, 5, 1, 0));
    put(p, 6, 0, n as i64, tensor as u64);
    put(p, 6, 1, n as i64, until as u64);
    put(p, 6, 2, n as i64, 0 as u64);
    let tail = ptr(tails, key);

    if tail != 0 {
        put(p, 6, 2, (tail - 1) as i64, n + 1);
    } else {
        ptr_put(heads, key, n + 1);
    }
    ptr_put(tails, key, n + 1);
    ptr_put(counts, key, ptr(counts, key) + 1);
    put(dep, 5, 2, 0, n + 1);
}

fn dep_head(dep: i64, until: i64) -> u64 {
    return ptr(ip(get(dep, 5, 7, 0)), dep_slot(dep, until));
}

fn ensure_graph_arrays(sched: i64, size: u64) {
    ensure(size <= 2147483647, 14);

    if s(sched, 40, 0) < size {
        var k: i64 = 9;
        var temp0: i64 = 0;
        var temp1: i64 = 0;
        var temp2: i64 = 0;
        var temp3: i64 = 0;

        while k <= 12 {
            let p = heap(2, mul(size, 4 as u64), 1);

            if p == 0 {
                drop(2, temp0);
                drop(2, temp1);
                drop(2, temp2);
                drop(2, temp3);
                fail(15);
                return;
            }
            mem(p, ip(s(sched, k, 0)), mul(s(sched, 40, 0), 4 as u64), 0);

            if k == 9 {
                temp0 = p;
            }

            if k == 10 {
                temp1 = p;
            }

            if k == 11 {
                temp2 = p;
            }

            if k == 12 {
                temp3 = p;
            }
            k += 1;
        }
        k = 9;

        while k <= 12 {
            drop(2, ip(s(sched, k, 0)));
            w(sched, k, 0, (if k == 9 {
                temp0
            } else {
                if k == 10 {
                    temp1
                } else {
                    if k == 11 {
                        temp2
                    } else {
                        temp3
                    }
                }
            }
            ) as u64);
            k += 1;
        }
        w(sched, 40, 0, size);
    }
    let graph = ip(s(sched, 13, 0));

    if gr(graph, 0, 0) < size {
        let nodes = heap(2, mul(size, 8 as u64), 0);
        ensure(nodes != 0, 16);
        let leafs = heap(2, mul(size, 8 as u64), 0);

        if leafs == 0 {
            drop(2, nodes);
            fail(17);
            return;
        }
        drop(2, ip(gr(graph, 3, 0)));
        drop(2, ip(gr(graph, 4, 0)));
        gw(graph, 3, 0, nodes as u64);
        gw(graph, 4, 0, leafs as u64);
        gw(graph, 0, 0, size);
    }
}

fn append(sched: i64, graph: i64, tensor: i64, id: i64, leaf: bool) {
    let n = gr(graph, if leaf {
        2
    } else {
        1
    }
    , 0);
    ensure(n < gr(graph, 0, 0) && n < s(sched, 40, 0), 18);
    word_put(ip(s(sched, if leaf {
        10
    } else {
        9
    }
    , 0)), n, id);
    ptr_put(ip(gr(graph, if leaf {
        4
    } else {
        3
    }
    , 0)), n, tensor as u64);
    gw(graph, if leaf {
        2
    } else {
        1
    }
    , 0, n + 1);
}

fn split_graph(sched: i64, graph: i64) {
    mark(52);
    deps_free(sched);
    candidates_free(sched);
    w(sched, 15, 0, 0 as u64);
    w(sched, 22, 0, 0 as u64);
    w(sched, 0, 0, 0 as u64);
    let old_ctx = ip(s(sched, 24, 0));
    w(sched, 24, 0, 0 as u64);
    component(5, old_ctx, 0, 0, 0 as u64, 0 as u64);
    w(sched, 24, 0, component(6, ip(s(sched, 33, 0)), 0, 0, s(sched, 34, 0), 0 as u64));
    ensure(s(sched, 24, 0) != 0, 19);
    gw(graph, 8, 0, component(7, 0, 0, 0, 0 as u64, 0 as u64));
    var i: i64 = 0;

    while i < ip(gr(graph, 2, 0)) {
        let leaf = ip(gr(graph, 6, i));

        if bid(sched, leaf) < 0 {
            set_bid(sched, leaf, current(sched, leaf));
        }
        i += 1;
    }
    i = 0;

    while i < ip(gr(graph, 1, 0)) {
        let node = ip(gr(graph, 5, i));

        if bid(sched, node) < 0 {
            set_bid(sched, node, current(sched, node));
        }
        i += 1;
    }
    var pass: i64 = 0;

    while pass < 4 {
        var cur: i64 = -1;
        let up = pass == 1 || pass == 3;
        i = if up {
            ip(gr(graph, 1, 0)) - 1
        } else {
            0
        }
        ;

        while i >= 0 && i < ip(gr(graph, 1, 0)) {
            let node = ip(gr(graph, 5, i));

            if !view_op(ip(t(node, 1, 0))) {
                let id = bid(sched, node);

                if id >= 0 {
                    cur = if pass < 2 && id == ip(s(sched, 2, 0)) - 1 {
                        -1
                    } else {
                        id
                    }
                    ;
                } else {
                    if cur >= 0 {
                        set_supported(sched, node, cur);
                    }
                }
            }
            i += if up {
                -1
            } else {
                1
            }
            ;
        }
        pass += 1;
    }
    i = 0;

    while i < ip(gr(graph, 1, 0)) {
        let node = ip(gr(graph, 5, i));

        if view_op(ip(t(node, 1, 0))) {
            i += 1;
            continue;
        }
        var id = bid(sched, node);

        if id < 0 {
            var best: i64 = -1;
            var b: i64 = 0;

            while b < ip(s(sched, 2, 0)) {
                if supports(sched, b, node) {
                    var n: i64 = 0;
                    var j: i64 = 0;

                    while (j as u64) < constant(4) {
                        let src = ip(t(node, 4, j));

                        if src != 0 && (bid(sched, src) >= 0 || bid(sched, ip(t(src, 3, 0))) >= 0) && compatible(sched, src, b) {
                            n += 1;
                        }
                        j += 1;
                    }

                    if n > best {
                        best = n;
                        set_bid(sched, node, b);
                    }
                }
                b += 1;
            }
        } else {
            var b: i64 = 0;

            while b < id {
                if s(sched, 4, b) == s(sched, 4, id) && supports(sched, b, node) {
                    var good = true;
                    var j: i64 = 0;

                    while (j as u64) < constant(4) {
                        let src = ip(t(node, 4, j));

                        if src != 0 && !compatible(sched, src, b) {
                            good = false;
                            break;
                        }
                        j += 1;
                    }

                    if good {
                        set_bid(sched, node, b);
                        break;
                    }
                }
                b += 1;
            }
        }
        i += 1;
    }
    i = 0;

    while i < ip(gr(graph, 1, 0)) {
        let node = ip(gr(graph, 5, i));
        let view = ip(t(node, 3, 0));

        if view != 0 && bid(sched, node) < 0 {
            set_bid(sched, node, bid(sched, view));
        }
        var j: i64 = 0;

        while (j as u64) < constant(4) {
            let src = ip(t(node, 4, j));

            if src != 0 && bid(sched, src) < 0 {
                let v = ip(t(src, 3, 0));
                set_bid(sched, src, if v != 0 {
                    bid(sched, v)
                } else {
                    bid(sched, node)
                }
                );
            }
            j += 1;
        }
        var b: i64 = 0;

        while b < ip(s(sched, 2, 0)) && bid(sched, node) < 0 {
            set_supported(sched, node, b);
            b += 1;
        }
        ensure(bid(sched, node) >= 0, 20);
        i += 1;
    }
    var isplit: i64 = 0;
    var p = split(sched, 0);
    i = 0;

    while i < ip(gr(graph, 1, 0)) {
        let node = ip(gr(graph, 5, i));

        if !view_op(ip(t(node, 1, 0))) {
            sw(p, 0, bid(sched, node) as !u64);
            break;
        }
        i += 1;
    }
    sw(p, 1, 0 as u64);
    sw(p, 4, 0 as u64);
    var cur = ip(sp(p, 0));

    while i < ip(gr(graph, 1, 0)) {
        let node = ip(gr(graph, 5, i));

        if view_op(ip(t(node, 1, 0))) {
            i += 1;
            continue;
        }
        let id = bid(sched, node);
        ensure(id >= 0, 21);
        var fresh = false;

        if id == cur && sp(p, 4) > 0 {
            var j: i64 = 0;

            while (j as u64) < constant(4) {
                let src = ip(t(node, 4, j));

                if src != 0 {
                    let buf = ip(t(src, 5, 0));

                    if buf != 0 && get(buf, 4, 1, 0) == constant(5) {
                        let sid = bid(sched, src);

                        if sid != cur && !compatible(sched, src, cur) {
                            fresh = true;
                            break;
                        }
                    }
                }
                j += 1;
            }
        }

        if id != cur || fresh {
            sw(p, 2, i as u64);
            isplit += 1;
            grow_splits(sched, isplit);
            p = split(sched, isplit);
            sw(p, 0, id as u64);
            sw(p, 1, i as u64);
            sw(p, 4, 0 as u64);
            cur = id;
        }
        var j: i64 = 0;

        while (j as u64) < constant(4) {
            let src = ip(t(node, 4, j));

            if src != 0 {
                let key = slot(sched, src);
                let sid = word(ip(s(sched, 7, 0)), key);
                ensure(sid >= 0, 22);

                if sid != cur && !compatible(sched, src, cur) {
                    if copy_get(sched, key, cur, 0) == 0 {
                        var c: i64 = 0;

                        while c < ip(s(sched, 17, 0)) {
                            let copy = duplicate(sched, src, cur, c);

                            if s(sched, 17, 0) > 1 {
                                component(8, copy, 0, 0, 0 as u64, 0 as u64);
                                component(9, copy, 0, 0, 0 as u64, 0 as u64);
                            }
                            copy_put(sched, key, cur, c, copy);
                            c += 1;
                        }
                        let n = sp(p, 4);

                        if n >= sp(p, 5) {
                            grow_inputs(p, false);
                        }
                        ptr_put(ip(sp(p, 3)), n, src as u64);
                        sw(p, 4, n + 1);
                    }
                    put(node, 3, 4, j, copy_get(sched, key, cur, ip(s(sched, 18, 0))) as u64);
                }
            }
            j += 1;
        }
        i += 1;
    }
    sw(p, 2, gr(graph, 1, 0));
    w(sched, 15, 0, (isplit + 1) as u64);

    if s(sched, 36, 0) != 0 {
        debug(0, sched, graph);
    }

    if s(sched, 17, 0) > 1 {
        i = 0;

        while i < ip(gr(graph, 2, 0)) {
            let leaf = ip(gr(graph, 6, i));

            if (t(leaf, 2, 0) & flag_input()) != 0 {
                let key = slot(sched, leaf);
                let id = bid(sched, leaf);
                ensure(id >= 0, 23);

                if copy_get(sched, key, id, 0) == 0 {
                    var c: i64 = 0;

                    while c < ip(s(sched, 17, 0)) {
                        let copy = if c == ip(s(sched, 18, 0)) {
                            leaf
                        } else {
                            duplicate(sched, leaf, id, c)
                        }
                        ;
                        component(8, copy, 0, 0, 0 as u64, 0 as u64);
                        component(9, copy, 0, 0, 0 as u64, 0 as u64);
                        copy_put(sched, key, id, c, copy);
                        c += 1;
                    }
                }
                let n = s(sched, 22, 0);

                if n >= s(sched, 23, 0) {
                    grow_inputs(sched, true);
                }
                ptr_put(ip(s(sched, 21, 0)), n, leaf as u64);
                w(sched, 22, 0, n + 1);
            }
            i += 1;
        }
    }
    let old = s(sched, 9, 0);
    w(sched, 9, 0, s(sched, 11, 0));
    w(sched, 11, 0, old);
    let oldleaf = s(sched, 10, 0);
    w(sched, 10, 0, s(sched, 12, 0));
    w(sched, 12, 0, oldleaf);
    let deps = deps_new(sched, gr(graph, 1, 0));
    i = 0;

    while i < ip(s(sched, 15, 0)) {
        let p = split(sched, i);
        let g = ip(sp(p, 6));
        graph_view(g, graph, ip(sp(p, 1)), ip(sp(p, 2)));
        optimize(backend(sched, ip(sp(p, 0))), g, deps);
        i += 1;
    }
    var ndep: u64 = 0;
    var key: u64 = 0;
    let counts = ip(get(deps, 5, 9, 0));

    while key < get(deps, 5, 4, 0) {
        let n = ptr(counts, key);
        ndep = add(ndep, (n + constant(4) - 1) / constant(4));
        key += 1;
    }
    var inputs = s(sched, 22, 0);
    i = 0;

    while i < ip(s(sched, 15, 0)) {
        inputs = add(inputs, sp(split(sched, i), 4));
        i += 1;
    }
    let orig = if gr(graph, 1, 0) > gr(graph, 2, 0) {
        gr(graph, 1, 0)
    } else {
        gr(graph, 2, 0)
    }
    ;
    let needed = add(add(orig, mul(mul(inputs, 2 as u64), s(sched, 17, 0))), ndep);
    w(sched, 38, 0, s(sched, 37, 0));
    w(sched, 37, 0, needed);
    ensure_graph_arrays(sched, needed);
    let metadata = mul(add(inputs - s(sched, 22, 0), ndep), component(10, 0, 0, 0, 0 as u64, 0 as u64));
    let used = component(11, ip(s(sched, 24, 0)), 0, 0, 0 as u64, 0 as u64);
    ensure(used <= s(sched, 34, 0) && metadata <= s(sched, 34, 0) - used, 25);
    let copygraph = ip(s(sched, 13, 0));
    gw(copygraph, 1, 0, 0 as u64);
    gw(copygraph, 2, 0, 0 as u64);
    var added: u64 = 0;
    i = 0;

    while i < ip(s(sched, 15, 0)) {
        let p = split(sched, i);
        let id = ip(sp(p, 0));
        var j: i64 = 0;

        while j < ip(sp(p, 4)) {
            let input = ip(ptr(ip(sp(p, 3)), j as u64));
            let key = slot(sched, input);
            let copy = copy_get(sched, key, id, ip(s(sched, 18, 0)));
            let dep = ip(component(12, ip(s(sched, 24, 0)), input, 0, 0 as u64, 0 as u64));
            put(dep, 3, 4, 0, input as u64);
            append(sched, copygraph, dep, word(ip(s(sched, 7, 0)), key), false);
            append(sched, copygraph, copy, id, false);
            j += 1;
        }
        j = ip(sp(p, 1));

        while j < ip(sp(p, 2)) {
            let node = ip(gr(graph, 5, j));
            append(sched, copygraph, node, bid(sched, node), false);
            let pairs = ip(get(deps, 5, 1, 0));
            var chain = dep_head(deps, node);

            while chain != 0 {
                let dep = ip(component(12, ip(s(sched, 24, 0)), ip(get(pairs, 6, 0, (chain - 1) as i64)), 0, 0 as u64, 0 as u64));
                var a: u64 = 0;

                while a < constant(4) && chain != 0 {
                    put(dep, 3, 4, a as i64, get(pairs, 6, 0, (chain - 1) as i64));
                    chain = get(pairs, 6, 2, (chain - 1) as i64);
                    a += 1;
                }
                append(sched, copygraph, dep, id, false);
                added += 1;
            }
            j += 1;
        }
        i += 1;
    }
    ensure(added == ndep, 26);

    if s(sched, 17, 0) > 1 {
        i = 0;

        while i < ip(s(sched, 22, 0)) {
            let input = ip(ptr(ip(s(sched, 21, 0)), i as u64));
            let key = slot(sched, input);
            let id = bid(sched, input);
            var c: i64 = 0;

            while c < ip(s(sched, 17, 0)) {
                append(sched, copygraph, copy_get(sched, key, id, c), id, true);
                c += 1;
            }
            i += 1;
        }
        i = 0;

        while i < ip(s(sched, 15, 0)) {
            let p = split(sched, i);
            let id = ip(sp(p, 0));
            var j: i64 = 0;

            while j < ip(sp(p, 4)) {
                let input = ip(ptr(ip(sp(p, 3)), j as u64));
                let key = slot(sched, input);
                var c: i64 = 0;

                while c < ip(s(sched, 17, 0)) {
                    append(sched, copygraph, copy_get(sched, key, id, c), id, true);
                    c += 1;
                }
                j += 1;
            }
            i += 1;
        }
    }
    i = 0;

    while i < ip(gr(graph, 2, 0)) {
        let leaf = ip(gr(graph, 6, i));
        append(sched, copygraph, leaf, bid(sched, leaf), true);
        i += 1;
    }
    i = 0;

    while i < ip(s(sched, 15, 0)) {
        gw(ip(sp(split(sched, i), 6)), 8, 0, component(7, 0, 0, 0, 0 as u64, 0 as u64));
        i += 1;
    }
    deps_free(sched);
}

fn candidate_add(sched: i64, b: i64, src: i64, dst: i64) {
    let n = s(sched, 43, 0);
    let cap = s(sched, 44, 0);
    let old = ip(s(sched, 42, 0));

    if n == cap {
        let count = if cap == 0 {
            16 as u64
        } else {
            mul(cap, 2 as u64)
        }
        ;
        let next = heap(6, count, 0);
        ensure(next != 0, 27);

        if n > 0 {
            mem(next, old, mul(n, constant(3)), 0);
        }
        drop(6, old);
        w(sched, 42, 0, next as u64);
        w(sched, 44, 0, count);
    }
    let array = ip(s(sched, 42, 0));
    put(array, 7, 0, n as i64, b as u64);
    put(array, 7, 1, n as i64, src as u64);
    put(array, 7, 2, n as i64, dst as u64);
    w(sched, 43, 0, n + 1);
}

fn prepare_expert(sched: i64) -> bool {
    if env(2) != 0 {
        debug(1, sched, 0);
    }

    if s(sched, 27, 0) == 0 || s(sched, 25, 0) != 0 || s(sched, 29, 0) == 0 || s(sched, 17, 0) != 1 {
        return false;
    }
    candidates_free(sched);
    var b: i64 = 0;

    while b < ip(s(sched, 2, 0)) {
        w(sched, 28, b, expert_proc(backend(sched, b)));
        w(sched, 45, b, 0 as u64);
        w(sched, 46, b, 0 as u64);
        w(sched, 47, b, 0 as u64);
        b += 1;
    }
    var i: i64 = 0;

    while i < ip(s(sched, 15, 0)) {
        let p = split(sched, i);
        let id = ip(sp(p, 0));
        let graph = ip(sp(p, 6));

        if s(sched, 28, id) != 0 && gr(graph, 1, 0) > 0 {
            let node = ip(gr(graph, 5, 0));
            let ids = ip(t(node, 4, 2));

            if ip(t(node, 1, 0)) == op_mmid() && ids != 0 && t(ids, 7, 1) >= 32 {
                var j: i64 = 0;

                while j < ip(sp(p, 4)) {
                    let src = ip(ptr(ip(sp(p, 3)), j as u64));
                    let dst = copy_get(sched, slot(sched, src), id, ip(s(sched, 18, 0)));
                    let buf = ip(t(src, 5, 0));

                    if dst == ip(t(node, 4, 0)) && buf != 0 && get(buf, 4, 1, 0) == constant(5) && component(2, buf, 0, 0, 0 as u64, 0 as u64) != 0 {
                        w(sched, 48, 0, 0 as u64);
                        let value = record(sched, 1, id);

                        if expert(sched, id, constant(21) as i64, src, dst, value) != 0 {
                            let size = get(value, 8, 0, 0);

                            if size > s(sched, 45, id) {
                                w(sched, 45, id, size);
                            }
                            candidate_add(sched, id, src, dst);
                        }
                    }
                    j += 1;
                }
            }
        }
        i += 1;
    }

    if env(2) != 0 {
        debug(2, sched, 0);
    }
    b = 0;

    while b < ip(s(sched, 2, 0)) {
        if s(sched, 28, b) != 0 {
            w(sched, 46, b, expert(sched, b, constant(22) as i64, 0, 0, record(sched, 2, b)));
        }
        b += 1;
    }
    var changed = false;
    i = 0;
    let candidates = ip(s(sched, 42, 0));

    while i < ip(s(sched, 43, 0)) {
        let id = ip(get(candidates, 7, 0, i));

        if s(sched, 46, id) != 0 {
            let bound = expert(
                sched,
                id,
                constant(23) as i64,
                ip(get(candidates, 7, 1, i)),
                ip(get(candidates, 7, 2, i)),
                record(sched, 3, id));
            ensure(bound != 0, 28);
            changed = true;
        }
        i += 1;
    }
    candidates_free(sched);
    return changed;
}

fn alloc_splits(sched: i64, expert_changed: bool) -> bool {
    let graph = ip(s(sched, 13, 0));
    var changed = false;
    var i: u64 = 0;

    while i < gr(graph, 1, 0) {
        let a = word(ip(s(sched, 9, 0)), i);
        let b = word(ip(s(sched, 11, 0)), i);
        backend(sched, a);
        backend(sched, b);

        if a != b && s(sched, 4, a) != s(sched, 4, b) {
            changed = true;
            break;
        }
        i += 1;
    }

    if !changed {
        i = 0;

        while i < gr(graph, 2, 0) {
            let a = word(ip(s(sched, 10, 0)), i);
            let b = word(ip(s(sched, 12, 0)), i);
            backend(sched, a);
            backend(sched, b);

            if a != b && s(sched, 4, a) != s(sched, 4, b) {
                changed = true;
                break;
            }
            i += 1;
        }
    }

    if expert_changed || changed || component(13, ip(s(sched, 5, 0)), graph, 0, 0 as u64, 0 as u64) == 0 {
        let resized = s(sched, 37, 0) != s(sched, 38, 0);

        if s(sched, 31, 0) != 0 {
            callback(2, sched, graph, if changed {
                1
            } else {
                0
            }
            , if resized {
                1
            } else {
                0
            }
            , 0);
        }
        let dbg = ip(s(sched, 39, 0));

        if dbg > 0 {
            ensure(! ( (!changed && !resized) || dbg > 1), 29);
        }
        i = 0;

        while i < s(sched, 2, 0) {
            component(14, backend(sched, i as i64), 0, 0, 0 as u64, 0 as u64);
            i += 1;
        }

        if component(15, ip(s(sched, 5, 0)), graph, ip(s(sched, 9, 0)), s(sched, 10, 0), 0 as u64) == 0 {
            return false;
        }

        if component(13, ip(s(sched, 5, 0)), graph, 0, 0 as u64, 0 as u64) == 0 {
            return false;
        }
    }
    return true;
}

fn host_weight(tensor: i64) -> bool {
    let buf = ip(t(tensor, 5, 0));
    return buf != 0 && get(buf, 4, 1, 0) == constant(5) && component(2, buf, 0, 0, 0 as u64, 0 as u64) != 0;
}

fn event(sched: i64, b: i64) -> i64 {
    backend(sched, b);
    return ip(s(sched, 20, b * constant(7) as i64 + ip(s(sched, 18, 0))));
}

fn get_tensor_backend(sched: i64, tensor: i64) -> i64 {
    let id = bid(sched, tensor);
    return if id < 0 {
        0
    } else {
        backend(sched, id)
    }
    ;
}

fn copy_input(sched: i64, p: i64, input: i64) {
    let id = ip(sp(p, 0));
    let dst_backend = backend(sched, id);
    let src_backend = get_tensor_backend(sched, input);
    let dst = copy_get(sched, slot(sched, input), id, ip(s(sched, 18, 0)));
    let ev = event(sched, id);

    if (t(input, 2, 0) & flag_input()) != 0 {
        if ev != 0 {
            component(16, ev, 0, 0, 0 as u64, 0 as u64);
        } else {
            component(14, dst_backend, 0, 0, 0 as u64, 0 as u64);
        }
        component(17, input, dst, 0, 0 as u64, 0 as u64);
        return;
    }

    if s(
        sched,
        28,
        id) != 0 && s(sched,
        29,
        0) != 0 && host_weight(input) && expert(sched,
        id,
        constant(24) as i64,
        input,
        dst,
        0) != 0 {
        let copied = callback(1, sched, dst_backend, input, dst, ip(sp(p, 6)));
        expert(sched, id, if copied != 0 {
            constant(25) as i64
        } else {
            constant(26) as i64
        }
        , input, dst, 0);

        if copied != 0 {
            return;
        }
    }

    if ev != 0 {
        component(18, dst_backend, ev, 0, 0 as u64, 0 as u64);
    } else {
        component(14, dst_backend, 0, 0, 0 as u64, 0 as u64);
    }

    if s(sched, 29, 0) != 0 && host_weight(input) && callback(1, sched, dst_backend, input, dst, ip(sp(p, 6))) != 0 {
        return;
    }

    if async_present(dst_backend) == 0 || try_async(src_backend, dst_backend, input, dst) == 0 {
        component(14, src_backend, 0, 0, 0 as u64, 0 as u64);

        if ev != 0 {
            component(16, ev, 0, 0, 0 as u64, 0 as u64);
        } else {
            component(14, dst_backend, 0, 0, 0 as u64, 0 as u64);
        }
        component(17, input, dst, 0, 0 as u64, 0 as u64);
    }
}

fn compute(sched: i64, workspace: i64) -> i64 {
    ensure(sched != 0, 30);
    var prev: i64 = -1;
    var i: i64 = 0;

    while i < ip(s(sched, 15, 0)) {
        let p = split(sched, i);
        let id = ip(sp(p, 0));
        let b = backend(sched, id);
        let graph = ip(sp(p, 6));

        if sp(p, 4) == 0 && prev >= 0 && prev != id {
            let ev = event(sched, prev);

            if ev != 0 {
                component(16, ev, 0, 0, 0 as u64, 0 as u64);
            } else {
                component(14, backend(sched, prev), 0, 0, 0 as u64, 0 as u64);
            }
        }
        var pass: i64 = 0;

        while pass < 2 {
            var j: u64 = 0;

            while j < sp(p, 4) {
                let input = ip(ptr(ip(sp(p, 3)), j));

                if host_weight(input) == (pass == 1) {
                    copy_input(sched, p, input);
                }
                j += 1;
            }
            pass += 1;
        }

        if s(sched, 25, 0) == 0 {
            let status = ip(component(19, b, graph, 0, 0 as u64, 0 as u64));

            if status != (constant(10) as i64) {
                return status;
            }
        } else {
            var lo: i64 = 0;

            while lo < ip(gr(graph, 1, 0)) {
                var tensor = ip(gr(graph, 5, lo));
                var need = callback(0, sched, tensor, 1, 0, 0) != 0;
                var hi = lo;

                while !need && hi < ip(gr(graph, 1, 0)) - 1 {
                    hi += 1;
                    tensor = ip(gr(graph, 5, hi));
                    need = callback(0, sched, tensor, 1, 0, 0) != 0;
                }
                graph_view(workspace, graph, lo, hi + 1);
                let status = ip(component(19, b, workspace, 0, 0 as u64, 0 as u64));

                if status != (constant(10) as i64) {
                    return status;
                }
                component(14, b, 0, 0, 0 as u64, 0 as u64);

                if need && callback(0, sched, tensor, 0, 0, 0) == 0 {
                    break;
                }
                lo = hi + 1;
            }
        }
        let ev = event(sched, id);

        if ev != 0 {
            component(20, ev, b, 0, 0 as u64, 0 as u64);
        }
        prev = id;
        i += 1;
    }
    return constant(10) as i64;
}

fn synchronize(sched: i64) {
    ensure(sched != 0, 31);
    var i: i64 = 0;

    while i < ip(s(sched, 2, 0)) {
        component(14, backend(sched, i), 0, 0, 0 as u64, 0 as u64);
        i += 1;
    }

    if s(sched, 1, 0) == 0 {
        w(sched, 19, 0, 0 as u64);
    }
}

fn reset(sched: i64) {
    ensure(sched != 0, 32);
    deps_free(sched);
    candidates_free(sched);

    if s(sched, 0, 0) == 0 {
        component(21, ip(s(sched, 6, 0)), 0, 0, 0 as u64, 0 as u64);
        let n = get(ip(s(sched, 6, 0)), 9, 0, 0);
        mem(ip(s(sched, 7, 0)), 0, mul(n, 4 as u64), 255);
        mem(ip(s(sched, 8, 0)), 0, mul(mul(mul(n, s(sched, 2, 0)), s(sched, 17, 0)), 8 as u64), 1);
        w(sched, 0, 0, 1 as u64);
    }
    w(sched, 1, 0, 0 as u64);
}

fn destroy(sched: i64) {
    if sched == 0 {
        return;
    }
    var b: i64 = 0;

    while b < ip(s(sched, 2, 0)) {
        var c: i64 = 0;

        while c < ip(s(sched, 17, 0)) {
            component(22, ip(s(sched, 20, b * constant(7) as i64 + c)), 0, 0, 0 as u64, 0 as u64);
            c += 1;
        }
        b += 1;
    }
    component(23, ip(s(sched, 5, 0)), 0, 0, 0 as u64, 0 as u64);
    component(5, ip(s(sched, 24, 0)), 0, 0, 0 as u64, 0 as u64);
    deps_free(sched);
    candidates_free(sched);
    let h = ip(s(sched, 6, 0));
    drop(2, ip(get(h, 9, 1, 0)));
    drop(2, ip(get(h, 9, 2, 0)));
    var i: i64 = 0;

    while i < ip(s(sched, 16, 0)) {
        drop(2, ip(sp(split(sched, i), 3)));
        i += 1;
    }
    drop(1, ip(s(sched, 14, 0)));
    var k: i64 = 7;

    while k <= 12 {
        drop(2, ip(s(sched, k, 0)));
        k += 1;
    }
    drop(2, ip(s(sched, 21, 0)));
    drop(2, ip(s(sched, 33, 0)));
    let graph = ip(s(sched, 13, 0));
    drop(2, ip(gr(graph, 3, 0)));
    drop(2, ip(gr(graph, 4, 0)));
    drop(0, sched);
}

fn create(backends: i64, bufts: i64, n: i64, hint: u64, parallel: bool, offload: bool) -> i64 {
    ensure(n > 0 && (n as u64) <= constant(8) && backends != 0, 33);
    ensure(component(24, ip(ptr(backends, (n - 1) as u64)), 0, 0, 0 as u64, 0 as u64) == constant(11), 34);
    ensure(hint <= 2147483647, 35);
    let hash = component(25, 0, 0, 0, hint, 0 as u64);
    let copies = if parallel {
        constant(7)
    } else {
        1 as u64
    }
    ;
    let ids = add(hint, mul(mul(hint, constant(6)), 2 as u64));
    let context_bytes = add(
        mul(mul(mul(hint, constant(6)), 2 as u64), constant(12)),
        component(26, 0, 0, 0, hint, 0 as u64));
    let sched = heap(0, 1 as u64, 1);

    if sched == 0 {
        return 0;
    }
    w(sched, 2, 0, n as u64);
    w(sched, 17, 0, copies);
    w(sched, 40, 0, ids);
    let hp = ip(s(sched, 6, 0));
    put(hp, 9, 0, 0, hash);
    let keys = heap(2, mul(hash, 8 as u64), 0);

    if keys == 0 {
        destroy(sched);
        return 0;
    }
    put(hp, 9, 2, 0, keys as u64);
    let used = heap(2, mul(hash / 32 + if hash % 32 != 0 {
        1 as u64
    } else {
        0 as u64
    }
    , 4 as u64), 1);

    if used == 0 {
        destroy(sched);
        return 0;
    }
    put(hp, 9, 1, 0, used as u64);
    let ids_ptr = heap(2, mul(hash, 4 as u64), 0);

    if ids_ptr == 0 {
        destroy(sched);
        return 0;
    }
    w(sched, 7, 0, ids_ptr as u64);
    let cp = heap(2, mul(mul(mul(hash, n as u64), copies), 8 as u64), 0);

    if cp == 0 {
        destroy(sched);
        return 0;
    }
    w(sched, 8, 0, cp as u64);
    var k: i64 = 9;

    while k <= 12 {
        let array = heap(2, mul(ids, 4 as u64), 1);

        if array == 0 {
            destroy(sched);
            return 0;
        }
        w(sched, k, 0, array as u64);
        k += 1;
    }
    let ctx = heap(2, context_bytes, 0);

    if ctx == 0 {
        destroy(sched);
        return 0;
    }
    w(sched, 33, 0, ctx as u64);
    w(sched, 34, 0, context_bytes);
    let splits = heap(1, 16 as u64, 1);

    if splits == 0 {
        destroy(sched);
        return 0;
    }
    w(sched, 14, 0, splits as u64);
    w(sched, 16, 0, 16 as u64);
    let inputs = heap(2, constant(6) * 8, 1);

    if inputs == 0 {
        destroy(sched);
        return 0;
    }
    w(sched, 21, 0, inputs as u64);
    w(sched, 23, 0, constant(6));
    let dbg = env(0);
    w(sched, 36, 0, (if dbg != 0 {
        atoi(dbg)
    } else {
        0
    }
    ) as !u64);
    let dr = env(1);
    w(sched, 39, 0, (if dr != 0 {
        atoi(dr)
    } else {
        constant(13) as i64
    }
    ) as !u64);
    var b: i64 = 0;

    while b < n {
        let be = ip(ptr(backends, b as u64));
        w(sched, 3, b, be as u64);
        let kind = if bufts != 0 {
            ptr(bufts, b as u64)
        } else {
            component(27, be, 0, 0, 0 as u64, 0 as u64)
        }
        ;
        w(sched, 4, b, kind);

        if !supports_kind(sched, b, ip(kind)) {
            destroy(sched);
            fail(36);
            return 0;
        }

        if parallel {
            var c: i64 = 0;

            while (c as u64) < copies {
                w(sched, 20, b * constant(7) as i64 + c, component(28, be, 0, 0, 0 as u64, 0 as u64));
                c += 1;
            }
        }
        b += 1;
    }
    w(sched, 5, 0, component(29, record(sched, 4, 0), 0, 0, n as u64, 0 as u64));
    w(sched, 35, 0, if offload {
        1 as u64
    } else {
        0 as u64
    }
    );
    reset(sched);
    return sched;
}

fn validate_graph(sched: i64, graph: i64) {
    ensure(sched != 0 && graph != 0, 37);
    ensure(get(ip(s(sched, 6, 0)), 9, 0, 0) >= add(gr(graph, 1, 0), gr(graph, 2, 0)), 38);
}

fn reserve(sched: i64, graph: i64, sizes: i64) -> bool {
    validate_graph(sched, graph);

    if sizes != 0 {
        reset(sched);
    }
    synchronize(sched);
    split_graph(sched, graph);

    if sizes != 0 {
        component(30, ip(s(sched, 5, 0)), ip(s(sched, 13, 0)), ip(s(sched, 9, 0)), s(sched, 10, 0), sizes as u64);
        return true;
    }

    if component(15, ip(s(sched, 5, 0)), ip(s(sched, 13, 0)), ip(s(sched, 9, 0)), s(sched, 10, 0), 0 as u64) == 0 {
        return false;
    }
    reset(sched);
    return true;
}

fn allocate(sched: i64, graph: i64) -> bool {
    validate_graph(sched, graph);
    ensure(s(sched, 1, 0) == 0, 39);
    w(sched, 18, 0, s(sched, 19, 0));
    w(sched, 19, 0, (s(sched, 19, 0) + 1) % s(sched, 17, 0));
    split_graph(sched, graph);
    let changed = prepare_expert(sched);

    if !alloc_splits(sched, changed) {
        return false;
    }
    w(sched, 1, 0, 1 as u64);
    return true;
}

fn compute_async(sched: i64, graph: i64, workspace: i64) -> i64 {
    ensure(sched != 0, 40);

    if s(sched, 0, 0) == 0 && s(sched, 1, 0) == 0 {
        reset(sched);
    }

    if s(sched, 1, 0) == 0 {
        if !allocate(sched, graph) {
            return constant(14) as !i64;
        }
    }
    return compute(sched, workspace);
}

fn dispatch(p: i64) -> u64 {
    let mode = ip(arg(p, 0));
    let a = ip(arg(p, 1));
    let b = ip(arg(p, 2));
    let c = ip(arg(p, 3));
    let d = arg(p, 4);
    let e = arg(p, 5);

    if mode == 0 {
        return create(a, b, c, d, e != 0, arg(p, 6) != 0) as u64;
    }

    if mode == 1 {
        destroy(a);
        return 0 as u64;
    }

    if mode == 2 {
        reset(a);
        return 0 as u64;
    }

    if mode == 3 {
        return if reserve(a, b, c) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 4 {
        return if reserve(a, b, 0) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 5 {
        return if allocate(a, b) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 6 || mode == 7 {
        let result = compute_async(a, b, record(p, 5, 0));

        if mode == 6 {
            synchronize(a);
        }
        return result as !u64;
    }

    if mode == 8 {
        synchronize(a);
        return 0 as u64;
    }

    if mode == 9 {
        split_graph(a, b);
        return 0 as u64;
    }

    if mode == 10 {
        return backend_index(a, b) as !u64;
    }

    if mode == 11 {
        return from_buffer(a, b, c) as !u64;
    }

    if mode == 12 {
        return current(a, b) as !u64;
    }

    if mode == 13 {
        return if compatible(a, b, c) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 14 {
        if supports(a, c, b) {
            word_put(ip(d), 0, c);
        }
        return 0 as u64;
    }

    if mode == 15 {
        grow_inputs(a, false);
        return 0 as u64;
    }

    if mode == 16 {
        grow_inputs(a, true);
        return 0 as u64;
    }

    if mode == 17 {
        return if prepare_expert(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 18 {
        return if alloc_splits(a, b != 0) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 19 {
        return if host_weight(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 20 {
        copy_input(a, b, c);
        return 0 as u64;
    }

    if mode == 21 {
        return compute(a, record(p, 5, 0)) as !u64;
    }

    if mode == 22 || mode == 24 || mode == 25 {
        ensure(a != 0, 41);
        let k = if mode == 22 {
            25
        } else {
            if mode == 24 {
                29
            } else {
                31
            }
        }
        ;
        w(a, k, 0, b as u64);
        w(a, k + 1, 0, c as u64);
        return 0 as u64;
    }

    if mode == 23 {
        ensure(a != 0 && s(a, 0, 0) != 0 && s(a, 1, 0) == 0, 42);
        w(a, 27, 0, b as u64);
        return 0 as u64;
    }

    if mode == 26 {
        component(31, ip(s(a, 5, 0)), b, 0, 0 as u64, 0 as u64);
        return 0 as u64;
    }

    if mode >= 27 && mode <= 29 {
        ensure(a != 0, 43);
        return s(a, if mode == 27 {
            15
        } else {
            if mode == 28 {
                17
            } else {
                2
            }
        }
        , 0);
    }

    if mode == 30 {
        return backend(a, b) as u64;
    }

    if mode == 31 || mode == 32 {
        ensure(a != 0, 44);
        let id = backend_index(a, b);
        backend(a, id);
        return if mode == 31 {
            s(a, 4, id)
        } else {
            component(32, ip(s(a, 5, 0)), 0, 0, id as u64, 0 as u64)
        }
        ;
    }

    if mode == 33 {
        ensure(a != 0, 45);
        let id = backend_index(a, c);
        backend(a, id);
        set_bid(a, b, id);
        w(a, 0, 0, 0 as u64);
        return 0 as u64;
    }

    if mode == 34 {
        return get_tensor_backend(a, b) as u64;
    }

    if mode == 35 {
        return if view_op(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 36 {
        let tensor = ip(component(4, a, b, 0, 0 as u64, 0 as u64));
        var i: i64 = 0;

        while i < 4 {
            put(tensor, 3, 6, i, t(b, 6, i));
            i += 1;
        }
        return tensor as u64;
    }

    if mode == 50 {
        dep_add(a, b, c);
        return 0 as u64;
    }
    fail(99);
    return 0 as u64;
}

fn main() {
    print(dispatch(0));
}

