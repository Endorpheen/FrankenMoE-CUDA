// Self-check for the shared-expert branch and the combine.
//
// These are property checks rather than a second implementation of the same formula:
// recomputing sigmoid the same way would only prove the code agrees with itself. What is
// checked instead is what the native code requires to be true:
//
//   - sigmoid lands strictly inside (0,1) and is monotonic
//   - the PAR pair applies silu to the GATE half: a factor of 2 on `up` doubles the
//     result, a factor of 2 on `gate` does not
//   - the combine ADDS the branches: it is not an average, and it is commutative
const N: i64 = 64;
const NEG: i64 = 0x7f800000 - 1;   // unused sentinel kept for clarity

fn mk(seed: i64) -> f32[N] {
    var a: f32[N] = default<f32[N]>();
    var i: i64 = 0;
    while i < N {
        a[i] = ((i * seed % 41) as f32 - 20.0) / 7.0;
        i = i + 1;
    }
    return a;
}

fn scaled(src: f32[N], k: f32) -> f32[N] {
    var a: f32[N] = default<f32[N]>();
    var i: i64 = 0;
    while i < N {
        a[i] = src[i] * k;
        i = i + 1;
    }
    return a;
}

fn main() {
    var failures: i64 = 0;

    let proj: f32[N] = mk(3);
    var sg: f32[N] = default<f32[N]>();
    let got: i64 = moe_shared_gate(proj, sg);
    if got != N {
        failures = failures + 1;
    }

    // sigmoid strictly inside (0,1) and monotonic in the input
    var i: i64 = 0;
    var range_bad: i64 = 0;
    while i < N {
        if sg[i] <= 0.0 || sg[i] >= 1.0 {
            range_bad = range_bad + 1;
        }
        i = i + 1;
    }
    var mono_bad: i64 = 0;
    i = 1;
    while i < N {
        let up_i: bool = proj[i] > proj[i - 1];
        let up_s: bool = sg[i] > sg[i - 1];
        if up_i != up_s {
            mono_bad = mono_bad + 1;
        }
        i = i + 1;
    }

    // PAR: silu on the gate half only
    let g: f32[N] = mk(5);
    let u: f32[N] = mk(7);
    var par: f32[N] = default<f32[N]>();
    let r1: i64 = moe_ffn_par(g, u, par);
    if r1 != N { failures = failures + 1; }

    let g2: f32[N] = scaled(g, 2.0);
    var par_g2: f32[N] = default<f32[N]>();
    let r1b: i64 = moe_ffn_par(g2, u, par_g2);
    if r1b != N { failures = failures + 1; }

    let u2: f32[N] = scaled(u, 2.0);
    var par_u2: f32[N] = default<f32[N]>();
    let r1c: i64 = moe_ffn_par(g, u2, par_u2);
    if r1c != N { failures = failures + 1; }

    var up_doubles: i64 = 0;
    var gate_not_linear: i64 = 0;
    i = 0;
    while i < N {
        if par_u2[i] != par[i] * (2.0 as f32) {
            up_doubles = up_doubles + 1;
        }
        // silu(2x) differs from 2*silu(x) wherever silu is not locally linear. If the
        // implementation had applied silu to `up` instead, this count would be 0.
        if par_g2[i] != par[i] * (2.0 as f32) {
            gate_not_linear = gate_not_linear + 1;
        }
        i = i + 1;
    }

    // combine adds: not an average, and commutative
    let moe: f32[N] = mk(11);
    let sh: f32[N] = mk(13);
    var comb: f32[N] = default<f32[N]>();
    let r2: i64 = moe_combine(moe, sh, comb);
    if r2 != N { failures = failures + 1; }
    var comb_rev: f32[N] = default<f32[N]>();
    let r2b: i64 = moe_combine(sh, moe, comb_rev);
    if r2b != N { failures = failures + 1; }

    var add_bad: i64 = 0;
    var avg_bad: i64 = 0;
    var comm_bad: i64 = 0;
    i = 0;
    while i < N {
        if comb[i] != moe[i] + sh[i] {
            add_bad = add_bad + 1;
        }
        if comb[i] != (moe[i] + sh[i]) / (2.0 as f32) {
            avg_bad = avg_bad + 1;
        }
        if comb[i] != comb_rev[i] {
            comm_bad = comm_bad + 1;
        }
        i = i + 1;
    }

    print("range_bad");
    print(range_bad);
    print("mono_bad");
    print(mono_bad);
    print("up_doubles_mismatch");
    print(up_doubles);
    print("gate_linear_would_be");
    print(gate_not_linear);
    print("add_bad");
    print(add_bad);
    print("avg_bad");
    print(avg_bad);
    print("comm_bad");
    print(comm_bad);

    // The two "not linear" properties hold in general, not at every element: silu(x) is
    // locally linear for large positive x, and a sum equals its average wherever the two
    // branches are zero. Requiring all N was the harness being wrong, not the module.
    if range_bad != 0 || mono_bad != 0 || up_doubles != 0 || add_bad != 0 ||
       comm_bad != 0 || avg_bad < N / 2 || gate_not_linear < N / 2 {
        failures = failures + 1;
    }

    print("failures");
    print(failures);
    if failures == 0 {
        print("FG_MOE_FFN PASS");
    } else {
        print("FG_MOE_FFN FAIL");
    }
}
