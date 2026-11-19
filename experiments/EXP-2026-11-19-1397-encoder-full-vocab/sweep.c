/* Every token's own text through llama_tokenize, with the ported encoder serving: the comparison inside the
 * library counts disagreements, so this is a full-vocabulary sweep of the encode path at the cost of one
 * vocabulary load instead of one model load. */
#include "llama.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0; mp.vocab_only = true;
    struct llama_model * m = llama_model_load_from_file(path, mp);
    if (!m) { printf("load failed\n"); return 2; }
    const struct llama_vocab * v = llama_model_get_vocab(m);
    const int n = llama_vocab_n_tokens(v);
    static llama_token buf[4096];
    int calls = 0;
    for (int id = 0; id < n; ++id) {
        const char * t = llama_token_get_text(v, id);
        if (!t || !*t) { continue; }
        const int len = (int) strlen(t);
        if (len <= 0 || len >= 4000) { continue; }
        const int k = llama_tokenize(v, t, len, buf, 4096, false, true);
        if (k == 0) {
            printf("native returns 0 for id %d, text len %d: [%.40s]\n", id, len, t);
        }
        ++calls;
    }
    printf("sweep: %d token texts tokenised of %d\n", calls, n);
    llama_model_free(m); llama_backend_free();
    return 0;
}
