// Prints the ids llama_tokenize returns for each line of stdin, on the shipped path - the library the server calls.
#include "llama.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { fprintf(stderr, "load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);

    char * line = NULL;
    size_t cap = 0;
    ssize_t len;
    static llama_token ids[262144];
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') { line[--len] = '\0'; }
        const int32_t n = llama_tokenize(vocab, line, (int32_t) len, ids, 262144, true, true);
        printf("%d:", n);
        for (int32_t i = 0; i < n; ++i) { printf(" %d", (int) ids[i]); }
        printf("\n");
    }
    free(line);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
