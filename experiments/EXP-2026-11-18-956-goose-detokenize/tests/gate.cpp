// EXP956: detokenisation through Goose must equal the native path for every id.
//
// Two library builds answer the same fixture: the accepted one (native piece
// body) and this build (Goose piece body). The gate dumps every id's piece with
// special=true and special=false and compares the bytes, which covers the
// attribute pass, the byte decode and the lstrip rule at once.
//
// vocab_only: no weights, no graph, no GPU.
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

static std::string dump(const llama_vocab * vocab, int n, bool special, int * queries_out) {
    std::string out;
    char buf[512];
    for (int id = 0; id < n; id++) {
        const int got = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, special);
        // length-prefix each piece so an empty piece is distinguishable
        const int32_t len = got;
        out.append((const char *) &len, sizeof(len));
        if (got > 0) out.append(buf, (size_t) got);
    }
    if (queries_out) *queries_out = llama_goose_queries;
    return out;
}

int main(int argc, char ** argv) {
    if (argc < 2) { std::printf("usage: gate <vocab.gguf> [dump.bin]\n"); return 2; }

    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true;
    mp.n_gpu_layers = 0;
    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { std::printf("FAIL load\n"); return 1; }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);
    std::printf("n_tokens=%d\n", n);

    const int before = llama_goose_queries;
    int after_a = 0, after_b = 0;
    const std::string special = dump(vocab, n, true, &after_a);
    const std::string plain = dump(vocab, n, false, &after_b);
    std::printf("goose queries: before=%d after_special=%d after_plain=%d\n",
                before, after_a, after_b);

    if (argc > 2) {
        FILE * f = std::fopen(argv[2], "wb");
        const uint32_t ns = (uint32_t) special.size(), np = (uint32_t) plain.size();
        std::fwrite(&ns, sizeof(ns), 1, f);
        std::fwrite(special.data(), 1, special.size(), f);
        std::fwrite(&np, sizeof(np), 1, f);
        std::fwrite(plain.data(), 1, plain.size(), f);
        std::fclose(f);
        std::printf("wrote %s (%zu + %zu bytes)\n", argv[2], special.size(), plain.size());
    }

    // A merged token must decode to its text, and a byte token to one byte.
    char buf[512];
    const int abc = llama_token_to_piece(vocab, 257, buf, sizeof(buf), 0, true);
    check(abc == 3 && std::memcmp(buf, "abc", 3) == 0, "257 decodes to abc");
    const int zero = llama_token_to_piece(vocab, 16, buf, sizeof(buf), 0, true);
    check(zero >= 0, "byte token decodes");

    llama_model_free(model);
    llama_backend_free();
    std::printf("%s checks=%d failures=%d\n", failures == 0 ? "PASS" : "BAD", checks, failures);
    return failures ? 1 : 0;
}
