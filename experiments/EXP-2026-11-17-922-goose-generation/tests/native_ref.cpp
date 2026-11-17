// EXP922 native control: the same prompt through llama.cpp's own tokenizer,
// decoder and greedy choice, so the Goose path has an independent reference.
//
// Greedy here is the same rule the Goose sampler uses: the first candidate with
// a strictly larger logit wins.
#include <llama.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: native_ref <model.gguf> <prompt> [max_tokens]\n");
        return 2;
    }
    const int max_tokens = argc > 3 ? std::atoi(argv[3]) : 8;
    const std::string mode = argc > 4 ? argv[4] : "greedy";
    const bool dump_logits = mode == "logits" || mode == "logits-f32kv" || mode == "logits-nofa";
    const bool f32_kv = mode == "logits-f32kv";
    const bool no_fa = mode == "logits-nofa";
    llama_backend_init();

    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.load_mode = LLAMA_LOAD_MODE_MMAP;
    struct llama_model * model = llama_model_load_from_file(argv[1], mp);
    if (!model) {
        std::fprintf(stderr, "native model load failed\n");
        return 1;
    }
    const struct llama_vocab * vocab = llama_model_get_vocab(model);
    const int32_t n_vocab = llama_vocab_n_tokens(vocab);

    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx = 256;
    cp.n_batch = 256;
    cp.n_ubatch = 256;
    cp.n_threads = 8;
    cp.n_threads_batch = 16;
    cp.offload_kqv = false;
    // the accepted profile: Q4_0 KV and flash attention, so the comparison is
    // against the same arithmetic the Goose fixture runs
    cp.type_k = f32_kv ? GGML_TYPE_F32 : GGML_TYPE_Q4_0;
    cp.type_v = f32_kv ? GGML_TYPE_F32 : GGML_TYPE_Q4_0;
    cp.flash_attn_type = no_fa ? LLAMA_FLASH_ATTN_TYPE_DISABLED : LLAMA_FLASH_ATTN_TYPE_ENABLED;
    struct llama_context * ctx = llama_init_from_model(model, cp);
    if (!ctx) {
        std::fprintf(stderr, "native context failed\n");
        return 1;
    }

    std::vector<llama_token> prompt(256);
    int32_t n_prompt = llama_tokenize(vocab, argv[2], (int32_t) std::strlen(argv[2]), prompt.data(),
                                      (int32_t) prompt.size(), false, true);
    if (n_prompt < 0) {
        std::fprintf(stderr, "native tokenize failed\n");
        return 1;
    }
    prompt.resize((size_t) n_prompt);
    std::printf("PROMPT ids=%d:", n_prompt);
    for (auto id : prompt) std::printf(" %d", (int) id);
    std::printf("\n");

    // Explicit positions and per-sequence ids: this model is hybrid (GDN layers
    // carry recurrent state), and a position-less batch let the decoder re-read
    // the prompt instead of continuing it.
    llama_pos n_past = 0;
    auto decode = [&](const llama_token * tokens, int32_t n) {
        struct llama_batch batch = llama_batch_init(n, 0, 1);
        for (int32_t i = 0; i < n; ++i) {
            batch.token[i] = tokens[i];
            batch.pos[i] = n_past + i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = (i == n - 1) ? 1 : 0;
        }
        batch.n_tokens = n;
        const int rc = llama_decode(ctx, batch);
        llama_batch_free(batch);
        n_past += n;
        return rc;
    };

    if (decode(prompt.data(), n_prompt) != 0) {
        std::fprintf(stderr, "native decode failed\n");
        return 1;
    }

    if (dump_logits) {
        if (argc <= 5) {
            std::fprintf(stderr, "logits mode needs an output path\n");
            return 2;
        }
        const float * logits = llama_get_logits_ith(ctx, -1);
        if (!logits) { std::fprintf(stderr, "no logits\n"); return 1; }
        FILE * f = std::fopen(argv[5], "wb");
        if (!f || std::fwrite(logits, sizeof(float), (size_t) n_vocab, f) != (size_t) n_vocab) {
            std::fprintf(stderr, "logits write failed\n");
            return 1;
        }
        std::fclose(f);
        int32_t best = 0;
        for (int32_t i = 1; i < n_vocab; ++i)
            if (logits[i] > logits[best]) best = i;
        std::printf("LOGITS_VOCAB %d ARGMAX %d ARGMAX_VALUE %.9g\n", n_vocab, best, logits[best]);
        std::printf("LOGITS_DUMP %s\n", argv[5]);
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return 0;
    }

    std::vector<llama_token> generated;
    for (int step = 0; step < max_tokens; ++step) {
        const float * logits = llama_get_logits_ith(ctx, -1);
        if (!logits) {
            std::fprintf(stderr, "no logits\n");
            return 1;
        }
        int32_t best = 0;
        for (int32_t i = 1; i < n_vocab; ++i)
            if (logits[i] > logits[best]) best = i;
        generated.push_back((llama_token) best);
        std::printf("TOKEN %d id=%d\n", step, (int) best);
        if (llama_vocab_is_eog(vocab, (llama_token) best)) {
            std::printf("STOP eog id=%d at step %d\n", (int) best, step);
            break;
        }
        llama_token next = (llama_token) best;
        if (decode(&next, 1) != 0) {
            std::fprintf(stderr, "native decode failed at step %d\n", step);
            return 1;
        }
    }

    std::string text;
    for (auto id : generated) {
        char buf[4096];
        const int32_t n = llama_token_to_piece(vocab, id, buf, (int32_t) sizeof buf, 0, true);
        std::printf("PIECE id=%d n=%d text=%s\n", (int) id, n, n > 0 ? std::string(buf, (size_t) n).c_str() : "");
        if (n > 0) text.append(buf, (size_t) n);
    }
    std::printf("GENERATED ids=%zu:", generated.size());
    for (auto id : generated) std::printf(" %d", (int) id);
    std::printf("\nTEXTLEN %zu\nTEXT_BEGIN\n%s\nTEXT_END\n", text.size(), text.c_str());

    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
