// Backend ownership/control is Goose;imports are typed storage or provider/heap effects.
extern "fg162_mark" fn mark(i: i64);
extern "fg162_arg" fn arg(p: i64, i: i64) -> u64;
extern "fg162_arg_put" fn arg_put(p: i64, i: i64, v: u64);
extern "fg162_constant" fn constant(i: i64) -> u64;
extern "fg162_heap" fn heap(kind: i64, n: u64) -> i64;
extern "fg162_drop" fn drop(kind: i64, p: i64);
extern "fg162_get" fn get(p: i64, kind: i64, k: i64, i: i64) -> u64;
extern "fg162_put" fn put(p: i64, kind: i64, k: i64, i: i64, v: u64);
extern "fg162_callback" fn callback(p: i64, kind: i64, k: i64) -> u64;
extern "fg162_buft_call" fn buft_call(which: i64, p: i64, t: i64, n: u64) -> u64;
extern "fg162_buffer_call" fn buffer_call(which: i64, p: i64, t: i64, v: u64) -> u64;
extern "fg162_iface_copy" fn iface_copy(p: i64, iface: i64);
extern "fg162_workspace" fn workspace(p: i64, which: i64) -> i64;
extern "fg162_multi_iface" fn multi_iface() -> i64;
extern "fg162_multi_free_marker" fn multi_marker() -> u64;
extern "fg162_nbytes" fn nbytes(t: i64) -> u64;
extern "fg162_quantized" fn quantized(t: i64) -> i64;
extern "fg162_is_meta" fn is_meta(p: i64) -> i64;
extern "fg162_meta_usage" fn meta_usage(p: i64, usage: i64);
extern "fg162_tallocr_init" fn tallocr_init(p: i64, b: i64);
extern "fg162_tallocr_step" fn tallocr_step(p: i64, t: i64) -> i64;
extern "fg162_snapshot" fn snapshot(p: i64, i: i64, t: i64, restore: i64);
extern "fg162_ptr" fn ptr(p: i64, i: i64) -> u64;
extern "fg162_ptr_put" fn ptr_put(p: i64, i: i64, v: u64);
extern "fg162_fail" fn fail(code: i64);

fn empty_op(op: i64) -> bool {
    if op == 0 {
        return true;
    }

    if op == 36 {
        return true;
    }

    if op == 39 {
        return true;
    }

    if op == 37 {
        return true;
    }

    if op == 38 {
        return true;
    }
    return false;
}

fn expand_op(op: i64) -> bool {
    if op == 74 {
        return true;
    }

    if op == 29 {
        return true;
    }

    if op == 30 {
        return true;
    }

    if op == 16 {
        return true;
    }

    if op == 69 {
        return true;
    }

    if op == 70 {
        return true;
    }

    if op == 18 {
        return true;
    }
    return false;
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

fn align_ok(a: u64) -> bool {
    return a > 0 && (a & (a - 1)) == 0;
}

fn multi(b: i64) -> bool {
    ensure(b != 0, 2);
    return callback(b, 1, 0) == multi_marker();
}

fn buft_alignment(b: i64) -> u64 {
    ensure(b != 0 && callback(b, 0, 3) != 0, 3);
    return buft_call(3, b, 0, 0 as u64);
}

fn buft_max(b: i64) -> u64 {
    ensure(b != 0, 4);
    return if callback(b, 0, 4) != 0 {
        buft_call(4, b, 0, 0 as u64)
    } else {
        U()
    }
    ;
}

fn alloc_size(b: i64, t: i64, scratch: i64) -> u64 {
    plan_fail(b != 0 && t != 0, scratch, 5);

    if callback(b, 0, 5) == 0 {
        return nbytes(t);
    }
    let size = buft_call(5, b, t, 0 as u64);
    let bytes = nbytes(t);
    plan_fail(size >= bytes, scratch, 6);
    plan_fail(
        size <= bytes || empty_op(ip(get(t, 2, 2, 0))) || quantized(t) != 0 || expand_op(ip(get(t, 2, 2, 0))),
        scratch,
        7);
    return size;
}

fn fill(b: i64, kind: i64, iface: i64, ctx: i64, size: u64) {
    iface_copy(b, iface);
    put(b, 0, 0, 0, kind as u64);
    put(b, 0, 1, 0, ctx as u64);
    put(b, 0, 2, 0, size);
    put(b, 0, 3, 0, constant(0));
}

fn create(kind: i64, iface: i64, ctx: i64, size: u64, p: i64) -> i64 {
    let b = heap(0, 1 as u64);

    if b != 0 {
        fill(b, kind, iface, ctx, size);
        return b;
    }
    // Release transferred provider context before returning a failed descriptor construction.
    let temp = workspace(p, 0);
    fill(temp, kind, iface, ctx, size);

    if callback(temp, 1, 0) != 0 {
        buffer_call(0, temp, 0, 0 as u64);
    }
    return 0;
}

fn free_buffer(b: i64) {
    if b == 0 {
        return;
    }

    if callback(b, 1, 0) != 0 {
        buffer_call(0, b, 0, 0 as u64);
    }
    drop(0, b);
}

fn buffer_base(b: i64) -> u64 {
    ensure(b != 0, 9);

    if is_meta(b) == 0 && get(b, 0, 2, 0) == 0 {
        return 0 as u64;
    }

    if callback(b, 1, 1) == 0 {
        return 0 as u64;
    }
    let v = buffer_call(1, b, 0, 0 as u64);
    ensure(v != 0, 10);
    return v;
}

fn buffer_init_tensor(b: i64, t: i64) -> i64 {
    ensure(b != 0, 11);
    return if callback(b, 1, 2) != 0 {
        ip(buffer_call(2, b, t, 0 as u64))
    } else {
        constant(1) as i64
    }
    ;
}

fn clear_buffer(b: i64, value: u64) {
    ensure(b != 0, 12);

    if get(b, 0, 2, 0) == 0 {
        return;
    }
    ensure(callback(b, 1, 3) != 0, 13);
    buffer_call(3, b, 0, value);
}
recursive fn set_usage(b: i64, usage: i64) {
    ensure(b != 0, 14);
    put(b, 0, 3, 0, usage as !u64);

    if multi(b) {
        multi_usage(b, usage);
    } else {
        if is_meta(b) != 0 {
            meta_usage(b, usage);
        }
    }
}

fn reset_buffer(b: i64) {
    ensure(b != 0, 15);

    if callback(b, 1, 4) != 0 {
        buffer_call(4, b, 0, 0 as u64);
    }
}

fn allocate_buffer(kind: i64, size: u64, p: i64) -> i64 {
    ensure(kind != 0, 16);

    if size == 0 {
        return create(kind, workspace(p, 2), 0, 0 as u64, p);
    }
    ensure(callback(kind, 0, 1) != 0, 17);
    return ip(buft_call(1, kind, 0, size));
}

fn release_plan(p: i64) {
    drop(3, p);
}

fn plan_fail(ok: bool, plan: i64, code: i64) {
    if !ok {
        release_plan(plan);
        fail(code);
    }
}

fn build_plan(kind: i64, tensors: i64, n: i64, p: i64) -> i64 {
    ensure(kind != 0 && n >= 0 && n <= 2147483647 && (n == 0 || tensors != 0), 18);
    mark(40);
    arg_put(p, 8, 0 as u64);
    arg_put(p, 9, 0 as u64);
    let align = buft_alignment(kind);
    let maxsize = buft_max(kind);
    ensure(align_ok(align), 19);

    if n == 0 {
        return 0;
    }
    ensure( (n as u64) <= U() / constant(2), 20);
    let items = heap(3, n as u64);

    if items == 0 {
        return 0;
    }
    var cur: u64 = 0;
    var total: u64 = 0;
    var first: i64 = 0;
    var count: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let t = ip(ptr(tensors, i));
        plan_fail(t != 0, items, 21);
        var bytes: u64 = 0;

        if get(t, 2, 1, 0) == 0 && get(t, 2, 3, 0) == 0 {
            let size = alloc_size(kind, t, items);
            plan_fail(size <= U() - (align - 1), items, 22);
            bytes = (size + align - 1) / align * align;
        }
        plan_fail(cur <= U() - bytes, items, 23);

        if cur > 0 && cur + bytes > maxsize {
            put(items, 3, 0, count, cur);
            put(items, 3, 1, count, first as u64);
            put(items, 3, 2, count, i as u64);
            plan_fail(total <= U() - cur, items, 24);
            total += cur;
            count += 1;
            cur = bytes;
            first = i;
        } else {
            cur += bytes;
        }
        i += 1;
    }

    if cur > 0 {
        put(items, 3, 0, count, cur);
        put(items, 3, 1, count, first as u64);
        put(items, 3, 2, count, n as u64);
        plan_fail(total <= U() - cur, items, 25);
        total += cur;
        count += 1;
    }
    arg_put(p, 8, count as u64);
    arg_put(p, 9, total);
    return items;
}

fn make_multi(buffers: i64, n: i64) -> i64 {
    mark(41);

    if n <= 0 || buffers == 0 {
        return 0;
    }

    if (n as u64) > (U() / 2) / 8 {
        return 0;
    }
    var total: u64 = 0;
    var i: i64 = 0;

    while i < n {
        let b = ip(ptr(buffers, i));

        if b == 0 {
            return 0;
        }
        var j: i64 = 0;

        while j < i {
            if ptr(buffers, j) == (b as u64) {
                return 0;
            }
            j += 1;
        }
        let size = get(b, 0, 2, 0);

        if total > U() - size {
            return 0;
        }
        total += size;
        i += 1;
    }
    ensure( (n as u64) <= U() / 8, 26);
    let ctx = heap(1, 1 as u64);

    if ctx == 0 {
        return 0;
    }
    let copy = heap(2, n as u64);

    if copy == 0 {
        drop(1, ctx);
        return 0;
    }
    i = 0;

    while i < n {
        ptr_put(copy, i, ptr(buffers, i));
        i += 1;
    }
    put(ctx, 1, 0, 0, copy as u64);
    put(ctx, 1, 1, 0, n as u64);
    let b = heap(0, 1 as u64);

    if b == 0 {
        drop(2, copy);
        drop(1, ctx);
        return 0;
    }
    let kind = ip(get(ip(ptr(buffers, 0)), 0, 0, 0));
    fill(b, kind, multi_iface(), ctx, total);
    return b;
}

fn multi_free(b: i64) {
    ensure(b != 0, 27);
    let ctx = ip(get(b, 0, 1, 0));
    let buffers = ip(get(ctx, 1, 0, 0));
    let n = ip(get(ctx, 1, 1, 0));
    var i: i64 = 0;

    while i < n {
        free_buffer(ip(ptr(buffers, i)));
        i += 1;
    }
    drop(2, buffers);
    drop(1, ctx);
}

fn multi_clear(b: i64, value: u64) {
    ensure(b != 0, 28);
    let ctx = ip(get(b, 0, 1, 0));
    let buffers = ip(get(ctx, 1, 0, 0));
    var i: i64 = 0;

    while (i as u64) < get(ctx, 1, 1, 0) {
        clear_buffer(ip(ptr(buffers, i)), value);
        i += 1;
    }
}
recursive fn multi_usage(b: i64, usage: i64) {
    ensure(multi(b), 29);
    let ctx = ip(get(b, 0, 1, 0));
    let buffers = ip(get(ctx, 1, 0, 0));
    var i: i64 = 0;

    while (i as u64) < get(ctx, 1, 1, 0) {
        set_usage(ip(ptr(buffers, i)), usage);
        i += 1;
    }
}

fn view_init(t: i64) -> i64 {
    ensure(t != 0 && get(t, 2, 0, 0) == 0 && get(t, 2, 3, 0) != 0, 30);
    let src = ip(get(t, 2, 3, 0));
    let b = get(src, 2, 0, 0);
    let data = get(src, 2, 1, 0);
    ensure(b != 0 && data != 0, 31);
    let address = add(data, get(t, 2, 4, 0));
    put(t, 2, 0, 0, b);
    put(t, 2, 1, 0, address);
    return buffer_init_tensor(ip(b), t);
}

fn tensor_alloc(b: i64, t: i64, address: u64) -> i64 {
    ensure(b != 0 && t != 0 && get(t, 2, 0, 0) == 0 && get(t, 2, 1, 0) == 0 && get(t, 2, 3, 0) == 0, 32);
    let base = buffer_base(b);
    ensure(address >= base, 33);

    if is_meta(b) == 0 {
        let size = alloc_size(ip(get(b, 0, 0, 0)), t, 0);
        let cap = get(b, 0, 2, 0);
        let off = address - base;
        ensure(off <= cap && size <= cap - off, 34);
        buffer_base(b);
    }
    put(t, 2, 0, 0, b as u64);
    put(t, 2, 1, 0, address);
    return buffer_init_tensor(b, t);
}

fn bulk_default(kind: i64, tensors: i64, n: i64, p: i64, size_only: bool) -> u64 {
    let plan = build_plan(kind, tensors, n, p);

    if plan == 0 {
        return 0 as u64;
    }
    let count = ip(arg(p, 8));
    let total = arg(p, 9);

    if size_only {
        release_plan(plan);
        return total;
    }

    if count == 0 {
        release_plan(plan);
        return 0 as u64;
    }
    let buffers = heap(2, count as u64);

    if buffers == 0 {
        release_plan(plan);
        return 0 as u64;
    }
    ensure( (n as u64) <= U() / constant(3), 35);
    let saved = heap(4, n as u64);

    if saved == 0 {
        drop(2, buffers);
        release_plan(plan);
        return 0 as u64;
    }
    var i: i64 = 0;

    while i < n {
        snapshot(saved, i, ip(ptr(tensors, i)), 0);
        i += 1;
    }
    var done: i64 = 0;
    var failed = false;

    while done < count {
        let b = allocate_buffer(kind, get(plan, 3, 0, done), p);

        if b == 0 {
            failed = true;
            break;
        }
        tallocr_init(p, b);
        var j = ip(get(plan, 3, 1, done));
        let end = ip(get(plan, 3, 2, done));
        var init_failed = false;

        while j < end {
            let t = ip(ptr(tensors, j));
            var status = constant(1) as i64;

            if get(t, 2, 1, 0) == 0 {
                if get(t, 2, 3, 0) == 0 {
                    status = tallocr_step(p, t);
                } else {
                    if get(t, 2, 0, 0) == 0 {
                        status = view_init(t);
                    }
                }
            } else {
                if get(t, 2, 3, 0) != 0 && get(t, 2, 0, 0) == 0 {
                    status = view_init(t);
                }
            }

            if status != (constant(1) as i64) {
                init_failed = true;
                break;
            }
            j += 1;
        }

        if init_failed {
            var k: i64 = 0;

            while k < done {
                free_buffer(ip(ptr(buffers, k)));
                k += 1;
            }
            free_buffer(b);
            done = 0;
            failed = true;
            break;
        }
        ptr_put(buffers, done, b as u64);
        done += 1;
    }
    var result: i64 = 0;

    if !failed {
        if count == 1 {
            result = ip(ptr(buffers, 0));
        } else {
            result = make_multi(buffers, count);

            if result == 0 {
                failed = true;
            }
        }
    }

    if failed {
        mark(42);
        var k: i64 = 0;

        while k < done {
            free_buffer(ip(ptr(buffers, k)));
            k += 1;
        }
        i = 0;

        while i < n {
            snapshot(saved, i, ip(ptr(tensors, i)), 1);
            i += 1;
        }
    }
    drop(4, saved);
    drop(2, buffers);
    release_plan(plan);
    return result as u64;
}

fn dispatch(p: i64) -> u64 {
    let mode = ip(arg(p, 0));
    let a = ip(arg(p, 1));
    let b = ip(arg(p, 2));
    let c = arg(p, 3);
    let d = arg(p, 4);

    if mode == 0 {
        return create(a, b, ip(c), d, p) as u64;
    }

    if mode == 1 {
        free_buffer(a);
        return 0 as u64;
    }

    if mode == 2 || mode == 7 || mode == 8 {
        ensure(a != 0, 36);
        return get(a, 0, if mode == 2 {
            2
        } else {
            if mode == 7 {
                3
            } else {
                0
            }
        }
        , 0);
    }

    if mode == 3 {
        return buffer_base(a);
    }

    if mode == 4 {
        return buffer_init_tensor(a, b) as !u64;
    }

    if mode == 5 {
        clear_buffer(a, arg(p, 2));
        return 0 as u64;
    }

    if mode == 6 {
        set_usage(a, b);
        return 0 as u64;
    }

    if mode == 9 {
        reset_buffer(a);
        return 0 as u64;
    }

    if mode == 10 {
        ensure(a != 0 && b != 0, 37);
        let view = ip(get(b, 2, 3, 0));
        let dstbuf = ip(get(if view != 0 {
            view
        } else {
            b
        }
        , 2, 0, 0));
        ensure(dstbuf != 0, 38);
        return if callback(dstbuf, 1, 5) != 0 {
            buffer_call(5, dstbuf, a, b as u64)
        } else {
            0 as u64
        }
        ;
    }

    if mode == 11 {
        ensure(a != 0 && callback(a, 0, 0) != 0, 39);
        return buft_call(0, a, 0, 0 as u64);
    }

    if mode == 12 {
        return allocate_buffer(a, arg(p, 2), p) as u64;
    }

    if mode == 13 {
        return buft_alignment(a);
    }

    if mode == 14 {
        return buft_max(a);
    }

    if mode == 15 {
        return alloc_size(a, b, 0);
    }

    if mode == 16 || mode == 17 {
        ensure(a != 0, 40);
        let which = if mode == 16 {
            2
        } else {
            6
        }
        ;

        if callback(a, 0, which) != 0 {
            mark(if mode == 16 {
                43
            } else {
                44
            }
            );
            return buft_call(which, a, b, c);
        }
        return bulk_default(a, b, ip(c), p, mode == 17);
    }

    if mode == 18 {
        ensure(a != 0, 41);
        return if callback(a, 0, 7) != 0 {
            buft_call(7, a, 0, 0 as u64)
        } else {
            0 as u64
        }
        ;
    }

    if mode == 19 {
        ensure(a != 0, 42);
        return get(a, 4, 0, 0);
    }

    if mode == 20 || mode == 21 {
        return bulk_default(a, b, ip(c), p, mode == 21);
    }

    if mode == 22 {
        return make_multi(a, b) as u64;
    }

    if mode == 23 {
        multi_free(a);
        return 0 as u64;
    }

    if mode == 24 {
        multi_clear(a, arg(p, 2));
        return 0 as u64;
    }

    if mode == 25 {
        multi_usage(a, b);
        return 0 as u64;
    }

    if mode == 26 {
        return if multi(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }

    if mode == 27 {
        return view_init(a) as !u64;
    }

    if mode == 28 {
        return tensor_alloc(a, b, c) as !u64;
    }

    if mode == 29 {
        return if expand_op(a) {
            1 as u64
        } else {
            0 as u64
        }
        ;
    }
    fail(99);
    return 0 as u64;
}

fn main() {
    print(dispatch(0));
}

