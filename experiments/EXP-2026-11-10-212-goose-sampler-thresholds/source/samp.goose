// Sampler threshold decisions in Goose (APX: P6-10).
//
// From llama-sampler.cpp: three pure-arithmetic decisions on the per-token logit array, which is
// the path Apex exercises with --temp 1 --top-p 0.95 --top-k 20 --min-p 0.
//
//   min_p_keep     : 1755-1790, keep tokens whose logit >= max_logit + log(p)
//   top_p_cut      : 1555-1608, the shortest prefix whose softmax mass reaches p, honouring min_keep
//   top_k_cut      : 321-338, clamp k to the candidate count
//
// What is NOT here, and why: the sorting those three rely on. Ordering is Goose's for pairs
// (EXP201 sort_pairs), but llama_token_data_array_partial_sort operates on descending logits with
// tie handling the native `partial_sort` defines, and the arrays are engine-owned. This module
// decides *how many* and *which threshold*, the host keeps the array and its sort. Stated rather
// than hidden: a full sampler port needs the sort contract too.
extern "fg212s_fail" fn fail(code: i64);
extern "fg212s_ld_f32" fn ld_f32(base: u64, byte: i64) -> f32;
extern "fg212s_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// top_k_cut: llama-sampler.cpp:321-338. k <= 0 keeps everything; otherwise clamp to the count.

fn top_k_cut(k: i64, size: i64) -> i64 {
    ensure(size >= 0, 10);

    if k <= 0 as i64 {
        return size;
    }

    if k < size {
        return k;
    }
    return size;
}
// min_p_threshold: line 1772, `max_logit + logf(p)`. log is supplied by the host as an extern
// primitive so this stays exact to the native call.
extern "fg212s_log" fn logf_(x: f32) -> f32;
// min_p_scan: 1768-1784. Walk the candidates once, record which survive and how many.
//   logits: f32 array, size entries
//   keep:   i32 out array, keep[i] = 1 when candidate i survives
// Returns the survivor count, or -1 when the result is unusable (empty, or below min_keep) --
// which is exactly when native falls through to the sorted implementation.

fn min_p_scan(logits: u64, size: i64, p: f32, min_keep: i64, keep: u64) -> i64 {
    ensure(size >= 0 && min_keep >= 0, 20);

    if p <= 0.0 as f32 {
        return size;
    }

    if size == 0 as i64 {
        return (0 as i64) - 1 as i64;
    }
    var max_logit: f32 = ld_f32(logits, 0 as i64);
    var i: i64 = 1;

    while i < size {
        let v: f32 = ld_f32(logits, i * 4 as i64);

        if v > max_logit {
            max_logit = v;
        }
        i += 1;
    }
    let min_logit: f32 = max_logit + logf_(p);
    var kept: i64 = 0;
    i = 0;

    while i < size {
        let v: f32 = ld_f32(logits, i * 4 as i64);

        if v >= min_logit {
            st_i64(keep, i * 8 as i64, 1 as i64);
            kept += 1;
        } else {
            st_i64(keep, i * 8 as i64, 0 as i64);
        }
        i += 1;
    }

    if kept == 0 as i64 {
        return (0 as i64) - 1 as i64;
    }

    if kept < min_keep {
        return (0 as i64) - 1 as i64;
    }
    return kept;
}
// top_p_scan: 1578-1598. Cumulative probability until it reaches p AND at least min_keep tokens
// are kept. The caller supplies probabilities already in descending order (the host sorts).
// Returns the number of entries to keep: at least 1, at most size, or size when p >= 1.

fn top_p_scan(probs: u64, size: i64, p: f32, min_keep: i64) -> i64 {
    ensure(size >= 0 && min_keep >= 0, 30);

    if p >= 1.0 as f32 {
        return size;
    }
    var cum: f32 = 0.0 as f32;
    var i: i64 = 0;

    while i < size {
        cum = cum + ld_f32(probs, i * 4 as i64);

        if cum >= p {
            if i + 1 as i64 >= min_keep {
                return i + 1 as i64;
            }
        }
        i += 1;
    }
    return size;
}

fn main() {
    top_k_cut(0 as i64, 0 as i64);
    top_k_cut(20 as i64, 100 as i64);
    min_p_scan(0 as u64, 0 as i64, 0.0 as f32, 0 as i64, 0 as u64);
    min_p_scan(8 as u64, 1 as i64, 0.5 as f32, 1 as i64, 32 as u64);
    top_p_scan(0 as u64, 0 as i64, 0.0 as f32, 0 as i64);
    top_p_scan(16 as u64, 1 as i64, 0.95 as f32, 1 as i64);
}

