// P9-08 S2: the perplexity pass — its request gates and the exact set of
// positions it scores.
//
// Native source: runtime/core/src/engine/session.cpp, Session::perplexity()
// (session.cpp:459-622). The tokenizer, the decode, the logits and the
// log-softmax stay native; this module owns the arithmetic around them: which
// requests are refused, which prefix is decoded as one batch, which positions
// carry a score, which row the choice distribution is read from, and how the
// aggregates are formed.
//
// Already accepted elsewhere, NOT repeated here: sl_* (EXP602) owns the
// generation loop's budget and failure classes; ss_* (EXP632) owns the hot
// store and the teardown; cc_* (EXP620) owns config validation, including the
// n_batch >= 1 guarantee this module relies on.
//
// Every export names the native lines it reproduces. Numbers, not texts: the
// error strings stay the carrier's.

import std;
import math;

// ppl_gate results — why a request never reaches the graph.
const PPL_OK: i64 = 0;
const PPL_TOO_SHORT: i64 = 1;   // "text too short to score"            (471-474)
const PPL_OVER_CTX: i64 = 2;    // "text of N tokens exceeds ... n_ctx" (476-480)

// ppl_choice_gate results.
const PPL_CH_OK: i64 = 0;
const PPL_CH_BAD: i64 = 1;      // "choice '...' does not tokenize"     (601-604)
const PPL_CH_NOTHING: i64 = 2;  // "nothing scored — ..."               (606-609)

// session.cpp:469-481 — the two request gates, in the native order:
//   if (n < 2) return "text too short to score";
//   if (n > im.cfg.n_ctx) return "text of N tokens exceeds ...";
// A text of exactly n_ctx tokens is accepted; one token is refused even when
// it fits, because there is no next token to score against.
export fn ppl_token_gate(n_tokens: i64, n_ctx: i64) -> i64 {
    if n_tokens < 2 {
        return PPL_TOO_SHORT;
    }
    if n_tokens > n_ctx {
        return PPL_OVER_CTX;
    }
    return PPL_OK;
}

// session.cpp:540 — the unscored prefix of the decode regime:
//   const int prefix = std::max(1, std::min(req.skip, n - 1));
// The floor of one is what makes the pass start with a real prefill; the
// ceiling keeps the final token out of the prefix, so position n-1 is always
// decoded with all_logits and the last distribution exists.
export fn ppl_prefix(skip: i64, n: i64) -> i64 {
    var s = skip;
    if s > n - 1 {
        s = n - 1;
    }
    if s < 1 {
        return 1;
    }
    return s;
}

// session.cpp:546 — with choices the LAST token is fed too, so the final
// distribution exists; without them it is left unscored:
//   const int last = req.choices.empty() ? n - 1 : n;
export fn ppl_step_last(has_choices: i64, n: i64) -> i64 {
    if has_choices != 0 {
        return n;
    }
    return n - 1;
}

// session.cpp:554-556 and 577-579 — the one scoring predicate both regimes
// share: position p is scored only when it lies at or after skip and a next
// token exists.
//   if (pos < req.skip || pos + 1 >= n) continue;
export fn ppl_scored(pos: i64, skip: i64, n: i64) -> i64 {
    if pos < skip {
        return 0;
    }
    if pos + 1 >= n {
        return 0;
    }
    return 1;
}

// How many positions the plan scores. Kept separate from the enumerator so a
// caller can size its buffer before asking for the rows.
export fn ppl_score_count(n: i64, skip: i64, step: i64, n_batch: i64) -> i64 {
    if n_batch < 1 {
        return -1;
    }
    var count: i64 = 0;
    if step != 0 {
        let prefix = ppl_prefix(skip, n);
        let last = n - 1; // count is choice-independent: choices add no score
        var pos = prefix;
        while pos < last {
            if ppl_scored(pos, skip, n) != 0 {
                count = count + 1;
            }
            pos = pos + 1;
        }
        return count;
    }
    var i: i64 = 0;
    while i < n {
        var chunk = n - i;
        if chunk > n_batch {
            chunk = n_batch;
        }
        var j: i64 = 0;
        while j < chunk {
            if ppl_scored(i + j, skip, n) != 0 {
                count = count + 1;
            }
            j = j + 1;
        }
        i = i + n_batch;
    }
    return count;
}

// session.cpp:538-596 — the whole enumeration, both regimes, as the (pos, row)
// pairs the carrier hands to score():
//   step  : one-token decodes from prefix to last-1, always row 0;
//   batch : n_batch-wide chunks from 0, local row j, all_logits every token.
// Packed as pos * 64 + row, so one i64 carries both and the gate can compare
// the plan without a second array. Returns the number of pairs, or -1 when the
// slice is too small or n_batch is not positive.
export fn ppl_score_plan(
    n: i64,
    skip: i64,
    step: i64,
    n_batch: i64,
    out: i64[:]
) -> i64 {
    let count = ppl_score_count(n, skip, step, n_batch);
    if count < 0 || count > out.len {
        return -1;
    }
    var k: i64 = 0;
    if step != 0 {
        let prefix = ppl_prefix(skip, n);
        var pos = prefix;
        while pos < n - 1 {
            if ppl_scored(pos, skip, n) != 0 {
                out[k] = pos * 64;
                k = k + 1;
            }
            pos = pos + 1;
        }
        return k;
    }
    var i: i64 = 0;
    while i < n {
        var chunk = n - i;
        if chunk > n_batch {
            chunk = n_batch;
        }
        var j: i64 = 0;
        while j < chunk {
            if ppl_scored(i + j, skip, n) != 0 {
                out[k] = (i + j) * 64 + j;
                k = k + 1;
            }
            j = j + 1;
        }
        i = i + n_batch;
    }
    return k;
}

// session.cpp:548 and 589 — the logits row the choice distribution is read
// from: the decode regime's single row is 0, the batch regime's is the last
// local row of the final chunk.
//   last_row = 0;  /  last_row = chunk - 1;
export fn ppl_last_row(n: i64, step: i64, n_batch: i64) -> i64 {
    if step != 0 {
        return 0;
    }
    if n_batch < 1 {
        return -1;
    }
    let rem = n - (n / n_batch) * n_batch;
    if rem == 0 {
        return n_batch - 1;
    }
    return rem - 1;
}

// session.cpp:598-610 — the closing gates, in order: a choice that does not
// tokenize is an error even when scores exist, and a pass that scored nothing
// without choices is an error rather than an empty result.
export fn ppl_choice_gate(has_choices: i64, bad_choices: i64, scored: i64) -> i64 {
    if bad_choices > 0 {
        return PPL_CH_BAD;
    }
    if scored <= 0 && has_choices == 0 {
        return PPL_CH_NOTHING;
    }
    return PPL_CH_OK;
}

// session.cpp:611-613 — the aggregate the carrier forms from the per-position
// NLL: mean over the positions actually scored, and perplexity as its exp.
// A run with choices and no positions keeps mean 0.0 and ppl exp(0) = 1.
export fn ppl_mean(nll: f64, scored: i64) -> f64 {
    if scored > 0 {
        return nll / (scored * 1.0);
    }
    return 0.0;
}

export fn ppl_from_nll(nll: f64, scored: i64) -> f64 {
    return exp(ppl_mean(nll, scored));
}

// session.cpp:621 — a counter delta: the routing tallies are session-cumulative
// and a pass reports its own share.
export fn ppl_delta(now: i64, prev: i64) -> i64 {
    return now - prev;
}

// session.cpp:543 and 592 — the eval-callback phase each decode is labelled
// with. A perplexity pass is decode work unless the caller asks otherwise, and
// the prefix batch stays prefill either way, so a decode-only policy does not
// leak into it.
export fn ppl_phase(step: i64, as_decode: i64) -> i64 {
    if step != 0 {
        return 1;
    }
    if as_decode != 0 {
        return 1;
    }
    return 0;
}
