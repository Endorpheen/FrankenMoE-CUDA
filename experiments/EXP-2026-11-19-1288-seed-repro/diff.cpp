#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

int main(int argc, char ** argv) {
    const uint32_t seed = (uint32_t) atoi(argv[1]);
    const char * out = argv[2];
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(555);           // inputs are fixed; only the sampler seed varies
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    auto sp = llama_sampler_chain_default_params();
    llama_sampler * chain = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(chain, llama_sampler_init_penalties(1000, 64, 1.1f, 0.05f, 0.05f));
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(20));
    llama_sampler_chain_add(chain, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(seed));
    for (int round = 0; round < 60; ++round) {
        const int n = 16 + (int)(rng() % 40);
        std::vector<llama_token_data> b(n);
        for (int i = 0; i < n; ++i) { b[i].id = (llama_token)(rng() % 1000); b[i].logit = -8.0f + 16.0f * u(rng); b[i].p = 0.0f; }
        std::sort(b.begin(), b.end(), [](const llama_token_data & a, const llama_token_data & c) { return a.logit > c.logit; });
        llama_token_data_array cur{ b.data(), b.size(), true };
        llama_sampler_apply(chain, &cur);
        const llama_token tok = (cur.selected >= 0 && (size_t) cur.selected < cur.size) ? cur.data[cur.selected].id : LLAMA_TOKEN_NULL;
        fprintf(f, "%d\n", (int) tok);
        llama_sampler_accept(chain, tok);
    }
    llama_sampler_free(chain);
    fclose(f);
    return 0;
}
