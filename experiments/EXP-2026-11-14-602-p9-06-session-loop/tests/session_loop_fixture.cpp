// Gate for P9-06 S5: the session loop's budget, cancellation and context
// decisions.
//
// The oracle is a transcription of runtime/core/src/engine/session.cpp, one
// function per decision, each quoting the native lines it reproduces. The port
// is source/session-loop.goose. Cases are data; every case asserts all
// thirteen decisions, and a second table pins the boundary values by hand so a
// transcription mistake cannot hide behind a matching port.
//
// The gate needs no model: tokenization, sampling, the graph and the KV are
// native and out of scope.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "g.h"

static int cases = 0, checks = 0, fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL %s / %s : got %lld want %lld\n", group, what, (long long) got, (long long) want);
    }
}

// ── oracle: the native decisions, transcribed ───────────────────────────────

// session.cpp:947-957
//   llama_set_abort_callback(ctx, [](void * ud) -> bool {
//       return p->cancel_requested.load(std::memory_order_relaxed) ||
//              p->source.fatal() || (p->hook && p->hook->fatal());
//   }, &im);
static int64_t nat_abort(int64_t cancel, int64_t src, int64_t hook) {
    return (cancel != 0 || src != 0 || hook != 0) ? 1 : 0;
}

// session.cpp:1658-1665
//   while (n_gen < req.n_predict) {
//       if (im.workload && im.workload->mode() == ...Replay) {
//           if ((size_t) n_gen == im.workload->token_count()) break;
//       if (llama_vocab_is_eog(im.vocab, tok)) break;
static int64_t nat_loop_step(int64_t ngen, int64_t budget, int64_t replay, int64_t rtok, int64_t eog) {
    if (!(ngen < budget)) return 1; // SL_STOP_BUDGET
    if (replay && (size_t) ngen == (size_t) rtok) return 2; // SL_STOP_REPLAY
    if (eog) return 3; // SL_STOP_EOG
    return 0;
}

// session.cpp:1400-1402
//   if (n_prompt + req.n_predict + 8 > im.cfg.n_ctx)
//       return fail("prompt + n_predict exceeds the session n_ctx (...");
static int64_t nat_ctx_ok(int64_t prompt, int64_t budget, int64_t nctx) {
    return (prompt + budget + 8 > nctx) ? 0 : 1;
}

// session.cpp:1309-1314
//   if ((size_t) req.n_predict < im.workload->token_count())
//       return fail("replay n_predict ... is below the workload's ...");
static int64_t nat_replay_ok(int64_t budget, int64_t rtok) {
    return ((size_t) budget < (size_t) rtok) ? 0 : 1;
}

// session.cpp:1677
//   const int room = req.n_predict - n_gen - 1;
static int64_t nat_draft_room(int64_t budget, int64_t ngen) {
    return budget - ngen - 1;
}

// session.cpp:1678 / 1685 / 1707
//   if (spec_on && room > 0) { ... dp.n_max = std::min(im.cfg.spec.draft_max, room);
//                              ngram_draft(..., std::min(im.cfg.spec.draft_max, room), ...)
static int64_t nat_draft_width(int64_t spec, int64_t room, int64_t dmax) {
    if (!(spec && room > 0)) return 0;
    return dmax < room ? dmax : room;
}

// session.cpp:1848
//   for (size_t e = 0; e < confirmed.size() && n_gen < req.n_predict; ++e)
static int64_t nat_emit_count(int64_t confirmed, int64_t budget, int64_t ngen) {
    int64_t n = 0;
    while (n < confirmed && ngen < budget) {
        ++n;
        ++ngen;
    }
    return n;
}

// session.cpp:1553-1565 (prefill) and 1752-1762 (decode)
//   if (im.cancel_requested.load(...)) { ... res.cancelled = true; ... }
//   if (im.workload && !im.workload->error().empty()) return fail(...)
//   if (im.hook->fatal()) return fail("... streaming failed ...")
//   if (moe.overlap && im.source.fatal()) return fail(...)
//   return fail("... decode failed")
static int64_t nat_step_verdict(int64_t phase, int64_t cancel, int64_t wl_err, int64_t hook, int64_t src,
                                int64_t overlap) {
    if (cancel) return phase == 0 ? 10 : 11;
    if (wl_err) return 20;
    if (hook) return 21;
    if (overlap && src) return 22;
    return 23;
}

// session.cpp:2011-2016 / 2048
//   if (res.cancelled) return fail("generation cancelled; the deterministic
//                                   workload is incomplete and was not saved");
//   if (res.cancelled) rollback_turn(); else if (chat_on) { ... }
static int64_t nat_result(int64_t cancelled, int64_t wl) {
    if (!cancelled) return 0;
    return wl ? 2 : 1;
}

// session.cpp:1286-1288
//   im.cancel_requested.store(false, std::memory_order_relaxed);
static int64_t nat_cancel_effective(int64_t entry, int64_t during) {
    int64_t latch = 0;
    latch = 0; // the store above
    (void) entry;
    if (during) latch = 1;
    return latch;
}

// session.cpp:1925
//   s.n_prompt = n_prompt - (int) n_common;
static int64_t nat_n_prompt_reported(int64_t prompt, int64_t ncommon) {
    return prompt - ncommon;
}

// session.cpp:1926
//   s.n_past = chat_on ? (int) im.kv_tokens.size() : n_prompt + n_gen;
static int64_t nat_n_past(int64_t chat, int64_t kv, int64_t prompt, int64_t ngen) {
    return chat ? kv : prompt + ngen;
}

// session.cpp:1898-1899
//   if (spec_on) {
//       const llama_pos emitted_end = (llama_pos) n_prompt + n_gen;
static int64_t nat_trim_end(int64_t spec, int64_t prompt, int64_t ngen) {
    if (!spec) return -1; // nothing was speculated past the emitted prefix
    return prompt + ngen;
}

// ── the case table ──────────────────────────────────────────────────────────

struct Case {
    const char * name;
    // request and session
    int64_t prompt = 128, budget = 32, nctx = 2048;
    // loop head
    int64_t ngen = 0, replay = 0, rtok = 0, eog = 0;
    // one verify group
    int64_t confirmed = 1;
    // a failed decode
    int64_t phase = 1, cancel = 0, wl_err = 0, hook = 0, src = 0, overlap = 0;
    // drafting
    int64_t spec = 0, dmax = 4;
    // summary and the cancel latch
    int64_t chat = 0, kv = 0, ncommon = 0, entry = 0, wl = 0;
};

static std::vector<Case> build_cases() {
    std::vector<Case> v;
    auto add = [&v](const char * n, auto set) {
        Case c;
        c.name = n;
        set(c);
        v.push_back(c);
    };

    // A plain greedy run, first token of the budget.
    add("baseline_first_token", [](Case &) {});
    // The context bound is inclusive: prompt+budget+8 == n_ctx passes.
    add("ctx_exact_fit", [](Case & c) { c.prompt = 1000; c.budget = 500; c.nctx = 1508; });
    add("ctx_one_over", [](Case & c) { c.prompt = 1000; c.budget = 500; c.nctx = 1507; });
    // A rendered chat template makes the prompt long enough that the request
    // fits by 40 tokens only.
    add("ctx_chat_prompt", [](Case & c) { c.prompt = 1900; c.budget = 100; c.nctx = 2048; c.chat = 1; });
    // Zero budget: the loop never runs, and that is a budget stop, not an error.
    add("budget_zero", [](Case & c) { c.budget = 0; });
    // A replay whose budget equals the captured length stops through the
    // budget arm, because the budget is tested first.
    add("replay_equal_budget", [](Case & c) { c.replay = 1; c.rtok = 16; c.budget = 16; c.ngen = 16; c.wl = 1; });
    // ... and one token short of it is rejected before the prompt is decoded.
    add("replay_floor", [](Case & c) { c.replay = 1; c.rtok = 16; c.budget = 15; c.wl = 1; });
    // A longer budget is legal and the loop still stops at the captured length.
    add("replay_longer_budget", [](Case & c) { c.replay = 1; c.rtok = 16; c.budget = 64; c.ngen = 15; c.wl = 1; });
    add("replay_stop_token", [](Case & c) { c.replay = 1; c.rtok = 16; c.budget = 64; c.ngen = 16; c.wl = 1; });
    // End-of-generation at the first token: nothing is emitted.
    add("eog_first", [](Case & c) { c.eog = 1; c.confirmed = 5; });
    // The replay stop is tested before end-of-generation, so a captured length
    // that lands on an eog token reports the replay stop.
    add("replay_before_eog",
        [](Case & c) { c.replay = 1; c.rtok = 16; c.budget = 32; c.ngen = 16; c.eog = 1; c.wl = 1; });
    // A cancel during decode wins over every other failure signal.
    add("cancel_decode_wins", [](Case & c) {
        c.phase = 1; c.cancel = 1; c.wl_err = 1; c.hook = 1; c.src = 1; c.overlap = 1; c.wl = 1;
    });
    // The same in prefill: the turn is rolled back and the call returns.
    add("cancel_prefill", [](Case & c) {
        c.phase = 0; c.cancel = 1; c.wl_err = 1; c.hook = 1; c.src = 1; c.overlap = 1; c.wl = 1;
    });
    // A workload mismatch names the record ahead of the generic causes.
    add("workload_error_first", [](Case & c) {
        c.phase = 1; c.wl_err = 1; c.hook = 1; c.src = 1; c.overlap = 1; c.wl = 1;
    });
    add("hook_fatal", [](Case & c) { c.phase = 0; c.hook = 1; c.src = 1; c.overlap = 1; });
    // A failed read is reported as an I/O failure only under overlap.
    add("source_fatal_overlap", [](Case & c) { c.phase = 1; c.src = 1; c.overlap = 1; });
    add("source_fatal_serial", [](Case & c) { c.phase = 1; c.src = 1; c.overlap = 0; });
    add("plain_graph_failure", [](Case & c) { c.phase = 0; });
    // Drafting: room leaves one slot for the token already sampled.
    add("draft_full_room", [](Case & c) { c.spec = 1; c.budget = 8; c.ngen = 0; c.dmax = 16; });
    add("draft_narrow_room", [](Case & c) { c.spec = 1; c.budget = 8; c.ngen = 5; c.dmax = 4; });
    add("draft_last_slot", [](Case & c) { c.spec = 1; c.budget = 8; c.ngen = 7; c.dmax = 4; });
    // No speculation: neither source is asked, whatever the room is.
    add("draft_no_spec", [](Case & c) { c.spec = 0; c.budget = 8; c.ngen = 0; c.dmax = 4; });
    // A verify group larger than the remaining budget emits only the budget.
    add("emit_truncated_group", [](Case & c) { c.confirmed = 4; c.budget = 5; c.ngen = 3; });
    add("emit_group_fits", [](Case & c) { c.confirmed = 3; c.budget = 8; c.ngen = 0; });
    add("emit_at_budget", [](Case & c) { c.confirmed = 2; c.budget = 4; c.ngen = 4; });
    // Chat summary: n_past is the engine-held list, and only the un-cached
    // suffix counts as prompted this turn.
    add("summary_chat", [](Case & c) { c.chat = 1; c.kv = 42; c.prompt = 128; c.ngen = 8; c.ncommon = 100; });
    add("summary_oneshot", [](Case & c) { c.chat = 0; c.prompt = 128; c.ngen = 8; c.ncommon = 0; });
    // Terminal states.
    add("result_cancelled", [](Case & c) { c.cancel = 1; c.wl = 0; });
    add("result_cancelled_workload", [](Case & c) { c.cancel = 1; c.wl = 1; });
    // A cancel() that arrived between two prompts does not stop the next one.
    add("stale_cancel_dropped", [](Case & c) { c.entry = 1; c.cancel = 0; });
    add("cancel_during_generation", [](Case & c) { c.entry = 1; c.cancel = 1; });
    return v;
}

// ── pinned boundaries ───────────────────────────────────────────────────────
// Hand-computed from the native lines, independent of the transcription above.
// A transcription that quietly disagrees with the source is caught here.

struct Pin {
    const char * what;
    int64_t got;
    int64_t want;
};

static std::vector<Pin> build_pins() {
    return {
    {"ctx 10+30+8==48 passes",            sl_ctx_ok(10, 30, 48), 1},
    {"ctx 10+31+8==49 fails",             sl_ctx_ok(10, 31, 48), 0},
    {"ctx 0+0+8==8 passes",               sl_ctx_ok(0, 0, 8), 1},
    {"ctx 0+0+8==7 fails",                sl_ctx_ok(0, 0, 7), 0},
    {"replay budget==tokens passes",      sl_replay_ok(16, 16), 1},
    {"replay budget one short fails",     sl_replay_ok(15, 16), 0},
    {"replay budget zero, no tokens",     sl_replay_ok(0, 0), 1},
    {"loop at budget",                    sl_loop_step(5, 5, 0, 0, 0), 1},
    {"loop one below budget",             sl_loop_step(4, 5, 0, 0, 0), 0},
    {"loop budget before replay",         sl_loop_step(5, 5, 1, 5, 0), 1},
    {"loop replay stop",                  sl_loop_step(4, 8, 1, 4, 0), 2},
    {"loop replay before eog",            sl_loop_step(4, 8, 1, 4, 1), 2},
    {"loop eog",                          sl_loop_step(4, 8, 0, 0, 1), 3},
    {"loop replay overshoot continues",   sl_loop_step(5, 8, 1, 4, 0), 0},
    {"abort on cancel",                   sl_abort(1, 0, 0), 1},
    {"abort on source",                   sl_abort(0, 1, 0), 1},
    {"abort on hook",                     sl_abort(0, 0, 1), 1},
    {"abort quiet",                       sl_abort(0, 0, 0), 0},
    {"room last slot",                    sl_draft_room(32, 31), 0},
    {"room one slot",                     sl_draft_room(32, 30), 1},
    {"width no room",                     sl_draft_width(1, 0, 4), 0},
    {"width no spec",                     sl_draft_width(0, 3, 4), 0},
    {"width room binds",                  sl_draft_width(1, 3, 4), 3},
    {"width dmax binds",                  sl_draft_width(1, 9, 4), 4},
    {"emit truncates group",              sl_emit_count(4, 5, 3), 2},
    {"emit at budget",                    sl_emit_count(4, 4, 4), 0},
    {"emit whole group",                  sl_emit_count(3, 8, 0), 3},
    {"cancel beats workload",             sl_step_verdict(1, 1, 1, 1, 1, 1), 11},
    {"cancel in prefill",                 sl_step_verdict(0, 1, 1, 1, 1, 1), 10},
    {"workload beats hook",               sl_step_verdict(1, 0, 1, 1, 1, 1), 20},
    {"hook beats source",                 sl_step_verdict(1, 0, 0, 1, 1, 1), 21},
    {"source needs overlap",              sl_step_verdict(1, 0, 0, 0, 1, 1), 22},
    {"source without overlap",            sl_step_verdict(1, 0, 0, 0, 1, 0), 23},
    {"result cancelled",                  sl_result(1, 0), 1},
    {"result cancelled workload",         sl_result(1, 1), 2},
    {"result plain",                      sl_result(0, 1), 0},
    {"stale cancel dropped",              sl_cancel_effective(1, 0), 0},
    {"cancel during generation",          sl_cancel_effective(0, 1), 1},
    {"n_prompt after reuse",              sl_n_prompt_reported(140, 100), 40},
    {"n_past chat",                       sl_n_past(1, 7, 10, 3), 7},
    {"n_past oneshot",                    sl_n_past(0, 7, 10, 3), 13},
    {"trim end under speculation",        sl_trim_end(1, 20, 5), 25},
    {"trim skipped without speculation",  sl_trim_end(0, 20, 5), -1},
    };
}

int main() {
    // The generated module requires its runtime to be initialised before any
    // exported call, and the pinned table calls exports, so it is built here.
    goose_init(0, nullptr);
    const std::vector<Case> cs = build_cases();
    if (cs.size() < 20) {
        std::printf("  FAIL table holds %zu cases, 20 required\n", cs.size());
        return 1;
    }

    for (const Case & c : cs) {
        ++cases;
        expect(c.name, "abort", sl_abort(c.cancel, c.src, c.hook), nat_abort(c.cancel, c.src, c.hook));
        expect(c.name, "loop_step", sl_loop_step(c.ngen, c.budget, c.replay, c.rtok, c.eog),
               nat_loop_step(c.ngen, c.budget, c.replay, c.rtok, c.eog));
        expect(c.name, "ctx_ok", sl_ctx_ok(c.prompt, c.budget, c.nctx), nat_ctx_ok(c.prompt, c.budget, c.nctx));
        expect(c.name, "replay_ok", sl_replay_ok(c.budget, c.rtok), nat_replay_ok(c.budget, c.rtok));
        expect(c.name, "draft_room", sl_draft_room(c.budget, c.ngen), nat_draft_room(c.budget, c.ngen));
        expect(c.name, "draft_width", sl_draft_width(c.spec, c.budget - c.ngen - 1, c.dmax),
               nat_draft_width(c.spec, c.budget - c.ngen - 1, c.dmax));
        expect(c.name, "emit_count", sl_emit_count(c.confirmed, c.budget, c.ngen),
               nat_emit_count(c.confirmed, c.budget, c.ngen));
        expect(c.name, "step_verdict", sl_step_verdict(c.phase, c.cancel, c.wl_err, c.hook, c.src, c.overlap),
               nat_step_verdict(c.phase, c.cancel, c.wl_err, c.hook, c.src, c.overlap));
        expect(c.name, "result", sl_result(c.cancel, c.wl), nat_result(c.cancel, c.wl));
        expect(c.name, "cancel_effective", sl_cancel_effective(c.entry, c.cancel),
               nat_cancel_effective(c.entry, c.cancel));
        expect(c.name, "n_prompt_reported", sl_n_prompt_reported(c.prompt, c.ncommon),
               nat_n_prompt_reported(c.prompt, c.ncommon));
        expect(c.name, "n_past", sl_n_past(c.chat, c.kv, c.prompt, c.ngen), nat_n_past(c.chat, c.kv, c.prompt, c.ngen));
        expect(c.name, "trim_end", sl_trim_end(c.spec, c.prompt, c.ngen), nat_trim_end(c.spec, c.prompt, c.ngen));
    }

    const std::vector<Pin> pins = build_pins();
    cases += (int) pins.size();
    for (const Pin & p : pins) expect("pin", p.what, p.got, p.want);

    std::printf("FG577_S5 cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails == 0 ? 0 : 1;
}
