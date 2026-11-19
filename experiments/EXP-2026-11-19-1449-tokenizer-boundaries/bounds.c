// The tokenizer's contracts at the edges: a buffer too small, an empty sequence, an out-of-range token id, and the
// difference parse_special makes. These are the answers a caller relies on when something is wrong, and none of them was
// covered by the equality tests, which only ever asked about well-formed input.
#include "llama.h"
#include <stdio.h>
#include <string.h>

#define GUARD 32

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0; mp.vocab_only = true;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);
    int fails = 0;

    // 1) a buffer too small must come back as -needed, and nothing may be written past it
    {
        const char * text = "a sentence long enough that one token cannot hold it";
        llama_token small[GUARD + 1 + GUARD];
        memset(small, 0xA5, sizeof(small));
        const int got = llama_tokenize(vocab, text, (int32_t) strlen(text), small + GUARD, 1, false, true);
        int smashed = 0;
        for (int i = 0; i < GUARD; ++i) { if (small[i] != (llama_token) 0xA5A5A5A5) { smashed = 1; break; } }
        printf("tokenize short buffer: got=%d smashed=%d (expected got < 0)\n", got, smashed);
        if (got >= 0 || smashed) { ++fails; }
    }

    // 2) an empty sequence detokenises to nothing and writes nothing
    {
        char buf[GUARD + 16 + GUARD];
        memset(buf, 0xA5, sizeof(buf));
        const int got = llama_detokenize(vocab, NULL, 0, buf + GUARD, 16, false, true);
        int smashed = 0;
        for (int i = 0; i < GUARD; ++i) { if ((unsigned char) buf[i] != 0xA5) { smashed = 1; break; } }
        printf("detokenize empty: got=%d smashed=%d (expected got = 0)\n", got, smashed);
        if (got != 0 || smashed) { ++fails; }
    }

    // 3) an id outside the vocabulary must be refused, not indexed
    {
        char piece[64];
        const int below = llama_token_to_piece(vocab, -1, piece, 64, 0, true);
        const int above = llama_token_to_piece(vocab, n + 1000, piece, 64, 0, true);
        // Before this guard both calls aborted the process with std::out_of_range from token_get_attr, which is a
        // server-ending answer for one bad id. The contract now is an empty piece and a line in the log.
        printf("token_to_piece out of range: -1 -> %d, n+1000 -> %d (expected both 0, no abort)\n", below, above);
        if (below != 0 || above != 0) { ++fails; }
    }

    // 4) parse_special must change the answer, or the flag means nothing
    {
        const char * text = "before <|im_end|> after";
        llama_token ids_plain[64], ids_special[64];
        const int plain = llama_tokenize(vocab, text, (int32_t) strlen(text), ids_plain, 64, false, false);
        const int spec  = llama_tokenize(vocab, text, (int32_t) strlen(text), ids_special, 64, false, true);
        printf("parse_special: off=%d ids, on=%d ids (expected on < off)\n", plain, spec);
        if (!(spec < plain)) { ++fails; }
    }

    // 5) an out-of-range id inside a sequence must not end the process either
    {
        static llama_token seq[3] = { 100, -7, 250 };
        char buf[256];
        const int got = llama_detokenize(vocab, seq, 3, buf, (int32_t) sizeof(buf), false, true);
        printf("detokenize with a bad id inside: got=%d (process alive)\n", got);
        if (got < 0) { ++fails; }
    }

    printf("tokenizer boundaries: failures=%d\n", fails);
    llama_model_free(model);
    llama_backend_free();
    return fails == 0 ? 0 : 1;
}
