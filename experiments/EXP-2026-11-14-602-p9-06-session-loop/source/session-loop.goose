// P9-06 S5: the session generation loop — budget, cancellation and context
// bounds as pure decisions.
//
// Native source: runtime/core/src/engine/session.cpp, Session::generate() and
// the abort callback installed by Session::open(). The graph, the tokenizer,
// the sampler, the decode and the KV stay native; this module answers the
// questions the loop asks around them, so the answers can be gated without a
// model.
//
// Every function below names the native lines it reproduces. Numbers, not
// texts: the error strings remain the carrier's.

import std;

// sl_loop_step results — why the loop head stops.
const SL_CONTINUE: i64 = 0;
const SL_STOP_BUDGET: i64 = 1; // n_gen reached n_predict            (1658)
const SL_STOP_REPLAY: i64 = 2; // the workload's captured length     (1660)
const SL_STOP_EOG: i64 = 3;    // the token is end-of-generation      (1665)

// sl_step_verdict results — what a failed decode means.
const SL_CANCEL_PREFILL: i64 = 10; // rollback_turn, return cancelled  (1554-1558)
const SL_CANCEL_DECODE: i64 = 11;  // cancelled = true, break          (1753-1755)
const SL_FAIL_WORKLOAD: i64 = 20;  // the workload names the record     (1562, 1759)
const SL_FAIL_HOOK: i64 = 21;      // the routing hook was fatal        (1563, 1760)
const SL_FAIL_SOURCE: i64 = 22;    // an overlap read failed            (1564, 1761)
const SL_FAIL_GENERIC: i64 = 23;   // nothing more specific             (1565, 1762)

// sl_result results — the terminal state of a generation.
const SL_RES_OK: i64 = 0;
const SL_RES_CANCELLED: i64 = 1;
const SL_RES_CANCELLED_WORKLOAD: i64 = 2;

// session.cpp:947-957 — the one predicate llama.cpp polls at a graph node
// boundary:
//
//   return p->cancel_requested.load(std::memory_order_relaxed) ||
//          p->source.fatal() || (p->hook && p->hook->fatal());
//
// It is installed unconditionally, which is what lets cancel() interrupt a
// serial decode too. The null-hook arm is not carried: open() always builds
// the hook (803), so the guard is dead in this engine and the caller passes 0
// for "the hook did not fail".
export fn sl_abort(cancel: i64, source_fatal: i64, hook_fatal: i64) -> i64 {
    if cancel != 0 || source_fatal != 0 || hook_fatal != 0 {
        return 1;
    }
    return 0;
}

// session.cpp:1658-1665 — the head of the generation loop, in the native order:
//
//   while (n_gen < req.n_predict) {
//       if (replay) { if (n_gen == token_count) break; ... }
//       if (llama_vocab_is_eog(im.vocab, tok)) break;
//
// The budget is tested first, so n_gen == n_predict == token_count reports a
// budget stop, not a replay stop. The replay stop uses equality, not >=: a
// longer n_predict is legal and the captured length is the length.
export fn sl_loop_step(n_gen: i64, n_predict: i64, replay: i64, replay_tokens: i64, eog: i64) -> i64 {
    if n_gen >= n_predict {
        return SL_STOP_BUDGET;
    }
    if replay != 0 && n_gen == replay_tokens {
        return SL_STOP_REPLAY;
    }
    if eog != 0 {
        return SL_STOP_EOG;
    }
    return SL_CONTINUE;
}

// session.cpp:1400-1402 — the context bound:
//
//   if (n_prompt + req.n_predict + 8 > im.cfg.n_ctx)
//       return fail("prompt + n_predict exceeds the session n_ctx (...)");
//
// The 8 is a margin for the turn's own framing, and the comparison is strict:
// a sum exactly equal to n_ctx passes. n_ctx cannot grow after open(), so this
// is a request rejection, not a clamp.
export fn sl_ctx_ok(n_prompt: i64, n_predict: i64, n_ctx: i64) -> i64 {
    if n_prompt + n_predict + 8 > n_ctx {
        return 0;
    }
    return 1;
}

// session.cpp:1309-1314 — the replay floor:
//
//   if ((size_t) req.n_predict < im.workload->token_count())
//       return fail("replay n_predict ... is below the workload's ...");
//
// An equal n_predict is accepted (and stops through the budget arm above); a
// longer one is accepted too, because the loop stops at the captured length.
// The check runs BEFORE the prompt is decoded, so a short budget costs nothing.
export fn sl_replay_ok(n_predict: i64, token_count: i64) -> i64 {
    if n_predict < token_count {
        return 0;
    }
    return 1;
}

// session.cpp:1672-1677 — the draft cap:
//
//   const int room = req.n_predict - n_gen - 1;  // after `tok` itself
//
// The confirmed token `tok` is not yet counted in n_gen, so one slot is
// subtracted for it. At loop entry n_gen < n_predict, hence room >= 0.
export fn sl_draft_room(n_predict: i64, n_gen: i64) -> i64 {
    return n_predict - n_gen - 1;
}

// session.cpp:1678-1707 — drafting needs speculation on AND room > 0, and both
// sources are capped at min(draft_max, room) (1685, 1707). A draft accepted
// past n_predict would be verified, charged for, and then discarded, so the cap
// goes in before drafting: no source is asked for tokens with nowhere to go.
export fn sl_draft_width(spec_on: i64, room: i64, draft_max: i64) -> i64 {
    if spec_on == 0 || room <= 0 {
        return 0;
    }
    if draft_max < room {
        return draft_max;
    }
    return room;
}

// session.cpp:1848 — the emit bound:
//
//   for (size_t e = 0; e < confirmed.size() && n_gen < req.n_predict; ++e)
//
// One verify decode confirms a whole group, but the caller's budget still
// bounds what is emitted. The surplus tokens are in the KV; the speculation
// trim (1898-1911) cuts the KV back to the emitted length afterwards.
export fn sl_emit_count(confirmed: i64, n_predict: i64, n_gen: i64) -> i64 {
    var left = n_predict - n_gen;
    if left < 0 {
        left = 0;
    }
    if confirmed < left {
        return confirmed;
    }
    return left;
}

// session.cpp:1553-1565 (prefill) and 1752-1762 (decode) — the same classifier
// twice, differing only in what cancellation does:
//
//   if (im.cancel_requested.load(...)) { rollback_turn(); res.ok = true;
//                                        res.cancelled = true; return res; }   prefill
//   if (im.cancel_requested.load(...)) { res.cancelled = true; break; }         decode
//   if (im.workload && !im.workload->error().empty()) return fail(...);
//   if (im.hook->fatal()) return fail(...);
//   if (moe.overlap && im.source.fatal()) return fail(...);
//   return fail("... decode failed");
//
// Cancellation is tested FIRST: the abort callback makes a cancelled decode
// return non-zero, and reporting that as a graph failure would lose the fact
// that a person asked. The source predicate is only reported under overlap;
// without it a fatal read is caught by the hook arm.
export fn sl_step_verdict(
    phase: i64,
    cancel: i64,
    workload_err: i64,
    hook_fatal: i64,
    source_fatal: i64,
    overlap: i64
) -> i64 {
    if cancel != 0 {
        if phase == 0 {
            return SL_CANCEL_PREFILL;
        }
        return SL_CANCEL_DECODE;
    }
    if workload_err != 0 {
        return SL_FAIL_WORKLOAD;
    }
    if hook_fatal != 0 {
        return SL_FAIL_HOOK;
    }
    if overlap != 0 && source_fatal != 0 {
        return SL_FAIL_SOURCE;
    }
    return SL_FAIL_GENERIC;
}

// session.cpp:2011-2016 and 2048-2051 — a cancelled generation is still ok
// (ok=true, cancelled=true), but half a deterministic workload is neither a
// capture (routes from a token the caller never saw) nor a replay (stopped
// mid-sequence), so it fails closed instead of being saved or accepted.
export fn sl_result(cancelled: i64, has_workload: i64) -> i64 {
    if cancelled == 0 {
        return SL_RES_OK;
    }
    if has_workload != 0 {
        return SL_RES_CANCELLED_WORKLOAD;
    }
    return SL_RES_CANCELLED;
}

// session.cpp:1286-1288 — the latch is refilled at the top of every generate():
//
//   im.cancel_requested.store(false, std::memory_order_relaxed);
//
// A cancel() that arrived while nothing was running — between two prompts, or
// aimed at a generation that had already ended — is therefore dropped. Only a
// request made during this generation stops it, so `entry` cannot matter and
// is carried for symmetry with the native store, not for its value.
export fn sl_cancel_effective(entry: i64, during: i64) -> i64 {
    let _ = entry;
    if during != 0 {
        return 1;
    }
    return 0;
}

// session.cpp:1925-1926 — the summary's two lengths: what this turn prefilled
// after KV reuse, and how long the context is now (the engine-held token list
// in chat mode, arithmetic otherwise).
export fn sl_n_prompt_reported(n_prompt: i64, n_common: i64) -> i64 {
    return n_prompt - n_common;
}

export fn sl_n_past(chat_on: i64, kv_len: i64, n_prompt: i64, n_gen: i64) -> i64 {
    if chat_on != 0 {
        return kv_len;
    }
    return n_prompt + n_gen;
}

// session.cpp:1894-1901 — the KV is trimmed to the tokens the caller actually
// received: an accepted end-of-generation token is decoded but never emitted,
// and a group can be cut short by n_predict. The whole block sits under
// `if (spec_on)`, so without speculation there is nothing to trim and -1 means
// "no trim point".
export fn sl_trim_end(spec_on: i64, n_prompt: i64, n_gen: i64) -> i64 {
    if spec_on == 0 {
        return 0 - 1;
    }
    return n_prompt + n_gen;
}
