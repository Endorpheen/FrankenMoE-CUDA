// P9-08 S3: the request's KV state — clearing, prefix reuse and turn rollback.
//
// Native source: runtime/core/src/engine/session.cpp, Session::generate()
// (session.cpp:1322-1478 and :1575-1576, :1854). The KV itself, the decode and
// the chat template stay native; this module owns the decisions about it: which
// stores a new chat clears, how much of the decoded prefix a turn may reuse,
// where the draft context is rewound to, and what a cancelled turn leaves
// behind.
//
// Already accepted elsewhere, NOT repeated here: sl_* (EXP602) owns the loop
// head, the abort predicate and the failure classes; ss_* (EXP632) owns the
// hot store, the probe and the teardown; rts_* owns the run() call order.
//
// Every export names the native lines it reproduces. Numbers, not texts: the
// error strings stay the carrier's.

import std;

// ── what clear_kv touches ───────────────────────────────────────────────────

// kvp_clear_action bits, one per store the native block writes:
const KVP_CL_TARGET: i64 = 1;  // target KV + its token list            (1325)
const KVP_CL_DRAFT: i64 = 2;   // draft-context KV                      (1327-1328)
const KVP_CL_HISTORY: i64 = 4; // chat_history                          (1329)
const KVP_CL_TOKENS: i64 = 8;  // kv_tokens                             (1330)
const KVP_CL_SAMPLER: i64 = 16; // sampler RNG                          (1334-1335)

// session.cpp:1324-1337 — "new chat": the target memory, the draft memory (or
// the first draft of the new conversation is conditioned on the previous one),
// the history, the token list, and the sampler RNG. The sampler reset is
// conditional on a sampler existing, and the draft clear on a draft context
// existing; neither condition is assumed here.
export fn kvp_clear_actions(clear_kv: i64, has_draft: i64, has_sampler: i64) -> i64 {
    if clear_kv == 0 {
        return 0;
    }
    var m: i64 = KVP_CL_TARGET + KVP_CL_HISTORY + KVP_CL_TOKENS;
    if has_draft != 0 {
        m = m + KVP_CL_DRAFT;
    }
    if has_sampler != 0 {
        m = m + KVP_CL_SAMPLER;
    }
    return m;
}

// ── prefix reuse ────────────────────────────────────────────────────────────

// session.cpp:1436 — the reuse ceiling:
//   const size_t max_common = tokens.size() > 0 ? tokens.size() - 1 : 0;
// One token is always left to decode, so a turn is never a no-op even when the
// whole prompt is already in the cache. An empty token list reuses nothing.
export fn kvp_max_common(n_tokens: i64) -> i64 {
    if n_tokens > 0 {
        return n_tokens - 1;
    }
    return 0;
}

// session.cpp:1437 — the matching prefix, bounded by both lists and by the
// ceiling above:
//   while (n_common < kv_tokens.size() && n_common < max_common &&
//          kv_tokens[n_common] == tokens[n_common]) ++n_common;
// The bound is the min of the three; the previous list may be longer (the last
// turn generated) or shorter (a shorter prompt after a long one).
export fn kvp_common_len(prev: const i64[:], cur: const i64[:], max_common: i64) -> i64 {
    var n: i64 = 0;
    while n < prev.len {
        if n >= max_common {
            return n;
        }
        if n >= cur.len {
            return n;
        }
        if prev[n] != cur[n] {
            return n;
        }
        n = n + 1;
    }
    return n;
}

// kvp_rewind actions.
const KVP_RW_KEEP: i64 = 0;   // nothing to drop: the cache is already a prefix
const KVP_RW_TRIM: i64 = 1;   // seq_rm accepted the partial removal
const KVP_RW_CLEAR: i64 = 2;  // seq_rm refused it: full clear and common = 0

// session.cpp:1438-1447 — the divergent tail is dropped only when there is
// one; a cache that is already a prefix is left alone (no seq_rm call at all).
// A partial removal the memory refuses (SWA-style) falls back to a full clear
// and to common = 0, and the stored token list follows the same point.
// out: 0 action, 1 the resulting n_common.
export fn kvp_rewind(
    prev_len: i64,
    common: i64,
    seq_rm_ok: i64,
    out: i64[:]
) -> i64 {
    if out.len < 2 {
        return -1;
    }
    var action: i64 = KVP_RW_KEEP;
    var n: i64 = common;
    if common < prev_len {
        if seq_rm_ok != 0 {
            action = KVP_RW_TRIM;
        } else {
            action = KVP_RW_CLEAR;
            n = 0;
        }
    }
    out[0] = action;
    out[1] = n;
    return action;
}

// session.cpp:1448-1455 — the draft context is rewound to the SAME point even
// when the target's prefix did not move, because the previous turn left it
// holding everything it generated. A refused removal clears it instead.
// 0 = no draft context, 1 = seq_rm, 2 = clear.
export fn kvp_draft_rewind(has_draft: i64, seq_rm_ok: i64) -> i64 {
    if has_draft == 0 {
        return 0;
    }
    if seq_rm_ok != 0 {
        return 1;
    }
    return 2;
}

// ── rollback ────────────────────────────────────────────────────────────────

// session.cpp:1459-1478 — rolling a turn back to the state before it started.
// Chat mode returns to n_common and keeps the prior turns usable; a raw-prompt
// session has no prefix to return to and clears instead. The user message is
// un-appended only when this turn pushed one. The draft context follows the
// target's keep point in both arms.
//
// One native asymmetry is preserved rather than corrected: the token-list
// resize uses n_common even when seq_rm refused the partial removal and the
// target memory was cleared (1463 vs 1461). The port keeps the same values so
// the gate pins the carrier's behaviour; REPORT.md records it as a candidate
// fix for the wiring step.
//
// out: 0 target action (KVP_RW_*), 1 token-list length, 2 history popped,
//      3 draft keep (-1 = clear), 4 target keep.
export fn kvp_rollback_keep(
    chat_on: i64,
    n_common: i64,
    history_pushed: i64,
    seq_rm_ok: i64,
    out: i64[:]
) -> i64 {
    if out.len < 5 {
        return -1;
    }
    var action: i64 = KVP_RW_KEEP;
    var tokens_len: i64 = 0;
    var popped: i64 = 0;
    var draft_keep: i64 = -1;
    var keep: i64 = 0;
    if chat_on != 0 {
        keep = n_common;
        tokens_len = n_common;
        if seq_rm_ok != 0 {
            action = KVP_RW_TRIM;
        } else {
            action = KVP_RW_CLEAR;
            keep = 0;
        }
        if history_pushed != 0 {
            popped = 1;
        }
        draft_keep = n_common;
    } else {
        action = KVP_RW_CLEAR;
        keep = 0;
        tokens_len = 0;
        draft_keep = 0;
    }
    out[0] = action;
    out[1] = tokens_len;
    out[2] = popped;
    out[3] = draft_keep;
    out[4] = keep;
    return action;
}

// session.cpp:1575-1576 — what a finished prefill appends to the token list:
// only the suffix that was actually decoded, from n_common to n_prompt, and
// only in chat mode.
//   for (int i = n_common; i < n_prompt; ++i) kv_tokens.push_back(tokens[i]);
export fn kvp_prefill_span(chat_on: i64, n_common: i64, n_prompt: i64) -> i64 {
    if chat_on == 0 {
        return 0;
    }
    if n_prompt <= n_common {
        return 0;
    }
    return n_prompt - n_common;
}

// session.cpp:1854 — a decoded token joins the list only in chat mode, and only
// for the plain (non-speculative) path: exported separately so the gate can
// show both arms.
export fn kvp_append_token(chat_on: i64, speculative: i64) -> i64 {
    if chat_on == 0 {
        return 0;
    }
    if speculative != 0 {
        return 0;
    }
    return 1;
}

// The invariant behind max_common: for any prompt of at least one token the
// reuse point is strictly inside it, so the prefill loop always has work.
export fn kvp_never_empty(n_tokens: i64) -> i64 {
    if n_tokens < 1 {
        return 0;
    }
    if kvp_max_common(n_tokens) < n_tokens {
        return 1;
    }
    return 0;
}
