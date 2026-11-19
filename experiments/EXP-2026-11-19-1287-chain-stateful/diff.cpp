// Stateful samplers inside the chain: DRY (with the corrected context order), typical, XTC, mirostat and
// adaptive-p, each composed with the rest the server uses.
//
// DRY matters most here. Its defect was an index convention, fixed and verified in isolation; a chain run is
// what shows the fix holds when the sampler is fed by a real accept history and followed by further stages.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

struct llama_sampler * llama_sampler_init_dry_testing(float, float, int32_t, int32_t,
                                                      const std::vector<std::vector<llama_token>> &);

static void run(FILE * f, int which, std::mt19937 & rng, std::uniform_real_distribution<float> & u) {
    auto sparams = llama_sampler_chain_default_params();
    llama_sampler * chain = llama_sampler_chain_init(sparams);
    std::vector<std::vector<llama_token>> br = { { 10 }, { 10, 13 } };
    if (which == 0) {
        llama_sampler_chain_add(chain, llama_sampler_init_penalties(1000, 48, 1.05f, 0.02f, 0.02f));
        llama_sampler_chain_add(chain, llama_sampler_init_dry_testing(0.8f, 1.75f, 2, 32, br));
        llama_sampler_chain_add(chain, llama_sampler_init_top_k(20));
        llama_sampler_chain_add(chain, llama_sampler_init_top_p(0.95f, 1));
        llama_sampler_chain_add(chain, llama_sampler_init_temp(0.8f));
        llama_sampler_chain_add(chain, llama_sampler_init_dist(31337u));
    } else if (which == 2) {
        // the configuration the flag explanation was established on: no mirostat between XTC and adaptive
        llama_sampler_chain_add(chain, llama_sampler_init_typical(0.9f, 1));
        llama_sampler_chain_add(chain, llama_sampler_init_xtc(0.8f, 0.1f, 1, 2718u));
        llama_sampler_chain_add(chain, llama_sampler_init_adaptive_p(0.5f, 0.5f, 1414u));
        llama_sampler_chain_add(chain, llama_sampler_init_dist(99991u));
    } else {
        llama_sampler_chain_add(chain, llama_sampler_init_typical(0.9f, 1));
        llama_sampler_chain_add(chain, llama_sampler_init_xtc(0.8f, 0.1f, 1, 2718u));
        llama_sampler_chain_add(chain, llama_sampler_init_mirostat(1000, 1618u, 5.0f, 0.1f, 100));
        llama_sampler_chain_add(chain, llama_sampler_init_adaptive_p(0.5f, 0.5f, 1414u));
        llama_sampler_chain_add(chain, llama_sampler_init_dist(99991u));
    }

    for (int round = 0; round < 100; ++round) {
        const int n = 12 + (int)(rng() % 40);
        std::vector<llama_token_data> base(n);
        for (int i = 0; i < n; ++i) {
            base[i].id = (llama_token)(rng() % 200);
            base[i].logit = -6.0f + 12.0f * u(rng);
            base[i].p = 0.0f;
        }
        std::sort(base.begin(), base.end(), [](const llama_token_data & a, const llama_token_data & b) { return a.logit > b.logit; });
        llama_token_data_array cur{ base.data(), base.size(), true };
        llama_sampler_apply(chain, &cur);
        // selected is an INDEX into cur.data, and the native path may have advanced that pointer (XTC
        // does), so the index alone is not comparable - the token it points at is.
        const int tok = (cur.selected >= 0 && (size_t) cur.selected < cur.size)
                            ? (int) cur.data[cur.selected].id : -1;
        fprintf(f, "%d %d %d %zu\n", which, round, tok, cur.size);
        // a history with breakers and repeats, so DRY has something real to read
        // cur.selected is an INDEX; accepting it as a token fed the history with positions instead of ids
        // and made configuration 2 look identical for the wrong reason.
        const llama_token accepted = (round % 5 == 0) ? 10 : tok;
        fprintf(f, "  %08x\n", *(const uint32_t *) &cur.data[0].logit);
        llama_sampler_accept(chain, accepted);
    }
    llama_sampler_free(chain);
}

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/chain_stateful.txt";
    FILE * f = fopen(out, "wb");
    if (!f) return 2;
    std::mt19937 rng(777111);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    run(f, 0, rng, u);
    run(f, 1, rng, u);
    run(f, 2, rng, u);
    fclose(f);
    printf("chain stateful: 3 configurations x 100 rounds\n");
    return 0;
}
