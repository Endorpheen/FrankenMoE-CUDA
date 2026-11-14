// Differential gate for P6-11 (speculative decoding: draft source, accept, draft state and
// budget). No model, no KV, no llama.cpp: the fixture carries a transcription of the native
// functions and the Goose module carries the port; both evaluate the same synthetic corpus in
// one process and every field is compared exactly.
//
// Native anchors, cited at each transcription:
//   runtime/core/src/engine/ngram_draft.cpp:10-98          ngram_draft
//   runtime/core/src/engine/session.cpp:1676-1740          the draft budget and the verify width
//   runtime/core/src/engine/session.cpp:1766-1795          the accept loop
//   runtime/core/src/engine/session.cpp:1810-1830          the catch-up and the rollback
//   runtime/core/src/engine/session.cpp:1848-1852          the emit bound
//   runtime/core/src/engine/session.cpp:1884-1890          n_past and the next logits row
//   runtime/core/src/engine/session.cpp:1899-1912          the end-of-turn trim
//   upstream/llama.cpp/common/speculative.cpp:1445-1470    the chained-head clamp
//   upstream/llama.cpp/common/speculative.cpp:1700-1800    the p_min width and the accept seed
//
// The corpus is synthetic token ids in [0, 5) plus an end-of-generation sentinel, scripted
// confidences in [0, 1], and positions from an empty context to a few thousand. Everything is
// integer policy, so the comparison is exact equality, not a tolerance.
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "sdp-gen.h"

static int cases = 0;   // comparisons performed
static int fails = 0;

static void failv(const char * name, const char * detail) {
    std::printf("[FAIL] %s: %s\n", name, detail);
    ++fails;
}

static void eq(const char * name, long long want, long long got) {
    ++cases;
    if (want == got) return;
    char b[256];
    std::snprintf(b, sizeof b, "want %lld got %lld", want, got);
    failv(name, b);
}

static std::string show(const std::vector<int32_t> & v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(v[i]);
    }
    return s + "]";
}

static void eqv(const char * name, const std::vector<int32_t> & want, const std::vector<int32_t> & got) {
    ++cases;
    if (want == got) return;
    char b[512];
    std::snprintf(b, sizeof b, "want %s got %s", show(want).c_str(), show(got).c_str());
    failv(name, b);
}

static sl_i32 si32(const std::vector<int32_t> & v) {
    return sl_i32{(int32_t *) (void *) v.data(), (int64_t) v.size()};
}
static sl_i32 si32_mut(std::vector<int32_t> & v) {
    return sl_i32{v.data(), (int64_t) v.size()};
}
static sl_i64 si64_mut(std::vector<long long> & v) {
    return sl_i64{(int64_t *) (void *) v.data(), (int64_t) v.size()};
}
static sl_f64 sf64(const std::vector<double> & v) {
    return sl_f64{(double *) (void *) v.data(), (int64_t) v.size()};
}

// ---------------------------------------------------------------------------------------------
// Native transcription: runtime/core/src/engine/ngram_draft.cpp:10-98
// ---------------------------------------------------------------------------------------------
static int ngram_draft(const std::vector<int32_t> & ctx,
                       int32_t last,
                       int n_max,
                       int min_match,
                       int max_match,
                       std::vector<int32_t> & out) {
    out.clear();

    if (n_max <= 0) return 0;
    if (min_match < 1) min_match = 1;
    if (max_match < min_match) return 0;

    const size_t L = ctx.size() + 1;
    const auto at = [&](size_t i) -> int32_t { return i < ctx.size() ? ctx[i] : last; };

    if (L < (size_t) min_match + 1) return 0;

    const size_t end = L - 1;
    const size_t j_min = (size_t) min_match - 1;

    int best_m = 0;
    size_t best_j = 0;

    for (size_t j = end - 1;; --j) {
        if (at(j) == at(end)) {
            const int lim = std::min(max_match, (int) (j + 1));
            int m = 1;
            while (m < lim && at(j - m) == at(end - m)) {
                ++m;
            }
            if (m > best_m) {
                best_m = m;
                best_j = j;
                if (best_m == max_match) break;
            }
        }
        if (j <= j_min) break;
    }

    if (best_m < min_match) return 0;

    const size_t avail = end - best_j;
    const size_t n = std::min((size_t) n_max, avail);
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(at(best_j + 1 + i));
    }
    return (int) n;
}

// ---------------------------------------------------------------------------------------------
// Native transcription: runtime/core/src/engine/session.cpp:1766-1795 (the accept loop).
// `tok` is confirmed before the loop; target[i] is the target's argmax at verify row i, which
// predicts drafts[i].
// ---------------------------------------------------------------------------------------------
struct AcceptOut {
    long long n_acc = 0;
    long long confirmed = 1; // `tok`
    long long eog_hit = 0;
    long long stop = 0;
};

static AcceptOut accept_native(const std::vector<int32_t> & drafts,
                               const std::vector<int32_t> & target,
                               const std::vector<int32_t> & eog) {
    AcceptOut a;
    const int n_draft = (int) drafts.size();
    a.stop = n_draft;
    for (int i = 0; i < n_draft; ++i) {
        const int32_t want = target[(size_t) i];
        if (want != drafts[(size_t) i]) { // session.cpp:1778
            a.stop = i;
            break;
        }
        ++a.n_acc;
        if (eog[(size_t) i] != 0) {
            a.eog_hit = 1;
            a.stop = i;
            break;
        }
        a.confirmed += 1;
        a.stop = i + 1;
    }
    return a;
}

// ---------------------------------------------------------------------------------------------
// Native transcription: upstream/llama.cpp/common/speculative.cpp:1700-1800, the MTP draft
// width. Each step decodes and samples; a candidate below p_min stops the loop before the push,
// a push that reaches n_max stops it after, and a result below n_min is cleared.
// ---------------------------------------------------------------------------------------------
struct WidthOut {
    long long drafted = 0;
    long long cleared = 0;
    long long steps = 0;
    long long ran_out = 0;
};

static WidthOut mtp_width_native(const std::vector<double> & conf, int n_max, double p_min, int n_min) {
    WidthOut w;
    if (n_max <= 0) return w;
    std::vector<int32_t> result;
    bool drafting = true;
    size_t i = 0;
    while (drafting) {
        if (i >= conf.size()) {
            w.ran_out = 1; // the head would have decoded another step
            break;
        }
        w.steps += 1;
        const double p = conf[i];
        if (p < p_min) { // speculative.cpp:1720
            break;
        }
        result.push_back((int32_t) i); // the id is irrelevant here, only the width is decided
        if (n_max <= (int) result.size()) { // speculative.cpp:1734
            break;
        }
        ++i;
    }
    if ((int) result.size() < n_min) { // speculative.cpp:1781
        w.cleared = 1;
        result.clear();
    }
    w.drafted = (long long) result.size();
    return w;
}

// ---------------------------------------------------------------------------------------------
// Native transcription: speculative.cpp:1445-1470, the chained-head clamp.
// ---------------------------------------------------------------------------------------------
static long long mtp_head_clamp_native(long long n_max, long long n_mtp_layers, long long is_mem_shared) {
    const long long layers = std::max<long long>(1, n_mtp_layers);
    const bool chain_heads = layers > 1 && is_mem_shared == 0;
    if (chain_heads) return std::min<long long>(n_max, layers);
    return n_max;
}

// ---------------------------------------------------------------------------------------------
// Native transcription: session.cpp:1700-1706, `seq_rm(mem_dft, 0, n_past, -1)` modelled on a
// position set: the rewind drops every stored row at or above n_past.
// ---------------------------------------------------------------------------------------------
static long long mtp_rewind_removed(long long seq_pos_max, long long n_past) {
    if (seq_pos_max < n_past) return 0;
    return seq_pos_max - n_past + 1;
}

static long long mtp_rewind_missing(long long seq_pos_max, long long n_past) {
    if (seq_pos_max >= n_past) return 0;
    return n_past - 1 - seq_pos_max;
}

// ---------------------------------------------------------------------------------------------
// Native transcription: speculative.cpp:1792-1800, the accept seed.
// ---------------------------------------------------------------------------------------------
static long long mtp_seed_row(long long n_acc, long long n_rows) {
    if (n_rows <= 0) return 0; // the native early return: no seed
    return std::min<long long>(n_acc, n_rows - 1);
}

// ---------------------------------------------------------------------------------------------
// Section A: the n-gram draft source
// ---------------------------------------------------------------------------------------------
static void gate_ngram_one(const char * name,
                           const std::vector<int32_t> & corpus,
                           int n_max,
                           int min_match,
                           int max_match) {
    std::vector<int32_t> ctx(corpus.begin(), corpus.size() ? corpus.end() - 1 : corpus.end());
    const int32_t last = corpus.size() ? corpus.back() : 0;

    std::vector<int32_t> want;
    const int nw = ngram_draft(ctx, last, n_max, min_match, max_match, want);

    std::vector<int32_t> got((size_t) (n_max > 0 ? n_max : 0), 0);
    const long long ng = sdp_ngram_draft(si32(ctx), last, n_max, min_match, max_match, si32_mut(got));
    if (ng >= 0 && ng <= (long long) got.size()) got.resize((size_t) ng);
    else got.clear();

    eq(name, nw, ng);
    eqv(name, want, got);
}

static void gate_ngram() {
    // Nothing to go on: the steps that must cost exactly a plain decode.
    gate_ngram_one("ngram_empty", {}, 3, 3, 12);
    gate_ngram_one("ngram_one_token", {7}, 3, 3, 12);
    gate_ngram_one("ngram_short", {1, 2}, 3, 3, 12);
    gate_ngram_one("ngram_no_repeat", {1, 2, 3, 4, 5, 6}, 3, 3, 12);
    // A repetition shorter than the gate is not evidence.
    gate_ngram_one("ngram_below_min_match", {1, 2, 1, 2, 9}, 3, 3, 12);
    gate_ngram_one("ngram_exact_min_match", {1, 2, 3, 1, 2, 3, 9}, 3, 3, 12);
    // A run of one token predicts itself through overlap.
    gate_ngram_one("ngram_run", {5, 5, 5, 5, 5, 5, 5}, 3, 3, 12);
    gate_ngram_one("ngram_run_short", {5, 5, 5, 5, 5}, 8, 2, 12);
    // A copied segment: the draft is the tokens that followed the earlier copy.
    gate_ngram_one("ngram_copy", {1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4}, 5, 3, 12);
    gate_ngram_one("ngram_copy_long", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 2, 3, 4, 5, 6}, 8, 3, 64);
    // Two matches of equal length: the most recent one wins.
    gate_ngram_one("ngram_tie_recent", {1, 2, 9, 1, 2, 8, 3, 1, 2}, 4, 2, 12);
    // max_match caps the scan and the draft length through `avail`.
    gate_ngram_one("ngram_capped", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 7},
                   8, 3, 4);
    // n_max caps the draft below what the corpus could supply.
    gate_ngram_one("ngram_nmax", {2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2}, 5, 3, 12);
    gate_ngram_one("ngram_nmax_one", {2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2}, 1, 3, 12);
    gate_ngram_one("ngram_nmax_zero", {2, 3, 4, 5, 6, 7, 2}, 0, 3, 12);
    // min_match is floored at 1 and a max_match below it drafts nothing.
    gate_ngram_one("ngram_min_zero", {2, 3, 4, 2, 3, 4}, 4, 0, 12);
    gate_ngram_one("ngram_max_below_min", {2, 3, 4, 2, 3, 4}, 4, 5, 4);
    // A long corpus: a 12-token block repeated, then a partial copy.
    {
        std::vector<int32_t> c;
        for (int r = 0; r < 3; ++r) {
            for (int i = 0; i < 12; ++i) c.push_back(i);
        }
        for (int i = 0; i < 5; ++i) c.push_back(i);
        gate_ngram_one("ngram_block", c, 8, 3, 12);
        gate_ngram_one("ngram_block_min12", c, 8, 12, 12);
        gate_ngram_one("ngram_block_cap6", c, 8, 3, 6);
    }

    // Deterministic random corpora: a small alphabet so repetitions are common.
    uint64_t s = 0x9e3779b97f4a7c15ull;
    auto rnd = [&]() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (int) ((s >> 33) % 1000); };
    const int alphabets[3] = {2, 3, 5};
    const int params[4][3] = {{3, 3, 12}, {1, 1, 64}, {8, 2, 4}, {4, 4, 12}};
    char nm[64];
    for (int c = 0; c < 36; ++c) {
        const int alpha = alphabets[c % 3];
        const int len = 4 + rnd() % 40;
        std::vector<int32_t> corpus;
        for (int i = 0; i < len; ++i) corpus.push_back(rnd() % alpha);
        const int * p = params[c % 4];
        std::snprintf(nm, sizeof nm, "ngram_rand_%02d", c);
        gate_ngram_one(nm, corpus, p[0], p[1], p[2]);
    }
}

// ---------------------------------------------------------------------------------------------
// Section B: the accept decision and the emit bound
// ---------------------------------------------------------------------------------------------
static void gate_accept_one(const char * name,
                            const std::vector<int32_t> & drafts,
                            const std::vector<int32_t> & target,
                            const std::vector<int32_t> & eog) {
    const AcceptOut want = accept_native(drafts, target, eog);

    std::vector<long long> out(5, -1);
    const long long rc = sdp_accept(si32(drafts), si32(target), si32(eog), si64_mut(out));
    eq(name, 0, rc);
    eq(name, want.n_acc, out[0]);
    eq(name, want.confirmed, out[1]);
    eq(name, want.eog_hit, out[2]);
    eq(name, want.stop, out[3]);
    eq(name, (long long) drafts.size(), out[4]);
}

static void gate_accept() {
    gate_accept_one("acc_all", {10, 11, 12}, {10, 11, 12}, {0, 0, 0});
    gate_accept_one("acc_first_mismatch", {10, 11, 12}, {99, 11, 12}, {0, 0, 0});
    gate_accept_one("acc_mid_mismatch", {10, 11, 12}, {10, 99, 12}, {0, 0, 0});
    gate_accept_one("acc_last_mismatch", {10, 11, 12}, {10, 11, 99}, {0, 0, 0});
    gate_accept_one("acc_none", {10, 11, 12}, {99, 98, 97}, {0, 0, 0});
    gate_accept_one("acc_empty", {}, {}, {});
    gate_accept_one("acc_one", {10}, {10}, {0});
    // An accepted EOG is in the KV, counted, and never emitted.
    gate_accept_one("acc_eog_first", {10, 11, 12}, {10, 11, 12}, {1, 0, 0});
    gate_accept_one("acc_eog_second", {10, 11, 12}, {10, 11, 12}, {0, 1, 0});
    gate_accept_one("acc_eog_last", {10, 11, 12}, {10, 11, 12}, {0, 0, 1});
    // EOG past a mismatch is never reached.
    gate_accept_one("acc_eog_unreached", {10, 11, 12}, {10, 99, 12}, {0, 0, 1});

    uint64_t s = 0x2545f4914f6cdd1dull;
    auto rnd = [&]() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (int) ((s >> 33) % 1000); };
    char nm[64];
    for (int c = 0; c < 40; ++c) {
        const int n = rnd() % 5;
        std::vector<int32_t> drafts, target, eog;
        for (int i = 0; i < n; ++i) {
            const int32_t d = 100 + rnd() % 4;
            drafts.push_back(d);
            // agreement with probability about two thirds
            target.push_back(rnd() % 3 == 0 ? (int32_t) (200 + rnd() % 4) : d);
            eog.push_back(0);
        }
        if (n > 0 && rnd() % 4 == 0) eog[(size_t) (rnd() % n)] = 1;
        std::snprintf(nm, sizeof nm, "acc_rand_%02d", c);
        gate_accept_one(nm, drafts, target, eog);
    }

    // The emit bound: min(confirmed, n_predict - n_gen), floored at 0.
    const long long np[6] = {1, 2, 3, 8, 32, 32};
    const long long ng[6] = {0, 0, 2, 7, 31, 32};
    const long long cf[6] = {1, 5, 4, 9, 2, 1};
    for (int i = 0; i < 6; ++i) {
        const long long left = np[i] - ng[i];
        long long want = cf[i] < left ? cf[i] : left;
        if (want < 0) want = 0;
        eq("emit_grid", want, sdp_emit_count(np[i], ng[i], cf[i]));
    }
    // A whole group that does not fit is cut by the caller's budget, not by the accept.
    eq("emit_cut", 1, sdp_emit_count(8, 7, 4));
    eq("emit_zero", 0, sdp_emit_count(8, 8, 4));
    eq("emit_over", 0, sdp_emit_count(8, 12, 4));
    eq("emit_all", 4, sdp_emit_count(8, 0, 4));
}

// ---------------------------------------------------------------------------------------------
// Section C: the MTP draft state
// ---------------------------------------------------------------------------------------------
static void gate_mtp_width_one(const char * name,
                               const std::vector<double> & conf,
                               int n_max,
                               double p_min,
                               int n_min) {
    const WidthOut want = mtp_width_native(conf, n_max, p_min, n_min);

    std::vector<long long> out(4, -1);
    const long long rc = sdp_mtp_width(sf64(conf), n_max, p_min, n_min, si64_mut(out));
    eq(name, 0, rc);
    eq(name, want.drafted, out[0]);
    eq(name, want.cleared, out[1]);
    eq(name, want.steps, out[2]);
    eq(name, want.ran_out, out[3]);
}

static void gate_mtp() {
    const std::vector<double> hi = {0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.65, 0.60};
    const std::vector<double> mixed = {0.95, 0.30, 0.90, 0.20};
    const std::vector<double> low = {0.10, 0.20, 0.30};
    const std::vector<double> edge = {0.5, 0.5, 0.4999999, 0.5};

    // p_min 0 is the measured configuration: the head drafts unconditionally up to n_max.
    gate_mtp_width_one("mtp_pmin0_nmax3", hi, 3, 0.0, 0);
    gate_mtp_width_one("mtp_pmin0_nmax8", hi, 8, 0.0, 0);
    gate_mtp_width_one("mtp_pmin0_nmax1", hi, 1, 0.0, 0);
    gate_mtp_width_one("mtp_pmin0_nmax0", hi, 0, 0.0, 0);
    gate_mtp_width_one("mtp_pmin0_short_conf", {0.9, 0.8}, 8, 0.0, 0);
    // The floor truncates adaptively: the step that decodes and is rejected costs a step.
    gate_mtp_width_one("mtp_pmin_half_hi", hi, 8, 0.5, 0);
    gate_mtp_width_one("mtp_pmin_half_mixed", mixed, 8, 0.5, 0);
    gate_mtp_width_one("mtp_pmin_half_low", low, 8, 0.5, 0);
    gate_mtp_width_one("mtp_pmin_boundary", edge, 8, 0.5, 0);
    gate_mtp_width_one("mtp_pmin_one", hi, 8, 1.0, 0);
    // n_min clears a result that is too short; the session passes 0.
    gate_mtp_width_one("mtp_nmin_clears", {0.9}, 8, 0.0, 2);
    gate_mtp_width_one("mtp_nmin_keeps", {0.9, 0.8}, 8, 0.0, 2);
    gate_mtp_width_one("mtp_nmin_pmin_clears", mixed, 8, 0.5, 1);

    // Deterministic random confidences.
    uint64_t s = 0x180ec6d33cfd0abaull;
    auto rnd = [&]() { s = s * 6364136223846793005ull + 1442695040888963407ull; return (int) ((s >> 33) % 1000); };
    char nm[64];
    for (int c = 0; c < 24; ++c) {
        const int n = rnd() % 9;
        std::vector<double> conf;
        for (int i = 0; i < n; ++i) conf.push_back((double) (rnd() % 101) / 100.0);
        const int n_max = 1 + rnd() % 8;
        const double p_min = (rnd() % 4 == 0) ? 0.0 : (double) (rnd() % 101) / 100.0;
        std::snprintf(nm, sizeof nm, "mtp_rand_%02d", c);
        gate_mtp_width_one(nm, conf, n_max, p_min, 0);
    }

    // The chained-head clamp.
    const long long cl[8][3] = {{3, 1, 0}, {3, 2, 0}, {8, 2, 1}, {8, 5, 0}, {1, 4, 0}, {0, 3, 0}, {8, 0, 0}, {8, 12, 0}};
    for (int i = 0; i < 8; ++i) {
        eq("mtp_clamp", mtp_head_clamp_native(cl[i][0], cl[i][1], cl[i][2]),
           sdp_mtp_head_clamp(cl[i][0], cl[i][1], cl[i][2]));
    }

    // The draft-context rewind: rows above n_past are drafting leftovers.
    const long long rw[7][2] = {{-1, 0}, {0, 0}, {3, 0}, {3, 4}, {7, 8}, {10, 10}, {100, 3}};
    for (int i = 0; i < 7; ++i) {
        std::vector<long long> out(2, -1);
        eq("mtp_rewind_rc", 0, sdp_mtp_rewind(rw[i][0], rw[i][1], si64_mut(out)));
        eq("mtp_rewind_removed", mtp_rewind_removed(rw[i][0], rw[i][1]), out[0]);
        eq("mtp_rewind_missing", mtp_rewind_missing(rw[i][0], rw[i][1]), out[1]);
    }
    // A draft context that never saw the last prefill ubatch leaves a hole; the native only
    // warns about it, so the port reports the size of the hole instead of failing.
    {
        std::vector<long long> out(2, -1);
        (void) sdp_mtp_rewind(-1, 0, si64_mut(out));
        eq("mtp_rewind_empty", 0, out[0]);
        eq("mtp_rewind_empty_missing", 0, out[1]);
    }

    // The catch-up width and the seed row.
    for (int acc = 0; acc <= 4; ++acc) {
        for (int rows = 0; rows <= 6; ++rows) {
            std::vector<long long> out(3, -1);
            eq("mtp_catchup_rc", 0, sdp_mtp_catchup(acc, rows, si64_mut(out)));
            eq("mtp_catchup_rows", 1 + acc, out[0]);
            eq("mtp_catchup_seed", mtp_seed_row(acc, rows), out[1]);
            // short_rows flags the native early return and the clamped seed.
            const long long want_short = (rows <= 0 || mtp_seed_row(acc, rows) != acc) ? 1 : 0;
            eq("mtp_catchup_short", want_short, out[2]);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Section D: the budget, the rollback and the end-of-turn trim
// ---------------------------------------------------------------------------------------------
struct PlanOut {
    long long room = 0;
    long long n_max = 0;
    long long call = 0;
    long long wide = 0;
    long long n_draft = 0;
    long long width = 1;
};

// Native transcription: session.cpp:1676-1740.
static PlanOut step_plan_native(long long n_predict, long long n_gen, long long draft_max,
                                long long spec_on, long long n_draft_in) {
    PlanOut p;
    const int room = (int) (n_predict - n_gen - 1);
    p.room = room;
    long long n_draft = 0;
    if (spec_on && room > 0) {
        p.call = 1;
        p.n_max = std::min<long long>(draft_max, room);
        n_draft = n_draft_in;
        // the MTP arm's defensive resize, under the same guard
        if (n_draft > room) n_draft = (long long) room;
    }
    p.n_draft = n_draft;
    const bool wide = spec_on && n_draft > 0;
    p.wide = wide ? 1 : 0;
    p.width = wide ? 1 + n_draft : 1;
    return p;
}

static void gate_budget() {
    const long long np[5] = {1, 2, 3, 8, 32};
    const long long ng[5] = {0, 1, 2, 7, 31};
    const long long dm[3] = {1, 3, 8};
    const long long nd[4] = {0, 1, 3, 8};
    for (int a = 0; a < 5; ++a) {
        for (int b = 0; b < 5; ++b) {
            for (int c = 0; c < 3; ++c) {
                for (int so = 0; so <= 1; ++so) {
                    for (int d = 0; d < 4; ++d) {
                        const PlanOut want = step_plan_native(np[a], ng[b], dm[c], so, nd[d]);
                        std::vector<long long> out(6, -1);
                        eq("plan_rc", 0, sdp_step_plan(np[a], ng[b], dm[c], so, nd[d], si64_mut(out)));
                        eq("plan_room", want.room, out[0]);
                        eq("plan_nmax", want.n_max, out[1]);
                        eq("plan_call", want.call, out[2]);
                        eq("plan_wide", want.wide, out[3]);
                        eq("plan_ndraft", want.n_draft, out[4]);
                        eq("plan_width", want.width, out[5]);
                    }
                }
            }
        }
    }

    // The rollback: keep = n_past + 1 + n_acc, only when something was rejected.
    const long long past[3] = {0, 5, 1000};
    for (int a = 0; a < 3; ++a) {
        for (int nd = 0; nd <= 4; ++nd) {
            for (int acc = 0; acc <= nd; ++acc) {
                std::vector<long long> out(3, -1);
                eq("rb_rc", 0, sdp_rollback(past[a], nd, acc, si64_mut(out)));
                const long long want_needed = acc < nd ? 1 : 0;
                eq("rb_needed", want_needed, out[0]);
                eq("rb_keep", past[a] + 1 + acc, out[1]);
                eq("rb_rejected", nd - acc, out[2]);
            }
        }
    }

    // The advance: n_past += 1 + n_acc and the logits row.
    for (int a = 0; a < 3; ++a) {
        for (int acc = 0; acc <= 4; ++acc) {
            for (int wide = 0; wide <= 1; ++wide) {
                std::vector<long long> out(2, -1);
                eq("adv_rc", 0, sdp_step_advance(past[a], acc, wide, si64_mut(out)));
                eq("adv_past", past[a] + 1 + acc, out[0]);
                eq("adv_row", wide ? acc : -1, out[1]);
            }
        }
    }

    // The trim: emitted_end = n_prompt + n_gen; rows ahead of it are removed.
    const long long pr[3] = {0, 7, 3000};
    for (int a = 0; a < 3; ++a) {
        for (int g = 0; g <= 8; ++g) {
            for (int d = -2; d <= 4; ++d) {
                const long long end = pr[a] + g;
                const long long kv = end + d;
                if (kv < 0) continue;
                std::vector<long long> out(2, -1);
                eq("tt_rc", 0, sdp_tail_trim(pr[a], g, kv, si64_mut(out)));
                eq("tt_end", end, out[0]);
                eq("tt_removed", kv > end ? kv - end : 0, out[1]);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Section E: a whole speculative run over a scripted model
//
// The model is a token function T(pos); the draft source is the n-gram matcher over the tokens
// emitted so far. The target's argmax at verify row i is T(n_past + i + 1), which is exactly what
// the native loop compares a draft against, so acceptance here is the real rule and the emitted
// sequence must equal plain greedy decoding - no approximation enters the loop.
// ---------------------------------------------------------------------------------------------
struct Script {
    std::vector<int32_t> tokens; // T(pos), the model's own continuation
    int32_t eog = 999;           // the end-of-generation id for this script
};

static int32_t script_tok(const Script & sc, long long pos) {
    if (pos < 0) return 0;
    if ((long long) sc.tokens.size() <= pos) return 0;
    return sc.tokens[(size_t) pos];
}

struct RunOut {
    std::vector<int32_t> emitted;
    std::vector<long long> trace; // per step: n_draft, n_acc, wide, keep, row
    long long n_past = 0;
    long long kv_end = 0;
    long long drafted = 0;
    long long accepted = 0;
    long long drafted_steps = 0;
    long long decodes = 0;
    long long trims = 0;
    long long final_end = 0;
};

// n_prompt tokens are the prompt; generation starts at that position.
static RunOut run_loop(const Script & sc, long long n_prompt, long long n_predict,
                       int draft_max, int min_match, bool spec_on, bool port) {
    RunOut r;
    std::vector<int32_t> history; // positions 0..n_past-1
    for (long long i = 0; i < n_prompt; ++i) history.push_back(script_tok(sc, i));

    long long n_past = n_prompt;
    long long kv_end = n_prompt; // stored rows, exclusive
    long long n_gen = 0;
    int32_t tok = script_tok(sc, n_past);

    while (n_gen < n_predict) {
        if (tok == sc.eog) break;

        // ── draft ── (session.cpp:1676-1712)
        long long n_draft = 0;
        std::vector<int32_t> drafts;
        const long long room = n_predict - n_gen - 1;
        if (spec_on && room > 0) {
            if (port) {
                // The plan sizes the cap; the source is asked for at most n_max tokens.
                std::vector<long long> plan(6, -1);
                eq("loop_plan_rc", 0, sdp_step_plan(n_predict, n_gen, draft_max, 1, 0, si64_mut(plan)));
                const long long n_max = plan[1];
                std::vector<int32_t> buf((size_t) (n_max > 0 ? n_max : 0), 0);
                const long long got = sdp_ngram_draft(si32(history), tok, n_max, min_match, 12, si32_mut(buf));
                if (got >= 0 && got <= (long long) buf.size()) buf.resize((size_t) got);
                else buf.clear();
                drafts = buf;
                // Re-plan with the actual proposal: the defensive cap and the verify width.
                eq("loop_plan_rc", 0, sdp_step_plan(n_predict, n_gen, draft_max, 1,
                                                    (long long) drafts.size(), si64_mut(plan)));
                if ((long long) drafts.size() > plan[4]) drafts.resize((size_t) plan[4]);
                n_draft = plan[4];
            } else {
                const int n_max = (int) std::min<long long>(draft_max, room);
                n_draft = ngram_draft(history, tok, n_max, min_match, 12, drafts);
            }
        }

        // ── verify ── row i is the target's argmax after verify_toks[i], so it predicts
        // drafts[i] at absolute position n_past + i + 1.
        std::vector<int32_t> target, eog;
        for (long long i = 0; i < n_draft; ++i) {
            const int32_t t = script_tok(sc, n_past + i + 1);
            target.push_back(t);
            eog.push_back(t == sc.eog ? 1 : 0);
        }

        long long n_acc = 0;
        long long confirmed = 1;
        long long eog_hit = 0;
        if (port) {
            std::vector<long long> ao(5, -1);
            eq("loop_acc_rc", 0, sdp_accept(si32(drafts), si32(target), si32(eog), si64_mut(ao)));
            n_acc = ao[0];
            confirmed = ao[1];
            eog_hit = ao[2];
            if (spec_on) {
                std::vector<long long> plan(6, -1);
                eq("loop_plan_rc", 0, sdp_step_plan(n_predict, n_gen, draft_max, 1, n_draft, si64_mut(plan)));
                eq("loop_wide", n_draft > 0 ? 1 : 0, plan[3]);
                eq("loop_width", n_draft > 0 ? 1 + n_draft : 1, plan[5]);
            }
        } else {
            const AcceptOut a = accept_native(drafts, target, eog);
            n_acc = a.n_acc;
            confirmed = a.confirmed;
            eog_hit = a.eog_hit;
        }

        // ── emit ── (session.cpp:1834-1880) The confirmed token, then the accepted ones, cut by
        // the caller's remaining budget.
        long long emit = std::min<long long>(confirmed, n_predict - n_gen);
        if (emit < 0) emit = 0;
        if (port) eq("loop_emit", emit, sdp_emit_count(n_predict, n_gen, confirmed));

        long long emitted_now = 0;
        r.emitted.push_back(tok); // confirmed before the decode, always emitted
        history.push_back(tok);
        emitted_now += 1;
        for (long long i = 0; i < n_acc; ++i) {
            const int32_t t = target[(size_t) i];
            if (t == sc.eog) break;
            if (emitted_now >= emit) break;
            r.emitted.push_back(t);
            history.push_back(t);
            emitted_now += 1;
        }
        n_gen += emitted_now;

        // ── rollback / advance ──
        kv_end = n_past + 1 + n_draft; // the verify batch wrote 1 + n_draft rows
        long long keep = n_past + 1 + n_acc;
        long long row = -1;
        if (port && spec_on) {
            std::vector<long long> rb(3, -1), ad(2, -1);
            eq("loop_rb_rc", 0, sdp_rollback(n_past, n_draft, n_acc, si64_mut(rb)));
            eq("loop_rb_needed", n_acc < n_draft ? 1 : 0, rb[0]);
            eq("loop_rb_keep", keep, rb[1]);
            if (rb[0] != 0) kv_end = rb[1];
            eq("loop_adv_rc", 0, sdp_step_advance(n_past, n_acc, n_draft > 0 ? 1 : 0, si64_mut(ad)));
            eq("loop_adv_past", n_past + 1 + n_acc, ad[0]);
            row = ad[1];
            if (n_draft > 0) eq("loop_adv_row", n_acc, row);
            else eq("loop_adv_row", -1, row);
        } else {
            if (n_acc < n_draft) kv_end = keep;
            if (spec_on && n_draft > 0) row = n_acc; // session.cpp:1889
        }
        r.trace.push_back(n_draft);
        r.trace.push_back(n_acc);
        r.trace.push_back(n_draft > 0 ? 1 : 0);
        r.trace.push_back(keep);
        r.trace.push_back(row);

        n_past = n_past + 1 + n_acc;
        r.drafted += n_draft;
        r.accepted += n_acc;
        if (n_draft > 0) r.drafted_steps += 1;
        r.decodes += 1;
        if (eog_hit) break;
        tok = script_tok(sc, n_past);
    }

    // The end-of-turn trim: the KV can stand ahead of what was emitted.
    if (spec_on) {
        const long long emitted_end = n_prompt + n_gen;
        long long removed = kv_end > emitted_end ? kv_end - emitted_end : 0;
        std::vector<long long> tt(2, -1);
        eq("loop_tt_rc", 0, sdp_tail_trim(n_prompt, n_gen, kv_end, si64_mut(tt)));
        eq("loop_tt_end", emitted_end, tt[0]);
        eq("loop_tt_removed", removed, tt[1]);
        kv_end = emitted_end;
        r.trims = removed;
        r.final_end = emitted_end;
    } else {
        r.final_end = kv_end;
    }
    r.n_past = n_past;
    r.kv_end = kv_end;
    return r;
}

static long long g_drafted = 0;
static long long g_accepted = 0;
static long long g_steps = 0;
static long long g_decodes = 0;
static long long g_trims = 0;
static long long g_emitted = 0;
static long long g_runs = 0;
static long long g_eog_runs = 0;
static long long g_floor_runs = 0;

static void gate_loop() {
    // A scripted continuation: a repeating block with a run and a break, so the n-gram source
    // both matches and abstains.
    Script sc;
    const int32_t pattern[5] = {1, 2, 3, 4, 5};
    for (int r = 0; r < 12; ++r) {
        for (int i = 0; i < 5; ++i) sc.tokens.push_back(pattern[i]);
    }
    for (int i = 0; i < 6; ++i) sc.tokens.push_back(3);
    for (int r = 0; r < 6; ++r) {
        for (int i = 0; i < 5; ++i) sc.tokens.push_back(pattern[i]);
    }

    const long long prompts[3] = {8, 15, 22};
    const int draft_max[3] = {1, 3, 8};
    const int min_match[2] = {2, 3};

    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) {
            for (int c = 0; c < 2; ++c) {
                const RunOut nat = run_loop(sc, prompts[a], 24, draft_max[b], min_match[c], true, false);
                const RunOut prt = run_loop(sc, prompts[a], 24, draft_max[b], min_match[c], true, true);
                eqv("loop_emitted", nat.emitted, prt.emitted);
                eq("loop_trace_len", (long long) nat.trace.size(), (long long) prt.trace.size());
                for (size_t i = 0; i < nat.trace.size() && i < prt.trace.size(); ++i) {
                    eq("loop_trace", nat.trace[i], prt.trace[i]);
                }
                eq("loop_kv_end", nat.kv_end, prt.kv_end);
                eq("loop_n_past", nat.n_past, prt.n_past);
                eq("loop_drafted", nat.drafted, prt.drafted);
                eq("loop_accepted", nat.accepted, prt.accepted);
                eq("loop_steps", nat.drafted_steps, prt.drafted_steps);
                eq("loop_decodes", nat.decodes, prt.decodes);
                eq("loop_trims", nat.trims, prt.trims);
                g_drafted += nat.drafted;
                g_accepted += nat.accepted;
                g_steps += nat.drafted_steps;
                g_decodes += nat.decodes;
                g_trims += nat.trims;
                g_emitted += (long long) nat.emitted.size();
                g_runs += 1;

                // No approximation: the speculative output is the plain greedy output.
                const RunOut plain = run_loop(sc, prompts[a], 24, draft_max[b], min_match[c], false, false);
                eqv("loop_plain", plain.emitted, nat.emitted);
                // A run that drafts nothing must cost the plain path.
                if (nat.drafted_steps == 0) {
                    eq("loop_floor", plain.decodes, nat.decodes);
                    g_floor_runs += 1;
                }
            }
        }
    }

    // An end-of-generation token the draft source actually PROPOSES: the pattern contains the
    // EOG id, so the draft reaches it before the plain path would. The accepted EOG is in the KV
    // and never emitted, so the trim is what keeps the KV and the caller in step.
    {
        Script e;
        e.eog = 4;
        for (int r = 0; r < 10; ++r) {
            for (int i = 1; i <= 4; ++i) e.tokens.push_back(i);
        }
        for (int pr = 0; pr < 3; ++pr) {
            const long long prompt = 6 + pr * 4;
            for (int dm = 0; dm < 3; ++dm) {
                const RunOut nat = run_loop(e, prompt, 24, draft_max[dm], 2, true, false);
                const RunOut prt = run_loop(e, prompt, 24, draft_max[dm], 2, true, true);
                eqv("loop_eog_emitted", nat.emitted, prt.emitted);
                eq("loop_eog_kv_end", nat.kv_end, prt.kv_end);
                eq("loop_eog_n_past", nat.n_past, prt.n_past);
                eq("loop_eog_trims", nat.trims, prt.trims);
                eq("loop_eog_trimmed", 1, nat.trims > 0 ? 1 : 0);
                const RunOut plain = run_loop(e, prompt, 24, draft_max[dm], 2, false, false);
                eqv("loop_eog_plain", plain.emitted, nat.emitted);
                g_drafted += nat.drafted;
                g_accepted += nat.accepted;
                g_steps += nat.drafted_steps;
                g_decodes += nat.decodes;
                g_trims += nat.trims;
                g_emitted += (long long) nat.emitted.size();
                g_runs += 1;
                g_eog_runs += 1;
            }
        }
    }

    // A caller budget that cuts a group short: accepted tokens are in the KV, not in the answer,
    // so the end-of-turn trim has to remove them.
    for (int np = 0; np < 3; ++np) {
        const long long predict = 13 + np * 5; // 13, 18, 23
        const RunOut nat = run_loop(sc, 8, predict, 8, 2, true, false);
        const RunOut prt = run_loop(sc, 8, predict, 8, 2, true, true);
        eqv("loop_cut_emitted", nat.emitted, prt.emitted);
        eq("loop_cut_n_past", nat.n_past, prt.n_past);
        eq("loop_cut_kv_end", nat.kv_end, prt.kv_end);
        eq("loop_cut_trims", nat.trims, prt.trims);
        // A source that honours its cap can never outrun the budget: room = remaining - 1 and
        // n_acc <= n_draft <= room, so the group always fits. The trim's other arm is the
        // accepted end-of-generation token, covered by the block above and by the tt grid.
        eq("loop_cut_filled", predict, (long long) nat.emitted.size());
        eq("loop_cut_no_trim", 0, nat.trims);
        g_drafted += nat.drafted;
        g_accepted += nat.accepted;
        g_steps += nat.drafted_steps;
        g_decodes += nat.decodes;
        g_trims += nat.trims;
        g_emitted += (long long) nat.emitted.size();
        g_runs += 1;
    }

    // A continuation with no repetition: the n-gram source drafts nothing, so every step must
    // cost exactly the plain decode - the floor the whole source rests on.
    {
        Script n;
        uint64_t s = 0x94d049bb133111ebull;
        for (int i = 0; i < 400; ++i) {
            s = s * 6364136223846793005ull + 1442695040888963407ull;
            n.tokens.push_back((int32_t) (7 + (s >> 33) % 97));
        }
        for (int dm = 0; dm < 3; ++dm) {
            const RunOut nat = run_loop(n, 16, 24, draft_max[dm], 3, true, false);
            const RunOut prt = run_loop(n, 16, 24, draft_max[dm], 3, true, true);
            eqv("loop_novel_emitted", nat.emitted, prt.emitted);
            eq("loop_novel_drafted", 0, nat.drafted);
            eq("loop_novel_kv_end", nat.kv_end, prt.kv_end);
            const RunOut plain = run_loop(n, 16, 24, draft_max[dm], 3, false, false);
            eq("loop_floor", plain.decodes, nat.decodes);
            eqv("loop_novel_plain", plain.emitted, nat.emitted);
            g_floor_runs += 1;
            g_drafted += nat.drafted;
            g_accepted += nat.accepted;
            g_steps += nat.drafted_steps;
            g_decodes += nat.decodes;
            g_trims += nat.trims;
            g_emitted += (long long) nat.emitted.size();
            g_runs += 1;
        }
    }
}

static void report(const char * name, int c0, int f0) {
    std::printf("FG626_SPC %-8s cases=%d failures=%d\n", name, cases - c0, fails - f0);
}

int main() {
    goose_init(0, nullptr);
    int c0 = cases, f0 = fails;
    gate_ngram();
    report("ngram", c0, f0);
    c0 = cases; f0 = fails;
    gate_accept();
    report("accept", c0, f0);
    c0 = cases; f0 = fails;
    gate_mtp();
    report("mtp", c0, f0);
    c0 = cases; f0 = fails;
    gate_budget();
    report("budget", c0, f0);
    c0 = cases; f0 = fails;
    gate_loop();
    report("loop", c0, f0);
    std::printf("FG626_SPC cases=%d failures=%d\n", cases, fails);
    std::printf("FG626_SPC loop runs=%lld emitted=%lld drafted=%lld accepted=%lld steps=%lld "
                "decodes=%lld trims=%lld eog_runs=%lld floor_runs=%lld\n",
                g_runs, g_emitted, g_drafted, g_accepted, g_steps, g_decodes, g_trims,
                g_eog_runs, g_floor_runs);
    return fails == 0 ? 0 : 1;
}
