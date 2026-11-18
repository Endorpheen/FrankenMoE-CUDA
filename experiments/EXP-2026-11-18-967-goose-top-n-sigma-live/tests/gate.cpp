// EXP967 differential gate: the Goose top-n-sigma mask, compared with the live
// sampler through the public API.
//
// EXP960 compared against a transcription and asked "is the arithmetic the same".
// This asks "does the mask agree with the sampler the runtime constructs", which
// is a different claim and a stricter one: the native apply reads
// cur_p->data[i].logit, writes -INFINITY, and the chain may then sort again.
//
// The one known divergence is the sentinel: native writes -INFINITY, this module
// writes the most negative finite float, because Goose has no infinity literal.
// The gate therefore compares the MASK DECISION per id, and separately checks that
// the surviving values are bit-identical - a masked entry is allowed to differ only
// in the sentinel, and an unmasked one is not allowed to differ at all.
//
// No model, no GPU, no context.
#include "runtime.h"

#include <llama.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what.c_str()); }
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, sizeof(u)); return u; }
static sl_f32 F(std::vector<float> & v) { return sl_f32{ v.data(), (long long) v.size() }; }

int main() {
    goose_init(0, nullptr);
    llama_backend_init();

    std::mt19937 rng(967);
    std::uniform_real_distribution<float> u(-6.0f, 6.0f);
    std::uniform_int_distribution<int> nsize(2, 48);

    for (int trial = 0; trial < 300; trial++) {
        const int n = nsize(rng);
        const float sigma = 0.25f + 3.5f * ((float) (rng() % 1000) / 1000.0f);

        std::vector<llama_token_data> cands((size_t) n);
        for (int i = 0; i < n; i++) {
            cands[(size_t) i].id = i;
            cands[(size_t) i].logit = u(rng);
            cands[(size_t) i].p = 0.0f;
        }

        // ---- native through the public API ----
        std::vector<llama_token_data> nat = cands;
        llama_token_data_array arr{ nat.data(), nat.size(), -1, false };
        llama_sampler * s = llama_sampler_init_top_n_sigma(sigma);
        llama_sampler_apply(s, &arr);
        llama_sampler_free(s);

        // ---- Goose on the same logits ----
        std::vector<float> got((size_t) n);
        for (int i = 0; i < n; i++) got[(size_t) i] = cands[(size_t) i].logit;
        const long long kept = tns_apply(F(got), n, sigma);
        check(kept >= 0, "tns_apply did not refuse");

        // ---- compare decisions, then values where both kept the entry ----
        for (int i = 0; i < n; i++) {
            const bool nat_masked = std::isinf(arr.data[i].logit) && arr.data[i].logit < 0;
            const bool got_masked = got[(size_t) i] < -1e38f;
            checks++;
            if (nat_masked != got_masked) {
                std::printf("FAIL trial %d id %d: native masked=%d goose=%d\n",
                            trial, i, nat_masked, got_masked);
                failures++;
                continue;
            }
            if (!nat_masked) {
                // Neither side masked it, so the value must be untouched, bit for bit.
                checks++;
                if (bits(arr.data[i].logit) != bits(got[(size_t) i])) {
                    std::printf("FAIL trial %d id %d: unmasked bits %08x != %08x\n",
                                trial, i, bits(arr.data[i].logit), bits(got[(size_t) i]));
                    failures++;
                }
            }
        }
    }

    // ---- the empty path ----
    {
        std::vector<llama_token_data> c = { {0, 1.0f, 0.0f} };
        llama_token_data_array arr{ c.data(), 1, -1, false };
        llama_sampler * s = llama_sampler_init_top_n_sigma(2.0f);
        llama_sampler_apply(s, &arr);   // size 1: native returns immediately
        check(arr.data[0].logit == 1.0f, "size 1 is untouched by the live sampler");

        std::vector<float> g = { 1.0f };
        check(tns_apply(F(g), 1, 2.0f) == 1 && g[0] == 1.0f, "size 1 is untouched by tns_apply");
        llama_sampler_free(s);
    }

    llama_backend_free();
    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
