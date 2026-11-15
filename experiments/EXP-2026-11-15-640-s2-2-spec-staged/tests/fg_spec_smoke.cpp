// Differential gate for the staged S2.2 slice (speculative-decoding decisions).
//
// One binary per build, one corpus, no model, no llama.cpp, no KV, no GPU:
//   OFF: fg_spec::* dispatch to the native bodies, the Goose object is not linked.
//   ON : fg_spec::* dispatch into goose/misc/spec-decode-decisions.goose (sdp_*).
// In both builds the oracle is the *_native body in the same process, and the end-to-end
// section compares two whole loop transcripts taken from the same scripted model.
//
//   ./fg-spec-smoke          the gate; prints per-section counts, exits non-zero on a failure
//   ./fg-spec-smoke --dump   the loop transcripts only, for the OFF vs ON text diff
//
// Corpus: synthetic token ids, positions from an empty context to a few thousand, an
// end-of-generation sentinel. Everything is integer policy, so equality is exact.
#include "fg-spec-decide.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static int cases = 0;
static int fails = 0;

static void eq(const char * name, long long want, long long got) {
    ++cases;
    if (want == got) return;
    char b[256];
    std::snprintf(b, sizeof b, "want %lld got %lld", want, got);
    std::printf("[FAIL] %s: %s\n", name, b);
    ++fails;
}

static std::string show(const std::vector<int32_t> & v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += std::to_string(v[i]);
    }
    return s + "]";
}

static void eqv(const char * name, const std::vector<int32_t> & want,
                const std::vector<int32_t> & got) {
    ++cases;
    if (want == got) return;
    char b[1024];
    std::snprintf(b, sizeof b, "want %s got %s", show(want).c_str(), show(got).c_str());
    std::printf("[FAIL] %s: %s\n", name, b);
    ++fails;
}

static void report(const char * name, int c0, int f0) {
    std::printf("FG642_SPEC %-8s cases=%d failures=%d\n", name, cases - c0, fails - f0);
}

// ---------------------------------------------------------------------------------------------
// Section A: the draft source (ngram_draft). The native side is the shipped file, not a
// transcription: fg_spec::ngram_draft_native calls bmoe::ngram_draft.
// ---------------------------------------------------------------------------------------------
static void gate_ngram_one(const char * name, const std::vector<int32_t> & corpus, int n_max,
                           int min_match, int max_match) {
    std::vector<int32_t> ctx(corpus.begin(), corpus.size() ? corpus.end() - 1 : corpus.end());
    const int32_t last = corpus.size() ? corpus.back() : 0;

    std::vector<int32_t> want;
    const int nw = fg_spec::ngram_draft_native(ctx, last, n_max, min_match, max_match, want);

    std::vector<int32_t> got((size_t) (n_max > 0 ? n_max : 0), -7);
    const int ng = fg_spec::ngram_draft(ctx, last, n_max, min_match, max_match, got);
    if (ng >= 0 && (size_t) ng <= got.size()) got.resize((size_t) ng);
    else got.clear();

    eq(name, nw, ng);
    eqv(name, want, got);
}

static void gate_ngram() {
    gate_ngram_one("ngram_empty", {}, 3, 3, 12);
    gate_ngram_one("ngram_one_token", {7}, 3, 3, 12);
    gate_ngram_one("ngram_short", {1, 2}, 3, 3, 12);
    gate_ngram_one("ngram_no_repeat", {1, 2, 3, 4, 5, 6}, 3, 3, 12);
    gate_ngram_one("ngram_below_min_match", {1, 2, 1, 2, 9}, 3, 3, 12);
    gate_ngram_one("ngram_exact_min_match", {1, 2, 3, 1, 2, 3, 9}, 3, 3, 12);
    gate_ngram_one("ngram_run", {5, 5, 5, 5, 5, 5, 5}, 3, 3, 12);
    gate_ngram_one("ngram_run_short", {5, 5, 5, 5, 5}, 8, 2, 12);
    gate_ngram_one("ngram_copy", {1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4}, 5, 3, 12);
    gate_ngram_one("ngram_copy_long", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 2, 3, 4, 5, 6}, 8, 3, 64);
    gate_ngram_one("ngram_tie_recent", {1, 2, 9, 1, 2, 8, 3, 1, 2}, 4, 2, 12);
    gate_ngram_one("ngram_capped",
                   {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 7}, 8, 3, 4);
    gate_ngram_one("ngram_nmax", {2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2}, 5, 3, 12);
    gate_ngram_one("ngram_nmax_one", {2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 2}, 1, 3, 12);
    gate_ngram_one("ngram_nmax_zero", {2, 3, 4, 5, 6, 7, 2}, 0, 3, 12);
    gate_ngram_one("ngram_min_zero", {2, 3, 4, 2, 3, 4}, 4, 0, 12);
    gate_ngram_one("ngram_max_below_min", {2, 3, 4, 2, 3, 4}, 4, 5, 4);
    {
        std::vector<int32_t> c;
        for (int r = 0; r < 3; ++r)
            for (int i = 0; i < 12; ++i) c.push_back(i);
        for (int i = 0; i < 5; ++i) c.push_back(i);
        gate_ngram_one("ngram_block", c, 8, 3, 12);
        gate_ngram_one("ngram_block_min12", c, 8, 12, 12);
        gate_ngram_one("ngram_block_cap6", c, 8, 3, 6);
    }

    uint64_t s = 0x9e3779b97f4a7c15ull;
    auto rnd = [&]() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (int) ((s >> 33) % 1000);
    };
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
// Section B: the accept decision and the emit bound.
// ---------------------------------------------------------------------------------------------
static void gate_accept_one(const char * name, const std::vector<int32_t> & drafts,
                            const std::vector<int32_t> & target, const std::vector<int32_t> & eog,
                            int32_t tok) {
    std::vector<int32_t> want_vec;
    const fg_spec::accept_t want =
        fg_spec::accept_native(drafts, target, eog, tok, &want_vec);

    std::vector<int32_t> got_vec;
    const fg_spec::accept_t got = fg_spec::accept(drafts, target, eog, tok, &got_vec);

    eq(name, want.n_acc, got.n_acc);
    eq(name, want.confirmed, got.confirmed);
    eq(name, want.eog_hit, got.eog_hit);
    eq(name, want.stop, got.stop);
    eq(name, want.n_draft, got.n_draft);
    eqv(name, want_vec, got_vec);
}

static void gate_accept() {
    gate_accept_one("acc_all", {10, 11, 12}, {10, 11, 12}, {0, 0, 0}, 9);
    gate_accept_one("acc_first_mismatch", {10, 11, 12}, {99, 11, 12}, {0, 0, 0}, 9);
    gate_accept_one("acc_mid_mismatch", {10, 11, 12}, {10, 99, 12}, {0, 0, 0}, 9);
    gate_accept_one("acc_last_mismatch", {10, 11, 12}, {10, 11, 99}, {0, 0, 0}, 9);
    gate_accept_one("acc_none", {10, 11, 12}, {99, 98, 97}, {0, 0, 0}, 9);
    gate_accept_one("acc_empty", {}, {}, {}, 9);
    gate_accept_one("acc_one", {10}, {10}, {0}, 9);
    gate_accept_one("acc_eog_first", {10, 11, 12}, {10, 11, 12}, {1, 0, 0}, 9);
    gate_accept_one("acc_eog_second", {10, 11, 12}, {10, 11, 12}, {0, 1, 0}, 9);
    gate_accept_one("acc_eog_last", {10, 11, 12}, {10, 11, 12}, {0, 0, 1}, 9);
    gate_accept_one("acc_eog_unreached", {10, 11, 12}, {10, 99, 12}, {0, 0, 1}, 9);
    // A target slice shorter than the drafts: the caller's error, reported as a rejection.
    gate_accept_one("acc_short_target", {10, 11, 12}, {10, 11}, {0, 0, 0}, 9);
    gate_accept_one("acc_no_target", {10, 11}, {}, {}, 9);
    gate_accept_one("acc_no_eog", {10, 11}, {10, 11}, {}, 9);
    gate_accept_one("acc_wide", {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 99}, {0, 0, 0, 0, 0, 0, 0, 0}, 9);

    uint64_t s = 0x2545f4914f6cdd1dull;
    auto rnd = [&]() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (int) ((s >> 33) % 1000);
    };
    char nm[64];
    for (int c = 0; c < 40; ++c) {
        const int n = rnd() % 5;
        std::vector<int32_t> drafts, target, eog;
        for (int i = 0; i < n; ++i) {
            const int32_t d = 100 + rnd() % 4;
            drafts.push_back(d);
            target.push_back(rnd() % 3 == 0 ? (int32_t) (200 + rnd() % 4) : d);
            eog.push_back(0);
        }
        if (n > 0 && rnd() % 4 == 0) eog[(size_t) (rnd() % n)] = 1;
        std::snprintf(nm, sizeof nm, "acc_rand_%02d", c);
        gate_accept_one(nm, drafts, target, eog, 99);
    }

    // The emit bound, from the emit loop of session.cpp:1848.
    const long long np[6] = {1, 2, 3, 8, 32, 32};
    const long long ng[6] = {0, 0, 2, 7, 31, 32};
    const long long cf[6] = {1, 5, 4, 9, 2, 1};
    for (int i = 0; i < 6; ++i) {
        eq("emit_grid", fg_spec::emit_count_native(np[i], ng[i], cf[i]),
           fg_spec::emit_count(np[i], ng[i], cf[i]));
    }
    eq("emit_cut", fg_spec::emit_count_native(8, 7, 4), fg_spec::emit_count(8, 7, 4));
    eq("emit_zero", fg_spec::emit_count_native(8, 8, 4), fg_spec::emit_count(8, 8, 4));
    eq("emit_over", fg_spec::emit_count_native(8, 12, 4), fg_spec::emit_count(8, 12, 4));
    eq("emit_all", fg_spec::emit_count_native(8, 0, 4), fg_spec::emit_count(8, 0, 4));
    eq("emit_huge", fg_spec::emit_count_native(1 << 20, 0, 1 << 19),
       fg_spec::emit_count(1 << 20, 0, 1 << 19));
    eq("emit_zero_confirmed", fg_spec::emit_count_native(8, 0, 0), fg_spec::emit_count(8, 0, 0));
    eq("emit_one_left", fg_spec::emit_count_native(8, 7, 9), fg_spec::emit_count(8, 7, 9));
    eq("emit_exact_fit", fg_spec::emit_count_native(8, 4, 4), fg_spec::emit_count(8, 4, 4));
}

// ---------------------------------------------------------------------------------------------
// Section C: the budget, the rollback, the advance and the trim.
// ---------------------------------------------------------------------------------------------
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
                        const fg_spec::plan_t want =
                            fg_spec::step_plan_native(np[a], ng[b], dm[c], so, nd[d]);
                        const fg_spec::plan_t got =
                            fg_spec::step_plan(np[a], ng[b], dm[c], so, nd[d]);
                        eq("plan_room", want.room, got.room);
                        eq("plan_nmax", want.n_max, got.n_max);
                        eq("plan_call", want.call, got.call);
                        eq("plan_wide", want.wide, got.wide);
                        eq("plan_ndraft", want.n_draft, got.n_draft);
                        eq("plan_width", want.width, got.width);
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
                const fg_spec::rollback_t want = fg_spec::rollback_native(past[a], nd, acc);
                const fg_spec::rollback_t got = fg_spec::rollback(past[a], nd, acc);
                eq("rb_needed", want.needed, got.needed);
                eq("rb_keep", want.keep, got.keep);
                eq("rb_rejected", want.rejected, got.rejected);
            }
        }
    }

    // The advance: n_past += 1 + n_acc and the logits row.
    for (int a = 0; a < 3; ++a) {
        for (int acc = 0; acc <= 4; ++acc) {
            for (int wide = 0; wide <= 1; ++wide) {
                const fg_spec::advance_t want = fg_spec::advance_native(past[a], acc, wide);
                const fg_spec::advance_t got = fg_spec::advance(past[a], acc, wide);
                eq("adv_past", want.n_past, got.n_past);
                eq("adv_row", want.row, got.row);
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
                const fg_spec::trim_t want = fg_spec::tail_trim_native(pr[a], g, kv);
                const fg_spec::trim_t got = fg_spec::tail_trim(pr[a], g, kv);
                eq("tt_end", want.emitted_end, got.emitted_end);
                eq("tt_removed", want.removed, got.removed);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Section D: the whole loop, dispatched decisions against native decisions, in one process.
// `plain` is the same run with speculation off: the emitted sequence must match, because the
// accept rule uses the target's own tokens and no approximation enters.
// ---------------------------------------------------------------------------------------------
static long long g_runs = 0, g_emitted = 0, g_drafted = 0, g_accepted = 0, g_steps = 0;
static long long g_decodes = 0, g_trims = 0, g_eog_runs = 0, g_floor_runs = 0;

static void gate_run(const char * name, const fg_spec::script_t & sc, long long prompt,
                     long long n_predict, int draft_max, int min_match) {
    const fg_spec::run_out want = fg_spec::run_native(sc, prompt, n_predict, draft_max, min_match, true);
    const fg_spec::run_out got = fg_spec::run(sc, prompt, n_predict, draft_max, min_match, true);

    eqv(name, want.emitted, got.emitted);
    eq(name, (long long) want.trace.size(), (long long) got.trace.size());
    for (size_t i = 0; i < want.trace.size() && i < got.trace.size(); ++i) {
        eq(name, want.trace[i], got.trace[i]);
    }
    eq(name, want.n_past, got.n_past);
    eq(name, want.kv_end, got.kv_end);
    eq(name, want.drafted, got.drafted);
    eq(name, want.accepted, got.accepted);
    eq(name, want.drafted_steps, got.drafted_steps);
    eq(name, want.decodes, got.decodes);
    eq(name, want.trims, got.trims);
    eq(name, want.eog_hits, got.eog_hits);
    eq(name, want.final_end, got.final_end);

    g_drafted += want.drafted;
    g_accepted += want.accepted;
    g_steps += want.drafted_steps;
    g_decodes += want.decodes;
    g_trims += want.trims;
    g_emitted += (long long) want.emitted.size();
    g_runs += 1;
    if (want.eog_hits > 0) g_eog_runs += 1;

    // Speculation must not change the answer.
    const fg_spec::run_out plain = fg_spec::run(sc, prompt, n_predict, draft_max, min_match, false);
    eqv(name, plain.emitted, want.emitted);
    if (want.drafted_steps == 0) {
        eq(name, plain.decodes, want.decodes); // an abstaining step costs the plain decode
        g_floor_runs += 1;
    }
}

static fg_spec::script_t script_repeat() {
    fg_spec::script_t sc;
    const int32_t pattern[5] = {1, 2, 3, 4, 5};
    for (int r = 0; r < 12; ++r)
        for (int i = 0; i < 5; ++i) sc.tokens.push_back(pattern[i]);
    for (int i = 0; i < 6; ++i) sc.tokens.push_back(3);
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < 5; ++i) sc.tokens.push_back(pattern[i]);
    return sc;
}

static fg_spec::script_t script_eog() {
    fg_spec::script_t sc;
    sc.eog = 4; // the draft source proposes it before the plain path would reach it
    for (int r = 0; r < 10; ++r)
        for (int i = 1; i <= 4; ++i) sc.tokens.push_back(i);
    return sc;
}

static fg_spec::script_t script_novel() {
    fg_spec::script_t sc;
    uint64_t s = 0x94d049bb133111ebull;
    for (int i = 0; i < 400; ++i) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        sc.tokens.push_back((int32_t) (7 + (s >> 33) % 97));
    }
    return sc;
}

static void gate_loop() {
    const fg_spec::script_t rep = script_repeat();
    const fg_spec::script_t eo = script_eog();
    const fg_spec::script_t nov = script_novel();

    const long long prompts[3] = {8, 15, 22};
    const int draft_max[3] = {1, 3, 8};
    const int min_match[2] = {2, 3};

    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            for (int c = 0; c < 2; ++c) gate_run("loop_rep", rep, prompts[a], 24, draft_max[b], min_match[c]);

    for (int pr = 0; pr < 3; ++pr)
        for (int dm = 0; dm < 3; ++dm) gate_run("loop_eog", eo, 6 + pr * 4, 24, draft_max[dm], 2);

    for (int np = 0; np < 3; ++np)
        gate_run("loop_cut", rep, 8, 13 + np * 5, 8, 2);

    for (int dm = 0; dm < 3; ++dm) gate_run("loop_novel", nov, 16, 24, draft_max[dm], 3);

    // Speculation off, for the record: the plain path must be identical in both builds.
    for (int a = 0; a < 3; ++a) {
        const fg_spec::run_out want = fg_spec::run_native(rep, prompts[a], 24, 8, 2, false);
        const fg_spec::run_out got = fg_spec::run(rep, prompts[a], 24, 8, 2, false);
        eqv("loop_plain", want.emitted, got.emitted);
        eq("loop_plain_past", want.n_past, got.n_past);
        eq("loop_plain_kv", want.kv_end, got.kv_end);
    }
}

// The same runs, printed, for the OFF vs ON text diff (scripts/spec-gate.sh).
static void dump() {
    const fg_spec::script_t rep = script_repeat();
    const fg_spec::script_t eo = script_eog();
    const fg_spec::script_t nov = script_novel();
    const long long prompts[3] = {8, 15, 22};
    const int draft_max[3] = {1, 3, 8};
    const int min_match[2] = {2, 3};

    struct one {
        const char * tag;
        const fg_spec::script_t * sc;
        long long prompt;
        long long n_predict;
        int draft_max;
        int min_match;
    };
    std::vector<one> all;
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            for (int c = 0; c < 2; ++c)
                all.push_back({"rep", &rep, prompts[a], 24, draft_max[b], min_match[c]});
    for (int pr = 0; pr < 3; ++pr)
        for (int dm = 0; dm < 3; ++dm)
            all.push_back({"eog", &eo, 6 + pr * 4, 24, draft_max[dm], 2});
    for (int np = 0; np < 3; ++np) all.push_back({"cut", &rep, 8, 13 + np * 5, 8, 2});
    for (int dm = 0; dm < 3; ++dm) all.push_back({"novel", &nov, 16, 24, draft_max[dm], 3});

    for (size_t i = 0; i < all.size(); ++i) {
        const one & o = all[i];
        const fg_spec::run_out r =
            fg_spec::run(*o.sc, o.prompt, o.n_predict, o.draft_max, o.min_match, true);
        std::printf("run %02zu %-5s prompt=%lld predict=%lld draft_max=%d min_match=%d\n", i,
                    o.tag, o.prompt, o.n_predict, o.draft_max, o.min_match);
        std::printf("  emitted=%s\n", show(r.emitted).c_str());
        std::printf("  trace=");
        for (size_t k = 0; k < r.trace.size(); ++k) std::printf("%s%lld", k ? "," : "", r.trace[k]);
        std::printf("\n  n_past=%lld kv_end=%lld drafted=%lld accepted=%lld steps=%lld "
                    "decodes=%lld trims=%lld eog=%lld end=%lld\n",
                    r.n_past, r.kv_end, r.drafted, r.accepted, r.drafted_steps, r.decodes,
                    r.trims, r.eog_hits, r.final_end);
    }
}

int main(int argc, char ** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--dump") == 0) {
        dump();
        return 0;
    }
#ifdef FG_GOOSE_SPEC
    const char * mode = "goose";
#else
    const char * mode = "native";
#endif
    int c0 = cases, f0 = fails;
    gate_ngram();
    report("ngram", c0, f0);
    c0 = cases; f0 = fails;
    gate_accept();
    report("accept", c0, f0);
    c0 = cases; f0 = fails;
    gate_budget();
    report("budget", c0, f0);
    c0 = cases; f0 = fails;
    gate_loop();
    report("loop", c0, f0);

    std::printf("FG642_SPEC mode=%s cases=%d failures=%d goose_calls=%lld\n", mode, cases, fails,
                fg_spec::goose_calls());
    std::printf("FG642_SPEC loop runs=%lld emitted=%lld drafted=%lld accepted=%lld steps=%lld "
                "decodes=%lld trims=%lld eog_runs=%lld floor_runs=%lld\n",
                g_runs, g_emitted, g_drafted, g_accepted, g_steps, g_decodes, g_trims,
                g_eog_runs, g_floor_runs);
    return fails == 0 ? 0 : 1;
}
