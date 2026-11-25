#include "llama.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 5120, VOCAB = 248320, MAX_ROWS = 16 };

struct trace {
    float rows[3][WIDTH];
    unsigned seen;
};

static bool capture(struct ggml_tensor *tensor, bool ask, void *data) {
    const char *names[] = {"mtp_eh_proj-64", "mtp_attn_residual-64", "mtp_shared_head_norm"};
    struct trace *trace = data;
    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        const size_t len = strlen(names[i]);
        if (strncmp(tensor->name, names[i], len) != 0 ||
            (tensor->name[len] != '\0' && tensor->name[len] != '-')) continue;
        if (!ask && tensor->type == GGML_TYPE_F32 && ggml_is_contiguous(tensor) &&
            ggml_nelements(tensor) == WIDTH) {
            ggml_backend_tensor_get(tensor, trace->rows[i], 0, sizeof(trace->rows[i]));
            trace->seen |= 1U << i;
        }
        return true;
    }
    return !ask;
}

int main(int argc, char **argv) {
    if (argc < 6 || argc > 4 + MAX_ROWS) return 2;
    const unsigned count = (unsigned)argc - 4;
    float *hidden = malloc(count * WIDTH * sizeof(*hidden));
    FILE *input = fopen(argv[2], "rb"), *output = NULL;
    struct llama_model *model = NULL;
    struct llama_context *context = NULL;
    struct llama_batch_ext *batch = NULL;
    int status = 1;
    llama_backend_init();
    if (!hidden || !input || fread(hidden, WIDTH * sizeof(*hidden), count, input) != count ||
        fgetc(input) != EOF) goto done;
    ggml_backend_dev_t gpu = ggml_backend_dev_by_name("CUDA0");
    if (!gpu) goto done;
    struct llama_model_tensor_buft_override overrides[] = {
        { "^token_embd\\.weight$", ggml_backend_dev_buffer_type(gpu) }, { NULL, NULL },
    };
    struct llama_model_params load = llama_model_default_params();
    load.n_gpu_layers = 99;
    load.load_mtp = true;
    load.tensor_buft_overrides = overrides;
    model = llama_model_load_from_file(argv[1], load);
    if (!model || llama_vocab_n_tokens(llama_model_get_vocab(model)) != VOCAB) goto done;

    struct trace trace = {0};
    struct llama_context_params params = llama_context_default_params();
    params.ctx_type = LLAMA_CONTEXT_TYPE_MTP;
    params.n_ctx = 1024;
    params.n_batch = params.n_ubatch = 128;
    params.n_threads = 8;
    params.n_threads_batch = 16;
    params.type_k = params.type_v = GGML_TYPE_Q4_0;
    params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
    params.cb_eval = capture;
    params.cb_eval_user_data = &trace;
    context = llama_init_from_model(model, params);
    if (!context || !(batch = llama_batch_ext_init(context)) || !(output = fopen(argv[3], "wb"))) goto done;

    for (unsigned row = 0; row < count; ++row) {
        char *end;
        long token = strtol(argv[4 + row], &end, 10);
        if (end == argv[4 + row] || *end || token < 0 || token >= VOCAB) goto done;
        llama_batch_ext_clear(batch);
        trace.seen = 0;
        const int idx = llama_batch_ext_add_token(batch, 0, (llama_token)token);
        const llama_pos pos = (llama_pos)row;
        const struct llama_embd state = {hidden + row * WIDTH, 1, WIDTH};
        if (idx != 0 || !llama_batch_ext_set_pos(batch, idx, &pos) ||
            !llama_batch_ext_set_embd_state(batch, idx, state) ||
            !llama_batch_ext_set_output_logits(batch, idx, true) ||
            llama_process(context, LLAMA_PROCESS_TYPE_DECODE, batch) != 0) goto done;
        const float *logits = llama_get_logits_ith(context, 0);
        if (!logits || trace.seen != 7) {
            fprintf(stderr, "Row %u: captured tensor mask %u\n", row, trace.seen);
            goto done;
        }
        unsigned best = 0;
        for (unsigned i = 0; i < VOCAB; ++i) {
            if (!isfinite(logits[i])) goto done;
            if (logits[i] > logits[best]) best = i;
        }
        printf("{\"position\":%u,\"input\":%ld,\"draft\":%u,\"logit\":%.9g}\n",
               row, token, best, logits[best]);
        if (fwrite(logits, sizeof(*logits), VOCAB, output) != VOCAB ||
            fwrite(trace.rows, sizeof(trace.rows), 1, output) != 1) goto done;
    }
    status = 0;
done:
    if (output && fclose(output) != 0) status = 1;
    if (input) fclose(input);
    if (batch) llama_batch_ext_free(batch);
    if (context) llama_free(context);
    if (model) llama_model_free(model);
    llama_backend_free();
    free(hidden);
    return status;
}
