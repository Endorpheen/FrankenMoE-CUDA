// P6-10 S2: locally typical sampling, the ordering and the cut, in Goose.
//
// Native source: src/llama-sampler.cpp, llama_sampler_typical_apply (1990-2043)
// and llama_sampler_init_typical (2069). The whole filter is a decision over a
// candidate array, so it is Apple-independent: no RNG, no model, no socket.
//
// The rules exactly as native applies them:
//   - p >= 1.0 is the empty sampler: the filter removes nothing, so the caller
//     must not call this at all. tp_apply reports that as 0 (kept everything)
//     rather than pretending it ran;
//   - probabilities come from the softmax the chain already computed. Native
//     re-runs llama_sampler_softmax_impl on a copy; here the caller passes the
//     probabilities it has, because the array is the same one;
//   - entropy is sum(-p * ln p) over the WHOLE candidate array, before any cut;
//   - each candidate's score is |(-ln p) - entropy|; the sort is ascending by
//     score, and ties keep index order, which std::sort does not promise but
//     this module does, so the result is at least as deterministic;
//   - the cut keeps candidates until the running sum exceeds p AND at least
//     min_keep have been kept; min_keep == 0 means no lower bound;
//   - the kept candidates stay in score order, and the array is marked unsorted
//     afterwards, exactly as native sets cur_p->sorted = false.
//
// What this module does NOT own: reading logits out of the graph, the RNG, and
// the chain that calls it. It answers "which indices survive, in what order".

import std;

// libm, declared the way the shipped unit already declares exp for perplexity
// (build/goose-main/main.goose: `extern "exp" fn exp64`). The names differ from
// the C functions only to keep one obvious spelling per width in Goose code.
extern "log" fn tp_ln64(x: f64) -> f64;
extern "fabs" fn tp_abs64(x: f64) -> f64;

// Returns the number of survivors, or -1 on a bad argument (n < 0, or any of the
// arrays shorter than n). `probs` are the caller's softmax values; `order` and
// `keep` are written for the survivors only. When p >= 1.0 nothing is filtered:
// the function still fills order/keep with the identity so the caller can chain.
export fn tp_apply(
    probs: const f64[:], n: i64, p: f64, min_keep: i64,
    order: i64[:], keep: i64[:], scratch: f64[:]
) -> i64 {
    if n < 0 || n > probs.len || order.len < n || keep.len < n || scratch.len < n || min_keep < 0 {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        order[i] = i;
        keep[i] = i;
        i = i + 1;
    }
    if n == 0 {
        return 0;
    }
    // p >= 1.0: llama_sampler_init_typical returns the empty sampler, so the
    // filter keeps the whole array untouched.
    if p >= 1.0 {
        return n;
    }

    // entropy over the whole array
    var entropy: f64 = 0.0;
    i = 0;
    while i < n {
        if probs[i] > 0.0 {
            entropy = entropy - probs[i] * tp_ln64(probs[i]);
        }
        i = i + 1;
    }

    // score = |(-ln p) - entropy|
    i = 0;
    while i < n {
        if probs[i] > 0.0 {
            scratch[i] = tp_abs64((0.0 - tp_ln64(probs[i])) - entropy);
        } else {
            // -ln(0) is +inf, so a zero-probability candidate scores +inf and
            // sorts last. Native computes the same way through fabsf(-logf(p)).
            scratch[i] = 1.0e308 * 1.0e308;
        }
        i = i + 1;
    }

    // insertion sort by (score, index): stable and small for a candidate list,
    // and it makes the tie order a property of this module, not of std::sort.
    var a: i64 = 1;
    while a < n {
        let cur: i64 = order[a];
        var j: i64 = a;
        while j > 0 {
            let prev: i64 = order[j - 1];
            let move: bool = if scratch[cur] < scratch[prev] { true } else { false };
            if !move {
                break;
            }
            order[j] = prev;
            j = j - 1;
        }
        order[j] = cur;
        a = a + 1;
    }

    // cumulative cut
    var cum: f64 = 0.0;
    var last: i64 = n;
    i = 0;
    while i < n {
        let idx: i64 = order[i];
        cum = cum + probs[idx];
        if cum > p {
            if min_keep == 0 || i >= min_keep - 1 {
                last = i + 1;
                break;
            }
        }
        i = i + 1;
    }

    // survivors in score order
    i = 0;
    while i < last {
        keep[i] = order[i];
        i = i + 1;
    }
    return last;
}

// True when the filter is a no-op, i.e. the chain should not contain it.
export fn tp_is_empty(p: f64) -> bool {
    return p >= 1.0;
}
