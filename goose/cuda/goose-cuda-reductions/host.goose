// Shared validation,dispatch and external-CUB temporary ownership in Goose.
extern "fg169_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg169_plan_ptr" fn plan_ptr(packet: i64) -> i64;
extern "fg169_hget" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg169_hput" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg169_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg169_pred" fn pred(k: i64, t: i64) -> bool;
extern "fg169_constant" fn constant(packet: i64, k: i64) -> i64;
extern "fg169_launch" fn launch(packet: i64);
extern "fg169_cub_query" fn cub_query(packet: i64) -> u64;
extern "fg169_pool_alloc" fn pool_alloc(packet: i64, bytes: u64) -> u64;
extern "fg169_cub_run" fn cub_run(packet: i64, temp: u64);
extern "fg169_pool_free" fn pool_free(packet: i64, temp: u64);
extern "fg169_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn add(a: u64, b: u64) -> u64 {
    ensure(a <= U() - b, 1);
    return a + b;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 2);
    return a * b;
}

fn tensor(p: i64, t: i64, which: i64, kind: i64) -> u64 {
    ensure(t != 0 && ti(t, 2, 0) == kind && ti(t, 1, 0) != 0, 3);
    let ptr = ti(t, 1, 0) as u64;
    put(p, 0, which, ptr);
    var total: u64 = 1;
    var span: u64 = 4;
    var i: i64 = 0;

    while i < 4 {
        let n = ti(t, 3, i);
        let nb = ti(t, 4, i);
        ensure(n > 0 && nb >= 0 && (nb as u64) % 4 == 0, 4);
        total = mul(total, n as u64);
        span = add(span, mul( (n - 1) as u64, nb as u64));
        put(p, 1, which * 4 + i, n as u64);
        put(p, 2, which * 4 + i, nb as u64);
        i += 1;
    }
    ensure(span <= 9223372036854775807 && ptr <= U() - span, 5);
    let declared = ti(t, 9, 0);
    ensure(declared > 0 && span <= declared as u64, 6);
    put(p, 3, 12 + which, span);
    return total;
}
// fg169r row-reductions host launch policy: pure host-side arithmetic, no FFI, no plan/packet access.
// Block-size selection shared by sumrows.cu and mean.cu (llama.cpp PR #15132 heuristic):
//   grid is one block per row (block_nums = nrows, zero dynamic shared bytes in the native launch);
//   if (nrows / nsm) < 2 -> 512 threads, to better hide latency on grids that underfill the device;
//   else                 -> 32 threads when ncols < 1024, else 128 threads for better scheduling.
// Only 32/128/512 are ever selected, matching the device module's supported block sizes.

fn rr_row_block_size(ncols: i64, nrows: i64, nsm: i64) -> i32 {
    if (nrows / nsm) < 2 {
        return 512;
    }

    if ncols < 1024 {
        return 32;
    }
    return 128;
}
// Shared F32 element count for the cross-warp reduction phase (native __shared__ float shared_vals[32];
// rr_block_sum touches only the first bs/32 entries). Parent allocates 32 elements = 128 bytes whenever
// bs > 32; for bs == 32 the cross-warp phase is absent and no shared storage is required.

fn rr_row_shared_floats(bs: i32) -> i32 {
    if bs > 32 {
        return 32;
    }
    return 0;
}
// Compile-time reachability stub with dummy scalars; never executed on the host.
// Argmax host launch policy from native ggml_cuda_argmax;pure Goose,no imports,no launch effects.
struct Launch {
    ok: bool, bs: i32, blocks: i64, shared: i64
}

fn minimum(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}
// Native:std::min<int64_t>(1024,(ne00+WARP_SIZE-1)/WARP_SIZE*WARP_SIZE);result is a multiple of 32,at least 32.

fn block_size(cols: i64) -> i32 {
    return (minimum(1024, ( (cols + 31) / 32) * 32)) as !i32;
}

fn grid(rows: i64) -> i64 {
    return rows;
}
// Device touches shared slots only when bs>32;256 bytes cover 32 float and 32 int slots.

fn shared_bytes(bs: i32) -> i64 {
    return if bs > 32 {
        256 as i64
    } else {
        0 as i64
    }
    ;
}
// Own validation bounds:positive sizes;argmax must fit I32;rows within CUDA grid.x limit;rows*cols then stays inside i64.

fn valid(cols: i64, rows: i64) -> bool {
    if cols <= 0 || rows <= 0 {
        return false;
    }

    if cols > 2147483647 || rows > 2147483647 {
        return false;
    }
    return true;
}
// Single query entry for the bridge:ok=false must reject before any launch effect.

fn policy(cols: i64, rows: i64) -> Launch {
    if !valid(cols, rows) {
        return Launch {
            false, 0, 0, 0
        }
        ;
    }
    let bs = block_size(cols);
    return Launch {
        true, bs, grid(rows), shared_bytes(bs)
    }
    ;
}
// Dummy retained entry;parent must not execute it.

fn run(packet: i64) {
    let mode = arg(packet, 0);
    let p = plan_ptr(packet);
    ensure(mode >= 0 && mode <= 5, 7);
    put(p, 3, 0, mode as u64);
    let f32 = constant(packet, 0);
    let i32 = constant(packet, 1);
    var cols: u64 = 0;
    var rows: u64 = 0;
    var xt: u64 = 0;

    if mode == 3 || mode == 5 {
        let x = arg(packet, 3) as u64;
        let d = arg(packet, 4) as u64;
        ensure(x != 0 && d != 0 && arg(packet, 5) > 0 && arg(packet, 6) > 0, 8);
        cols = arg(packet, 5) as u64;
        rows = arg(packet, 6) as u64;
        ensure(mode != 5 || rows == 1, 9);
        put(p, 0, 0, x);
        put(p, 0, 1, d);
        let xs = mul(mul(cols, rows), 4 as u64);
        let ds = mul(rows, 4 as u64);
        ensure(x <= U() - xs && d <= U() - ds, 10);
        put(p, 3, 12, xs);
        put(p, 3, 13, ds);
    } else {
        ensure(arg(packet, 1) != 0 && arg(packet, 2) != 0, 11);
        let d = arg(packet, 2);
        let x = ti(d, 0, 0);
        xt = tensor(p, x, 0, f32);
        let dt = tensor(p, d, 1, if mode == 2 {
            i32
        } else {
            f32
        }
        );
        ensure(pred(0, d), 12);
        cols = get(p, 1, 0);
        rows = mul(mul(get(p, 1, 1), get(p, 1, 2)), get(p, 1, 3));

        if mode == 4 {
            ensure(pred(2, x) && dt == 1, 13);
            cols = xt;
            rows = 1;
        } else {
            ensure(dt == rows, 14);

            if mode == 2 {
                ensure(pred(0, x), 15);
            } else {
                ensure(pred(1, x), 16);
                put(p, 3, 6, if pred(0, x) {
                    0 as u64
                } else {
                    1 as u64
                }
                );
            }
        }
    }
    ensure(cols > 0 && rows > 0 && cols <= 9223372036854775807 && rows <= 2147483647, 17);
    let xp = get(p, 0, 0);
    let dp = get(p, 0, 1);
    ensure(xp + get(p, 3, 12) <= dp || dp + get(p, 3, 13) <= xp, 18);
    put(p, 3, 1, cols);
    put(p, 3, 2, rows);
    put(p, 3, 4, rows);
    var cub = false;

    if mode == 4 || mode == 5 {
        cub = constant(packet, 3) != 0;
    }

    if mode == 1 && rows == 1 && constant(packet, 3) != 0 {
        var threshold: u64 = 65536;

        if constant(packet, 4) != 0 {
            let wide = (constant(packet, 5) == 0 && constant(packet, 6) == 0) || constant(packet, 7) != 0;

            if !wide {
                threshold = 32768;
            }
        }
        cub = cols > threshold;
    }

    if cub {
        put(p, 3, 7, 1 as u64);
        let bytes = cub_query(packet);
        ensure(bytes > 0 && bytes <= 9223372036854775807, 19);
        let temp = pool_alloc(packet, bytes);
        ensure(temp != 0, 20);

        if get(p, 3, 8) < bytes {
            pool_free(packet, temp);
            fail(21);
            return;
        }
        cub_run(packet, temp);

        if mode == 1 {
            put(p, 3, 7, 2 as u64);
            put(p, 3, 3, 32 as u64);
            put(p, 3, 4, 1 as u64);
            launch(packet);
        }
        pool_free(packet, temp);
        return;
    }
    ensure(cols <= 2147483647, 22);

    if mode == 2 {
        let q = policy(cols as i64, rows as i64);
        ensure(q.ok, 23);
        put(p, 3, 3, q.bs as u64);
        put(p, 3, 4, q.blocks as u64);
        put(p, 3, 5, q.shared as u64);
    } else {
        let nsm = constant(packet, 2);
        ensure(nsm > 0, 24);
        let bs = rr_row_block_size(cols as i64, rows as i64, nsm);
        put(p, 3, 3, bs as u64);
        put(p, 3, 5, (rr_row_shared_floats(bs) as u64) * 4);
    }
    launch(packet);
}

fn main() {
    run(0);
}

