// A streamed answer is a sequence of pieces; the same tokens detokenised whole must give the same bytes.
//
// This is the property a client sees: the server sends piece by piece and the concatenation is the answer. The crash of
// 2026-11-19 lived in exactly this path - the piece form's buffer - so the relation between the two forms is measured here
// rather than assumed.
#include "llama.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static uint64_t g_state = 0x2545F4914F6CDD1Dull;
static uint32_t rnd() {
    g_state ^= g_state << 13; g_state ^= g_state >> 7; g_state ^= g_state << 17;
    return (uint32_t) (g_state >> 32);
}

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("load failed\n"); return 2; }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);

    // 1) fixed texts: encode then decode must give the text back
    static const char * texts[] = {
        "Hello, world!",
        "The quick brown fox jumps over the lazy dog.",
        "  leading and trailing spaces  ",
        "line one\nline two\n\nline three\n",
        "def f(x):\n    return x * 2\n",
        "{\"a\": 1, \"b\": [true, false, null]}",
        "Ünïcödé and 日本語 and emoji 🙂",
        "\tTabbed\tcolumns\tand | pipes |",
        "SELECT * FROM t WHERE x >= 10 AND y <> 2;",
        "",
    };
    long text_cases = 0, text_bad = 0;
    std::vector<llama_token> ids(4096);
    std::vector<char> buf(1 << 16);
    for (const char * t : texts) {
        const int len = (int) strlen(t);
        const int got = llama_tokenize(vocab, t, len, ids.data(), (int32_t) ids.size(), false, true);
        if (got < 0) { ids.resize((size_t) -got); llama_tokenize(vocab, t, len, ids.data(), (int32_t) ids.size(), false, true); }
        const int n_ids = got < 0 ? -got : got;
        const int back = llama_detokenize(vocab, ids.data(), n_ids, buf.data(), (int32_t) buf.size(), false, true);
        ++text_cases;
        if (back != len || (len > 0 && memcmp(buf.data(), t, (size_t) len) != 0)) {
            ++text_bad;
            if (text_bad <= 5) printf("  TEXT DIFFERS len=%d back=%d: %.40s\n", len, back, t);
        }
    }

    // 2) random token sequences: the pieces joined must equal the whole-sequence answer
    long seq_cases = 0, seq_bad = 0;
    std::vector<llama_token> seq(4096);
    std::string joined;
    for (int trial = 0; trial < 400; ++trial) {
        const size_t len = 1 + rnd() % 200;
        seq.resize(len);
        for (size_t i = 0; i < len; ++i) { seq[i] = (llama_token) (rnd() % (uint32_t) n); }
        joined.clear();
        for (size_t i = 0; i < len; ++i) {
            char piece[512];
            // special=true renders a special token as its text, which is what unparse_special=true does in the
            // sequence form. With special=false the piece form returns nothing for those tokens and the two answers
            // differ by design - measured as seven of four hundred before this was understood.
            const int got = llama_token_to_piece(vocab, seq[i], piece, (int32_t) sizeof(piece), 0, true);
            if (got < 0) break;
            joined.append(piece, (size_t) got);
        }
        std::vector<char> whole(1 << 18);
        int want = llama_detokenize(vocab, seq.data(), (int) len, whole.data(), (int32_t) whole.size(), false, true);
        if (want < 0) { whole.resize((size_t) -want); want = llama_detokenize(vocab, seq.data(), (int) len,
                                                                            whole.data(), (int32_t) whole.size(), false, true); }
        ++seq_cases;
        if (want != (int) joined.size() || (want > 0 && memcmp(whole.data(), joined.data(), (size_t) want) != 0)) {
            ++seq_bad;
            if (seq_bad <= 3) printf("  SEQUENCE DIFFERS tokens=%zu whole=%d joined=%zu\n", len, want, joined.size());
        }
    }

    printf("piece/sequence: text_cases=%ld text_diffs=%ld sequence_cases=%ld sequence_diffs=%ld\n",
           text_cases, text_bad, seq_cases, seq_bad);
    llama_model_free(model);
    llama_backend_free();
    return (text_bad == 0 && seq_bad == 0) ? 0 : 1;
}
