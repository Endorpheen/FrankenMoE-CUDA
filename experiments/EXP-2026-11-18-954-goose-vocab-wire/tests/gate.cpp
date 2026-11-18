// EXP954: after the loader registers the file with the Goose reader, the
// vocabulary must still be built correctly. The gate loads a tiny synthetic
// vocabulary in vocab_only mode and checks the resulting vocab object against
// what the file says, then tokenises and detokenises a short string.
//
// vocab_only: no weights are read, no graph is built, no GPU is touched. The
// file is 20 KB of metadata.
#include <llama.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" int llama_goose_queries;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char * what) {
    checks++;
    if (!cond) { failures++; std::printf("FAIL %s\n", what); }
}

int main(int argc, char ** argv) {
    if (argc < 2) { std::printf("usage: gate <vocab.gguf>\n"); return 2; }

    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true;
    mp.n_gpu_layers = 0;

    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { std::printf("FAIL load\n"); return 1; }

    // The adapter must have answered while the loader ran. This is the positive
    // witness: a linked-but-unused path would leave this at zero.
    std::printf("goose queries during load=%d\n", llama_goose_queries);
    check(llama_goose_queries > 0, "Goose answered the vocabulary questions");

    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);
    std::printf("n_tokens=%d\n", n);
    check(n == 260, "token count matches the fixture");

    // The fixture's ids are the 256 byte tokens in byte order, then ab, abc,
    // <|end|>, and one filler. Check the texts survived the Goose path.
    for (int id = 0; id < n && id < 8; id++) {
        char buf[64];
        const int got = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, true);
        std::printf("id %d -> %d bytes\n", id, got);
        check(got > 0, "piece is not empty");
    }

    // Tokenise and detokenise a word that needs a merge.
    const char * text = "abc";
    std::vector<llama_token> ids(16);
    const int n_ids = llama_tokenize(vocab, text, (int) std::strlen(text), ids.data(), (int) ids.size(), false, true);
    std::printf("tokenize(\"%s\") -> %d ids\n", text, n_ids);
    check(n_ids == 1 && ids[0] == 257, "abc is the merged token");

    char out[64];
    const int len = llama_token_to_piece(vocab, ids[0], out, sizeof(out), 0, true);
    check(len == 3 && std::memcmp(out, "abc", 3) == 0, "abc round-trips");

    llama_model_free(model);
    llama_backend_free();
    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
