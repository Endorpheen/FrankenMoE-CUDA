#include "qwen2.h"
#include "qwen2_unit.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv) {
    uint32_t device = FG_VK_AUTO_DEVICE, predict = 1;
    int cpu = 0;
    const char *dump = NULL;
    if (argc < 4) goto usage;
    int start = 2;
    for (; start < argc && strcmp(argv[start], "--"); ++start) {
        if (!strcmp(argv[start], "--allow-cpu")) cpu = 1;
        else if (!strcmp(argv[start], "--device-index") && start + 1 < argc) {
            if (!read_number(argv[++start], FG_VK_AUTO_DEVICE, &device)) goto usage;
        } else if (!strcmp(argv[start], "--n-predict") && start + 1 < argc) {
            if (!read_number(argv[++start], 32769, &predict) || !predict) goto usage;
        } else if (!strcmp(argv[start], "--logits") && start + 1 < argc) dump = argv[++start];
        else goto usage;
    }
    if (start + 1 >= argc || (cpu && device == FG_VK_AUTO_DEVICE)) goto usage;
    ++start;
    const unsigned prompt = argc - start;
    if (prompt > 32768 - (predict - 1)) goto usage;

    uint32_t *ids = malloc(prompt * sizeof(*ids));
    float *logits = malloc(FG_QWEN_VOCAB * sizeof(*logits));
    fg_vk_qwen2 *model = calloc(1, sizeof(*model));
    if (!ids || !logits || !model) {
        free(ids); free(logits); free(model);
        return 1;
    }
    int ok = 1;
    for (unsigned i = 0; i < prompt && ok; ++i) ok = read_number(argv[start + i], FG_QWEN_VOCAB, &ids[i]);
    if (ok) {
        goose_init(0, NULL);
        ok = fg_vk_qwen2_open(model, argv[1], prompt + predict - 1, device, cpu);
    }
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
    for (uint32_t t = 0; t < predict && ok; ++t) {
        uint32_t token;
        ok = choose(logits, &token);
        if (!ok) break;
        printf("%u\n", token);
        if (t + 1 < predict) ok = fg_vk_qwen2_step(model, token, logits);
    }
    fg_vk_qwen2_close(model);
    free(model); free(logits); free(ids);
    if (!ok) fputs("Vulkan Qwen2 inference failed\n", stderr);
    return ok ? 0 : 1;
usage:
    fputs("Usage: frankenmoe-vulkan MODEL [--device-index N] [--allow-cpu] [--n-predict N] [--logits FILE] -- TOKEN ...\n", stderr);
    return 2;
}
