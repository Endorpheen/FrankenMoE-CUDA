// Self-check for goose/cpu/goose-moe-router.goose.
//
// The reference selection here is deliberately a different algorithm: a brute-force
// scan of the full probability vector, not the incremental selection the module uses.
// If both agreed because they shared code, the check would prove nothing.
//
// Determinism note: the checker rejects a variable that is declared and then filled by
// index, so every array is produced by a function that returns it fully formed.
const N_EXPERT: i64 = 256;
const N_USED: i64 = 8;
const SCALE: f32 = 2.5;

fn build_logits(trial: i64) -> f32[N_EXPERT] {
    var l: f32[N_EXPERT] = default<f32[N_EXPERT]>();
    var i: i64 = 0;
    while i < N_EXPERT {
        l[i] = 0.0;
        i = i + 1;
    }
    i = 0;
    while i < N_EXPERT {
        if trial == 0 {
            l[i] = ((i * 37 % 101) as f32 - 50.0) / 8.0;
        } else {
            l[i] = 0.0;
        }
        i = i + 1;
    }
    if trial == 1 {
        l[173] = 24.0;
    }
    return l;
}

fn zero_ids() -> i64[N_USED] {
    var z: i64[N_USED] = default<i64[N_USED]>();
    var i: i64 = 0;
    while i < N_USED {
        z[i] = -1;
        i = i + 1;
    }
    return z;
}

fn zero_w() -> f32[N_USED] {
    var z: f32[N_USED] = default<f32[N_USED]>();
    var i: i64 = 0;
    while i < N_USED {
        z[i] = 0.0;
        i = i + 1;
    }
    return z;
}

fn zero_probs() -> f32[N_EXPERT] {
    var z: f32[N_EXPERT] = default<f32[N_EXPERT]>();
    var i: i64 = 0;
    while i < N_EXPERT {
        z[i] = 0.0;
        i = i + 1;
    }
    return z;
}

// Brute-force reference: repeatedly take the largest probability not yet taken.
fn reference_top_k(probs: f32[N_EXPERT]) -> i64[N_USED] {
    var ref: i64[N_USED] = default<i64[N_USED]>();
    var taken: i64 = 0;
    while taken < N_USED {
        var best: i64 = -1;
        var best_v: f32 = -3.0e38 as f32;
        var j: i64 = 0;
        while j < N_EXPERT {
            var seen: bool = false;
            var t: i64 = 0;
            while t < taken {
                if ref[t] == j {
                    seen = true;
                }
                t = t + 1;
            }
            if !seen && probs[j] > best_v {
                best_v = probs[j];
                best = j;
            }
            j = j + 1;
        }
        ref[taken] = best;
        taken = taken + 1;
    }
    return ref;
}

fn main() {
    var failures: i64 = 0;
    var dominant_first: i64 = -1;

    var trial: i64 = 0;
    while trial < 2 {
        let logits: f32[N_EXPERT] = build_logits(trial);
        var ids: i64[N_USED] = zero_ids();
        var weights: f32[N_USED] = zero_w();
        var probs: f32[N_EXPERT] = zero_probs();

        let got: i64 = moe_route(logits, N_USED, 1, SCALE, ids, weights, probs);
        if got != N_USED {
            print("moe_route returned");
            print(got);
            failures = failures + 1;
        }

        let ref: i64[N_USED] = reference_top_k(probs);
        var d: i64 = 0;
        var i: i64 = 0;
        while i < N_USED {
            if ids[i] != ref[i] {
                d = d + 1;
            }
            i = i + 1;
        }

        var wsum: f64 = 0.0;
        i = 0;
        while i < N_USED {
            wsum = wsum + weights[i] as f64;
            i = i + 1;
        }
        let sum_ok: bool = wsum > 2.4999 && wsum < 2.5001;

        // After normalisation the weight is the selected probability divided by the sum
        // of the SELECTED probabilities, then scaled - not probs[id] * scale. Getting
        // this expectation wrong is easy: in the dominant trial the selected sum is
        // almost 1, so the wrong formula passes there and fails only on flat logits.
        var s8: f64 = 0.0;
        i = 0;
        while i < N_USED {
            s8 = s8 + probs[ids[i]];
            i = i + 1;
        }
        var wbad: i64 = 0;
        i = 0;
        while i < N_USED {
            let want: f32 = ((probs[ids[i]] as f64 / s8) * 2.5) as f32;
            if weights[i] != want {
                wbad = wbad + 1;
            }
            i = i + 1;
        }

        print("trial");
        print(trial);
        print("ids_differ");
        print(d);
        print("weights_mismatch");
        print(wbad);
        print("sum_x1000");
        print(wsum * 1000.0);
        if d != 0 || wbad != 0 || !sum_ok || got != N_USED {
            failures = failures + 1;
        }
        if trial == 1 {
            dominant_first = ids[0];
        }
        trial = trial + 1;
    }

    print("dominant_first");
    print(dominant_first);
    if dominant_first != 173 {
        failures = failures + 1;
    }

    if failures == 0 {
        print("FG_MOE_ROUTER PASS");
        print(N_EXPERT);
        print(N_USED);
    } else {
        print("FG_MOE_ROUTER FAIL");
        print(failures);
    }
}
