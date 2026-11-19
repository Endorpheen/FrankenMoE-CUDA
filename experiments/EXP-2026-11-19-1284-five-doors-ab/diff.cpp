// The five remaining sampler doors, each driven through the public API with the door on and off.
//
// penalties, typical, xtc, mirostat and adaptive-p all sit behind a `fg_goose_*_enabled()` switch that
// reads the environment once per process, so the harness writes one file per mode and the wrapper
// compares them. What is compared is the array the sampler returns - values, and where applicable
// order and size - because that is what the caller sees.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

static void dump(FILE * f, const char * arm, int it, llama_token_data_array & cur,
                 const std::vector<llama_token_data> & d, int extra_a, float extra_b) {
    fprintf(f, "%s %d a=%d b=%.6f size=%zu selected=%lld\n", arm, it, extra_a, extra_b, cur.size,
            (long long) cur.selected);
    // cur.data, not d.data(): the native path may ADVANCE the pointer (llama_sample_xtc_apply does
    // `cur_p->data += pos_last`), and printing from the original start shows the discarded entries as
    // if they had been kept. That was this harness's own defect, and it made XTC look broken.
    for (size_t i = 0; i < cur.size; ++i) {
        fprintf(f, "%d %08x %08x\n", (int) cur.data[i].id,
                *(const uint32_t *) &cur.data[i].logit, *(const uint32_t *) &cur.data[i].p);
    }
}

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/five.txt";
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(777001);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    for (int it = 0; it < 60; ++it) {
        const int n = 3 + (int)(rng() % 40);
        std::vector<llama_token_data> base(n);
        float mx = -1e30f;
        for (int i = 0; i < n; ++i) {
            base[i].id = (llama_token)(rng() % 500);
            base[i].logit = -10.0f + 20.0f * u(rng);
            mx = std::max(mx, base[i].logit);
        }
        float z = 0.0f;
        for (auto & c : base) { c.p = expf(c.logit - mx); z += c.p; }
        for (auto & c : base) c.p /= z;
        // The default chain runs top_k before these samplers, so the array arrives sorted by logit.
        // Both modes get the same sorted input, and `sorted` reflects it.
        std::sort(base.begin(), base.end(), [](const llama_token_data & a, const llama_token_data & b) { return a.logit > b.logit; });

        // penalties: needs accepted tokens, and its door is enabled by GGML_GOOSE_SAMPLER_PENALTIES
        {
            const float rep = 1.0f + 0.5f * u(rng);
            const float freq = 0.3f * u(rng), pres = 0.3f * u(rng);
            llama_sampler * s = llama_sampler_init_penalties(500, 24, rep, freq, pres);
            for (int i = 0; i < 12; ++i) llama_sampler_accept(s, (llama_token)(rng() % 500));
            auto d = base;
            llama_token_data_array cur{ d.data(), d.size(), true };
            llama_sampler_apply(s, &cur);
            dump(f, "penalties", it, cur, d, 0, rep);
            llama_sampler_free(s);
        }
        // typical
        {
            const float p = 0.05f + 0.9f * u(rng);
            llama_sampler * s = llama_sampler_init_typical(p, (size_t)(rng() % 5));
            auto d = base;
            llama_token_data_array cur{ d.data(), d.size(), true };
            llama_sampler_apply(s, &cur);
            dump(f, "typical", it, cur, d, 0, p);
            llama_sampler_free(s);
        }
        // xtc
        {
            const float p = 0.3f + 0.6f * u(rng), th = 0.02f + 0.2f * u(rng);
            const size_t mk = (size_t)(rng() % 4);
            llama_sampler * s = llama_sampler_init_xtc(p, th, mk, 4242u + (uint32_t) it);
            auto d = base;
            llama_token_data_array cur{ d.data(), d.size(), true };
            llama_sampler_apply(s, &cur);
            fprintf(f, "xtc %d prob=%.6f thr=%.6f mk=%zu n_in=%zu size=%zu selected=%lld\n",
                    it, p, th, mk, base.size(), cur.size, (long long) cur.selected);
            for (size_t i = 0; i < cur.size; ++i) {
                fprintf(f, "%d %08x %08x\n", (int) cur.data[i].id,
                        *(const uint32_t *) &cur.data[i].logit, *(const uint32_t *) &cur.data[i].p);
            }
            llama_sampler_free(s);
        }
        // mirostat: keeps state across draws, so the same sampler is applied several times
        {
            llama_sampler * s = llama_sampler_init_mirostat(500, 99u + (uint32_t) it, 5.0f, 0.1f, 100);
            auto d = base;
            for (int k = 0; k < 4; ++k) {
                d = base;
                llama_token_data_array cur{ d.data(), d.size(), true };
                llama_sampler_apply(s, &cur);
                dump(f, "mirostat", it * 4 + k, cur, d, k, 5.0f);
                llama_sampler_accept(s, cur.selected);
            }
            llama_sampler_free(s);
        }
        // adaptive-p
        {
            const float target = 0.2f + 0.6f * u(rng), decay = 0.1f + 0.8f * u(rng);
            llama_sampler * s = llama_sampler_init_adaptive_p(target, decay, 5150u + (uint32_t) it);
            auto d = base;
            for (int k = 0; k < 4; ++k) {
                d = base;
                llama_token_data_array cur{ d.data(), d.size(), true };
                llama_sampler_apply(s, &cur);
                dump(f, "adaptive", it * 4 + k, cur, d, k, target);
                llama_sampler_accept(s, cur.selected);
            }
            llama_sampler_free(s);
        }
    }
    fclose(f);
    printf("five doors: 60 rounds written\n");
    return 0;
}
