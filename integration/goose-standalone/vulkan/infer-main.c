#include "qwen2.h"
#include "gguf.h"
#include "qwen2_unit.h"
#include "tokenizer.h"
#include "http.h"
#include "bonsai-model.h"
#include "apex-model.h"
#include "inkling-model.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int read_number(const char *text, uint32_t limit, uint32_t *value) {
    char *end;
    uintmax_t n = strtoumax(text, &end, 10);
    if (*text < '0' || *text > '9' || *end || n >= limit) return 0;
    *value = (uint32_t)n;
    return 1;
}

static char *read_prompt(const char *path, size_t *bytes) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return NULL;
    }
    char *text = malloc(FG_VK_TEXT_BYTES + 1);
    if (text) {
        *bytes = fread(text, 1, FG_VK_TEXT_BYTES + 1, file);
        if (ferror(file) || !*bytes || *bytes > FG_VK_TEXT_BYTES || memchr(text, 0, *bytes)) {
            free(text);
            text = NULL;
        }
    }
    fclose(file);
    return text;
}

static int detect_model_kind(const char *path, fg_vk_model_kind *kind) {
    FILE *model = fopen(path, "rb");
    if (!model) return 0;
    fg_gguf file = {0};
    int recognized = 0;
    goose_init(0, NULL);
    if (fg_gguf_read_header(&file, model) >= 0) {
        if (fg_gguf_metadata_is(&file, "general.architecture", "inkling2_loop")) {
            *kind = FG_VK_MODEL_INKLING;
            recognized = 1;
        } else if (fg_gguf_metadata_is(&file, "general.architecture", "qwen35moe")) {
            *kind = FG_VK_MODEL_APEX;
            recognized = 1;
        } else if (fg_gguf_metadata_is(&file, "general.architecture", "qwen35")) {
            *kind = FG_VK_MODEL_BONSAI;
            recognized = 1;
        } else if (fg_gguf_metadata_is(&file, "general.architecture", "qwen2")) {
            *kind = FG_VK_MODEL_QWEN2;
            recognized = 1;
        }
    }
    free(file.data.data);
    fclose(model);
    return recognized;
}

int main(int argc, char **argv) {
    uint32_t device = FG_VK_AUTO_DEVICE, predict = 1;
    uint32_t port = 0, requests = 0, server_context = 0;
    int cpu = 0, special = 0, tokenize_only = 0;
    int explicit_kind = 0;
    fg_vk_model_kind kind = FG_VK_MODEL_QWEN2;
    const char *dump = NULL, *prompt_text = NULL, *prompt_file = NULL;
    if (argc < 3) goto usage;
    int start = 2;
    for (; start < argc && strcmp(argv[start], "--"); ++start) {
        if (!strcmp(argv[start], "--allow-cpu")) cpu = 1;
        else if (!strcmp(argv[start], "--bonsai") && !explicit_kind) { kind = FG_VK_MODEL_BONSAI; explicit_kind = 1; }
        else if (!strcmp(argv[start], "--apex") && !explicit_kind) { kind = FG_VK_MODEL_APEX; explicit_kind = 1; }
        else if (!strcmp(argv[start], "--inkling") && !explicit_kind) { kind = FG_VK_MODEL_INKLING; explicit_kind = 1; }
        else if (!strcmp(argv[start], "--device-index") && start + 1 < argc) {
            if (!read_number(argv[++start], FG_VK_AUTO_DEVICE, &device)) goto usage;
        } else if (!strcmp(argv[start], "--n-predict") && start + 1 < argc) {
            if (!read_number(argv[++start], 32769, &predict) || !predict) goto usage;
        } else if (!strcmp(argv[start], "--logits") && start + 1 < argc) dump = argv[++start];
        else if (!strcmp(argv[start], "--prompt") && start + 1 < argc) prompt_text = argv[++start];
        else if (!strcmp(argv[start], "--prompt-file") && start + 1 < argc) prompt_file = argv[++start];
        else if (!strcmp(argv[start], "--special")) special = 1;
        else if (!strcmp(argv[start], "--tokenize-only")) tokenize_only = 1;
        else if (!strcmp(argv[start], "--serve") && start + 1 < argc) {
            if (!read_number(argv[++start], 65536, &port) || !port) goto usage;
        } else if (!strcmp(argv[start], "--ctx-size") && start + 1 < argc) {
            if (!read_number(argv[++start], 32769, &server_context) || !server_context) goto usage;
        } else if (!strcmp(argv[start], "--max-requests") && start + 1 < argc) {
            if (!read_number(argv[++start], UINT32_MAX, &requests) || !requests) goto usage;
        }
        else goto usage;
    }
    if (!explicit_kind && !detect_model_kind(argv[1], &kind)) {
        fputs("Unable to detect a supported Vulkan model architecture\n", stderr);
        return 1;
    }
    const int text_mode = prompt_text || prompt_file;
    if (port) {
        if (start != argc || text_mode || dump || special || tokenize_only || predict != 1 ||
            (cpu && device == FG_VK_AUTO_DEVICE)) goto usage;
        goose_init(0, NULL);
        return fg_vk_http_run(argv[1], server_context ? server_context : 512, device, cpu, port, requests, kind) ? 0 : 1;
    }
    if (requests || server_context) goto usage;
    if ((prompt_text && prompt_file) || (cpu && device == FG_VK_AUTO_DEVICE) ||
        (text_mode ? start != argc : start + 1 >= argc || special || tokenize_only) ||
        (tokenize_only && dump)) goto usage;
    const unsigned capacity = 32768 - (tokenize_only ? 0 : predict - 1);
    unsigned prompt = text_mode ? 0 : (unsigned)(argc - ++start);
    const unsigned vocabulary = kind == FG_VK_MODEL_INKLING ? FG_INKLING_VOCAB : kind == FG_VK_MODEL_APEX ? FG_APEX_VOCAB :
        kind == FG_VK_MODEL_BONSAI ? FG_BONSAI_VOCAB : FG_QWEN_VOCAB;
    if (prompt > capacity) goto usage;

    uint32_t *ids = malloc((text_mode ? capacity : prompt) * sizeof(*ids));
    if (!ids) return 1;
    fg_vk_tokenizer *tokenizer = NULL;
    int ok = 1;
    goose_init(0, NULL);
    if (text_mode) {
        size_t bytes = prompt_text ? strlen(prompt_text) : 0;
        char *owned = prompt_file ? read_prompt(prompt_file, &bytes) : NULL;
        const char *text = prompt_text ? prompt_text : owned;
        tokenizer = fg_vk_model_tokenizer_open(argv[1], kind);
        const int count = fg_vk_tokenizer_encode(tokenizer, text, bytes, special, ids, capacity);
        free(owned);
        ok = count > 0;
        if (ok) prompt = (unsigned)count;
        else fputs("Unable to tokenize the prompt within the context limit\n", stderr);
    } else {
        for (unsigned i = 0; i < prompt && ok; ++i) ok = read_number(argv[start + i], vocabulary, &ids[i]);
        if (ok && (kind == FG_VK_MODEL_APEX || kind == FG_VK_MODEL_INKLING)) {
            tokenizer = fg_vk_model_tokenizer_open(argv[1], kind);
            ok = tokenizer != NULL;
        }
    }
    if (!ok || tokenize_only) {
        for (unsigned i = 0; ok && i < prompt; ++i) printf("%u\n", ids[i]);
        fg_vk_tokenizer_close(tokenizer);
        free(ids);
        return ok ? 0 : 1;
    }

    float *logits = malloc(vocabulary * sizeof(*logits));
    fg_vk_qwen2 *model = NULL;
    fg_vk_bonsai *bonsai_model = NULL;
    fg_vk_apex *apex_model = NULL;
    fg_vk_inkling *inkling_model = NULL;
    if (logits && kind == FG_VK_MODEL_BONSAI) bonsai_model = fg_vk_bonsai_open(argv[1], prompt + predict - 1, device, cpu);
    if (logits && kind == FG_VK_MODEL_APEX) apex_model = fg_vk_apex_open(argv[1], prompt + predict - 1, device, cpu);
    if (logits && kind == FG_VK_MODEL_INKLING) inkling_model = fg_vk_inkling_open(argv[1], prompt + predict - 1, device, cpu);
    if (logits && kind == FG_VK_MODEL_QWEN2) model = calloc(1, sizeof(*model));
    ok = bonsai_model || apex_model || inkling_model || (model && fg_vk_qwen2_open(model, argv[1], prompt + predict - 1, device, cpu));
    if (ok && model) fprintf(stderr, "Vulkan Qwen2 on %s, context=%u\n", model->vk.properties.deviceName, model->context);
    for (unsigned i = 0; i < prompt && ok; ++i) {
        float *output = i + 1 == prompt ? logits : NULL;
        ok = inkling_model ? fg_vk_inkling_step(inkling_model, ids[i], output) :
             apex_model ? fg_vk_apex_step(apex_model, ids[i], output) :
             bonsai_model ? fg_vk_bonsai_step(bonsai_model, ids[i], output) : fg_vk_qwen2_step(model, ids[i], output);
    }
    if (ok && dump) {
        FILE *file = fopen(dump, "wb");
        ok = file != NULL;
        if (ok) {
            ok = fwrite(logits, sizeof(*logits), vocabulary, file) == vocabulary;
            if (fclose(file)) ok = 0;
        }
    }
#ifdef _WIN32
    if (ok && text_mode) ok = _setmode(_fileno(stdout), _O_BINARY) != -1;
#endif
    for (uint32_t t = 0; t < predict && ok; ++t) {
        uint32_t token;
        ok = fg_vk_choose(logits, vocabulary, NULL, &token);
        if (!ok) break;
        if (text_mode) {
            if (fg_vk_tokenizer_is_eog(tokenizer, token)) break;
            char piece[1024];
            const int bytes = fg_vk_tokenizer_piece(tokenizer, token, piece, sizeof(piece));
            ok = bytes >= 0 && fwrite(piece, 1, (size_t)bytes, stdout) == (size_t)bytes;
        } else {
            ok = printf("%u\n", token) > 0;
            if (bonsai_model && fg_vk_bonsai_is_eog(bonsai_model, token)) break;
            if ((apex_model || inkling_model) && fg_vk_tokenizer_is_eog(tokenizer, token)) break;
        }
        if (ok && t + 1 < predict) {
            ok = inkling_model ? fg_vk_inkling_step(inkling_model, token, logits) :
                 apex_model ? fg_vk_apex_step(apex_model, token, logits) :
                 bonsai_model ? fg_vk_bonsai_step(bonsai_model, token, logits) : fg_vk_qwen2_step(model, token, logits);
        }
    }
    if (ok && text_mode) ok = fputc('\n', stdout) != EOF;
    if (model) fg_vk_qwen2_close(model);
    fg_vk_bonsai_close(bonsai_model);
    fg_vk_apex_close(apex_model);
    fg_vk_inkling_close(inkling_model);
    fg_vk_tokenizer_close(tokenizer);
    free(model); free(logits); free(ids);
    if (!ok) fputs("Vulkan inference failed\n", stderr);
    return ok ? 0 : 1;
usage:
    fputs("Usage: gooseinfer --backend=vulkan MODEL [--device-index N] [--allow-cpu] [--n-predict N] [--logits FILE]\n"
          "       -- TOKEN ... | --prompt TEXT | --prompt-file UTF8_FILE [--special] [--tokenize-only]\n"
          "       --bonsai -- TOKEN ...\n"
          "       --apex -- TOKEN ...\n"
          "       --inkling -- TOKEN ...\n"
          "       --serve PORT [--ctx-size N] [--max-requests N]\n", stderr);
    return 2;
}
