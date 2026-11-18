// EXP959 differential gate: the Goose locally-typical filter must keep exactly
// what the native sampler keeps, in the same order.
//
// The oracle is a transcription of llama_sampler_typical_apply's decision on the
// same input array: softmax is supplied by the caller in both cases, so this
// compares the entropy, the shifted-score sort and the cumulative cut - not the
// softmax, which the chain owns.
//
// No model: the arrays are generated here.
#include "runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char * what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what); }
}

static sl_i64 I(std::vector<int64_t> & v) { return sl_i64{ v.data(), (long long) v.size() }; }
static sl_f64 F(std::vector<double> & v) { return sl_f64{ v.data(), (long long) v.size() }; }

// llama-sampler.cpp:1990-2043, on probabilities instead of logits.
static std::vector<int64_t> native_typical(const std::vector<double> & probs, double p, int64_t min_keep) {
    const size_t n = probs.size();
    std::vector<int64_t> out(n);
    std::iota(out.begin(), out.end(), 0);
    if (p >= 1.0) return out;

    double entropy = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (probs[i] > 0.0) entropy += -probs[i] * std::log(probs[i]);
    }
    std::vector<double> score(n);
    for (size_t i = 0; i < n; i++) {
        score[i] = probs[i] > 0.0 ? std::fabs(-std::log(probs[i]) - entropy) : INFINITY;
    }
    std::vector<size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return score[a] < score[b]; });

    double cum = 0.0;
    size_t last = n;
    for (size_t i = 0; i < n; i++) {
        cum += probs[idx[i]];
        if (cum > p && (min_keep == 0 || (int64_t) i >= min_keep - 1)) { last = i + 1; break; }
    }
    std::vector<int64_t> keep(last);
    for (size_t i = 0; i < last; i++) keep[i] = (int64_t) idx[i];
    return keep;
}

static bool run_case(const std::vector<double> & probs, double p, int64_t min_keep, const char * label) {
    const size_t n = probs.size();
    std::vector<double> scratch(n, 0.0);
    std::vector<int64_t> order(n, -1), keep(n, -1);
    std::vector<double> copy = probs;
    const long long got = tp_apply(F(copy), (long long) n, p, min_keep, I(order), I(keep), F(scratch));
    const std::vector<int64_t> want = native_typical(probs, p, min_keep);

    if (got != (long long) want.size()) {
        std::printf("FAIL %s: kept %lld, native %zu\n", label, got, want.size());
        failures++;
        checks++;
        return false;
    }
    checks++;
    for (long long i = 0; i < got; i++) {
        checks++;
        if (keep[i] != want[i]) {
            std::printf("FAIL %s: survivor %lld is %lld, native %lld\n", label, i, (long long) keep[i], (long long) want[i]);
            failures++;
            return false;
        }
    }
    return true;
}

int main() {
    goose_init(0, nullptr);

    // ---- the empty sampler: p >= 1.0 keeps everything in order ----
    {
        std::vector<double> probs = { 0.5, 0.3, 0.2 };
        check(tp_is_empty(1.0), "p == 1.0 is empty");
        check(tp_is_empty(1.5), "p > 1.0 is empty");
        check(!tp_is_empty(0.99), "p < 1.0 is not empty");
        run_case(probs, 1.0, 0, "p=1.0 identity");
        run_case(probs, 2.0, 0, "p=2.0 identity");
    }

    // ---- a flat distribution where the entropy term dominates ----
    {
        std::vector<double> probs(8, 0.125);
        for (double p : { 0.1, 0.5, 0.8, 0.9, 0.95 }) run_case(probs, p, 0, "flat");
    }

    // ---- a peaked distribution ----
    {
        std::vector<double> probs = { 0.70, 0.10, 0.06, 0.05, 0.04, 0.03, 0.015, 0.005 };
        for (double p : { 0.1, 0.5, 0.9, 0.99 }) run_case(probs, p, 0, "peaked");
    }

    // ---- min_keep lower bound ----
    {
        std::vector<double> probs = { 0.9, 0.05, 0.05 };
        for (int64_t mk : { 1, 2, 3, 0 }) run_case(probs, 0.1, mk, "min_keep");
    }

    // ---- a zero-probability candidate must sort last, not crash ----
    {
        std::vector<double> probs = { 0.5, 0.0, 0.5 };
        run_case(probs, 0.9, 0, "zero probability");
    }

    // ---- single candidate and two candidates ----
    {
        std::vector<double> one = { 1.0 };
        run_case(one, 0.5, 0, "n=1");
        std::vector<double> two = { 0.6, 0.4 };
        run_case(two, 0.5, 0, "n=2");
    }

    // ---- randomised: the shapes a real chain produces ----
    {
        std::mt19937 rng(959);
        std::uniform_real_distribution<double> eps(1e-9, 1.0);
        for (int trial = 0; trial < 200; trial++) {
            const size_t n = 1 + (rng() % 64);
            std::vector<double> raw(n);
            double sum = 0.0;
            for (size_t i = 0; i < n; i++) { raw[i] = -std::log(eps(rng)); sum += raw[i]; }
            for (size_t i = 0; i < n; i++) raw[i] /= sum;
            const double p = 0.05 + 0.94 * ((rng() % 1000) / 1000.0);
            const int64_t mk = (int64_t) (rng() % 4);
            run_case(raw, p, mk, "random");
        }
    }

    // ---- bad arguments are refused ----
    {
        std::vector<double> probs = { 0.5, 0.5 };
        std::vector<double> scratch(2, 0.0);
        std::vector<int64_t> order(2), keep(2);
        check(tp_apply(F(probs), 5, 0.5, 0, I(order), I(keep), F(scratch)) == -1, "n past the array refused");
        check(tp_apply(F(probs), 2, 0.5, -1, I(order), I(keep), F(scratch)) == -1, "negative min_keep refused");
        std::vector<int64_t> short_keep(1);
        check(tp_apply(F(probs), 2, 0.5, 0, I(order), I(short_keep), F(scratch)) == -1, "short keep refused");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
