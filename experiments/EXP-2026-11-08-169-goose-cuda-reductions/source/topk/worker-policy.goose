// EXP171 topk-moe host policy: validation, arithmetic-free plan projection, launch selection.
// No routing arithmetic here; the bridge provides tensor introspection and the launch effect.
extern "fg171t_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg171t_plan_ptr" fn plan_ptr(packet: i64) -> i64;
extern "fg171t_hget" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg171t_hput" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg171t_hfloat" fn put_float(p: i64, i: i64, v: f32);
extern "fg171t_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg171t_tf" fn tf(t: i64, i: i64) -> f32;
extern "fg171t_contiguous" fn contiguous(t: i64) -> bool;
extern "fg171t_f32_type" fn f32_type() -> i64;
extern "fg171t_i32_type" fn i32_type() -> i64;
extern "fg171t_launch" fn launch(packet: i64);
extern "fg171t_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn ceil_div(a: u64, b: u64) -> u64 {
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn supported(n: u64) -> bool {
    return n == 1 || n == 2 || n == 4 || n == 8 || n == 16 || n == 32 || n == 64 || n == 128 || n == 256 || n == 288 || n == 512 || n == 576;
}

fn run(packet: i64) {
    let mode = arg(packet, 0);
    let node = arg(packet, 2);
    let p = plan_ptr(packet);
    ensure(mode == 0 && arg(packet, 1) != 0 && node != 0 && p != 0, 1);
    // node is the weights destination; srcs: 0 logits, 1 ids, 2 clamp, 3 scale, 4 bias.
    ensure(ti(node, 2, 0) == f32_type() && ti(node, 1, 0) != 0 && ti(node, 4, 0) == 4, 4);
    ensure(ti(node, 3, 0) > 0 && ti(node, 3, 0) <= 2147483647, 4);
    ensure(ti(node, 3, 1) > 0 && ti(node, 3, 1) <= 2147483647, 4);
    ti(node, 9, 0);
    let logits = ti(node, 0, 0);
    let ids = ti(node, 0, 1);
    let clamp_t = ti(node, 0, 2);
    let scale_t = ti(node, 0, 3);
    let bias_t = ti(node, 0, 4);
    ensure(logits != 0 && ids != 0, 2);
    ensure(ti(logits, 2, 0) == f32_type() && ti(logits, 1, 0) != 0 && ti(logits, 4, 0) == 4, 2);
    ensure(
        ti(logits, 3, 0) > 0 && ti(logits, 3, 0) <= 2147483647 && ti(logits, 3, 1) > 0 && ti(logits, 3, 1) <= 2147483647,
        3);
    ensure(contiguous(logits), 3);
    ti(logits, 9, 0);
    let n_experts = ti(logits, 3, 0);
    let n_rows = ti(logits, 3, 1);
    ensure(supported(n_experts as u64), 7);
    let n_used = ti(node, 3, 1);
    ensure(n_used <= n_experts, 8);
    ensure(ti(ids, 2, 0) == i32_type() && ti(ids, 1, 0) != 0 && ti(ids, 4, 0) == 4, 5);
    ensure( (ti(ids, 4, 1) as u64) == (n_experts as u64) * 4, 6);
    ti(ids, 9, 0);
    let cfg = ti(node, 8, 0);
    ensure(cfg >= 0 && cfg < 32, 9);
    let use_sigmoid = cfg % 2 == 1;
    let use_softplus = (cfg / 2) % 2 == 1;
    let delayed = (cfg / 8) % 2 == 1;
    let with_norm = clamp_t != 0;
    let has_bias = bias_t != 0;
    ensure(! (use_sigmoid && use_softplus), 10);
    ensure(! (with_norm && delayed), 11);
    let n_groups = ti(node, 8, 1);
    let n_group_used = ti(node, 8, 2);

    if n_groups > 0 {
        ensure(n_groups <= 32 && n_groups <= n_experts && n_group_used >= 1, 12);
    } else {
        ensure(n_groups == 0, 12);
    }
    var clamp_val: f32 = 0.0;

    if with_norm {
        clamp_val = tf(clamp_t, 0);
    }
    var scale_val: f32 = 1.0;

    if scale_t != 0 {
        scale_val = tf(scale_t, 0);
    }

    if has_bias {
        ensure(ti(bias_t, 2, 0) == f32_type() && ti(bias_t, 1, 0) != 0 && ti(bias_t, 4, 0) == 4, 13);
        ensure(ti(bias_t, 3, 0) > 0 && ti(bias_t, 3, 0) <= 2147483647 && ti(bias_t, 3, 0) >= n_experts, 13);
        ti(bias_t, 9, 0);
    }
    put(p, 0, 0, ti(logits, 1, 0) as u64);
    put(p, 0, 1, ti(node, 1, 0) as u64);
    put(p, 0, 2, ti(ids, 1, 0) as u64);

    if has_bias {
        put(p, 0, 3, ti(bias_t, 1, 0) as u64);
    }
    put(p, 3, 0, n_experts as u64);
    put(p, 3, 1, n_rows as u64);
    put(p, 3, 2, n_used as u64);
    var fword: u64 = 0;

    if use_sigmoid {
        fword += 1;
    }

    if use_softplus {
        fword += 2;
    }

    if with_norm {
        fword += 4;
    }

    if delayed {
        fword += 8;
    }

    if has_bias {
        fword += 16;
    }
    put(p, 3, 3, fword);
    put(p, 3, 4, n_groups as u64);
    put(p, 3, 5, n_group_used as u64);
    let ept = if n_experts > 32 {
        (n_experts as u64) / 32
    } else {
        1 as u64
    }
    ;
    put(p, 3, 6, ept);
    put(p, 3, 7, 8);
    put(p, 3, 8, ceil_div(n_rows as u64, 8));
    put(p, 3, 9, 32);
    put(p, 3, 10, 8);
    put(p, 3, 11, 0);
    put(p, 3, 12, 18);
    put_float(p, 0, clamp_val);
    put_float(p, 1, scale_val);
    launch(packet);
}

fn main() {
    run(0);
}

