#ifdef _WIN32
#include "../windows/platform.h"
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

#include "http.h"
#include "bonsai-model.h"
#include "apex-model.h"
#include "inkling-model.h"
#include "qwen2.h"
#include "qwen2_unit.h"
#include "tokenizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    fg_vk_qwen2 model;
    fg_vk_bonsai *bonsai;
    fg_vk_apex *apex;
    fg_vk_inkling *inkling;
    fg_vk_tokenizer *tokenizer;
    float *logits;
    uint32_t previous, vocabulary, capacity;
} fg_vk_http;

static int step(fg_vk_http *h, uint32_t id, float *logits) {
    if (h->inkling) return fg_vk_inkling_step(h->inkling, id, logits);
    if (h->apex) return fg_vk_apex_step(h->apex, id, logits);
    return h->bonsai ? fg_vk_bonsai_step(h->bonsai, id, logits) : fg_vk_qwen2_step(&h->model, id, logits);
}

int64_t fg_vk_http_tokenize(uint64_t owner, sl_u8 prompt, sl_i64 ids, uint8_t special) {
    fg_vk_http *h = (fg_vk_http *)(uintptr_t)owner;
    if (prompt.len <= 0 || prompt.len > FG_VK_TEXT_BYTES || ids.len <= 0 || ids.len > 32768) return -1;
    char *text = malloc((size_t)prompt.len);
    uint32_t *tokens = malloc((size_t)ids.len * sizeof(*tokens));
    int count = -1;
    if (text && tokens) {
        memcpy(text, prompt.data, (size_t)prompt.len);
        count = fg_vk_tokenizer_encode(h->tokenizer, text, (size_t)prompt.len, special, tokens, (uint32_t)ids.len);
        for (int i = 0; i < count; ++i) ids.data[i] = tokens[i];
    }
    free(tokens); free(text);
    return count;
}

int64_t fg_vk_http_prefill(uint64_t owner, sl_i64 prompt, int64_t limit) {
    fg_vk_http *h = (fg_vk_http *)(uintptr_t)owner;
    if (prompt.len <= 0 || limit <= 0 ||
        prompt.len > h->capacity || limit - 1 > h->capacity - prompt.len) return -1;
    for (int64_t i = 0; i < prompt.len; ++i) {
        if (prompt.data[i] < 0 || prompt.data[i] >= h->vocabulary) return -1;
    }
    // Submissions are synchronous. A rewind hides old KV and the next step
    // resets any command buffer left recording after a dispatch error.
    if (h->inkling) {
        if (!fg_vk_inkling_reset(h->inkling)) return -1;
    } else if (h->apex) {
        if (!fg_vk_apex_reset(h->apex)) return -1;
    } else if (h->bonsai) {
        if (!fg_vk_bonsai_reset(h->bonsai)) return -1;
    } else {
        h->model.position = 0;
        h->model.graph.recording = 0;
    }
    h->previous = UINT32_MAX;
    for (int64_t i = 0; i < prompt.len; ++i) {
        if (!step(h, (uint32_t)prompt.data[i], i + 1 == prompt.len ? h->logits : NULL)) return -1;
    }
    return 0;
}

int64_t fg_vk_http_next(uint64_t owner, uint8_t ignore_eog, sl_u8 text, sl_i64 id) {
    fg_vk_http *h = (fg_vk_http *)(uintptr_t)owner;
    if (id.len < 1 || text.len < 0 ||
        (h->previous != UINT32_MAX && !step(h, h->previous, h->logits))) return -1;
    uint32_t token;
    if (!fg_vk_choose(h->logits, h->vocabulary, ignore_eog ? h->tokenizer : NULL, &token)) return -1;
    if (!ignore_eog && fg_vk_tokenizer_is_eog(h->tokenizer, token)) return -2;
    const int n = fg_vk_tokenizer_piece(h->tokenizer, token, (char *)text.data, (size_t)text.len);
    if (n < 0 || n > text.len) return -1;
    h->previous = token;
    id.data[0] = token;
    return n;
}

int64_t fg_vk_http_generate(uint64_t owner, sl_i64 prompt, int64_t limit,
    uint8_t ignore_eog, sl_i64 ids, sl_u8 text, sl_i64 info) {
    if (limit > ids.len || info.len < 2 || fg_vk_http_prefill(owner, prompt, limit)) return -1;
    int64_t at = 0;
    info.data[0] = 0;
    info.data[1] = 0;
    for (int64_t step = 0; step < limit; ++step) {
        int64_t n = fg_vk_http_next(owner, ignore_eog, (sl_u8){text.data + at, text.len - at},
                                    (sl_i64){ids.data + step, 1});
        if (n == -2) {
            info.data[1] = 1;
            break;
        }
        if (n < 0) return -1;
        at += n;
        ++info.data[0];
    }
    return at;
}

int64_t fg_vk_http_now(void) {
    return (int64_t)time(NULL);
}

int64_t fg_vk_http_timeout(int64_t fd) {
#ifdef _WIN32
    return fg_win_socket_timeout(fd);
#else
    const struct timeval timeout = {.tv_sec = 30};
    if (setsockopt((int)fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) return -1;
    return setsockopt((int)fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

void fg_vk_http_ready(int64_t port) {
    fprintf(stderr, "Vulkan HTTP listening on 127.0.0.1:%lld\n", (long long)port);
    fflush(stderr);
}

int fg_vk_http_run(const char *model_file, uint32_t capacity, uint32_t index, int cpu,
    uint32_t port, uint32_t requests, fg_vk_model_kind kind) {
#ifdef _WIN32
    if (!fg_win_network_ready()) return 0;
#endif
    fg_vk_http *h = calloc(1, sizeof(*h));
    int ok = 0;
    if (h) {
        h->capacity = capacity;
        h->vocabulary = kind == FG_VK_MODEL_INKLING ? FG_INKLING_VOCAB : kind == FG_VK_MODEL_APEX ? FG_APEX_VOCAB :
            kind == FG_VK_MODEL_BONSAI ? FG_BONSAI_VOCAB : FG_QWEN_VOCAB;
        h->tokenizer = fg_vk_model_tokenizer_open(model_file, kind);
        h->logits = malloc(h->vocabulary * sizeof(*h->logits));
        if (h->tokenizer && h->logits) {
            if (kind == FG_VK_MODEL_BONSAI) h->bonsai = fg_vk_bonsai_open(model_file, capacity, index, cpu);
            if (kind == FG_VK_MODEL_APEX) h->apex = fg_vk_apex_open(model_file, capacity, index, cpu);
            if (kind == FG_VK_MODEL_INKLING) h->inkling = fg_vk_inkling_open(model_file, capacity, index, cpu);
            if (h->bonsai || h->apex || h->inkling ||
                (kind == FG_VK_MODEL_QWEN2 && fg_vk_qwen2_open(&h->model, model_file, capacity, index, cpu))) {
                ok = fg_vk_http_serve((uintptr_t)h, port, requests, capacity, h->vocabulary, kind) >= 0;
            }
        }
        fg_vk_bonsai_close(h->bonsai);
        fg_vk_apex_close(h->apex);
        fg_vk_inkling_close(h->inkling);
        fg_vk_qwen2_close(&h->model);
        fg_vk_tokenizer_close(h->tokenizer);
        free(h->logits);
        free(h);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    if (!ok) fputs("Vulkan HTTP stopped with an error\n", stderr);
    return ok;
}
