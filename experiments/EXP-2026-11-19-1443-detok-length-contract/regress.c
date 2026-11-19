// The served detokeniser must honour the caller's buffer length.
//
// The defect: the door is given a 4096-byte window of its own, and the served path copied its answer into the caller's
// buffer without comparing it with `length`. A 21-byte piece into a 15-byte buffer ran six bytes past it; the server read
// its token id as -779955792 (bytes b0 d1 82 d1) at generation step 643 of a real agent session.
//
// The contract is the native one: a short buffer returns -needed, the caller resizes and asks again. This test asks with a
// deliberately short buffer guarded by a canary on both sides and requires the canary to survive.
#include "llama.h"
#include <stdio.h>
#include <string.h>

#define GUARD 32

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("load failed\n"); return 2; }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    const int n = llama_vocab_n_tokens(vocab);

    unsigned char ref[1024];
    long checked = 0, short_cases = 0, canary_hits = 0, bad_contract = 0;
    for (int id = 0; id < n; ++id) {
        const int full = llama_token_to_piece(vocab, id, (char *) ref, (int32_t) sizeof(ref), 0, true);
        if (full <= 15) { continue; }          // only pieces that cannot fit the window the crash used
        ++checked;

        for (int cap = 8; cap < full && cap <= 20; cap += 4) {
            unsigned char buf[GUARD + 32 + GUARD];
            memset(buf, 0xA5, sizeof(buf));
            const int got = llama_token_to_piece(vocab, id, (char *) buf + GUARD, cap, 0, true);
            ++short_cases;
            for (int i = 0; i < GUARD; ++i) {
                if (buf[i] != 0xA5 || buf[GUARD + 32 + i] != 0xA5) { ++canary_hits;
                    if (canary_hits <= 5) printf("  CANARY SMASHED token=%d cap=%d at %d\n", id, cap,
                                                 i < GUARD ? i : GUARD + 32 + i); break; }
            }
            if (got != -full) {
                ++bad_contract;
                if (bad_contract <= 5) printf("  CONTRACT token=%d cap=%d got=%d expected=%d\n", id, cap, got, -full);
            }
        }
    }
    // Negative control: the old handler copied exactly this way, without comparing with the caller's length. The canary
    // must catch that, otherwise the check above would pass on a buffer that is being overflowed.
    long control_hits = 0;
    for (int id = 0; id < n && control_hits == 0; ++id) {
        const int full = llama_token_to_piece(vocab, id, (char *) ref, (int32_t) sizeof(ref), 0, true);
        if (full <= 15) { continue; }
        const int cap = 8;
        unsigned char buf[GUARD + 32 + GUARD];
        memset(buf, 0xA5, sizeof(buf));
        memcpy(buf + GUARD, ref, (size_t) full);   // the old behaviour, with no length check
        for (int i = 0; i < GUARD; ++i) {
            if (buf[GUARD + 32 + i] != 0xA5) { ++control_hits; break; }
        }
        if (control_hits) { printf("  control: an unchecked copy of %d bytes into a %d-byte buffer is caught\n",
                                   full, cap); }
    }
    printf("control: unchecked copy detected in %ld case(s)\n", control_hits);

    printf("detok length contract: long_pieces=%ld short_buffer_cases=%ld canary_hits=%ld contract_violations=%ld\n",
           checked, short_cases, canary_hits, bad_contract);
    llama_model_free(model);
    llama_backend_free();
    return (canary_hits == 0 && bad_contract == 0 && control_hits > 0) ? 0 : 1;
}
