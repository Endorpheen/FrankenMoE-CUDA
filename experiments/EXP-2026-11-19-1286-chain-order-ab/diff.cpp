// The filter order, tested as the chain the server actually builds.
//
// Individual doors can each match and still compose wrongly: a door that consumes RNG words in a different
// order, or hands back an array in a different state, changes what the next stage sees. This harness builds one
// chain in the order the server adds them, applies it to the same logits for many rounds from one seed, and
// records the token each round selected. Door on and door off must agree token for token.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/chain_order.txt";
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(24601);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    auto sparams = llama_sampler_chain_default_params();
    llama_sampler * chain = llama_sampler_chain_init(sparams);
    // the server's order: penalties, top_k, top_p, temp, then the selection
    llama_sampler_chain_add(chain, llama_sampler_init_penalties(1000, 64, 1.1f, 0.05f, 0.05f));
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(20));
    llama_sampler_chain_add(chain, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(987654321u));

    for (int round = 0; round < 120; ++round) {
        const int n = 16 + (int)(rng() % 60);
        std::vector<llama_token_data> base(n);
        for (int i = 0; i < n; ++i) {
            base[i].id = (llama_token)(rng() % 1000);
            base[i].logit = -8.0f + 16.0f * u(rng);
            base[i].p = 0.0f;
        }
        std::sort(base.begin(), base.end(), [](const llama_token_data & a, const llama_token_data & b) { return a.logit > b.logit; });
        llama_token_data_array cur{ base.data(), base.size(), true };
        llama_sampler_apply(chain, &cur);
        fprintf(f, "%d %d %zu\n", round, (int) cur.selected, cur.size);
        fprintf(f, "  logit %08x p %08x\n",
                *(const uint32_t *) &cur.data[0].logit, *(const uint32_t *) &cur.data[0].p);
        llama_sampler_accept(chain, cur.selected);
    }
    llama_sampler_free(chain);
    fclose(f);
    printf("chain order: 120 rounds written\n");
    return 0;
}
