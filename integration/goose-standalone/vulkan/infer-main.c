#include "qwen2.h"
#include "qwen2_unit.h"
#include "tokenizer.h"

#include <inttypes.h>
#include <math.h>
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

static int choose(const float *logits, uint32_t *chosen) {
    uint32_t best = 0;
    for (uint32_t i = 0; i < FG_QWEN_VOCAB; ++i) {
        if (!isfinite(logits[i])) return 0;
        if (logits[i] > logits[best]) best = i;
    }
    *chosen = best;
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

int main(int argc, char **argv) {
    uint32_t device = FG_VK_AUTO_DEVICE, predict = 1;
    int cpu = 0, special = 0, tokenize_only = 0;
    const char *dump = NULL, *prompt_text = NULL, *prompt_file = NULL;
    if (argc < 3) goto usage;
    int start = 2;
    for (; start < argc && strcmp(argv[start], "--"); ++start) {
        if (!strcmp(argv[start], "--allow-cpu")) cpu = 1;
        else if (!strcmp(argv[start], "--device-index") && start + 1 < argc) {
            if (!read_number(argv[++start], FG_VK_AUTO_DEVICE, &device)) goto usage;
        } else if (!strcmp(argv[start], "--n-predict") && start + 1 < argc) {
            if (!read_number(argv[++start], 32769, &predict) || !predict) goto usage;
        } else if (!strcmp(argv[start], "--logits") && start + 1 < argc) dump = argv[++start];
        else if (!strcmp(argv[start], "--prompt") && start + 1 < argc) prompt_text = argv[++start];
        else if (!strcmp(argv[start], "--prompt-file") && start + 1 < argc) prompt_file = argv[++start];
        else if (!strcmp(argv[start], "--special")) special = 1;
        else if (!strcmp(argv[start], "--tokenize-only")) tokenize_only = 1;
        else goto usage;
    }
    const int text_mode = prompt_text || prompt_file;
    if ((prompt_text && prompt_file) || (cpu && device == FG_VK_AUTO_DEVICE) ||
        (text_mode ? start != argc : start + 1 >= argc || special || tokenize_only) ||
        (tokenize_only && dump)) goto usage;
    const unsigned capacity = 32768 - (tokenize_only ? 0 : predict - 1);
    unsigned prompt = text_mode ? 0 : (unsigned)(argc - ++start);
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
        tokenizer = fg_vk_qwen2_tokenizer_open(argv[1]);
        const int count = fg_vk_tokenizer_encode(tokenizer, text, bytes, special, ids, capacity);
        free(owned);
        ok = count > 0;
        if (ok) prompt = (unsigned)count;
        else fputs("Unable to tokenize the Qwen2 prompt within the context limit\n", stderr);
    } else {
        for (unsigned i = 0; i < prompt && ok; ++i) ok = read_number(argv[start + i], FG_QWEN_VOCAB, &ids[i]);
    }
    if (!ok || tokenize_only) {
        for (unsigned i = 0; ok && i < prompt; ++i) printf("%u\n", ids[i]);
        fg_vk_tokenizer_close(tokenizer);
        free(ids);
        return ok ? 0 : 1;
    }

    float *logits = malloc(FG_QWEN_VOCAB * sizeof(*logits));
    fg_vk_qwen2 *model = calloc(1, sizeof(*model));
    ok = logits && model && fg_vk_qwen2_open(model, argv[1], prompt + predict - 1, device, cpu);
    if (ok) fprintf(stderr, "Vulkan Qwen2 on %s, context=%u\n", model->vk.properties.deviceName, model->context);
    for (unsigned i = 0; i < prompt && ok; ++i) ok = fg_vk_qwen2_step(model, ids[i], i + 1 == prompt ? logits : NULL);
    if (ok && dump) {
        FILE *file = fopen(dump, "wb");
        ok = file != NULL;
        if (ok) {
            ok = fwrite(logits, sizeof(*logits), FG_QWEN_VOCAB, file) == FG_QWEN_VOCAB;
            if (fclose(file)) ok = 0;
        }
    }
#ifdef _WIN32
    if (ok && text_mode) ok = _setmode(_fileno(stdout), _O_BINARY) != -1;
#endif
    for (uint32_t t = 0; t < predict && ok; ++t) {
        uint32_t token;
        ok = choose(logits, &token);
        if (!ok) break;
        if (text_mode) {
            if (fg_vk_tokenizer_is_eog(tokenizer, token)) break;
            char piece[1024];
            const int bytes = fg_vk_tokenizer_piece(tokenizer, token, piece, sizeof(piece));
            ok = bytes >= 0 && fwrite(piece, 1, (size_t)bytes, stdout) == (size_t)bytes;
        } else {
            ok = printf("%u\n", token) > 0;
        }
        if (ok && t + 1 < predict) ok = fg_vk_qwen2_step(model, token, logits);
    }
    if (ok && text_mode) ok = fputc('\n', stdout) != EOF;
    if (model) fg_vk_qwen2_close(model);
    fg_vk_tokenizer_close(tokenizer);
    free(model); free(logits); free(ids);
    if (!ok) fputs("Vulkan Qwen2 inference failed\n", stderr);
    return ok ? 0 : 1;
usage:
    fputs("Usage: frankenmoe-vulkan MODEL [--device-index N] [--allow-cpu] [--n-predict N] [--logits FILE]\n"
          "       -- TOKEN ... | --prompt TEXT | --prompt-file UTF8_FILE [--special] [--tokenize-only]\n", stderr);
    return 2;
}
