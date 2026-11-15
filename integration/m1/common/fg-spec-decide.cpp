// Speculative-decoding decisions, native bodies and dispatch (staged S2.2 trial).
//
// Every native body cites the anchor it transcribes. The draft source has no transcription:
// fg_spec::ngram_draft_native calls bmoe::ngram_draft from
// runtime/core/src/engine/ngram_draft.cpp, compiled unmodified into this library.
//
// The loop model at the bottom is a model-free driver over a scripted token function: it is
// what makes the decisions observable end to end without llama.cpp, a model or a GPU.

#include "fg-spec-decide.h"

#include "bmoe/ngram_draft.h"

#ifdef FG_GOOSE_SPEC
#include "fg-goose-spec-bridge.h"
#endif

#include <algorithm>
#include <cstddef>

namespace fg_spec {

namespace {

// The scripted model: T(pos). Out of range is token 0, so a run cannot read past its tape.
int32_t script_tok(const script_t & sc, long long pos) {
    if (pos < 0) return 0;
    if ((long long) sc.tokens.size() <= pos) return 0;
    return sc.tokens[(size_t) pos];
}

} // namespace

// ---- native bodies -------------------------------------------------------------------------

// runtime/core/src/engine/ngram_draft.cpp:8-66, the real file. No transcription here: the
// native source is compiled into this library unchanged, so the OFF path is the shipped code.
int ngram_draft_native(const std::vector<int32_t> & ctx, int32_t last, int n_max,
                       int min_match, int max_match, std::vector<int32_t> & out) {
    return bmoe::ngram_draft(ctx, last, n_max, min_match, max_match, out);
}

// runtime/core/src/engine/session.cpp:1772-1800, the accept loop. The native body computes
// `want` with argmax(llama_get_logits_ith(ctx, i)) and asks llama_vocab_is_eog; both stay
// native and are supplied precomputed here (`target[i]`, `eog[i]`). What the carrier owns is
// only what the loop does with them.
accept_t accept_native(const std::vector<int32_t> & drafts, const std::vector<int32_t> & target,
                       const std::vector<int32_t> & eog, int32_t tok,
                       std::vector<int32_t> * confirmed) {
    if (confirmed) {
        confirmed->clear();
        confirmed->push_back(tok); // `tok` was settled before the decode
    }
    accept_t a;
    const int n_draft = (int) drafts.size();
    a.n_draft = n_draft;
    a.stop = n_draft;
    for (int i = 0; i < n_draft; ++i) {
        if ((size_t) i >= target.size()) {
            // No logits row: the caller's error, not a native case (the verify batch always
            // asks for 1 + n_draft rows). The port reports it as a rejection at i.
            a.stop = i;
            break;
        }
        const int32_t want = target[(size_t) i];
        if (want != drafts[(size_t) i]) { // session.cpp:1778
            a.stop = i;
            break;
        }
        ++a.n_acc; // accepted: it is in the KV whether or not the caller gets to see it
        if ((size_t) i < eog.size() && eog[(size_t) i] != 0) {
            a.eog_hit = 1; // end-of-generation is never emitted, here as on the plain path
            a.stop = i;
            break;
        }
        a.confirmed += 1;
        if (confirmed) confirmed->push_back(want);
    }
    return a;
}

// runtime/core/src/engine/session.cpp:1848, the emit loop bound:
//   for (size_t e = 0; e < confirmed.size() && n_gen < req.n_predict; ++e)
long long emit_count_native(long long n_predict, long long n_gen, long long confirmed) {
    if (n_gen >= n_predict) return 0;
    return std::min(confirmed, n_predict - n_gen);
}

// runtime/core/src/engine/session.cpp:1675-1740, the draft budget and the verify width.
plan_t step_plan_native(long long n_predict, long long n_gen, long long draft_max,
                        long long spec_on, long long n_draft_in) {
    plan_t p;
    p.room = n_predict - n_gen - 1; // tokens still wanted after `tok` itself
    long long n_draft = 0;
    if (spec_on != 0) {
        if (p.room > 0) {
            p.call = 1;
            p.n_max = std::min(draft_max, p.room);
            n_draft = n_draft_in;
            // The MTP arm's defensive resize after the source returns. The n-gram arm is
            // already capped by n_max <= room, so this is a no-op there.
            if (n_draft > p.room) n_draft = p.room;
        }
    }
    p.n_draft = n_draft;
    const bool wide = spec_on != 0 && n_draft > 0;
    p.wide = wide ? 1 : 0;
    p.width = wide ? 1 + n_draft : 1;
    return p;
}

// runtime/core/src/engine/session.cpp:1821-1826, `if (n_acc < n_draft) keep = n_past + 1 + n_acc`.
rollback_t rollback_native(long long n_past, long long n_draft, long long n_acc) {
    rollback_t r;
    r.keep = n_past + 1 + n_acc;
    if (n_acc < n_draft) {
        r.needed = 1;
        r.rejected = n_draft - n_acc;
    }
    return r;
}

// runtime/core/src/engine/session.cpp:1888-1889, `n_past += 1 + n_acc; row = wide ? n_acc : -1`.
advance_t advance_native(long long n_past, long long n_acc, long long wide) {
    advance_t a;
    a.n_past = n_past + 1 + n_acc;
    a.row = wide != 0 ? n_acc : -1;
    return a;
}

// runtime/core/src/engine/session.cpp:1899-1912, the end-of-turn trim.
trim_t tail_trim_native(long long n_prompt, long long n_gen, long long kv_end) {
    trim_t t;
    t.emitted_end = n_prompt + n_gen;
    if (kv_end > t.emitted_end) t.removed = kv_end - t.emitted_end;
    return t;
}

// ---- dispatched ----------------------------------------------------------------------------

int ngram_draft(const std::vector<int32_t> & ctx, int32_t last, int n_max, int min_match,
                int max_match, std::vector<int32_t> & out) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::ngram_draft(ctx, last, n_max, min_match, max_match, out);
#else
    return ngram_draft_native(ctx, last, n_max, min_match, max_match, out);
#endif
}

accept_t accept(const std::vector<int32_t> & drafts, const std::vector<int32_t> & target,
                const std::vector<int32_t> & eog, int32_t tok, std::vector<int32_t> * confirmed) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::accept(drafts, target, eog, tok, confirmed);
#else
    return accept_native(drafts, target, eog, tok, confirmed);
#endif
}

long long emit_count(long long n_predict, long long n_gen, long long confirmed) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::emit_count(n_predict, n_gen, confirmed);
#else
    return emit_count_native(n_predict, n_gen, confirmed);
#endif
}

plan_t step_plan(long long n_predict, long long n_gen, long long draft_max, long long spec_on,
                 long long n_draft_in) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::step_plan(n_predict, n_gen, draft_max, spec_on, n_draft_in);
#else
    return step_plan_native(n_predict, n_gen, draft_max, spec_on, n_draft_in);
#endif
}

rollback_t rollback(long long n_past, long long n_draft, long long n_acc) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::rollback(n_past, n_draft, n_acc);
#else
    return rollback_native(n_past, n_draft, n_acc);
#endif
}

advance_t advance(long long n_past, long long n_acc, long long wide) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::advance(n_past, n_acc, wide);
#else
    return advance_native(n_past, n_acc, wide);
#endif
}

trim_t tail_trim(long long n_prompt, long long n_gen, long long kv_end) {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::tail_trim(n_prompt, n_gen, kv_end);
#else
    return tail_trim_native(n_prompt, n_gen, kv_end);
#endif
}

long long goose_calls() {
#ifdef FG_GOOSE_SPEC
    return fg_goose_spec::calls();
#else
    return 0;
#endif
}

// ---- the loop ------------------------------------------------------------------------------

namespace {

// NATIVE selects the oracle: every decision in the loop is taken by the *_native body.
template <bool NATIVE>
struct select;

template <>
struct select<true> {
    static int draft(const std::vector<int32_t> & ctx, int32_t last, int n_max, int min_match,
                     int max_match, std::vector<int32_t> & out) {
        return ngram_draft_native(ctx, last, n_max, min_match, max_match, out);
    }
    static accept_t acc(const std::vector<int32_t> & d, const std::vector<int32_t> & t,
                        const std::vector<int32_t> & e, int32_t tok,
                        std::vector<int32_t> * c) {
        return accept_native(d, t, e, tok, c);
    }
    static long long emit(long long np, long long ng, long long cf) {
        return emit_count_native(np, ng, cf);
    }
    static plan_t plan(long long np, long long ng, long long dm, long long so, long long nd) {
        return step_plan_native(np, ng, dm, so, nd);
    }
    static rollback_t rb(long long np, long long nd, long long na) {
        return rollback_native(np, nd, na);
    }
    static advance_t adv(long long np, long long na, long long w) {
        return advance_native(np, na, w);
    }
    static trim_t trim(long long np, long long ng, long long ke) {
        return tail_trim_native(np, ng, ke);
    }
};

template <>
struct select<false> {
    static int draft(const std::vector<int32_t> & ctx, int32_t last, int n_max, int min_match,
                     int max_match, std::vector<int32_t> & out) {
        return ngram_draft(ctx, last, n_max, min_match, max_match, out);
    }
    static accept_t acc(const std::vector<int32_t> & d, const std::vector<int32_t> & t,
                        const std::vector<int32_t> & e, int32_t tok,
                        std::vector<int32_t> * c) {
        return accept(d, t, e, tok, c);
    }
    static long long emit(long long np, long long ng, long long cf) {
        return emit_count(np, ng, cf);
    }
    static plan_t plan(long long np, long long ng, long long dm, long long so, long long nd) {
        return step_plan(np, ng, dm, so, nd);
    }
    static rollback_t rb(long long np, long long nd, long long na) {
        return rollback(np, nd, na);
    }
    static advance_t adv(long long np, long long na, long long w) {
        return advance(np, na, w);
    }
    static trim_t trim(long long np, long long ng, long long ke) {
        return tail_trim(np, ng, ke);
    }
};

// The loop of runtime/core/src/engine/session.cpp:1660-1912, reduced to what this slice
// decides. Both arms run the same statements; only the decision bodies differ.
template <bool NATIVE>
run_out run_impl(const script_t & sc, long long n_prompt, long long n_predict, int draft_max,
                 int min_match, bool spec_on) {
    run_out r;
    std::vector<int32_t> history; // positions 0..n_past-1
    for (long long i = 0; i < n_prompt; ++i) history.push_back(script_tok(sc, i));

    long long n_past = n_prompt;
    long long kv_end = n_prompt; // rows stored, exclusive
    long long n_gen = 0;
    int32_t tok = script_tok(sc, n_past);

    while (n_gen < n_predict) {
        if (tok == sc.eog) break; // the pre-decode end-of-generation test, native

        // ---- draft ----
        long long n_draft = 0;
        std::vector<int32_t> drafts;
        if (spec_on) {
            // First call: room, the cap that goes in BEFORE drafting, and whether to call.
            const plan_t p0 = select<NATIVE>::plan(n_predict, n_gen, draft_max, 1, 0);
            if (p0.call != 0) {
                const long long cap = p0.n_max > 0 ? p0.n_max : 0;
                std::vector<int32_t> buf((size_t) cap, 0);
                const int got = select<NATIVE>::draft(history, tok, (int) cap, min_match, 12, buf);
                if (got > 0 && (long long) got <= cap) buf.resize((size_t) got);
                else buf.clear();
                drafts = buf;
                // Second call: the defensive cap on the proposal, and the verify width.
                const plan_t p1 =
                    select<NATIVE>::plan(n_predict, n_gen, draft_max, 1, (long long) drafts.size());
                if ((long long) drafts.size() > p1.n_draft) drafts.resize((size_t) p1.n_draft);
                n_draft = p1.n_draft;
            }
        }

        // ---- verify rows: the target's own argmax and its eog verdict, both supplied ----
        std::vector<int32_t> target, eogv;
        for (long long i = 0; i < n_draft; ++i) {
            const int32_t t = script_tok(sc, n_past + i + 1);
            target.push_back(t);
            eogv.push_back(t == sc.eog ? 1 : 0);
        }

        // ---- accept ----
        std::vector<int32_t> confirmed;
        accept_t acc;
        if (spec_on) {
            acc = select<NATIVE>::acc(drafts, target, eogv, tok, &confirmed);
        } else {
            // Speculation off: `tok` is the whole group (session.cpp skips the block).
            confirmed.clear();
            confirmed.push_back(tok);
            acc.n_acc = 0;
            acc.confirmed = 1;
            acc.eog_hit = 0;
            acc.stop = 0;
            acc.n_draft = 0;
        }

        // ---- emit: the confirmed prefix, cut by the caller's remaining budget ----
        const long long emit =
            spec_on ? select<NATIVE>::emit(n_predict, n_gen, acc.confirmed) : acc.confirmed;
        long long emitted_now = 0;
        for (size_t e = 0; e < confirmed.size() && emitted_now < emit; ++e) {
            r.emitted.push_back(confirmed[e]);
            history.push_back(confirmed[e]);
            ++emitted_now;
        }
        n_gen += emitted_now;

        // ---- rollback, advance ----
        kv_end = n_past + 1 + n_draft; // the verify batch stored 1 + n_draft rows
        long long keep = n_past + 1 + acc.n_acc;
        long long row = -1;
        if (spec_on) {
            const rollback_t rb = select<NATIVE>::rb(n_past, n_draft, acc.n_acc);
            keep = rb.keep;
            if (rb.needed != 0) kv_end = rb.keep;
            const advance_t ad = select<NATIVE>::adv(n_past, acc.n_acc, n_draft > 0 ? 1 : 0);
            row = ad.row;
            n_past = ad.n_past;
        } else {
            n_past = n_past + 1 + acc.n_acc;
        }

        r.trace.push_back(n_draft);
        r.trace.push_back(acc.n_acc);
        r.trace.push_back(n_draft > 0 ? 1 : 0);
        r.trace.push_back(keep);
        r.trace.push_back(row);

        r.drafted += n_draft;
        r.accepted += acc.n_acc;
        if (n_draft > 0) r.drafted_steps += 1;
        r.decodes += 1;
        r.eog_hits += acc.eog_hit;
        if (acc.eog_hit != 0) break;
        tok = script_tok(sc, n_past);
    }

    // ---- the end-of-turn trim ----
    if (spec_on) {
        const trim_t t = select<NATIVE>::trim(n_prompt, n_gen, kv_end);
        kv_end = t.emitted_end;
        r.trims = t.removed;
        r.final_end = t.emitted_end;
    } else {
        r.final_end = kv_end;
    }
    r.n_past = n_past;
    r.kv_end = kv_end;
    return r;
}

} // namespace

run_out run(const script_t & sc, long long n_prompt, long long n_predict, int draft_max,
            int min_match, bool spec_on) {
    return run_impl<false>(sc, n_prompt, n_predict, draft_max, min_match, spec_on);
}

run_out run_native(const script_t & sc, long long n_prompt, long long n_predict, int draft_max,
                   int min_match, bool spec_on) {
    return run_impl<true>(sc, n_prompt, n_predict, draft_max, min_match, spec_on);
}

} // namespace fg_spec
