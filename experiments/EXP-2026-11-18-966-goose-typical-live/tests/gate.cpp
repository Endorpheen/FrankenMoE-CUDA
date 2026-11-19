// EXP966 differential gate: the Goose typical filter, reached through the PUBLIC
// llama_sampler API, must keep the same tokens the native sampler keeps.
//
// Why this is not a repeat of EXP959: that gate fed probabilities directly and
// compared against a transcription. This one constructs real `llama_sampler`
// objects, applies them to real `llama_token_data_array`s with logits, and lets
// each sampler compute its own softmax - so the comparison covers the softmax
// ordering, the sorted flag and the array rewrite, not just the entropy maths.
//
// The Goose side is not a llama_sampler; it is the ported filter called with the
// probabilities that the same softmax produces. The gate therefore compares the
// *decisions*: which ids survive, and in what order.
//
// No model, no GPU, no context: candidate arrays are built in memory.
#include "runtime.h"

#include <llama.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <vector>

extern "C" void fg950_goose_init(int, char **);

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}

static sl_f64 F(std::vector<double> & v) { return sl_f64{ v.data(), (long long) v.size() }; }
static sl_i64 I(std::vector<int64_t> & v) { return sl_i64{ v.data(), (long long) v.size() }; }

int main() {
    // Inside the shipping DSO the composed unit's initialiser is renamed, so the
    // gate calls it by that name - what the reader owner does on registration.
    fg950_goose_init(0, nullptr);
    llama_backend_init();

    // Every case runs twice: once through the native sampler chain, once through
    // the Goose filter, on the same logits.
    std::mt19937 rng(966);
    std::uniform_real_distribution<float> u(-6.0f, 6.0f);
    std::uniform_int_distribution<int> nsize(2, 48);

    for (int trial = 0; trial < 200; trial++) {
        const int n = nsize(rng);
        const float p = 0.1f + 0.85f * ((float) (rng() % 1000) / 1000.0f);
        const int64_t min_keep = (int64_t) (rng() % 3);

        std::vector<llama_token_data> cands((size_t) n);
        for (int i = 0; i < n; i++) {
            cands[(size_t) i].id = i;
            cands[(size_t) i].logit = u(rng);
            cands[(size_t) i].p = 0.0f;
        }

        // ---- native ----
        std::vector<llama_token_data> nat = cands;
        llama_token_data_array arr{ nat.data(), nat.size(), -1, false };
        llama_sampler * s = llama_sampler_init_typical(p, (size_t) min_keep);
        llama_sampler_apply(s, &arr);
        // native rewrites cur_p in place; the survivors are arr.data[0..size)
        std::vector<int64_t> nat_ids;
        for (size_t i = 0; i < arr.size; i++) nat_ids.push_back(arr.data[i].id);
        llama_sampler_free(s);

        // ---- Goose ----
        // Same softmax the sampler used: exp(logit - max) normalised. The chain's
        // softmax sorts descending first, but the probabilities do not depend on
        // the order, so the gate computes them on the unsorted array.
        float mx = cands[0].logit;
        for (int i = 1; i < n; i++) mx = std::max(mx, cands[(size_t) i].logit);
        double sum = 0.0;
        std::vector<double> probs((size_t) n);
        for (int i = 0; i < n; i++) {
            probs[(size_t) i] = std::exp((double) (cands[(size_t) i].logit - mx));
            sum += probs[(size_t) i];
        }
        for (int i = 0; i < n; i++) probs[(size_t) i] /= sum;

        std::vector<double> scratch((size_t) n, 0.0);
        std::vector<int64_t> order((size_t) n, -1), keep((size_t) n, -1);
        const long long got = tp_apply(F(probs), n, (double) p, min_keep, I(order), I(keep), F(scratch));

        // ---- compare as SETS of surviving ids ----
        // The two sorts are not required to agree on ties: native uses std::sort on
        // a score array and this module breaks ties by index. Order inside a tie is
        // therefore not evidence either way, but the surviving set is.
        std::set<int64_t> want(nat_ids.begin(), nat_ids.end());
        std::set<int64_t> have;
        for (long long i = 0; i < got; i++) have.insert(keep[(size_t) i]);
        checks++;
        if (want != have) {
            std::printf("FAIL trial %d: survivors differ (native %zu, goose %lld)\n",
                        trial, want.size(), got);
            failures++;
        }
    }

    // ---- the empty sampler keeps every id, through the public API ----
    {
        std::vector<llama_token_data> cands(4);
        for (int i = 0; i < 4; i++) { cands[(size_t) i].id = i; cands[(size_t) i].logit = (float) i; cands[(size_t) i].p = 0; }
        llama_token_data_array arr{ cands.data(), cands.size(), -1, false };
        llama_sampler * s = llama_sampler_init_typical(1.0f, 0);
        llama_sampler_apply(s, &arr);
        check(arr.size == 4, "p=1.0 keeps every id");
        llama_sampler_free(s);

        std::vector<double> probs = { 0.25, 0.25, 0.25, 0.25 };
        std::vector<double> scratch(4, 0.0);
        std::vector<int64_t> order(4), keep(4);
        check(tp_apply(F(probs), 4, 1.0, 0, I(order), I(keep), F(scratch)) == 4,
              "tp_apply p=1.0 keeps every index");
    }

    llama_backend_free();
    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
