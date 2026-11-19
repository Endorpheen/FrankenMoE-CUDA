// DRY penalty application: the ported door against the native formula, through the PUBLIC sampler API
// and the shipped library.
//
// The accepted record for this arm compared the port with a C++ restatement of the native formula. That
// is the weak kind of reference - it proves the port matches my reading of the code. Here the same
// sampler object is driven twice, once with the Goose door enabled and once with it disabled
// (GGML_GOOSE_SAMPLER_DRY), and the two logit vectors are compared bit for bit. The switch is read
// once per process, so the harness takes the mode as an argument and a wrapper runs both.
#include "llama.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>
#include <string>
#include <cstdlib>

struct llama_sampler * llama_sampler_init_dry_testing(float dry_multiplier, float dry_base,
                                                      int32_t dry_allowed_length, int32_t dry_penalty_last_n,
                                                      const std::vector<std::vector<llama_token>> & seq_breakers);

int main(int argc, char ** argv) {
    const char * out = argc > 1 ? argv[1] : "/tmp/dry_ab.txt";
    std::mt19937 rng(31337);
    std::vector<std::vector<llama_token>> breakers = { { 10 }, { 10, 13 } };
    FILE * f = fopen(out, "wb");
    if (!f) { printf("cannot open %s\n", out); return 2; }

    for (int it = 0; it < 200; ++it) {
        llama_sampler * smpl = llama_sampler_init_dry_testing(0.8f, 1.75f, 2, 32, breakers);
        if (!smpl) { printf("init failed\n"); return 3; }
        // a context with repeats, breakers and non-breaker tokens
        const int n_ctx = 24 + (int)(rng() % 24);
        std::vector<llama_token> ctx_tokens;
        for (int i = 0; i < n_ctx; ++i) {
            const int pick = (int)(rng() % 4);
            llama_token t = pick == 0 ? 10 : pick == 1 ? 7 : pick == 2 ? (llama_token)(20 + (int)(rng() % 5)) : (llama_token)(rng() % 40);
            llama_sampler_accept(smpl, t);
            ctx_tokens.push_back(t);
        }
        const int n_cand = 16 + (int)(rng() % 32);
        std::vector<llama_token_data> data(n_cand);
        for (int i = 0; i < n_cand; ++i) {
            data[i].id = (llama_token)(rng() % 40);
            data[i].logit = -8.0f + 8.0f * (float)(rng() % 1000) / 1000.0f;
            data[i].p = 0.0f;
        }
        llama_token_data_array cur{ data.data(), data.size(), false };
        llama_sampler_apply(smpl, &cur);
        fprintf(f, "case %d n_ctx=%d n_cand=%d\n", it, n_ctx, (int) data.size());
        if (const char * ctxfile = getenv("DRY_AB_CONTEXT")) {
            FILE * g = fopen(ctxfile, it == 0 ? "wb" : "ab");
            if (g) { for (llama_token t : ctx_tokens) fprintf(g, "%d ", (int) t); fprintf(g, "|"); for (auto & dc : data) fprintf(g, "%d ", (int) dc.id); fprintf(g, "\n"); fclose(g); }
        }
        for (const auto & d : data) fprintf(f, "%d %08x\n", (int) d.id, *(const uint32_t *) &d.logit);
        llama_sampler_free(smpl);
    }
    fclose(f);
    printf("dry ab: 200 cases written to %s\n", out);
    return 0;
}
