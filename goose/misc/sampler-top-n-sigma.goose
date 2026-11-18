// P6-10 S3: top-n-sigma, the mask that cuts candidates more than n standard
// deviations below the max logit, in Goose.
//
// Native source: src/llama-sampler.cpp, llama_sampler_top_n_sigma_apply
// (3325-3357) and llama_sampler_init_top_n_sigma (3385).
//
// The rules exactly as native applies them:
//   - n <= 0, or a candidate list of size <= 1, is the empty sampler: nothing is
//     masked. tns_is_empty reports that; tns_apply still runs and masks nothing,
//     so a caller can chain unconditionally;
//   - max is seeded from candidates[0] and then maxed over the array, so the
//     first element participates even when it is -INFINITY. That is native's
//     behaviour and it matters: a leading -INFINITY does not poison the max;
//   - the mean and the deviation SKIP candidates whose logit is -INFINITY, and
//     a list with no valid candidate uses mean 0 and std 0;
//   - std is sqrt(acc / valid_count), the population deviation, not sample;
//   - a candidate is masked when logit < max - n * std, strict;
//   - masked candidates get -INFINITY, and the array keeps its order. This
//     filter never reorders, unlike typical.
//
// Arithmetic is f32 because native is float throughout: the mean, the
// accumulation and the sqrt must round the same way for the mask boundary to
// agree on ties. The gate compares bit patterns, not values.

import std;

extern "sqrtf" fn sqrt32(x: f32) -> f32;

const TNS_NEG_INF: f32 = 0.0 - 3.4028234663852886e38;

fn tns_is_neg_inf(v: f32) -> bool {
    // -INFINITY compares equal to itself and is below every finite value, so the
    // test against the most negative finite float is exact here: native writes
    // -INFINITY and never a finite value that low.
    return v <= TNS_NEG_INF;
}

// True when the filter is a no-op and the chain should not contain it.
export fn tns_is_empty(n: f32, size: i64) -> bool {
    return n <= 0.0 || size <= 1;
}

// Masks the candidates in place. Returns the number of candidates left unmasked,
// or -1 when the arrays disagree on length.
export fn tns_apply(logits: f32[:], n_candidates: i64, n: f32) -> i64 {
    if n_candidates < 0 || n_candidates > logits.len {
        return -1;
    }
    if n_candidates <= 1 || n <= 0.0 {
        // Native returns before touching anything: the mask is the identity.
        var kept: i64 = 0;
        var j: i64 = 0;
        while j < n_candidates {
            if !tns_is_neg_inf(logits[j]) {
                kept = kept + 1;
            }
            j = j + 1;
        }
        return kept;
    }

    // max over the array, seeded from element 0 exactly as native does
    var mx: f32 = logits[0];
    var sum: f32 = 0.0;
    var valid: i64 = 0;
    var i: i64 = 0;
    while i < n_candidates {
        if !tns_is_neg_inf(logits[i]) {
            if logits[i] > mx {
                mx = logits[i];
            }
            sum = sum + logits[i];
            valid = valid + 1;
        }
        i = i + 1;
    }
    var mean: f32 = 0.0;
    if valid > 0 {
        mean = sum / (valid as f32);
    }

    // population deviation over the valid candidates only
    var acc: f32 = 0.0;
    i = 0;
    while i < n_candidates {
        if !tns_is_neg_inf(logits[i]) {
            let d: f32 = logits[i] - mean;
            acc = acc + d * d;
        }
        i = i + 1;
    }
    var std: f32 = 0.0;
    if valid > 0 {
        std = sqrt32(acc / (valid as f32));
    }

    let cut: f32 = mx - n * std;
    var kept: i64 = 0;
    i = 0;
    while i < n_candidates {
        // Native compares every logit against `cut`. When the whole list is
        // already masked, mx is -INFINITY and cut is -INFINITY, so the strict
        // compare is false and native leaves the list as it is - the mask does
        // not re-apply. The module reproduces that, which is why an all-masked
        // list reports zero survivors rather than being rewritten.
        if logits[i] < cut {
            logits[i] = TNS_NEG_INF;
        } else {
            kept = kept + 1;
        }
        i = i + 1;
    }
    return kept;
}
