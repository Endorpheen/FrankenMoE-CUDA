// Regression for the piece-buffer overlap (EXP-2026-11-19-1439).
//
// The defect: common_token_to_piece asked the producer for a 15-byte window (the string's internal cache) and wrote the
// answer into it. A piece of 21 bytes ran past that window and landed on the neighbouring field of the caller's struct,
// which in the server was the token id itself - tok became -779955792, the bytes b0 d1 82 d1, i.e. text.
//
// The test reproduces the caller's layout, sets the token id in a struct, calls the function and checks that the id is
// still the id and that the piece is exactly what a full-size buffer produces. A content-only comparison would not catch
// this: the returned string was correct, the damage was to the caller's memory.
#include "llama.h"
#include "common.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct caller_layout {
    llama_token tok;
    float       prob;
    std::string text;
};

// The implementation as it was before the fix, kept so the test can show it detects the defect it exists for. A check
// that has never been seen to fail proves nothing.
static std::string old_piece(const llama_vocab * vocab, llama_token token) {
    std::string piece;
    piece.resize(piece.capacity());
    const int n = llama_token_to_piece(vocab, token, &piece[0], (int32_t) piece.size(), 0, true);
    if (n < 0) {
        piece.resize((size_t) -n);
        llama_token_to_piece(vocab, token, &piece[0], (int32_t) piece.size(), 0, true);
    } else {
        piece.resize((size_t) n);
    }
    return piece;
}

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("vocab load failed\n"); return 2; }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);

    std::vector<char> buf(512);
    long checked = 0, long_pieces = 0, bad_id = 0, bad_text = 0;
    int first_long = -1;
    for (int id = 0; id < n; ++id) {
        const int ref = llama_token_to_piece(vocab, id, buf.data(), (int32_t) buf.size(), 0, true);
        if (ref <= 0) { continue; }

        caller_layout c;
        c.tok  = id;
        c.prob = 0.5f;
        c.text = common_token_to_piece(vocab, id, true);

        ++checked;
        if (ref > 15) { ++long_pieces; if (first_long < 0) { first_long = id; } }

        if (c.tok != id) {
            ++bad_id;
            if (bad_id <= 5) {
                printf("  ID OVERWRITTEN token=%d tok=%d piece_len=%zu\n", id, (int) c.tok, c.text.size());
            }
        }
        if ((int) c.text.size() != ref || std::memcmp(c.text.data(), buf.data(), (size_t) ref) != 0) {
            ++bad_text;
            if (bad_text <= 5) { printf("  TEXT DIFFERS token=%d ref=%d got=%zu\n", id, ref, c.text.size()); }
        }
    }

    // negative control: the old implementation must be seen damaging the caller, otherwise this test cannot detect
    int control_hits = 0;
    for (int id = 0; id < n && control_hits == 0; ++id) {
        const int ref = llama_token_to_piece(vocab, id, buf.data(), (int32_t) buf.size(), 0, true);
        if (ref <= 15) { continue; }
        caller_layout c;
        c.tok  = id;
        c.prob = 0.5f;
        c.text = old_piece(vocab, id);
        if (c.tok != id) { ++control_hits; printf("  control: old implementation overwrote token %d -> %d\n",
                                                 id, (int) c.tok); }
    }
    printf("control: old implementation damaged the caller in %d case(s)\n", control_hits);

    printf("piece buffer: checked=%ld long_pieces=%ld first_long=%d id_overwrites=%ld text_diffs=%ld\n",
           checked, long_pieces, first_long, bad_id, bad_text);
    llama_model_free(model);
    llama_backend_free();
    return (bad_id == 0 && bad_text == 0) ? 0 : 1;
}
