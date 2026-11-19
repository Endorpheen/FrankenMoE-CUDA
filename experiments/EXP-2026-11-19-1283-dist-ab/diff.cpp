// The dist sampler: the two doors against the native generators, through the public API.
//
// This is the door every sampled token passes through, and the drawing order is the part that is easy
// to get wrong: the native code consumes the RNG words in a specific order and the door has to see
// them in that same order. What is compared is the token actually selected, over a long run from one
// seed - not a formula, the behaviour.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <vector>

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/dist_ab.txt";
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(90210);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    // several distributions, each sampled many times from its own seed
    for (int d = 0; d < 40; ++d) {
        const int n = 2 + (int)(rng() % 50);
        std::vector<llama_token_data> base(n);
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) {
            base[i].id = (llama_token)(rng() % 32000);
            base[i].logit = -12.0f + 12.0f * u(rng);
            base[i].p = 0.0f;
            sum += 1.0f;
        }
        llama_sampler * s = llama_sampler_init_dist(1234 + (uint32_t) d);
        fprintf(f, "dist %d n=%d\n", d, n);
        for (int k = 0; k < 200; ++k) {
            auto arr = base;
            llama_token_data_array cur{ arr.data(), arr.size(), false };
            // the chain normally has probabilities ready; supply them so both paths see the same input
            float mx = -1e30f;
            for (auto & c : arr) mx = std::max(mx, c.logit);
            float z = 0.0f;
            for (auto & c : arr) { c.p = expf(c.logit - mx); z += c.p; }
            for (auto & c : arr) c.p /= z;
            llama_sampler_apply(s, &cur);
            fputc(' ', f);
            fprintf(f, "%d", (int) cur.selected);
        }
        fputc('\n', f);
        llama_sampler_free(s);
    }
    fclose(f);
    printf("dist ab: 40 distributions x 200 draws\n");
    return 0;
}
