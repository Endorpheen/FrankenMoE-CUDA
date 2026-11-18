#include <llama.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
int main(int argc, char ** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: pieces <model.gguf> <out.bin>\n"); return 2; }
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true; mp.n_gpu_layers = 0;
    struct llama_model * m = llama_model_load_from_file(argv[1], mp);
    if (!m) { std::fprintf(stderr, "load failed\n"); return 1; }
    const struct llama_vocab * v = llama_model_get_vocab(m);
    const int32_t n = llama_vocab_n_tokens(v);
    FILE * f = std::fopen(argv[2], "wb");
    std::vector<char> buf(4096);
    int64_t total = 0;
    for (int32_t id = 0; id < n; ++id) {
        for (int special = 0; special <= 1; ++special) {
            const int32_t r = llama_token_to_piece(v, (llama_token) id, buf.data(), (int32_t) buf.size(), 0, special != 0);
            std::fwrite(&r, sizeof r, 1, f);
            if (r > 0) std::fwrite(buf.data(), 1, (size_t) r, f);
            total += r > 0 ? r : 0;
        }
    }
    std::fclose(f);
    std::printf("n_tokens=%d piece_bytes=%lld\n", n, (long long) total);
    llama_model_free(m); llama_backend_free();
    return 0;
}
