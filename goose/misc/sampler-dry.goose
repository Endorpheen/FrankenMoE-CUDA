// P6-10 S4: DRY - the "don't repeat yourself" repetition penalty, in Goose.
//
// Native source: src/llama-sampler.cpp, llama_sampler_dry_apply (3473-3682) and
// llama_sampler_init_dry (3726). Fully deterministic: no RNG, no model, no
// socket. The gate compares logits bit for bit.
//
// The rules exactly as native applies them:
//   - multiplier == 0, base < 1, or penalty_last_n == 0 is the empty sampler;
//   - the window is min(history, penalty_last_n); a window not longer than
//     dry_allowed_length returns before doing anything;
//   - restart sequences (the breakers) limit how far back a repeat may count:
//     the scan walks the window from its start, finds the longest breaker tail
//     that matches the tokens BEFORE position i, and sets rep_limit = i - len.
//     A breaker whose tail is empty matches trivially at every head;
//   - a rep_limit below dry_allowed_length abandons the penalty entirely;
//   - the reverse Z-algorithm fills repeat_count[last - k] for k in 1..last,
//     clamped by rep_limit, with the two standard Z-box cases;
//   - only repeats of at least dry_allowed_length leave the count array and
//     become a candidate for a penalty, keyed by the token that would EXTEND the
//     run, and the largest length per token wins;
//   - a single-token breaker is never penalised;
//   - the penalty exponent is repeat_len - dry_allowed_length, clamped to
//     max_exponent = 88.7228391 / ln(base) when base > 1.000001, and the logit is
//     reduced by multiplier * base^exponent.
//
// Layout: the caller owns the history, the repeat counts, the max-repeat pairs
// and the breaker tables. `dry_breakers` is (head, tail_off, tail_len) triples
// whose tails index `dry_breaker_tails`; a tail length of 0 is the single-token
// breaker. Pair tables are written as (token, len) in `pairs` and the count in
// `pairs[0]`-style arrays would be awkward, so the caller passes the count out
// through `pair_count`.

import std;

extern "powf" fn dry_pow32(x: f32, y: f32) -> f32;
extern "logf" fn dry_log32(x: f32) -> f32;

// Empty sampler test: the chain should not contain DRY at all.
export fn dry_is_empty(multiplier: f32, base: f32, penalty_last_n: i64) -> bool {
    return multiplier == 0.0 || base < 1.0 || penalty_last_n == 0;
}

fn dry_rat(history: const i64[:], n_history: i64, i: i64) -> i64 {
    // Native's ring buffer answers rat(i) as "i tokens back from the newest",
    // not "the i-th from the oldest": ring_buffer::rat reads
    // data[(first + size - i - 1) % capacity]. The caller passes the history in
    // chronological order, so the mapping has to be reversed here. EXP968 found
    // this the hard way - the first version indexed forwards and penalised the
    // wrong token.
    if i < 0 || i >= n_history {
        return -1;
    }
    return history[n_history - 1 - i];
}

// Step 1: how far back a repeat may count, or -1 when no breaker matched.
fn dry_rep_limit(
    history: const i64[:], n_history: i64, last_n: i64,
    breakers: const i64[:], n_breakers: i64,
    tails: const i64[:], n_tails: i64
) -> i64 {
    var rep_limit: i64 = last_n;
    var i: i64 = 0;
    while i < last_n {
        let token: i64 = dry_rat(history, n_history, i);
        var longest: i64 = -1;
        var b: i64 = 0;
        while b < n_breakers {
            if breakers[b * 3] == token {
                let seq_len: i64 = breakers[b * 3 + 2];
                if seq_len > longest && seq_len <= i {
                    var hit: bool = true;
                    var off: i64 = 0;
                    while off < seq_len {
                        let want: i64 = tails[breakers[b * 3 + 1] + off];
                        let got: i64 = dry_rat(history, n_history, i - off - 1);
                        if want != got {
                            hit = false;
                            break;
                        }
                        off = off + 1;
                    }
                    if hit {
                        longest = seq_len;
                    }
                }
            }
            b = b + 1;
        }
        if longest >= 0 {
            rep_limit = i - longest;
            break;
        }
        i = i + 1;
    }
    return rep_limit;
}

// Steps 2 to 4. `counts` receives the per-position repeat length and is sized
// to last_n. `pair_tok` and `pair_len` receive the (token, max repeat) pairs;
// `pair_count` is a one-element array that returns how many were written.
// Returns the number of pairs, or -1 on a bad argument.
export fn dry_apply(
    history: const i64[:], n_history: i64,
    logits: f32[:], ids: const i64[:], n_logits: i64,
    multiplier: f32, base: f32, allowed_length: i64, penalty_last_n: i64,
    breakers: const i64[:], n_breakers: i64,
    tails: const i64[:], n_tails: i64,
    counts: i64[:], pair_tok: i64[:], pair_len: i64[:], pair_count: i64[:]
) -> i64 {
    if pair_count.len < 1 {
        return -1;
    }
    pair_count[0] = 0;
    if n_history < 0 || n_history > history.len || n_logits < 0 || n_logits > logits.len ||
       n_logits > ids.len || n_history > counts.len || n_breakers < 0 ||
       n_breakers * 3 > breakers.len || n_tails < 0 || n_tails > tails.len {
        return -1;
    }
    if dry_is_empty(multiplier, base, penalty_last_n) {
        return 0;
    }

    var last_n: i64 = n_history;
    if penalty_last_n < last_n {
        last_n = penalty_last_n;
    }
    if last_n <= allowed_length {
        return 0;
    }

    var i: i64 = 0;
    while i < last_n {
        counts[i] = 0;
        i = i + 1;
    }

    // Step 1
    let rep_limit: i64 = dry_rep_limit(history, n_history, last_n, breakers, n_breakers, tails, n_tails);
    if rep_limit < allowed_length {
        return 0;
    }

    // Step 2: reverse Z-algorithm over the window
    let last: i64 = last_n - 1;
    var rt: i64 = 0;
    var lt: i64 = 0;
    var k: i64 = 1;
    while k < last_n {
        if k > rt {
            var n: i64 = 0;
            while n + k < last_n && dry_rat(history, n_history, n) == dry_rat(history, n_history, n + k) {
                n = n + 1;
            }
            counts[last - k] = if n < rep_limit { n } else { rep_limit };
            if n > 0 {
                lt = k;
                rt = k + n - 1;
            }
        } else {
            let p: i64 = k - lt;
            let right_part_len: i64 = rt - k + 1;
            if counts[last - p] < right_part_len {
                let n: i64 = if counts[last - p] < rep_limit { counts[last - p] } else { rep_limit };
                counts[last - k] = n;
            } else {
                var ii: i64 = rt + 1;
                while ii < last_n && dry_rat(history, n_history, ii) == dry_rat(history, n_history, ii - k) {
                    ii = ii + 1;
                }
                let n: i64 = if ii - k < rep_limit { ii - k } else { rep_limit };
                counts[last - k] = n;
                lt = k;
                rt = ii - 1;
            }
        }
        k = k + 1;
    }

    // Step 3: which token would extend a long enough repeat, and by how much
    var n_pairs: i64 = 0;
    i = 0;
    while i < last_n - 1 {
        let repeat_len: i64 = counts[i];
        if repeat_len >= allowed_length {
            let token: i64 = dry_rat(history, n_history, last_n - 2 - i);
            // largest length per token wins
            var found: i64 = -1;
            var f: i64 = 0;
            while f < n_pairs {
                if pair_tok[f] == token {
                    found = f;
                    break;
                }
                f = f + 1;
            }
            if found >= 0 {
                if pair_len[found] < repeat_len {
                    pair_len[found] = repeat_len;
                }
            } else {
                if n_pairs >= pair_tok.len || n_pairs >= pair_len.len {
                    return -1;
                }
                pair_tok[n_pairs] = token;
                pair_len[n_pairs] = repeat_len;
                n_pairs = n_pairs + 1;
            }
        }
        i = i + 1;
    }
    pair_count[0] = n_pairs;
    if n_pairs == 0 {
        return 0;
    }

    // Step 4: penalty, with the exponent clamp native applies
    var max_exponent: i64 = 0;
    if base > 1.000001 {
        let lim: f32 = 88.7228391 / dry_log32(base);
        max_exponent = lim as i64;
    }

    var c: i64 = 0;
    while c < n_logits {
        let token: i64 = ids[c];
        var found: i64 = -1;
        var f: i64 = 0;
        while f < n_pairs {
            if pair_tok[f] == token {
                found = f;
                break;
            }
            f = f + 1;
        }
        if found >= 0 {
            // a single-token breaker is never penalised
            var single_breaker: bool = false;
            var b: i64 = 0;
            while b < n_breakers {
                if breakers[b * 3] == token && breakers[b * 3 + 2] == 0 {
                    single_breaker = true;
                    break;
                }
                b = b + 1;
            }
            if !single_breaker {
                var repeat_exp: i64 = pair_len[found] - allowed_length;
                if max_exponent > 0 && repeat_exp > max_exponent {
                    repeat_exp = max_exponent;
                }
                let penalty: f32 = multiplier * dry_pow32(base, repeat_exp as f32);
                logits[c] = logits[c] - penalty;
            }
        }
        c = c + 1;
    }
    return n_pairs;
}
