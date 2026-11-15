// Differential gate for P9-08 S2: the perplexity pass's gates and scoring plan.
//
// Oracle: a transcription of runtime/core/src/engine/session.cpp:459-622.
// Port: source/ppl-perplexity-plan.goose.
//
// The tokenizer, the decode and the logits are inputs here: the gate compares
// the decision, not the model. Every scored (pos, row) pair the native loop
// would hand to score() is enumerated the same way on both sides.
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "ppl.h"

static int g_cases = 0, g_checks = 0, g_fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 80) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what, (long long) got, (long long) want);
        }
    }
}

static void expect_f64(const char * group, const char * what, double got, double want) {
    ++g_checks;
    // Bit equality: both arms call the same libm entry point on the same input.
    if (std::memcmp(&got, &want, sizeof(double)) != 0) {
        ++g_fails;
        if (g_fails <= 80) {
            std::printf("FAIL %s / %s: got %.17g want %.17g\n", group, what, got, want);
        }
    }
}

// ── oracle: the perplexity pass, transcribed (session.cpp:459-622) ─────────

enum { PPL_NAT_OK = 0, PPL_NAT_SHORT = 1, PPL_NAT_OVER = 2 };
enum { CH_NAT_OK = 0, CH_NAT_BAD = 1, CH_NAT_NOTHING = 2 };

static int64_t nat_gate(int64_t n, int64_t n_ctx) {
    if (n < 2) return PPL_NAT_SHORT;
    if (n > n_ctx) return PPL_NAT_OVER;
    return PPL_NAT_OK;
}

// session.cpp:540
static int64_t nat_prefix(int64_t skip, int64_t n) {
    int64_t s = skip < n - 1 ? skip : n - 1;
    return s < 1 ? 1 : s;
}

// session.cpp:546
static int64_t nat_step_last(int64_t has_choices, int64_t n) {
    return has_choices ? n : n - 1;
}

// session.cpp:554-556, 577-579
static int64_t nat_scored(int64_t pos, int64_t skip, int64_t n) {
    if (pos < skip || pos + 1 >= n) return 0;
    return 1;
}

// session.cpp:538-596 — the pairs handed to score(), packed as pos*64+row.
static int64_t nat_plan(int64_t n, int64_t skip, int64_t step, int64_t n_batch, int64_t * out, int64_t cap) {
    int64_t k = 0;
    if (step) {
        const int64_t prefix = nat_prefix(skip, n);
        const int64_t last = nat_step_last(0, n); // count is choice-independent
        for (int64_t pos = prefix; pos < last; ++pos) {
            if (!nat_scored(pos, skip, n)) continue;
            if (k >= cap) return -1;
            out[k++] = pos * 64;
        }
        return k;
    }
    for (int64_t i = 0; i < n; i += n_batch) {
        const int64_t chunk = (n - i) < n_batch ? (n - i) : n_batch;
        for (int64_t j = 0; j < chunk; ++j) {
            const int64_t pos = i + j;
            if (!nat_scored(pos, skip, n)) continue;
            if (k >= cap) return -1;
            out[k++] = pos * 64 + j;
        }
    }
    return k;
}

// session.cpp:548, 589
static int64_t nat_last_row(int64_t n, int64_t step, int64_t n_batch) {
    if (step) return 0;
    return (n % n_batch == 0 ? n_batch : n % n_batch) - 1;
}

// session.cpp:598-610
static int64_t nat_choice_gate(int64_t has_choices, int64_t bad, int64_t scored) {
    if (bad > 0) return CH_NAT_BAD;
    if (scored <= 0 && !has_choices) return CH_NAT_NOTHING;
    return CH_NAT_OK;
}

// session.cpp:611-614
static double nat_mean(double nll, int64_t scored) { return scored > 0 ? nll / (double) scored : 0.0; }
static double nat_from_nll(double nll, int64_t scored) { return std::exp(nat_mean(nll, scored)); }

int main() {
    goose_init(0, nullptr);

    // ── the two request gates ───────────────────────────────────────────────
    {
        struct c { const char * name; int64_t n, n_ctx, want; } cases[] = {
            {"empty",              0, 2048, PPL_NAT_SHORT},
            {"one token",          1, 2048, PPL_NAT_SHORT},
            {"two tokens",         2, 2048, PPL_NAT_OK},
            {"exactly n_ctx",   2048, 2048, PPL_NAT_OK},
            {"one over n_ctx",  2049, 2048, PPL_NAT_OVER},
            {"tiny context",       3,    2, PPL_NAT_OVER},
            {"one token, 0 ctx",   1,    0, PPL_NAT_SHORT},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "gate", ppl_token_gate(c.n, c.n_ctx), c.want);
            expect(c.name, "gate vs oracle", ppl_token_gate(c.n, c.n_ctx), nat_gate(c.n, c.n_ctx));
        }
    }

    // ── the prefix and the step-mode bound ──────────────────────────────────
    {
        struct c { const char * name; int64_t skip, n, want; } cases[] = {
            {"skip 0 floors to 1",   0, 10, 1},
            {"skip inside",          5, 10, 5},
            {"skip n-1",             9, 10, 9},
            {"skip n clamps",       10, 10, 9},
            {"skip far beyond",    100, 10, 9},
            {"negative skip floors", -3, 10, 1},
            {"two-token text",       5,  2, 1},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "prefix", ppl_prefix(c.skip, c.n), c.want);
            expect(c.name, "prefix vs oracle", ppl_prefix(c.skip, c.n), nat_prefix(c.skip, c.n));
        }

        ++g_cases;
        expect("step last", "without choices", ppl_step_last(0, 10), 9);
        expect("step last", "with choices", ppl_step_last(1, 10), 10);
        expect("step last", "two-token text", ppl_step_last(0, 2), 1);
        expect("step last", "with choices, two tokens", ppl_step_last(1, 2), 2);
    }

    // ── the shared scoring predicate ────────────────────────────────────────
    {
        ++g_cases;
        for (int64_t pos = -2; pos <= 12; ++pos) {
            for (int64_t skip = 0; skip <= 4; ++skip) {
                expect("scored", "predicate", ppl_scored(pos, skip, 10), nat_scored(pos, skip, 10));
            }
        }
    }

    // ── the whole plan, both regimes, over a grid ───────────────────────────
    {
        static const int64_t ns[]      = {2, 3, 5, 8, 17};
        static const int64_t skips[]   = {0, 1, 2, 5, 20};
        static const int64_t batches[] = {1, 2, 3, 4, 16};

        int64_t out[256];
        int64_t want[256];
        for (int64_t n : ns) {
            for (int64_t skip : skips) {
                for (int64_t step = 0; step <= 1; ++step) {
                    for (int64_t nb : batches) {
                        ++g_cases;
                        char name[96];
                        std::snprintf(name, sizeof(name), "plan n=%lld skip=%lld step=%lld nb=%lld",
                                      (long long) n, (long long) skip, (long long) step, (long long) nb);
                        const int64_t got_n = ppl_score_plan(n, skip, step, nb, sl_i64{out, 256});
                        const int64_t want_n = nat_plan(n, skip, step, nb, want, 256);
                        expect(name, "count", got_n, want_n);
                        expect(name, "count helper", ppl_score_count(n, skip, step, nb), want_n);
                        for (int64_t i = 0; i < want_n; ++i) {
                            expect(name, "pair", out[i], want[i]);
                        }
                        // Every pair carries a row inside the batch width.
                        for (int64_t i = 0; i < got_n; ++i) {
                            expect(name, "row in range", (out[i] % 64) < nb ? 1 : 0, 1);
                        }
                    }
                }
            }
        }

        // Capacity: a slice that cannot hold the plan is refused, not clipped.
        ++g_cases;
        int64_t small[3] = {-1, -1, -1};
        expect("capacity", "too small refused", ppl_score_plan(8, 0, 1, 1, sl_i64{small, 3}), -1);
        expect("capacity", "values untouched", small[0], -1);
        expect("capacity", "non-positive batch refused", ppl_score_plan(8, 0, 1, 0, sl_i64{out, 256}), -1);
        expect("capacity", "negative batch refused", ppl_score_plan(8, 0, 1, -4, sl_i64{out, 256}), -1);
        // n=2 in the decode regime scores nothing: the prefix floor of one feeds
        // position 0 without logits, and position 1 has no successor. The native
        // pass reports "nothing scored" here, so an empty plan is correct.
        expect("capacity", "two-token step plan is empty", ppl_score_plan(2, 0, 1, 1, sl_i64{small, 1}), 0);
        expect("capacity", "two-token step is refused at close", ppl_choice_gate(0, 0, 0), CH_NAT_NOTHING);
        // An exact fit of one pair is accepted: n=3 skips 0, prefix 1, scores 1.
        expect("capacity", "exact fit accepted", ppl_score_plan(3, 0, 1, 1, sl_i64{small, 1}), 1);
        expect("capacity", "packed pair", small[0], 1 * 64);
    }

    // ── the choice row ──────────────────────────────────────────────────────
    {
        struct c { const char * name; int64_t n, step, nb, want; } cases[] = {
            {"decode regime",      10, 1, 4,  0},
            {"batch exact",         8, 0, 4,  3},
            {"batch remainder",    10, 0, 4,  1},
            {"batch single",        5, 0, 1,  0},
            {"batch one chunk",    16, 0, 16, 15},
            {"batch last row 2",   17, 0, 16, 0},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "row", ppl_last_row(c.n, c.step, c.nb), c.want);
            expect(c.name, "row vs oracle", ppl_last_row(c.n, c.step, c.nb), nat_last_row(c.n, c.step, c.nb));
        }
        ++g_cases;
        expect("row", "bad batch refused", ppl_last_row(8, 0, 0), -1);
    }

    // ── the closing gates ───────────────────────────────────────────────────
    {
        struct c { const char * name; int64_t ch, bad, scored, want; } cases[] = {
            {"plain pass with scores",    0, 0, 12, CH_NAT_OK},
            {"plain pass, nothing scored", 0, 0,  0, CH_NAT_NOTHING},
            {"choices with scores",       1, 0, 12, CH_NAT_OK},
            {"choices, nothing scored",   1, 0,  0, CH_NAT_OK},
            {"bad choice beats scores",   1, 1, 12, CH_NAT_BAD},
            {"bad choice, no choice flag", 0, 2, 12, CH_NAT_BAD},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "gate", ppl_choice_gate(c.ch, c.bad, c.scored), c.want);
            expect(c.name, "gate vs oracle", ppl_choice_gate(c.ch, c.bad, c.scored),
                   nat_choice_gate(c.ch, c.bad, c.scored));
        }
    }

    // ── aggregates ──────────────────────────────────────────────────────────
    {
        struct c { const char * name; double nll; int64_t scored; } cases[] = {
            {"twelve over three", 12.0, 3},
            {"one position",       3.5, 1},
            {"nothing scored",     9.9, 0},
            {"large nll",        512.25, 8},
            {"fractional",       0.125, 5},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect_f64(c.name, "mean", ppl_mean(c.nll, c.scored), nat_mean(c.nll, c.scored));
            expect_f64(c.name, "ppl", ppl_from_nll(c.nll, c.scored), nat_from_nll(c.nll, c.scored));
            expect_f64(c.name, "ppl is exp(mean)", ppl_from_nll(c.nll, c.scored),
                       std::exp(ppl_mean(c.nll, c.scored)));
        }

        ++g_cases;
        for (int64_t prev = -5; prev <= 5; ++prev) {
            for (int64_t now = 0; now <= 100; now += 17) {
                expect("delta", "now - prev", ppl_delta(now, prev), now - prev);
            }
        }
    }

    // ── the eval phase label ────────────────────────────────────────────────
    {
        ++g_cases;
        expect("phase", "step is decode", ppl_phase(1, 0), 1);
        expect("phase", "step ignores as_decode", ppl_phase(1, 1), 1);
        expect("phase", "batch default is prefill", ppl_phase(0, 0), 0);
        expect("phase", "batch as_decode", ppl_phase(0, 1), 1);
    }

    std::printf("FG634_PPL cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
