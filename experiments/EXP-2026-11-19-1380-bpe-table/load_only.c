/* Vocabulary-only load: the metadata the encoder needs, without the 20 GB of weights.
 * Every encoder check was paying three to four minutes for the weights it never uses. */
#include "llama.h"
#include <stdio.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("vocab load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    printf("vocab loaded: %d tokens\n", llama_vocab_n_tokens(vocab));
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
