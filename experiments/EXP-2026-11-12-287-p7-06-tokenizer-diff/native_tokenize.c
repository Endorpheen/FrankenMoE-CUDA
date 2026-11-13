// Native tokenizer driver: loads the model once and tokenizes every JSONL line of a corpus.
// Each input line is a JSON string literal; the driver prints one JSON array of token ids per line.
// add_bos = false, parse_special = true, i.e. the same contract the server's /tokenize is called with.
#include "llama.h"
#include "common.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool unescape(const std::string & in, std::string & out) {
    out.clear();
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '\\') { out.push_back(in[i]); continue; }
        if (++i >= in.size()) return false;
        switch (in[i]) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '"': out.push_back('"');  break;
            case '\\': out.push_back('\\'); break;
            case 'u': {
                if (i + 4 >= in.size()) return false;
                unsigned cp = 0;
                for (int k = 0; k < 4; ++k) { char c = in[++i]; cp <<= 4; cp |= (c <= '9' ? c - '0' : (c | 32) - 'a' + 10); }
                // UTF-8 encode
                if (cp < 0x80) out.push_back((char) cp);
                else if (cp < 0x800) { out.push_back((char) (0xC0 | (cp >> 6))); out.push_back((char) (0x80 | (cp & 0x3F))); }
                else { out.push_back((char) (0xE0 | (cp >> 12))); out.push_back((char) (0x80 | ((cp >> 6) & 0x3F))); out.push_back((char) (0x80 | (cp & 0x3F))); }
                break;
            }
            default: return false;
        }
    }
    return true;
}

int main(int argc, char ** argv) {
    if (argc != 3) { fprintf(stderr, "usage: %s model.gguf corpus.jsonl\n", argv[0]); return 2; }
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only    = true;
    mp.n_gpu_layers  = 0;
    llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) { fprintf(stderr, "load failed\n"); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    FILE * f = fopen(argv[2], "rb");
    if (!f) { fprintf(stderr, "corpus open failed\n"); return 1; }
    char * line = nullptr; size_t cap = 0; ssize_t len;
    while ((len = getline(&line, &cap, f)) != -1) {
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        std::string text;
        if (!unescape(line, text)) { printf("[]\n"); continue; }
        std::vector<llama_token> toks = common_tokenize(vocab, text, false, true);
        printf("[");
        for (size_t i = 0; i < toks.size(); ++i) printf(i ? ", %d" : "%d", toks[i]);
        printf("]\n");
    }
    free(line); fclose(f);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
