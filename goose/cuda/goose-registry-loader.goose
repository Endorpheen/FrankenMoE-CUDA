// Registry and Linux loader policy;imports are typed field/raw OS effects.
extern "fg165_arg" fn arg(p: i64, k: i64) -> u64;
extern "fg165_state" fn state() -> i64;
extern "fg165_get" fn get(p: i64, k: i64) -> u64;
extern "fg165_put" fn put(p: i64, k: i64, v: u64);
extern "fg165_ptr" fn ptr(p: i64, i: u64) -> u64;
extern "fg165_ptr_put" fn ptr_put(p: i64, i: u64, v: u64);
extern "fg165_alloc" fn alloc(n: u64) -> i64;
extern "fg165_free" fn free(p: i64);
extern "fg165_copy" fn raw_copy(dst: i64, src: i64, n: u64);
extern "fg165_byte" fn byte(p: i64, i: u64) -> u64;
extern "fg165_byte_put" fn byte_put(p: i64, i: u64, v: u64);
extern "fg165_lower" fn lower(c: u64) -> u64;
extern "fg165_builtin" fn builtin(i: i64) -> i64;
extern "fg165_literal" fn literal(i: i64) -> i64;
extern "fg165_effect" fn effect(op: i64, a: i64, b: i64, c: i64) -> u64;
extern "fg165_env" fn env(k: i64) -> i64;
extern "fg165_mark" fn mark(k: i64);
extern "fg165_log" fn log(k: i64, a: i64, b: i64);
extern "fg165_fail" fn fail(k: i64);

fn ip(v: u64) -> i64 {
    return v as !i64;
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn ensure(b: bool, k: i64) {
    if !b {
        fail(k);
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

fn length(p: i64) -> u64 {
    ensure(p != 0, 3);
    var n: u64 = 0;

    while byte(p, n) != 0 {
        n = add(n, 1 as u64);
    }
    return n;
}

fn equal(a: i64, b: i64) -> bool {
    ensure(a != 0 && b != 0, 4);
    var i: u64 = 0;

    while byte(a, i) != 0 && byte(b, i) != 0 {
        if lower(byte(a, i)) != lower(byte(b, i)) {
            return false;
        }
        i += 1;
    }
    return byte(a, i) == byte(b, i);
}

fn clone(p: i64) -> i64 {
    let n = add(length(p), 1 as u64);
    let q = alloc(n);
    ensure(q != 0, 5);
    raw_copy(q, p, n);
    return q;
}

fn join2(a: i64, b: i64, slash: bool) -> i64 {
    let x = length(a);
    let y = length(b);
    let sep = slash && x != 0 && byte(a, x - 1) != 47;
    let n = add(add(x, y), if sep {
        2 as u64
    } else {
        1 as u64
    }
    );
    let p = alloc(n);
    ensure(p != 0, 6);
    raw_copy(p, a, x);
    var at = x;

    if sep {
        byte_put(p, at, 47 as u64);
        at += 1;
    }
    raw_copy(p + at as i64, b, y + 1);
    return p;
}

fn grow(s: i64, devices: bool, need: u64) {
    let k = if devices {
        3
    } else {
        0
    }
    ;
    let cap = get(s, k + 2);

    if need <= cap {
        return;
    }
    let n = if cap == 0 {
        8 as u64
    } else {
        mul(cap, 2 as u64)
    }
    ;
    let count = if n < need {
        need
    } else {
        n
    }
    ;
    let stride = if devices {
        8 as u64
    } else {
        16 as u64
    }
    ;
    let q = alloc(mul(count, stride));
    ensure(q != 0, 7);
    let old = ip(get(s, k));
    let used = get(s, k + 1);

    if used != 0 {
        raw_copy(q, old, mul(used, stride));
    }
    free(old);
    put(s, k, q as u64);
    put(s, k + 2, count);
}

fn find_reg(s: i64, reg: i64) -> i64 {
    var i: u64 = 0;

    while i < get(s, 1) {
        if ptr(ip(get(s, 0)), i * 2) == (reg as u64) {
            return i as i64;
        }
        i += 1;
    }
    return -1;
}

fn add_device(s: i64, dev: i64) {
    ensure(dev != 0, 8);
    var i: u64 = 0;

    while i < get(s, 4) {
        if ptr(ip(get(s, 3)), i) == (dev as u64) {
            return;
        }
        i += 1;
    }
    grow(s, true, add(get(s, 4), 1 as u64));
    ptr_put(ip(get(s, 3)), get(s, 4), dev as u64);
    put(s, 4, get(s, 4) + 1);
}

fn register(s: i64, reg: i64, handle: i64) {
    if reg == 0 {
        if handle != 0 {
            effect(12, handle, 0, 0);
        }
        return;
    }

    if find_reg(s, reg) >= 0 {
        if handle != 0 {
            effect(12, handle, 0, 0);
        }
        return;
    }
    grow(s, false, add(get(s, 1), 1 as u64));
    let entries = ip(get(s, 0));
    let at = get(s, 1);
    ptr_put(entries, at * 2, reg as u64);
    ptr_put(entries, at * 2 + 1, handle as u64);
    put(s, 1, at + 1);
    var i: u64 = 0;

    while i < effect(1, reg, 0, 0) {
        add_device(s, ip(effect(2, reg, i as i64, 0)));
        i += 1;
    }
}

fn initialize(s: i64) {
    if get(s, 6) != 0 {
        return;
    }
    put(s, 6, 1 as u64);
    var i: i64 = 0;

    while i < 19 {
        if i != 3 || env(1) == 0 {
            let reg = builtin(i);
            register(s, reg, 0);
        }
        i += 1;
    }
    put(s, 6, 2 as u64);
    mark(40);
}

fn reg_at(s: i64, i: u64) -> i64 {
    ensure(i < get(s, 1), 9);
    return ip(ptr(ip(get(s, 0)), i * 2));
}

fn dev_at(s: i64, i: u64) -> i64 {
    ensure(i < get(s, 4), 10);
    return ip(ptr(ip(get(s, 3)), i));
}

fn by_name(s: i64, name: i64, device: bool) -> i64 {
    ensure(name != 0, 11);
    var i: u64 = 0;

    while i < get(s, if device {
        4
    } else {
        1
    }
    ) {
        let p = if device {
            dev_at(s, i)
        } else {
            reg_at(s, i)
        }
        ;

        if equal(ip(effect(if device {
            3
        } else {
            0
        }
        , p, 0, 0)), name) {
            return p;
        }
        i += 1;
    }
    return 0;
}

fn by_type(s: i64, kind: i64) -> i64 {
    var i: u64 = 0;

    while i < get(s, 4) {
        let p = dev_at(s, i);

        if effect(4, p, 0, 0) == (kind as u64) {
            return p;
        }
        i += 1;
    }
    return 0;
}

fn unload(s: i64, reg: i64) {
    let at = find_reg(s, reg);

    if at < 0 {
        return;
    }
    let entries = ip(get(s, 0));
    let handle = ip(ptr(entries, (at as u64) * 2 + 1));
    let devices = ip(get(s, 3));
    var dst: u64 = 0;
    var i: u64 = 0;

    while i < get(s, 4) {
        let dev = ip(ptr(devices, i));

        if effect(5, dev, 0, 0) != (reg as u64) {
            ptr_put(devices, dst, dev as u64);
            dst += 1;
        }
        i += 1;
    }
    put(s, 4, dst);
    i = at as u64;

    while i + 1 < get(s, 1) {
        ptr_put(entries, i * 2, ptr(entries, (i + 1) * 2));
        ptr_put(entries, i * 2 + 1, ptr(entries, (i + 1) * 2 + 1));
        i += 1;
    }
    put(s, 1, get(s, 1) - 1);

    if handle != 0 {
        effect(12, handle, 0, 0);
    }
    mark(41);
}

fn load(s: i64, path: i64, silent: bool) -> i64 {
    ensure(path != 0, 12);
    mark(42);
    let h = ip(effect(10, path, 0, 0));

    if h == 0 {
        if !silent {
            log(0, path, 0);
        }
        return 0;
    }
    let score = ip(effect(11, h, literal(0), 0));

    if score != 0 && effect(13, score, 0, 0) == 0 {
        if !silent {
            log(1, path, 0);
        }
        effect(12, h, 0, 0);
        return 0;
    }
    let init = ip(effect(11, h, literal(1), 0));

    if init == 0 {
        if !silent {
            log(2, path, 0);
        }
        effect(12, h, 0, 0);
        return 0;
    }
    let reg = ip(effect(14, init, 0, 0));

    if reg == 0 || effect(6, reg, 0, 0) != effect(7, 0, 0, 0) {
        if !silent {
            log(3, path, reg);
        }
        effect(12, h, 0, 0);
        return 0;
    }
    log(4, path, reg);
    register(s, reg, h);
    return reg;
}

fn executable_dir() -> i64 {
    var cap: u64 = 1024;

    while true {
        let p = alloc(add(cap, 1 as u64));
        ensure(p != 0, 13);
        let n = ip(effect(20, p, cap as i64, 0));

        if n < 0 {
            free(p);
            return clone(literal(2));
        }

        if (n as u64) < cap {
            byte_put(p, n as u64, 0 as u64);
            var at = n;

            while at > 0 {
                at -= 1;

                if byte(p, at as u64) == 47 {
                    byte_put(p, (at + 1) as u64, 0 as u64);
                    return p;
                }
            }
            return p;
        }
        free(p);
        cap = mul(cap, 2 as u64);
    }
    return 0;
}

fn cwd() -> i64 {
    var cap: u64 = 1024;

    while true {
        let p = alloc(cap);
        ensure(p != 0, 14);
        let r = ip(effect(21, p, cap as i64, 0));

        if r > 0 {
            return p;
        }
        free(p);

        if r == 0 {
            return 0;
        }
        cap = mul(cap, 2 as u64);
    }
    return 0;
}

fn matches(name: i64, prefix: i64) -> bool {
    let n = length(name);
    let p = length(prefix);

    if n < p + 3 {
        return false;
    }
    var i: u64 = 0;

    while i < p {
        if byte(name, i) != byte(prefix, i) {
            return false;
        }
        i += 1;
    }
    return byte(name, n - 3) == 46 && byte(name, n - 2) == 115 && byte(name, n - 1) == 111;
}

fn search_path(index: i64, user: i64, exe: i64, current: i64) -> i64 {
    if user != 0 {
        return if index == 0 {
            user
        } else {
            0
        }
        ;
    }
    let configured = literal(3);

    if configured != 0 {
        if index == 0 {
            return configured;
        }

        if index == 1 {
            return exe;
        }

        if index == 2 {
            return current;
        }
        return 0;
    }

    if index == 0 {
        return exe;
    }

    if index == 1 {
        return current;
    }
    return 0;
}

fn best(s: i64, name: i64, silent: bool, user: i64) -> i64 {
    mark(43);
    var exe: i64 = 0;
    var current: i64 = 0;

    if user == 0 {
        exe = executable_dir();
        current = cwd();
    }
    let base = join2(literal(4), name, false);
    let prefix = join2(base, literal(5), false);
    var score: i64 = 0;
    var chosen: i64 = 0;
    var i: i64 = 0;

    while i < 3 {
        let path = search_path(i, user, exe, current);

        if path != 0 {
            let dir = ip(effect(22, path, 0, 0));

            if dir != 0 {
                var entry = ip(effect(23, dir, 0, 0));

                while entry != 0 {
                    if matches(entry, prefix) {
                        let file = join2(path, entry, true);

                        if effect(25, file, 0, 0) != 0 {
                            let h = ip(effect(10, file, 0, 0));

                            if h != 0 {
                                let fnp = ip(effect(11, h, literal(0), 0));

                                if fnp != 0 {
                                    let value = ip(effect(13, fnp, 0, 0));

                                    if value > score {
                                        score = value;
                                        free(chosen);
                                        chosen = clone(file);
                                    }
                                } else {
                                    if !silent {
                                        log(5, file, 0);
                                    }
                                }
                                effect(12, h, 0, 0);
                            } else {
                                if !silent {
                                    log(0, file, 0);
                                }
                            }
                        }
                        free(file);
                    }
                    entry = ip(effect(23, dir, 0, 0));
                }
                effect(24, dir, 0, 0);
            }
        }
        i += 1;
    }
    var result: i64 = 0;

    if score == 0 {
        let filename = join2(base, literal(6), false);
        i = 0;
        var found = false;

        while i < 3 && !found {
            let dir = search_path(i, user, exe, current);

            if dir != 0 {
                let p = join2(dir, filename, true);

                if effect(26, p, 0, 0) != 0 {
                    result = load(s, p, silent);
                    found = true;
                }
                free(p);
            }
            i += 1;
        }
        free(filename);
    } else {
        result = load(s, chosen, silent);
    }
    free(chosen);
    free(prefix);
    free(base);
    free(exe);
    free(current);
    return result;
}

fn load_all(s: i64, dir: i64) {
    var i: i64 = 0;

    while i < 15 {
        best(s, literal(10 + i), true, dir);
        i += 1;
    }
    let extra = env(0);

    if extra != 0 {
        load(s, extra, false);
    }
}

fn destroy(s: i64) {
    free(ip(get(s, 0)));
    free(ip(get(s, 3)));
    put(s, 0, 0 as u64);
    put(s, 1, 0 as u64);
    put(s, 2, 0 as u64);
    put(s, 3, 0 as u64);
    put(s, 4, 0 as u64);
    put(s, 5, 0 as u64);
    put(s, 6, 0 as u64);
    mark(44);
}

fn dispatch(p: i64) -> u64 {
    let mode = ip(arg(p, 0));
    let a = ip(arg(p, 1));
    let b = ip(arg(p, 2));

    if mode == 30 {
        destroy(a);
        return 0 as u64;
    }
    let s = state();
    initialize(s);

    if mode == 0 {
        register(s, a, 0);
        return 0 as u64;
    }

    if mode == 1 {
        add_device(s, a);
        return 0 as u64;
    }

    if mode == 2 {
        return get(s, 1);
    }

    if mode == 3 {
        return reg_at(s, arg(p, 1)) as u64;
    }

    if mode == 4 {
        return by_name(s, a, false) as u64;
    }

    if mode == 5 {
        return get(s, 4);
    }

    if mode == 6 {
        return dev_at(s, arg(p, 1)) as u64;
    }

    if mode == 7 {
        return by_name(s, a, true) as u64;
    }

    if mode == 8 {
        return by_type(s, a) as u64;
    }

    if mode == 9 || mode == 10 {
        let d = if mode == 9 {
            by_name(s, a, true)
        } else {
            by_type(s, a)
        }
        ;
        return if d == 0 {
            0 as u64
        } else {
            effect(8, d, b, 0)
        }
        ;
    }

    if mode == 11 {
        var d = by_type(s, ip(effect(30, 0, 0, 0)));

        if d == 0 {
            d = by_type(s, ip(effect(31, 0, 0, 0)));
        }

        if d == 0 {
            d = by_type(s, ip(effect(32, 0, 0, 0)));
        }
        return if d == 0 {
            0 as u64
        } else {
            effect(8, d, 0, 0)
        }
        ;
    }

    if mode == 12 {
        return load(s, a, false) as u64;
    }

    if mode == 13 {
        unload(s, a);
        return 0 as u64;
    }

    if mode == 14 || mode == 15 {
        load_all(s, if mode == 14 {
            0
        } else {
            a
        }
        );
        return 0 as u64;
    }

    if mode == 20 {
        return best(s, a, b != 0, ip(arg(p, 3))) as u64;
    }
    fail(99);
    return 0 as u64;
}

fn main() {
    print(dispatch(0));
}

