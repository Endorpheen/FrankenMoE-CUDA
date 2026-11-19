/* The native tokenizer, one prompt per line on stdin, ids on stdout: the reference the standalone Goose
 * tokenizer is compared against. Same flags the CLI uses by default: special tokens added, specials parsed. */
#include "llama.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    const bool add_special = !(argc > 2 && strcmp(argv[2], "--no-bos") == 0);
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("model load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);

    const bool whole = (argc > 3 && strcmp(argv[3], "--whole") == 0);
    static char all[1 << 20];
    char line[8192];
    llama_token buf[4096];
    if (whole) {
        /* the CLI reads all of stdin as one text, so the reference must see the same bytes */
        size_t len = fread(all, 1, sizeof(all) - 1, stdin);
        all[len] = 0;
        const int got = llama_tokenize(vocab, all, (int32_t) len, buf, 4096, add_special, true);
        printf("[");
        for (int i = 0; i < got; ++i) printf("%s%d", i ? ", " : "", (int) buf[i]);
        printf("]\n");
        llama_model_free(model);
        llama_backend_free();
        return 0;
    }
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) { line[--n] = 0; }
        const int got = llama_tokenize(vocab, line, (int32_t) n, buf, 4096, add_special, true);
        printf("[");
        for (int i = 0; i < got; ++i) printf("%s%d", i ? ", " : "", (int) buf[i]);
        printf("]\n");
    }
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
