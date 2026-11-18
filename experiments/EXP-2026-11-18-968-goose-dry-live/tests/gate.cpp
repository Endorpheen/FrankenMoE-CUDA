// EXP-2026-11-18-968 differential gate: the Goose DRY penalty, compared with the
// live sampler that the runtime constructs.
//
// EXP961 compared against a transcription. This runs llama_sampler_init_dry_testing,
// which takes token-id sequence breakers and needs no vocab, then llama_sampler_accept
// for the history and llama_sampler_apply for the penalty - the same three calls a
// generation loop makes.
//
// Compared: every logit, bit for bit. DRY subtracts a float penalty and nothing
// else, so there is no sentinel divergence to allow here, unlike top-n-sigma.
//
// No model, no GPU, no context.
#include "runtime.h"

#include <llama.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

// Not in the public header, but exported by the library and declared in
// src/llama-sampler.h; the signature is copied from there.
// The real declaration, matching src/llama-sampler.h:41.
struct llama_sampler * llama_sampler_init_dry_testing(
        float dry_multiplier, float dry_base, int32_t dry_allowed_length, int32_t dry_penalty_last_n,
        const std::vector<std::vector<llama_token>> & seq_breakers);

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }
static sl_i64 I(const std::vector<int64_t> & v) { return sl_i64{ const_cast<int64_t *>(v.data()), (long long) v.size() }; }
static sl_f32 F(std::vector<float> & v) { return sl_f32{ v.data(), (long long) v.size() }; }

int main() {
    goose_init(0, nullptr);
    // No llama_backend_init: DRY needs no backend, and the call loads CUDA and the
    // device stack, which this comparison does not use.

    std::mt19937 rng(968);
    std::uniform_int_distribution<int> tok(0, 6);
    std::uniform_int_distribution<int> hist_len(3, 30);
    std::uniform_real_distribution<float> logit(-4.0f, 4.0f);

    for (int trial = 0; trial < 300; trial++) {
        const int n = hist_len(rng);
        const int32_t last_n = 64;
        const int32_t allowed = 2;
        // multiplier 0 is skipped for the LIVE arm: llama_sampler_init_dry_testing
        // returns the empty sampler at that value and then writes breakers into a
        // context the empty sampler does not have, which aborts inside the library.
        // That is a defect in the native testing helper, reproduced in EXP968's
        // README; the empty case is checked separately through dry_apply alone.
        const float mult = 0.4f + 0.15f * (float) (trial % 5);
        const float base = (trial % 3 == 0) ? 1000.0f : 1.75f;

        std::vector<std::vector<llama_token>> breakers;
        if (trial % 2 == 0) breakers.push_back({ 13 });          // single-token breaker

        std::vector<llama_token> hist((size_t) n);
        for (int i = 0; i < n; i++) hist[(size_t) i] = tok(rng);

        const int n_logits = 7;
        std::vector<llama_token_data> nat((size_t) n_logits);
        std::vector<float> base_logits((size_t) n_logits);
        for (int i = 0; i < n_logits; i++) {
            base_logits[(size_t) i] = logit(rng);
            nat[(size_t) i].id = i;
            nat[(size_t) i].logit = base_logits[(size_t) i];
            nat[(size_t) i].p = 0.0f;
        }

        // ---- native: construct, accept the history, apply ----
        llama_sampler * s = llama_sampler_init_dry_testing(mult, base, allowed, last_n, breakers);
        check(s != nullptr, "dry_init_testing returned a sampler");
        for (int i = 0; i < n; i++) llama_sampler_accept(s, hist[(size_t) i]);
        llama_token_data_array arr{ nat.data(), nat.size(), -1, false };
        llama_sampler_apply(s, &arr);
        llama_sampler_free(s);

        // ---- Goose on the same history and logits ----
        std::vector<int64_t> ghist(hist.begin(), hist.end());
        std::vector<int64_t> gids;
        for (int i = 0; i < n_logits; i++) gids.push_back(i);
        std::vector<float> got = base_logits;

        // breakers and tails in the flat form dry_apply takes
        std::vector<int64_t> brk, tails;
        for (const auto & b : breakers) {
            brk.push_back(b[0]);
            brk.push_back((int64_t) tails.size());
            brk.push_back((int64_t) b.size() - 1);
            for (size_t k = 1; k < b.size(); k++) tails.push_back(b[k]);
        }
        std::vector<int64_t> counts((size_t) n + 1, 0), ptok((size_t) n_logits + 1, 0),
                             plen((size_t) n_logits + 1, 0), pcount(1, 0);
        const long long rc = dry_apply(I(ghist), (long long) ghist.size(), F(got), I(gids), n_logits,
                                       mult, base, allowed, last_n,
                                       I(brk), (long long) breakers.size(), I(tails), (long long) tails.size(),
                                       I(counts), I(ptok), I(plen), I(pcount));
        check(rc >= 0, "dry_apply did not refuse");

        for (int i = 0; i < n_logits; i++) {
            checks++;
            if (bits(arr.data[i].logit) != bits(got[(size_t) i])) {
                std::printf("FAIL trial %d id %d: logit %08x != %08x (native %g goose %g)\n",
                            trial, i, bits(arr.data[i].logit), bits(got[(size_t) i]),
                            arr.data[i].logit, got[(size_t) i]);
                failures++;
            }
        }
    }

    // ---- the empty path, Goose only ----
    //
    // The live arm cannot cover multiplier 0: llama_sampler_init_dry_testing
    // segfaults for it even with an empty breaker list, because it builds the empty
    // sampler and then dereferences a context that sampler does not have. Verified
    // in isolation in EXP968's README. dry_apply is checked here directly, and its
    // empty case is the same predicate the live sampler uses to return early.
    {
        std::vector<float> g = { 1.0f, 2.0f };
        std::vector<int64_t> gids = { 0, 1 }, h = { 1, 2, 3 };
        std::vector<int64_t> counts(4), ptok(3), plen(3), pcount(1), brk, tails;
        dry_apply(I(h), 3, F(g), I(gids), 2, 0.0f, 1.75f, 2, 64, I(brk), 0, I(tails), 0,
                  I(counts), I(ptok), I(plen), I(pcount));
        check(g[0] == 1.0f && g[1] == 2.0f, "dry_apply with multiplier 0 changes nothing");
    }

    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
