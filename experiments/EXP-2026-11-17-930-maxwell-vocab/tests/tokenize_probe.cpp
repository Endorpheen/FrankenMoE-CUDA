// Ask the runtime's vocabulary what it loaded: tokenize a string and decode an id.
#include "llama.h"

#include <cstdio>
#include <cstring>

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s model.gguf text [text...]\n", argv[0]);
        return 2;
    }
    llama_backend_init();
    auto mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;
    llama_model * model = llama_model_load_from_file(argv[1], mparams);
    if (!model) {
        std::fprintf(stderr, "MODEL LOAD FAILED\n");
        return 1;
    }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    std::printf("vocab=%d pre-tokenizer loaded\n", llama_vocab_n_tokens(vocab));

    for (int i = 2; i < argc; ++i) {
        const char * text = argv[i];
        int n = -llama_tokenize(vocab, text, (int) std::strlen(text), nullptr, 0, false, true);
        if (n <= 0) {
            std::printf("TOKENIZE FAILED for '%s' (n=%d)\n", text, n);
            return 1;
        }
        int32_t * ids = new int32_t[n];
        const int got = llama_tokenize(vocab, text, (int) std::strlen(text), ids, n, false, true);
        std::printf("text='%s' n=%d ids=", text, got);
        for (int k = 0; k < got; ++k) std::printf("%d ", ids[k]);
        std::printf("\n");
        for (int k = 0; k < got; ++k) {
            char piece[64];
            const int m = llama_token_to_piece(vocab, ids[k], piece, sizeof(piece), 0, false);
            std::printf("  id=%d piece='%.*s'\n", ids[k], m > 0 ? m : 0, piece);
        }
        delete[] ids;
    }
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
