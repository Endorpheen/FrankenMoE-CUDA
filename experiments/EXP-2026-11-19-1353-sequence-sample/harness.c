/* Makes the sequence detokeniser a sample rather than a case: one model load, then llama_detokenize over
 * many sequences and every flag combination the API exposes. The comparison itself happens inside the
 * library (FG_TK_DETOK_DIAG at llama_detokenize); this harness only drives the calls. */
#include "llama.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;      /* the vocabulary does not need the GPU */
    /* mmap is the default load mode */
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("model load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    const int32_t n_vocab = llama_vocab_n_tokens(vocab);

    const int lengths[] = { 1, 2, 5, 17, 64, 257, 1025 };
    const int n_lengths = (int) (sizeof(lengths) / sizeof(lengths[0]));
    static char buf[1 << 20];

    int calls = 0;
    for (int li = 0; li < n_lengths; ++li) {
        const int n = lengths[li];
        llama_token * ids = malloc(sizeof(llama_token) * (size_t) n);
        for (int shape = 0; shape < 3; ++shape) {
            for (int i = 0; i < n; ++i) {
                switch (shape) {
                    case 0: ids[i] = (llama_token) (i % 1000); break;                     /* low ids, some special */
                    case 1: ids[i] = (llama_token) (n_vocab - 1 - (i % 50)); break;        /* tail of the vocab */
                    default: ids[i] = (llama_token) ((i * 7919) % n_vocab); break;         /* spread */
                }
            }
            for (int rs = 0; rs < 2; ++rs) {
                for (int us = 0; us < 2; ++us) {
                    const int32_t got = llama_detokenize(vocab, ids, n, buf, sizeof(buf), rs, us);
                    ++calls;
                    if (got < 0 && got != -1) { printf("unexpected negative %d\n", got); }
                }
            }
        }
        free(ids);
    }
    printf("sequence harness: calls=%d\n", calls);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
