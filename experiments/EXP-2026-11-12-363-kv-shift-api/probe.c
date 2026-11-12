#include "llama.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char ** argv) {
    if (argc < 2) { printf("usage: probe model.gguf\n"); return 2; }
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    struct llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { printf("load failed\n"); return 3; }
    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 64; cp.n_batch = 64; cp.n_threads = 4; cp.n_threads_batch = 4;
    struct llama_context * ctx = llama_init_from_model(model, cp);
    if (!ctx) { printf("ctx failed\n"); return 4; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    llama_token toks[64]; int n = llama_tokenize(vocab, "The quick brown fox", 19, toks, 64, true, false);
    if (n <= 0) { printf("tokenize failed\n"); return 5; }
    struct llama_batch b = llama_batch_init(64, 0, 1);
    for (int i = 0; i < n; ++i) { b.token[i] = toks[i]; b.pos[i] = i; b.n_seq_id[i] = 1; b.seq_id[i][0] = 0; b.logits[i] = 0; }
    b.n_tokens = n;
    if (llama_decode(ctx, b)) { printf("decode1 failed\n"); return 6; }
    // shift everything back by 4 positions -> forces the k-shift path on the next graph build
    llama_memory_seq_add(llama_get_memory(ctx), 0, 0, -1, -4);
    struct llama_batch b2 = llama_batch_init(1, 0, 1);
    b2.token[0] = toks[0]; b2.pos[0] = n - 4; b2.n_seq_id[0] = 1; b2.seq_id[0][0] = 0; b2.logits[0] = 1; b2.n_tokens = 1;
    if (llama_decode(ctx, b2)) { printf("decode2 failed\n"); return 7; }
    const float * lg = llama_get_logits_ith(ctx, 0);
    int nv = llama_vocab_n_tokens(vocab);
    unsigned long long h = 1469598103934665603ull;
    for (int i = 0; i < nv; ++i) { unsigned int u; memcpy(&u, &lg[i], 4); h = (h ^ u) * 1099511628211ull; }
    printf("shift_logits_hash=%016llx count=%d\n", h, nv);
    llama_batch_free(b); llama_batch_free(b2); llama_free(ctx); llama_model_free(model); llama_backend_free();
    return 0;
}
