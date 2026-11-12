// Native detokenizer driver: vocab-only load, then either prints n_vocab, detokenizes JSON id arrays,
// or prints the piece for each id on its own line. Pieces are JSON-escaped the same way in every mode.
#include "llama.h"
#include "common.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

static std::string esc(const std::string & s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20 || c == 0x7f) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o.push_back((char) c);
        }
    }
    o += "\"";
    return o;
}

int main(int argc, char ** argv) {
    if (argc != 4) { fprintf(stderr, "usage: %s model.gguf mode(vocab|ids|pieces) file\n", argv[0]); return 2; }
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only   = true;
    mp.n_gpu_layers = 0;
    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { fprintf(stderr, "load failed\n"); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    const std::string mode = argv[2];

    if (mode == "vocab") {
        printf("%d\n", llama_vocab_n_tokens(vocab));
    } else {
        FILE * f = fopen(argv[3], "rb");
        if (!f) { fprintf(stderr, "input open failed\n"); return 1; }
        char * line = nullptr; size_t cap = 0; ssize_t len;
        while ((len = getline(&line, &cap, f)) != -1) {
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
            if (mode == "ids") {
                std::vector<llama_token> ids;
                const char * p = line;
                while (*p) {
                    while (*p && (*p < '0' || *p > '9') && *p != '-') ++p;
                    if (!*p) break;
                    ids.push_back((llama_token) strtol(p, nullptr, 10));
                    while (*p && ((*p >= '0' && *p <= '9') || *p == '-')) ++p;
                }
                std::string text;
                for (llama_token t : ids) text += common_token_to_piece(vocab, t);
                printf("%s\n", esc(text).c_str());
            } else if (mode == "pieces_bin") {
                const llama_token t = (llama_token) strtol(line, nullptr, 10);
                const std::string piece = common_token_to_piece(vocab, t);
                const uint32_t n = (uint32_t) piece.size();
                fwrite(&n, 4, 1, stdout);
                fwrite(piece.data(), 1, piece.size(), stdout);
            } else {
                const llama_token t = (llama_token) strtol(line, nullptr, 10);
                printf("%s\n", esc(common_token_to_piece(vocab, t)).c_str());
            }
        }
        free(line); fclose(f);
    }
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
