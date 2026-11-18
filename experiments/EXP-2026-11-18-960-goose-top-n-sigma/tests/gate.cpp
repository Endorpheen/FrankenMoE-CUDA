// EXP960 differential gate: the Goose top-n-sigma mask must match the native
// float arithmetic bit for bit, because the mask boundary is a strict compare
// against max - n*std and a rounding difference flips a candidate.
//
// The oracle is llama_sampler_top_n_sigma_apply transcribed with float (not
// double) so the same rounding applies. Comparison is on the resulting bit
// patterns, so -0.0 vs 0.0 and every tie is visible.
#include "runtime.h"

#include <cmath>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}

static sl_f32 F(std::vector<float> & v) { return sl_f32{ v.data(), (long long) v.size() }; }
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }

// src/llama-sampler.cpp:3325-3357, float throughout.
static std::vector<float> native_mask(std::vector<float> logits, float n) {
    const size_t size = logits.size();
    if (n <= 0.0f || size <= 1) return logits;

    float mx = logits[0];
    float logits_sum = 0;
    size_t valid_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (logits[i] != -INFINITY) {
            mx = std::max(mx, logits[i]);
            logits_sum += logits[i];
            valid_count++;
        }
    }
    float mean = valid_count > 0 ? logits_sum / valid_count : 0;

    float acc = 0;
    for (size_t i = 0; i < size; i++) {
        if (logits[i] != -INFINITY) {
            acc += std::pow(logits[i] - mean, 2.0f);
        }
    }
    float stddev = valid_count > 0 ? std::sqrt(acc / valid_count) : 0;

    for (size_t i = 0; i < size; i++) {
        if (logits[i] < mx - (n * stddev)) {
            logits[i] = -INFINITY;
        }
    }
    return logits;
}

static void run_case(const std::vector<float> & logits, float n, const char * label) {
    std::vector<float> got = logits;
    std::vector<float> want = native_mask(logits, n);
    const long long rc = tns_apply(F(got), (long long) got.size(), n);
    check(rc >= 0, std::string(label) + ": not refused");
    for (size_t i = 0; i < got.size(); i++) {
        // -INFINITY and the module's most-negative float both mean "masked";
        // compare the masked-or-not decision, then the value when unmasked.
        const bool got_masked = got[i] < -1e38f;
        const bool want_masked = want[i] == -INFINITY;
        if (got_masked != want_masked) {
            std::printf("FAIL %s: candidate %zu masked=%d native=%d\n", label, i, got_masked, want_masked);
            failures++;
        } else if (!got_masked && bits(got[i]) != bits(want[i])) {
            std::printf("FAIL %s: candidate %zu bits differ %08x vs %08x\n", label, i, bits(got[i]), bits(want[i]));
            failures++;
        }
        checks++;
    }
}

int main() {
    goose_init(0, nullptr);

    // ---- the empty paths ----
    check(tns_is_empty(0.0f, 8), "n=0 empty");
    check(tns_is_empty(-1.0f, 8), "n<0 empty");
    check(tns_is_empty(1.0f, 1), "size 1 empty");
    check(!tns_is_empty(1.0f, 2), "n>0 size>1 not empty");

    // ---- shapes ----
    run_case({ 1.0f, 2.0f, 3.0f, 4.0f }, 1.0f, "ascending");
    run_case({ 4.0f, 3.0f, 2.0f, 1.0f }, 1.0f, "descending");
    run_case({ 2.0f, 2.0f, 2.0f, 2.0f }, 1.0f, "all equal");
    run_case({ 0.0f, 0.0f }, 2.0f, "pair equal");
    run_case({ 5.0f }, 1.0f, "single");
    run_case({ 1.0f, 2.0f }, 0.0f, "n=0 keeps all");
    run_case({ 100.0f, -100.0f, 0.0f }, 0.5f, "wide spread");
    run_case({ 1.0f, 2.0f, 3.0f }, 10.0f, "huge n masks all but max");

    // ---- leading -INFINITY must not poison the max (native seeds mx from [0]) ----
    run_case({ -INFINITY, 1.0f, 2.0f, 3.0f }, 1.0f, "leading -inf");

    // ---- all masked candidates: valid_count 0 uses mean 0 and std 0 ----
    //
    // Native leaves an all-(-INFINITY) list untouched: mx is -INFINITY, cut is
    // -INFINITY, and the strict compare is false. My first attempt asserted
    // "keeps zero" here, which was wrong about the native behaviour; the module
    // already matched it and the assertion was what had to change.
    {
        std::vector<float> all_inf = { -INFINITY, -INFINITY };
        std::vector<float> got = all_inf;
        const long long rc = tns_apply(F(got), 2, 1.0f);
        check(rc == 2, "all -inf reports every candidate unmasked, as native does");
        check(got[0] < -1e38f && got[1] < -1e38f, "all -inf stays masked");
        run_case(all_inf, 1.0f, "all -inf");
    }

    // ---- randomised, including exact ties that sit on the boundary ----
    {
        std::mt19937 rng(960);
        std::uniform_real_distribution<float> u(-8.0f, 8.0f);
        std::uniform_int_distribution<int> nsize(2, 64);
        std::uniform_real_distribution<float> nval(0.1f, 4.0f);
        for (int trial = 0; trial < 300; trial++) {
            const int sz = nsize(rng);
            std::vector<float> logits((size_t) sz);
            for (int i = 0; i < sz; i++) logits[(size_t) i] = u(rng);
            run_case(logits, nval(rng), "random");
        }
        // Quantised values produce many exact ties, which is where a rounding
        // difference in std shows up as a different mask.
        std::uniform_int_distribution<int> q(-40, 40);
        for (int trial = 0; trial < 200; trial++) {
            const int sz = nsize(rng);
            std::vector<float> logits((size_t) sz);
            for (int i = 0; i < sz; i++) logits[(size_t) i] = (float) q(rng) * 0.25f;
            run_case(logits, 1.0f, "quantised ties");
        }
    }

    // ---- bad arguments ----
    {
        std::vector<float> logits = { 1.0f, 2.0f };
        check(tns_apply(F(logits), 3, 1.0f) == -1, "n_candidates past the array refused");
        check(tns_apply(F(logits), -1, 1.0f) == -1, "negative count refused");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
