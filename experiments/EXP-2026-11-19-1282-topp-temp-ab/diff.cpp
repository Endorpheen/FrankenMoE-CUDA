// Top-P and temperature: the ported doors against the native code, through the public sampler API.
//
// Both are on the acceptance path - the fixed profile samples with top_p 0.95 and temp 0.8 - and their
// evidence so far came from harnesses that build the array themselves. DRY taught what that can hide:
// a harness that mirrors the input agrees with a port that mirrors the input.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <random>
#include <vector>

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/topp_ab.txt";
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(60321);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    for (int it = 0; it < 300; ++it) {
        const int n = 2 + (int)(rng() % 60);
        const float p = 0.05f + 0.94f * u(rng);
        const size_t mk = (size_t)(rng() % 8);
        const float t = 0.05f + 3.0f * u(rng);
        std::vector<llama_token_data> base(n);
        for (int i = 0; i < n; ++i) {
            base[i].id = (llama_token)(rng() % 200);
            base[i].logit = -10.0f + 20.0f * u(rng);
            base[i].p = 0.0f;
        }

        // top-p: the native sorts by logit and truncates, so the array it returns is the sorted one
        {
            llama_sampler * s = llama_sampler_init_top_p(p, mk);
            auto d = base;
            llama_token_data_array cur{ d.data(), d.size(), false };
            llama_sampler_apply(s, &cur);
            fprintf(f, "topp %d p=%.6f mk=%zu size=%zu\n", it, p, mk, cur.size);
            for (size_t i = 0; i < cur.size; ++i) fprintf(f, "%d %08x\n", (int) d[i].id, *(const uint32_t *) &d[i].logit);
            llama_sampler_free(s);
        }
        // dynamic temperature: this is the sampler whose apply calls the Goose door. The plain
        // llama_sampler_init_temp runs llama_sampler_temp_impl directly, so comparing it would compare
        // native with native and prove nothing.
        {
            const float delta = (rng() % 3 == 0) ? 0.0f : 0.5f * u(rng);
            const float exponent = 1.0f + 2.0f * u(rng);
            llama_sampler * s = llama_sampler_init_temp_ext(t, delta, exponent);
            auto d = base;
            llama_token_data_array cur{ d.data(), d.size(), false };
            llama_sampler_apply(s, &cur);
            fprintf(f, "tempext %d t=%.6f d=%.6f e=%.6f size=%zu\n", it, t, delta, exponent, cur.size);
            for (size_t i = 0; i < cur.size; ++i) fprintf(f, "%d %08x\n", (int) d[i].id, *(const uint32_t *) &d[i].logit);
            llama_sampler_free(s);
        }
    }
    fclose(f);
    printf("topp+temp ab: 300 cases written\n");
    return 0;
}
