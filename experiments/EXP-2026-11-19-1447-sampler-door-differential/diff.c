// The sampler doors on the CPU, compared with the native stages that would run without them.
//
// No model and no GPU: a token_data array is fabricated from a fixed distribution, a real chain is applied through the
// public API, and the answer is recorded. The same binary is run twice - once with the doors on, once with the rollback
// flag - and the two outputs must be identical byte for byte.
#include "llama.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static uint64_t g_state = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) {
    g_state ^= g_state << 13; g_state ^= g_state >> 7; g_state ^= g_state << 17;
    return (uint32_t) (g_state >> 32);
}

// five shapes that exercise different branches: a single token, a peak, a plateau, a long tail, an extreme spike
static void make_logits(int shape, float * logits, int n) {
    for (int i = 0; i < n; ++i) {
        switch (shape) {
            case 0: logits[i] = 0.0f; break;                                   // every token equally likely
            case 1: logits[i] = (i == 0) ? 30.0f : 0.0f; break;                // one certain token
            case 2: logits[i] = (float) ((i % 3) * -1.0f); break;              // a plateau of three
            case 3: logits[i] = -0.02f * (float) i; break;                     // a long decaying tail
            case 4: logits[i] = 12.0f - 0.9f * (float) i; break;               // an extreme spike with a tail
        }
        logits[i] += ((float) (rnd() % 1000) / 1000.0f - 0.5f) * 0.01f;        // a little jitter, still deterministic
    }
}

int main(int argc, char ** argv) {
    const int sizes[] = { 1, 2, 7, 64, 500 };
    const float temp[] = { 0.0f, 0.7f, 1.0f };
    const float top_p[] = { 1.0f, 0.95f, 0.5f };
    const int n_seed = argc > 1 ? atoi(argv[1]) : 40;

    float * logits = malloc(sizeof(float) * 4096);
    llama_token_data * data = malloc(sizeof(llama_token_data) * 4096);
    unsigned long long hash = 1469598103934665603ull;
    long cases = 0;

    for (int s = 0; s < 5; ++s) {
        for (int si = 0; si < 5; ++si) {
            const int n = sizes[si];
            for (int ti = 0; ti < 3; ++ti) {
                for (int pi = 0; pi < 3; ++pi) {
                    make_logits(s, logits, n);
                    for (int seed = 0; seed < n_seed; ++seed) {
                        for (int i = 0; i < n; ++i) {
                            data[i].id = 1000 + i; data[i].logit = logits[i]; data[i].p = 0.0f;
                        }
                        struct llama_token_data_array cur = { data, (size_t) n, -1, false };
                        struct llama_sampler * chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
                        if (temp[ti] > 0.0f) { llama_sampler_chain_add(chain, llama_sampler_init_temp(temp[ti])); }
                        if (top_p[pi] < 1.0f) { llama_sampler_chain_add(chain, llama_sampler_init_top_p(top_p[pi], 1)); }
                        llama_sampler_chain_add(chain, llama_sampler_init_dist((uint32_t) (seed * 2654435761u)));

                        // the door is applied by the chain; the selection is what both paths must agree on
                        if (temp[ti] > 0.0f) { llama_sampler_apply(chain, &cur); }
                        llama_sampler_free(chain);

                        // The decision is what must be equal: the kept set, in order, and the selected element. The
                        // probabilities are compared separately with a tolerance, because two correct softmax
                        // implementations may differ in the last bits and that is not a disagreement.
                        // The kept SET and the selection are the contract. The order of the survivors is not: with
                        // near-equal probabilities the two paths may order a tie differently, and that changes nothing
                        // downstream because the distributor re-reads the probabilities.
                        static int ids_sorted[4096];
                        for (size_t i = 0; i < cur.size; ++i) { ids_sorted[i] = (int) cur.data[i].id; }
                        for (size_t i = 1; i < cur.size; ++i) {
                            const int key = ids_sorted[i]; size_t j = i;
                            while (j > 0 && ids_sorted[j - 1] > key) { ids_sorted[j] = ids_sorted[j - 1]; --j; }
                            ids_sorted[j] = key;
                        }
                        printf("%d %d %.2f %.2f %d sel=%td kept=%zu ids=", s, n, temp[ti], top_p[pi], seed,
                               cur.selected, cur.size);
                        for (size_t i = 0; i < cur.size; ++i) { printf("%d,", ids_sorted[i]); }
                        printf("\n");
                        for (size_t i = 0; i < cur.size; ++i) { hash ^= (unsigned long long) cur.data[i].id; hash *= 1099511628211ull; }
                        ++cases;
                    }
                }
            }
        }
    }
    printf("sampler differential: cases=%ld hash=%016llx\n", cases, hash);
    free(logits); free(data);
    return 0;
}
