// Two differentials, both against something that is not my own code.
//
// 1. sm_mt_next against std::mt19937, bit for bit, for four seeds and a thousand
//    draws each. The draw decides the token; a generator that is merely similar
//    produces a different sentence, not an error.
// 2. sm_select - top_k, top_p, min_p, temperature, draw - against the real libllama
//    sampler chain. The module's own comment says the chain "is" these samplers; this
//    checks that claim instead of repeating it.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "llama.h"
#include "runtime.h"

static int checks = 0;
static int fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        fails++;
        std::printf("FAIL %s\n", what);
    }
}

static sl_i64 I(std::vector<int64_t> &v) { return sl_i64{ v.data(), (int64_t) v.size() }; }
static sl_f32 F(std::vector<float> &v) { return sl_f32{ v.data(), (int64_t) v.size() }; }

// Deterministic logits with no ties: a tie would make the comparison depend on
// ordering rather than on arithmetic.
static std::vector<float> logits(int n, unsigned seed) {
    std::mt19937 r(seed);
    std::uniform_real_distribution<float> u(-8.0f, 8.0f);
    std::vector<float> v((size_t) n);
    for (int i = 0; i < n; i++) {
        v[(size_t) i] = u(r);
    }
    v[0] += 0.37f;   // keep index 0 off any accidental tie with its neighbour
    return v;
}

int main() {
    goose_init(nullptr == nullptr ? 0 : 0, (char **) 0);

    // ---- 1. the generator ----
    const int seeds[4] = { 0, 1, 42, 123456789 };
    for (int s = 0; s < 4; s++) {
        std::vector<int64_t> st(625, 0);
        if (sm_mt_seed(I(st), seeds[s]) != 0) {
            check(0, "sm_mt_seed accepted a 625-word state");
            continue;
        }
        std::mt19937 ref((unsigned) seeds[s]);
        int first_bad = -1;
        for (int i = 0; i < 1000; i++) {
            uint32_t want = ref();
            int64_t got = sm_mt_next(I(st));
            if ((uint32_t) got != want && first_bad < 0) {
                first_bad = i;
            }
        }
        char msg[128];
        std::snprintf(msg, sizeof msg, "mt19937 seed %d: 1000 draws match std::mt19937", seeds[s]);
        check(first_bad < 0, msg);
        if (first_bad >= 0) {
            std::printf("     first difference at draw %d\n", first_bad);
        }
    }

    // ---- 2. the chain ----
    struct Case { int k; float p; float minp; float temp; } cases[3] = {
        { 20, 1.00f, 0.00f, 1.0f },   // top_k only
        { 20, 0.95f, 0.00f, 1.0f },   // top_k then top_p
        { 20, 0.95f, 0.05f, 1.0f },   // and min_p
    };
    const int n = 32;
    const int seeds_c[3] = { 7, 99, 2024 };

    for (int c = 0; c < 3; c++) {
        int agree = 0, total = 0;
        for (int si = 0; si < 3; si++) {
            const unsigned seed = (unsigned) seeds_c[si];
            std::vector<float> base = logits(n, seed ^ 0x5bd1e995u);

            // native chain
            llama_sampler_chain_params cp = llama_sampler_chain_default_params();
            llama_sampler * chain = llama_sampler_chain_init(cp);
            llama_sampler_chain_add(chain, llama_sampler_init_top_k(cases[c].k));
            llama_sampler_chain_add(chain, llama_sampler_init_top_p(cases[c].p, 0));
            llama_sampler_chain_add(chain, llama_sampler_init_min_p(cases[c].minp, 1));
            llama_sampler_chain_add(chain, llama_sampler_init_temp(cases[c].temp));
            llama_sampler_chain_add(chain, llama_sampler_init_dist(seed));

            std::vector<llama_token_data> cur((size_t) n);
            for (int i = 0; i < n; i++) {
                cur[(size_t) i] = llama_token_data{ i, base[(size_t) i], 0.0f };
            }
            llama_token_data_array arr{ cur.data(), cur.size(), false };
            llama_sampler_apply(chain, &arr);
            const llama_token native_id = arr.data[arr.selected].id;
            llama_sampler_free(chain);

            // goose
            std::vector<float> lv = base;
            std::vector<int64_t> id((size_t) n, 0);
            std::vector<float> p((size_t) n, 0.0f);
            std::vector<int64_t> ip = { cases[c].k, 0, 1 };
            std::vector<float> fp = { cases[c].p, cases[c].minp, cases[c].temp };
            std::vector<int64_t> st(625, 0), out(4, -1);
            sm_mt_seed(I(st), (int64_t) seed);
            const int64_t rc = sm_select(F(lv), I(id), F(p), n, (sl_i64){ ip.data(), (int64_t) ip.size() },
                                         (sl_f32){ fp.data(), (int64_t) fp.size() }, I(st), I(out));
            total++;
            // out[0] is the INDEX in the candidate array and out[1] is the token id;
            // the first version of this gate compared out[0] with the native token and
            // reported a mismatch that was the gate's own bug.
            if (rc >= 0 && out[1] == (int64_t) native_id) {
                agree++;
            } else if (rc < 0) {
                std::printf("     case %d seed %u: sm_select returned %lld\n", c, seed, (long long) rc);
            } else {
                std::printf("     case %d seed %u: goose id %lld index %lld vs native %d\n", c, seed,
                            (long long) out[1], (long long) out[0], (int) native_id);
            }
        }
        char msg[160];
        std::snprintf(msg, sizeof msg,
                      "chain case %d (k=%d p=%.2f minp=%.2f): goose picks the native token %d/%d",
                      c, cases[c].k, cases[c].p, cases[c].minp, agree, total);
        check(agree == total, msg);
    }

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
