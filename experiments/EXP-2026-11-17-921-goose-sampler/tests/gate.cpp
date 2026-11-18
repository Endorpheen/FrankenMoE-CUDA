// EXP921 differential gate: Goose sampler chain vs llama_sampler_chain on the
// same logits, the same configuration and the same seed.
#include "runtime.h"

#include <llama.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
static long long compared = 0;

static sl_u8 S(const void * p, long long n) {
    return sl_u8{const_cast<uint8_t *>(static_cast<const uint8_t *>(p)), n};
}
static sl_i64 I(const std::vector<int64_t> & v) { return sl_i64{const_cast<int64_t *>(v.data()), (long long) v.size()}; }
static sl_i64 I(int64_t * p, long long n) { return sl_i64{p, n}; }
static sl_f32 F(const std::vector<float> & v) { return sl_f32{const_cast<float *>(v.data()), (long long) v.size()}; }

struct Config {
    const char * name;
    int32_t top_k;
    float top_p;
    float min_p;
    float temp;
};

static const Config configs[] = {
    {"k20-p0.95-t1", 20, 0.95f, 0.0f, 1.0f},
    {"k40-p0.9-t0.8", 40, 0.90f, 0.0f, 0.8f},
    {"k100-p0.5-t1.2", 100, 0.50f, 0.0f, 1.2f},
    {"k1", 1, 1.0f, 0.0f, 1.0f},
    {"greedy", 20, 0.95f, 0.0f, 0.0f},
};

static uint64_t lcg_state = 12345;
static float lcg_f() {
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float) ((lcg_state >> 40) & 0xFFFFFF) / (float) 0x1000000;
}

static std::vector<float> make_logits(int which, int32_t n) {
    std::vector<float> v((size_t) n);
    switch (which) {
        case 0: for (auto & x : v) x = lcg_f() * 20.0f - 10.0f; break;
        case 1: for (auto & x : v) x = 1.5f; break;                       // every logit equal
        case 2: for (size_t i = 0; i < v.size(); i++) v[i] = (i == 7 || i == 1000) ? 3.0f : 0.0f; break;
        case 3: for (size_t i = 0; i < v.size(); i++) v[i] = (float) (i % 100); v[12345] = 500.0f; break;
        case 4: for (auto & x : v) x = 12.0f + lcg_f() * 0.01f; break;    // narrow
        case 5: for (auto & x : v) x = lcg_f() * 600.0f - 300.0f; break;  // wide
        case 6:                                                           // top logit repeated across the cut
            for (size_t i = 0; i < v.size(); i++) v[i] = (i < 25) ? 4.0f : 1.0f;
            break;
        default: for (auto & x : v) x = 0.0f; break;
    }
    return v;
}

static llama_sampler * make_chain(const Config & c, uint32_t seed) {
    auto cp = llama_sampler_chain_default_params();
    llama_sampler * ch = llama_sampler_chain_init(cp);
    llama_sampler_chain_add(ch, llama_sampler_init_top_k(c.top_k));
    llama_sampler_chain_add(ch, llama_sampler_init_top_p(c.top_p, 1));
    llama_sampler_chain_add(ch, llama_sampler_init_min_p(c.min_p, 1));
    llama_sampler_chain_add(ch, llama_sampler_init_temp(c.temp));
    llama_sampler_chain_add(ch, llama_sampler_init_dist(seed));
    return ch;
}

static int32_t native_pick(llama_sampler * ch, const std::vector<float> & logits,
                           std::vector<llama_token_data> & data) {
    const int32_t n = (int32_t) logits.size();
    for (int32_t i = 0; i < n; i++) data[(size_t) i] = llama_token_data{i, logits[(size_t) i], 0.0f};
    llama_token_data_array cur{data.data(), data.size(), -1, false};
    llama_sampler_apply(ch, &cur);
    if (cur.selected < 0 || (size_t) cur.selected >= cur.size) return -1;
    return cur.data[cur.selected].id;
}

int main(int argc, char ** argv) {
    (void) argv;
    goose_init(0, nullptr);
    llama_backend_init();

    const int32_t n = 248320;  // the Apex vocabulary size the profile samples from
    std::vector<llama_token_data> data((size_t) n);
    std::vector<float> lv((size_t) n), p((size_t) n);
    std::vector<int64_t> id((size_t) n), state(625), out(3), ip(3);
    // 0xFFFFFFFF is LLAMA_DEFAULT_SEED: the native sampler replaces it with a
    // clock/random value, so it cannot be compared and is not used.
    const uint32_t seeds[] = {0u, 1u, 42u, 123456789u, 987654321u};

    for (int which = 0; which < 7; which++) {
        auto logits = make_logits(which, n);
        for (const Config & c : configs) {
            for (uint32_t seed : seeds) {
                llama_sampler * ch = make_chain(c, seed);
                const int32_t want = native_pick(ch, logits, data);
                llama_sampler_free(ch);

                std::copy(logits.begin(), logits.end(), lv.begin());
                if (sm_mt_seed(I(state), seed) != 0) {
                    std::printf("FAIL seed\n");
                    failures++;
                    continue;
                }
                ip[0] = c.top_k;
                ip[1] = 1;
                ip[2] = 1;
                std::vector<float> fp = {c.top_p, c.min_p, c.temp};
                const int64_t rc = sm_select(F(lv), I(id), F(p), n, I(ip), F(fp), I(state), I(out));
                compared++;
                if (rc != 0) {
                    std::printf("FAIL rc=%lld case=%d config=%s seed=%u\n", (long long) rc, which, c.name, seed);
                    failures++;
                    continue;
                }
                if (out[1] != want) {
                    std::printf("FAIL case=%d config=%s seed=%u goose=%lld native=%d survivors=%lld\n",
                                which, c.name, seed, (long long) out[1], want, (long long) out[2]);
                    failures++;
                }
            }
        }
    }

    // a run of draws: both generators must advance identically
    {
        auto logits = make_logits(0, n);
        const Config & c = configs[0];
        const uint32_t seed = 42;
        llama_sampler * ch = make_chain(c, seed);
        std::vector<float> lv2(logits);
        sm_mt_seed(I(state), seed);
        ip[0] = c.top_k;
        ip[1] = 1;
        ip[2] = 1;
        std::vector<float> fp = {c.top_p, c.min_p, c.temp};
        for (int draw = 0; draw < 32; draw++) {
            const int32_t want = native_pick(ch, logits, data);
            std::copy(logits.begin(), logits.end(), lv2.begin());
            const int64_t rc = sm_select(F(lv2), I(id), F(p), n, I(ip), F(fp), I(state), I(out));
            compared++;
            if (rc != 0 || out[1] != want) {
                std::printf("FAIL draw=%d goose=%lld native=%d rc=%lld\n", draw, (long long) out[1], want,
                            (long long) rc);
                failures++;
                break;
            }
        }
        llama_sampler_free(ch);
    }

    std::printf("\ncompared=%lld failures=%d\n", compared, failures);
    llama_backend_free();
    return failures == 0 ? 0 : 1;
}
