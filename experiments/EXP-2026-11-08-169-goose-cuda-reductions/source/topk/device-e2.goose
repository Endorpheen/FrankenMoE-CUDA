// EXP171 topk-moe device: native fused MoE routing, one warp per row.
// Native order: row guard, slot init, PDL grid sync, logits load, block barrier (outputs may alias logits),
// transform (softmax | sigmoid | sqrt-softplus), NaN sanitize to -FLT_MAX, bias selection view,
// optional expert-group mask (top-2 group score), iterative strict top-k (lowest expert index wins ties),
// optional weight normalization with clamped denominator, optional delayed softmax over selected weights,
// scaled weight store. Selection/normalization/indexing/control are Goose; memory/ISA/sync effects are extern.
extern "fg169t_di" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg169t_df" fn fl(p: i64, i: i64) -> f32;
extern "fg169t_load" fn load(p: u64, i: u64) -> f32;
extern "fg169t_store" fn store(p: u64, i: u64, v: f32);
extern "fg169t_storei" fn storei(p: u64, i: u64, v: i32);
extern "fg169t_exp" fn expf(v: f32) -> f32;
extern "fg169t_sqrt" fn sqrtf(v: f32) -> f32;
extern "fg169t_log" fn logf(v: f32) -> f32;
extern "fg169t_shuffle" fn shuffle(v: f32, offset: i32) -> f32;
extern "fg169t_shufflei" fn shufflei(v: f32, src: i32) -> f32;
extern "fg169t_ishuffle" fn ishuffle(v: i32, offset: i32) -> i32;
extern "fg169t_ballot" fn ballot(v: bool) -> u64;
extern "fg169t_barrier" fn barrier();
extern "fg169t_sync" fn sync();
extern "fg169t_lc" fn lc();
// FLT_MAX exact: (8192*8192) squared twice, times (2^24-1); every f32 step is exact.

fn flt_max() -> f32 {
    let a: f32 = (8192.0 as f32) * (8192.0 as f32);
    let b: f32 = a * a;
    let c: f32 = b * b;
    return c * 16777215.0;
}
// -INFINITY via IEEE overflow of FLT_MAX*2.

fn ninf() -> f32 {
    let m: f32 = flt_max() * 2.0;
    return -m;
}
// CUDA fmaxf/fminf NaN semantics: a NaN operand resolves to the other operand.
extern "fg169t_fmax" fn fmaxf(a: f32, b: f32) -> f32;
extern "fg169t_max" fn maximum(a: f32, b: f32) -> f32;
extern "fg169t_fmin" fn fminf(a: f32, b: f32) -> f32;

fn warp_max(v: f32) -> f32 {
    var value: f32 = v;
    var offset: i32 = 16;

    while offset > 0 {
        value = fmaxf(value, shuffle(value, offset));
        offset >>= 1;
    }
    return value;
}

fn warp_sum(v: f32) -> f32 {
    var value: f32 = v;
    var offset: i32 = 16;

    while offset > 0 {
        value += shuffle(value, offset);
        offset >>= 1;
    }
    return value;
}

fn topk_row(p: i64, row: u64, lane: i32, n_experts: i32, ept: i32, has_bias: bool) {
    let n_rows = get(p, 3, 1) as i32;
    let n_used = get(p, 3, 2) as i32;
    let flags = get(p, 3, 3);
    let use_sigmoid = flags % 2 == 1;
    let use_softplus = (flags / 2) % 2 == 1;
    let with_norm = (flags / 4) % 2 == 1;
    let delayed = (flags / 8) % 2 == 1;
    let n_groups = get(p, 3, 4) as i32;
    let n_group_used = get(p, 3, 5) as i32;

    if row >= (n_rows as u64) {
        return;
    }
    let inf2 = ninf();
    let fmax_v = flt_max();
    let neg_max = -fmax_v;
    let logits = get(p, 0, 0) + (n_experts as u64) * row * 4;
    let wbase = get(p, 0, 1) + (n_used as u64) * row * 4;
    let ibase = get(p, 0, 2) + (n_experts as u64) * row * 4;
    let bias = get(p, 0, 3);
    var wt: f32[2] = default < f32[2] > ();
    var k: i32 = 0;

    while k < ept {
        wt[k] = inf2;
        k += 1;
    }
    sync();
    k = 0;

    while k < ept {
        let expert = k * 32 + lane;
        wt[k] = if n_experts % 32 == 0 || expert < n_experts {
            load(logits, expert as u64)
        } else {
            inf2
        }
        ;
        k += 1;
    }
    barrier();

    if !delayed {
        if use_sigmoid {
            k = 0;

            while k < ept {
                wt[k] = (1.0 as f32) / ( (1.0 as f32) + expf(-wt[k]));
                k += 1;
            }
        } else {
            if use_softplus {
                k = 0;

                while k < ept {
                    let v = wt[k];
                    wt[k] = sqrtf(if v > 20.0 {
                        v
                    } else {
                        logf( (1.0 as f32) + expf(v))
                    }
                    );
                    k += 1;
                }
            } else {
                var mv: f32 = inf2;
                k = 0;

                while k < ept {
                    mv = maximum(mv, wt[k]);
                    k += 1;
                }
                mv = warp_max(mv);
                var sum: f32 = 0.0;
                k = 0;

                while k < ept {
                    let v = expf(wt[k] - mv);
                    wt[k] = v;
                    sum += v;
                    k += 1;
                }
                sum = warp_sum(sum);
                let inv: f32 = (1.0 as f32) / sum;
                k = 0;

                while k < ept {
                    wt[k] = wt[k] * inv;
                    k += 1;
                }
            }
        }
    }
    k = 0;

    while k < ept {
        if wt[k] != wt[k] {
            wt[k] = neg_max;
        }
        k += 1;
    }
    var sel: f32[2] = default < f32[2] > ();

    if has_bias {
        k = 0;

        while k < ept {
            let expert = k * 32 + lane;
            sel[k] = if n_experts % 32 == 0 || expert < n_experts {
                wt[k] + load(bias, expert as u64)
            } else {
                inf2
            }
            ;
            k += 1;
        }
    }

    if n_groups > 0 {
        let n_per_group = n_experts / n_groups;
        var score: f32 = inf2;
        var g: i32 = 0;

        while g < n_groups {
            var top1: f32 = inf2;
            var top2: f32 = inf2;
            k = 0;

            while k < ept {
                if (k * 32 + lane) / n_per_group == g {
                    let v = if has_bias {
                        sel[k]
                    } else {
                        wt[k]
                    }
                    ;
                    top2 = fmaxf(top2, fminf(top1, v));
                    top1 = fmaxf(top1, v);
                }
                k += 1;
            }
            var offset: i32 = 16;

            while offset > 0 {
                let o1 = shuffle(top1, offset);
                let o2 = shuffle(top2, offset);
                top2 = fmaxf(fminf(top1, o1), fmaxf(top2, o2));
                top1 = fmaxf(top1, o1);
                offset >>= 1;
            }

            if lane == g {
                score = top1 + top2;
            }
            g += 1;
        }
        var rank: i32 = 0;
        g = 0;

        while g < n_groups {
            let other = shufflei(score, g);

            if other > score || (other == score && g < lane) {
                rank += 1;
            }
            g += 1;
        }
        let keep = ballot(lane < n_groups && rank < n_group_used);
        k = 0;

        while k < ept {
            let gidx = (k * 32 + lane) / n_per_group;

            if gidx >= n_groups || (keep >> (gidx as u64)) % 2 == 0 {
                if has_bias {
                    sel[k] = inf2;
                } else {
                    wt[k] = inf2;
                }
            }
            k += 1;
        }
    }
    var wt_sum: f32 = 0.0;
    var outw: f32[2] = default < f32[2] > ();
    k = 0;

    while k < ept {
        outw[k] = 0.0;
        k += 1;
    }
    lc();
    var kk: i32 = 0;

    while kk < n_used {
        var max_val: f32 = if lane < n_experts {
            wt[0]
        } else {
            inf2
        }
        ;
        var max_key: f32 = if lane < n_experts {
            if has_bias {
                sel[0]
            } else {
                wt[0]
            }
        } else {
            inf2
        }
        ;
        var max_expert: i32 = lane;
        k = 1;

        while k < ept {
            let expert = lane + k * 32;
            let key = if has_bias {
                sel[k]
            } else {
                wt[k]
            }
            ;

            if (n_experts % 32 == 0 || expert < n_experts) && key > max_key {
                max_val = wt[k];
                max_key = key;
                max_expert = expert;
            }
            k += 1;
        }
        var offset: i32 = 16;

        while offset > 0 {
            let val_v = shuffle(max_val, offset);
            let val_k = shuffle(max_key, offset);
            let expert = ishuffle(max_expert, offset);

            if val_k > max_key || (val_k == max_key && expert < max_expert) {
                max_val = val_v;
                max_key = val_k;
                max_expert = expert;
            }
            offset >>= 1;
        }

        if max_expert % 32 == lane {
            if has_bias {
                sel[max_expert / 32] = inf2;
            } else {
                wt[max_expert / 32] = inf2;
            }
        }

        if kk % 32 == lane {
            outw[kk / 32] = max_val;
        }

        if max_expert % 32 == lane {
            storei(ibase, kk as u64, max_expert);

            if with_norm {
                wt_sum += max_val;
            }
        }
        kk += 1;
    }

    if with_norm {
        wt_sum = warp_sum(wt_sum);
        let clamp_val = fl(p, 0);
        let denom = maximum(wt_sum, clamp_val);
        let inv: f32 = (1.0 as f32) / denom;
        k = 0;

        while k < ept {
            outw[k] = outw[k] * inv;
            k += 1;
        }
    }

    if delayed {
        var mv: f32 = inf2;
        k = 0;

        while k < ept {
            let idx = lane + k * 32;

            if idx < n_used {
                mv = maximum(mv, outw[k]);
            }
            k += 1;
        }
        mv = warp_max(mv);
        var sum: f32 = 0.0;
        k = 0;

        while k < ept {
            let idx = lane + k * 32;

            if idx < n_used {
                let v = expf(outw[k] - mv);
                outw[k] = v;
                sum += v;
            } else {
                outw[k] = 0.0;
            }
            k += 1;
        }
        sum = warp_sum(sum);
        let inv: f32 = (1.0 as f32) / sum;
        k = 0;

        while k < ept {
            let idx = lane + k * 32;

            if idx < n_used {
                outw[k] = outw[k] * inv;
            }
            k += 1;
        }
    }
    let scale_val = fl(p, 1);
    k = 0;

    while k < ept {
        let idx = k * 32 + lane;

        if idx < n_used {
            store(wbase, idx as u64, outw[k] * scale_val);
        }
        k += 1;
    }
}

fn main() {
    topk_row(0, 0 as u64, 0, 256, 8, false);
}

