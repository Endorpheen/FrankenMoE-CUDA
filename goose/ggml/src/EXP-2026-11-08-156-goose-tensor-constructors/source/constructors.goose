// Arena and tensor ownership policy. Every imported effect is typed and primitive.
extern "fg156_arg" fn arg(p: i64, i: i64) -> u64;
extern "fg156_dim" fn dim(p: i64, i: i64) -> i64;
extern "fg156_constant" fn constant(i: i64) -> u64;
extern "fg156_ctx" fn ctxget(p: i64, i: i64) -> u64;
extern "fg156_ctx_set" fn ctxset(p: i64, i: i64, v: u64);
extern "fg156_obj" fn objget(p: i64, i: i64) -> u64;
extern "fg156_obj_set" fn objset(p: i64, i: i64, v: u64);
extern "fg156_ptr_add" fn ptradd(p: i64, n: u64) -> i64;
extern "fg156_zero" fn zero(p: i64, n: u64);
extern "fg156_tget" fn tget(p: i64, k: i64, i: i64) -> u64;
extern "fg156_tset" fn tset(p: i64, k: i64, i: i64, v: u64);
extern "fg156_type_block" fn blocksize(t: i64) -> i64;
extern "fg156_type_size" fn typesize(t: i64) -> u64;
extern "fg156_span" fn span(t: i64) -> u64;
extern "fg156_elements" fn elements(t: i64) -> i64;
extern "fg156_contiguous" fn contiguous(t: i64) -> i64;
extern "fg156_name" fn name(t: i64, src: i64, style: i64);
extern "fg156_name_equal" fn name_equal(t: i64, s: i64) -> i64;
extern "fg156_store_index" fn store_index(p: i64, v: i64);
extern "fg156_warn_arena" fn warn_arena(code: i64, need: u64, cap: u64);
extern "fg156_fail" fn fail(code: i64);

fn ensure(ok: bool, code: i64) {
    if !ok {
        fail(code);
    }
}

fn umax() -> u64 {
    return 18446744073709551615 as u64;
}

fn add(a: u64, b: u64) -> u64 {
    ensure(b <= umax() - a, 1);
    return a + b;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= umax() / a, 2);
    return a * b;
}

fn ip(x: u64) -> i64 {
    return x as !i64;
}

fn ne(t: i64, i: i64) -> i64 {
    return tget(t, 0, i) as !i64;
}

fn nb(t: i64, i: i64) -> u64 {
    return tget(t, 1, i);
}

fn arena(ctx: i64, kind: i64, size: u64) -> i64 {
    let align = constant(0);
    ensure(size <= umax() - (align - 1), 3);
    let old = ip(ctxget(ctx, 3));
    var end: u64 = 0 as u64;

    if old != 0 {
        end = add(objget(old, 0), objget(old, 1));
    }
    let padded = (size + align - 1) / align * align;
    let os = constant(1);

    if padded > umax() - end {
        warn_arena(0, padded, ctxget(ctx, 0));
        return 0;
    }
    let partial = end + padded;

    if os > umax() - partial {
        warn_arena(0, padded, ctxget(ctx, 0));
        return 0;
    }
    let total = partial + os;

    if total > ctxget(ctx, 0) {
        warn_arena(1, total, ctxget(ctx, 0));
        return 0;
    }
    let obj = ptradd(ip(ctxget(ctx, 1)), end);
    zero(obj, os);
    objset(obj, 0, end + os);
    objset(obj, 1, padded);
    objset(obj, 2, 0 as u64);
    objset(obj, 3, kind as u64);

    if old != 0 {
        objset(old, 2, obj as u64);
    } else {
        ctxset(ctx, 2, obj as u64);
    }
    ctxset(ctx, 3, obj as u64);
    return obj;
}

fn tensor(ctx: i64, ty: i64, nd: i64, dims: i64, view: i64, off: u64) -> i64 {
    ensure(ty >= 0 && (ty as u64) < constant(3), 10);
    ensure(nd >= 1 && nd <= 4, 11);
    let bs = blocksize(ty);
    ensure(bs > 0, 12);
    var source = view;
    var offset = off;

    if source != 0 && tget(source, 2, 0) != 0 {
        offset = add(offset, tget(source, 3, 0));
        source = ip(tget(source, 2, 0));
    }
    var count = dim(dims, 0);
    ensure(count >= 0, 13);
    var i: i64 = 1;

    while i < nd {
        let n = dim(dims, i);
        ensure(n >= 0, 13);

        if n > 1 {
            ensure(9223372036854775807 / n > count, 14);
            count *= n;
        }
        i += 1;
    }
    // native ggml_new_tensor_impl does not require ne0 to be a multiple of the block size; it divides.
    // The extra ensure here rejected ggml_view_3d(ctx, q4_K[256,8,3], 128, ...) which native accepts,
    // so the check is removed to restore parity. Code 15 is left unused rather than renumbered.
    let n0 = dim(dims, 0);
    var bytes = mul(typesize(ty), n0 as u64) / (bs as u64);
    i = 1;

    while i < nd {
        bytes = mul(bytes, dim(dims, i) as u64);
        i += 1;
    }

    if source != 0 && bytes != 0 {
        ensure(add(bytes, offset) <= span(source), 16);
    }
    var data: i64 = 0;

    if source != 0 && tget(source, 4, 0) != 0 {
        data = ip(add(tget(source, 4, 0), offset));
    }
    var extra: u64 = 0 as u64;

    if source == 0 && ctxget(ctx, 4) == 0 {
        extra = bytes;
    }
    let size = add(constant(2), extra);
    let obj = arena(ctx, constant(4) as i64, size);
    ensure(obj != 0, 17);
    let result = ptradd(ip(ctxget(ctx, 1)), objget(obj, 0));
    zero(result, constant(2));
    tset(result, 5, 0, ty as u64);
    tset(result, 2, 0, source as u64);
    tset(result, 3, 0, offset);

    if extra > 0 {
        data = ptradd(result, constant(5));
    }
    tset(result, 4, 0, data as u64);
    i = 0;

    while i < 4 {
        var n: i64 = 1;

        if i < nd {
            n = dim(dims, i);
        }
        tset(result, 0, i, n as u64);
        i += 1;
    }
    tset(result, 1, 0, typesize(ty));
    tset(result, 1, 1, mul(typesize(ty), (n0 / bs) as u64));
    i = 2;

    while i < 4 {
        tset(result, 1, i, mul(nb(result, i - 1), ne(result, i - 1) as u64));
        i += 1;
    }
    ensure(ctxget(ctx, 5) < 2147483647, 18);
    ctxset(ctx, 5, ctxget(ctx, 5) + 1);
    return result;
}

fn copy_view(ctx: i64, src: i64) -> i64 {
    let result = tensor(ctx, ip(tget(src, 5, 0)), 4, ip(tget(src, 6, 0)), src, 0 as u64);
    name(result, src, 0);
    var i: i64 = 0;

    while i < 4 {
        tset(result, 1, i, nb(src, i));
        i += 1;
    }
    return result;
}

fn reshape(ctx: i64, src: i64, nd: i64, dims: i64) -> i64 {
    ensure(contiguous(src) != 0, 20);
    var count: i64 = 1;
    var i: i64 = 0;

    while i < nd {
        let n = dim(dims, i);
        ensure(n >= 0, 13);
        ensure(n == 0 || count <= 9223372036854775807 / n, 14);
        count *= n;
        i += 1;
    }
    ensure(elements(src) == count, 21);
    let result = tensor(ctx, ip(tget(src, 5, 0)), nd, dims, src, 0 as u64);
    name(result, src, 1);
    tset(result, 7, 0, constant(6));
    tset(result, 8, 0, src as u64);
    return result;
}

fn view_metadata(ctx: i64, src: i64, nd: i64, dims: i64, off: u64) -> i64 {
    let result = tensor(ctx, ip(tget(src, 5, 0)), nd, dims, src, off);
    name(result, src, 0);
    tset(result, 9, 0, off);
    tset(result, 7, 0, constant(7));
    tset(result, 8, 0, src as u64);
    return result;
}

fn view_tensor(p: i64) -> i64 {
    let ctx = ip(arg(p, 0));
    let src = ip(arg(p, 1));
    let nd = ip(arg(p, 2));
    let dims = ip(arg(p, 3));
    let off = arg(p, 4);
    var stride2 = arg(p, 6);
    var stride3 = arg(p, 7);

    if nd == 2 {
        stride2 = mul(arg(p, 5), dim(dims, 1) as u64);
        stride3 = stride2;
    }

    if nd == 3 {
        stride3 = mul(arg(p, 6), dim(dims, 2) as u64);
    }
    let result = view_metadata(ctx, src, nd, dims, off);

    if nd >= 2 {
        tset(result, 1, 1, arg(p, 5));
    }

    if nd == 2 {
        tset(result, 1, 2, stride2);
        tset(result, 1, 3, stride3);
    }

    if nd >= 3 {
        tset(result, 1, 2, stride2);
    }

    if nd == 3 {
        tset(result, 1, 3, stride3);
    }

    if nd == 4 {
        tset(result, 1, 3, stride3);
    }
    return result;
}

fn permute(p: i64) -> i64 {
    var i: i64 = 0;

    while i < 4 {
        let a = ip(arg(p, i + 2));
        ensure(a >= 0 && a < 4, 30);
        var j: i64 = 0;

        while j < i {
            ensure(arg(p, j + 2) != arg(p, i + 2), 31);
            j += 1;
        }
        i += 1;
    }
    let src = ip(arg(p, 1));
    let result = copy_view(ip(arg(p, 0)), src);
    name(result, src, 2);
    i = 0;

    while i < 4 {
        let axis = ip(arg(p, i + 2));
        tset(result, 0, axis, tget(src, 0, i));
        tset(result, 1, axis, nb(src, i));
        tset(result, 10, i, axis as u64);
        i += 1;
    }
    tset(result, 7, 0, constant(8));
    tset(result, 8, 0, src as u64);
    return result;
}

fn transpose(ctx: i64, src: i64) -> i64 {
    let result = copy_view(ctx, src);
    name(result, src, 3);
    tset(result, 0, 0, tget(src, 0, 1));
    tset(result, 0, 1, tget(src, 0, 0));
    tset(result, 1, 0, nb(src, 1));
    tset(result, 1, 1, nb(src, 0));
    tset(result, 7, 0, constant(9));
    tset(result, 8, 0, src as u64);
    return result;
}

fn find_from(ctx: i64, obj: i64, needle: i64) -> i64 {
    var cur = obj;

    while cur != 0 {
        if objget(cur, 3) == constant(4) {
            let t = ptradd(ip(ctxget(ctx, 1)), objget(cur, 0));

            if needle == 0 || name_equal(t, needle) != 0 {
                return t;
            }
        }
        cur = ip(objget(cur, 2));
    }
    return 0;
}

fn entry(mode: i64, p: i64) -> i64 {
    if mode == 0 {
        return arena(ip(arg(p, 0)), ip(arg(p, 1)), arg(p, 2));
    }

    if mode == 1 {
        return tensor(ip(arg(p, 0)), ip(arg(p, 1)), ip(arg(p, 2)), ip(arg(p, 3)), ip(arg(p, 4)), arg(p, 5));
    }

    if mode == 2 {
        let src = ip(arg(p, 1));
        return tensor(ip(arg(p, 0)), ip(tget(src, 5, 0)), 4, ip(tget(src, 6, 0)), 0, 0 as u64);
    }

    if mode == 3 {
        return copy_view(ip(arg(p, 0)), ip(arg(p, 1)));
    }

    if mode == 4 {
        return reshape(ip(arg(p, 0)), ip(arg(p, 1)), ip(arg(p, 2)), ip(arg(p, 3)));
    }

    if mode == 5 {
        return view_tensor(p);
    }

    if mode == 6 {
        return permute(p);
    }

    if mode == 7 {
        return transpose(ip(arg(p, 0)), ip(arg(p, 1)));
    }

    if mode == 8 {
        return find_from(ip(arg(p, 0)), ip(ctxget(ip(arg(p, 0)), 2)), 0);
    }

    if mode == 9 {
        let obj = ptradd(ip(arg(p, 1)), (0 as u64) - constant(1));
        return find_from(ip(arg(p, 0)), ip(objget(obj, 2)), 0);
    }

    if mode == 10 {
        return find_from(ip(arg(p, 0)), ip(ctxget(ip(arg(p, 0)), 2)), ip(arg(p, 1)));
    }

    if mode == 11 {
        let ctx = ip(arg(p, 0));
        let obj = arena(ctx, constant(10) as i64, arg(p, 1));
        ensure(obj != 0, 17);
        return ptradd(ip(ctxget(ctx, 1)), objget(obj, 0));
    }

    if mode == 12 {
        var obj = ip(ctxget(ip(arg(p, 0)), 2));
        var largest: u64 = 0 as u64;

        while obj != 0 {
            if objget(obj, 3) == constant(4) {
                let n = span(ptradd(ip(ctxget(ip(arg(p, 0)), 1)), objget(obj, 0)));

                if n > largest {
                    largest = n;
                }
            }
            obj = ip(objget(obj, 2));
        }
        return largest as !i64;
    }

    if mode == 14 {
        return view_metadata(ip(arg(p, 0)), ip(arg(p, 1)), ip(arg(p, 2)), ip(arg(p, 3)), arg(p, 4));
    }

    if mode == 13 {
        let src = ip(arg(p, 0));
        let index = ip(arg(p, 1));
        let n0 = ne(src, 0);
        let n1 = ne(src, 1);
        let n2 = ne(src, 2);
        ensure(n0 > 0 && n1 > 0 && n2 > 0, 40);
        let i3 = index / (n2 * n1 * n0);
        let i2 = (index - i3 * n2 * n1 * n0) / (n1 * n0);
        let i1 = (index - i3 * n2 * n1 * n0 - i2 * n1 * n0) / n0;
        let i0 = index - i3 * n2 * n1 * n0 - i2 * n1 * n0 - i1 * n0;
        store_index(ip(arg(p, 2)), i0);
        store_index(ip(arg(p, 3)), i1);
        store_index(ip(arg(p, 4)), i2);
        store_index(ip(arg(p, 5)), i3);
        return 0;
    }
    return 0;
}

fn main() {
    print(entry(99, 0));
}

