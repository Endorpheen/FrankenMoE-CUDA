// Load a Maxwell GGUF through the runtime API and run one forward pass.
// Test-only: the GGUF comes from tools/maxwell-convert.py with --stub-vocab.
#include "llama.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char ** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s model.gguf [logits.tsv]\n", argv[0]);
        return 2;
    }
    llama_backend_init();

    auto mparams = llama_model_default_params();
    mparams.n_gpu_layers = std::getenv("MX_GPU") ? 999 : 0;
    llama_model * model = llama_model_load_from_file(argv[1], mparams);
    if (!model) {
        std::fprintf(stderr, "MODEL LOAD FAILED\n");
        return 1;
    }
    const int32_t n_vocab = llama_vocab_n_tokens(llama_model_get_vocab(model));
    const int32_t n_embd  = llama_model_n_embd(model);
    std::printf("loaded n_vocab=%d n_embd=%d n_layer=%d\n", n_vocab, n_embd,
                llama_model_n_layer(model));

    auto cparams = llama_context_default_params();
    cparams.n_ctx = 64;
    cparams.n_batch = 8;
    cparams.n_ubatch = 8;
    cparams.n_threads = 4;
    cparams.n_threads_batch = 4;
    cparams.no_perf = true;
    cparams.type_k = GGML_TYPE_F32;
    cparams.type_v = GGML_TYPE_F32;
    llama_context * ctx = llama_init_from_model(model, cparams);
    if (!ctx) {
        std::fprintf(stderr, "CONTEXT FAILED\n");
        return 1;
    }

    llama_batch batch = llama_batch_init(8, 0, 1);
    const int32_t toks[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; ++i) {
        batch.token[i]    = toks[i];
        batch.pos[i]      = i;
        batch.n_seq_id[i] = 1;
        batch.seq_id[i][0]= 0;
        batch.logits[i]   = (i == 3);
    }
    batch.n_tokens = 4;

    const int rc = llama_decode(ctx, batch);
    std::printf("decode rc=%d\n", rc);
    if (rc != 0) {
        return 1;
    }

    const float * logits = llama_get_logits_ith(ctx, 3);
    if (!logits) {
        std::fprintf(stderr, "NO LOGITS\n");
        return 1;
    }
    int best = 0;
    double sum = 0.0;
    bool finite = true;
    for (int i = 0; i < n_vocab; ++i) {
        if (!std::isfinite(logits[i])) finite = false;
        if (logits[i] > logits[best]) best = i;
        sum += std::fabs(logits[i]);
    }
    std::printf("finite=%d argmax=%d l1=%.6f logits[0..3]=%.4f %.4f %.4f %.4f\n",
                finite ? 1 : 0, best, sum, logits[0], logits[1], logits[2], logits[3]);
    if (argc >= 3) {
        FILE * f = std::fopen(argv[2], "w");
        if (!f) { std::perror("logits file"); return 1; }
        for (int i = 0; i < n_vocab; ++i) std::fprintf(f, "%.9g\n", logits[i]);
        std::fclose(f);
        std::printf("wrote %s\n", argv[2]);
    }

    llama_batch_free(batch);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return finite ? 0 : 1;
}
