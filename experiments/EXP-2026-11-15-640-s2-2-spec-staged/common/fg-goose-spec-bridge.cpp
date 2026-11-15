// Goose bridge for the speculative-decoding decisions (staged S2.2 trial).
// See fg-goose-spec-bridge.h for the contract; the module is
// goose/misc/spec-decode-decisions.goose (composed to goose-gen/spec-decide.goose, generated
// to goose-gen/spec-decide.c).

#include "fg-goose-spec-bridge.h"

#include "spec-decide.h"

#include <mutex>

namespace fg_goose_spec {

namespace {

long long g_calls = 0;

void init() {
    static std::once_flag once;
    std::call_once(once, []() { goose_init(0, nullptr); });
}

// A slice over a vector that the module may read; the empty vector stays a null pointer with
// len 0, which every export treats as "nothing there" before it dereferences.
sl_i32 sl(const std::vector<int32_t> & v) {
    return sl_i32{const_cast<int32_t *>(v.data()), (int64_t) v.size()};
}

sl_i64 sl64(int64_t * p, size_t n) {
    return sl_i64{p, (int64_t) n};
}

} // namespace

long long calls() { return g_calls; }

int ngram_draft(const std::vector<int32_t> & ctx, int32_t last, int n_max, int min_match,
                int max_match, std::vector<int32_t> & out) {
    init();
    ++g_calls;
    const int64_t n = sdp_ngram_draft(sl(ctx), last, (int64_t) n_max, (int64_t) min_match,
                                      (int64_t) max_match,
                                      sl_i32{out.data(), (int64_t) out.size()});
    if (n < 0) {
        out.clear();
        return 0;
    }
    if ((size_t) n < out.size()) out.resize((size_t) n);
    return (int) n;
}

fg_spec::accept_t accept(const std::vector<int32_t> & drafts,
                         const std::vector<int32_t> & target,
                         const std::vector<int32_t> & eog, int32_t tok,
                         std::vector<int32_t> * confirmed) {
    init();
    ++g_calls;
    int64_t out[5] = {-1, -1, -1, -1, -1};
    (void) sdp_accept(sl(drafts), sl(target), sl(eog), sl64(out, 5));
    fg_spec::accept_t a;
    a.n_acc = out[0];
    a.confirmed = out[1];
    a.eog_hit = out[2];
    a.stop = out[3];
    a.n_draft = out[4];
    if (confirmed) {
        // The module returns the length; the elements are the accepted drafts, which equal
        // the target's own tokens by construction.
        confirmed->clear();
        confirmed->push_back(tok);
        for (long long i = 0; i + 1 < a.confirmed && (size_t) i < drafts.size(); ++i) {
            confirmed->push_back(drafts[(size_t) i]);
        }
    }
    return a;
}

long long emit_count(long long n_predict, long long n_gen, long long confirmed) {
    init();
    ++g_calls;
    return sdp_emit_count(n_predict, n_gen, confirmed);
}

fg_spec::plan_t step_plan(long long n_predict, long long n_gen, long long draft_max,
                          long long spec_on, long long n_draft_in) {
    init();
    ++g_calls;
    int64_t out[6] = {-1, -1, -1, -1, -1, -1};
    (void) sdp_step_plan(n_predict, n_gen, draft_max, spec_on, n_draft_in, sl64(out, 6));
    fg_spec::plan_t p;
    p.room = out[0];
    p.n_max = out[1];
    p.call = out[2];
    p.wide = out[3];
    p.n_draft = out[4];
    p.width = out[5];
    return p;
}

fg_spec::rollback_t rollback(long long n_past, long long n_draft, long long n_acc) {
    init();
    ++g_calls;
    int64_t out[3] = {-1, -1, -1};
    (void) sdp_rollback(n_past, n_draft, n_acc, sl64(out, 3));
    fg_spec::rollback_t r;
    r.needed = out[0];
    r.keep = out[1];
    r.rejected = out[2];
    return r;
}

fg_spec::advance_t advance(long long n_past, long long n_acc, long long wide) {
    init();
    ++g_calls;
    int64_t out[2] = {-1, -1};
    (void) sdp_step_advance(n_past, n_acc, wide, sl64(out, 2));
    fg_spec::advance_t a;
    a.n_past = out[0];
    a.row = out[1];
    return a;
}

fg_spec::trim_t tail_trim(long long n_prompt, long long n_gen, long long kv_end) {
    init();
    ++g_calls;
    int64_t out[2] = {-1, -1};
    (void) sdp_tail_trim(n_prompt, n_gen, kv_end, sl64(out, 2));
    fg_spec::trim_t t;
    t.emitted_end = out[0];
    t.removed = out[1];
    return t;
}

} // namespace fg_goose_spec
