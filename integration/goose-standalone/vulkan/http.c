#ifdef _WIN32
#include "../windows/platform.h"
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

#include "http.h"
#include "qwen2.h"
#include "qwen2_unit.h"
#include "tokenizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    fg_vk_qwen2 model;
    fg_vk_tokenizer *tokenizer;
    float logits[FG_QWEN_VOCAB];
    uint32_t previous;
} fg_vk_http;

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
    fg_vk_qwen2 *m = &h->model;
    if (prompt.len <= 0 || limit <= 0 ||
        prompt.len > m->context || limit - 1 > m->context - prompt.len) return -1;
    for (int64_t i = 0; i < prompt.len; ++i) {
        if (prompt.data[i] < 0 || prompt.data[i] >= FG_QWEN_VOCAB) return -1;
    }
    // Submissions are synchronous. A rewind hides old KV and the next step
    // resets any command buffer left recording after a dispatch error.
    m->position = 0;
    m->graph.recording = 0;
    h->previous = UINT32_MAX;
    for (int64_t i = 0; i < prompt.len; ++i) {
        if (!fg_vk_qwen2_step(m, (uint32_t)prompt.data[i], i + 1 == prompt.len ? h->logits : NULL)) return -1;
    }
    return 0;
}

int64_t fg_vk_http_next(uint64_t owner, uint8_t ignore_eog, sl_u8 text, sl_i64 id) {
    fg_vk_http *h = (fg_vk_http *)(uintptr_t)owner;
    if (id.len < 1 || text.len < 0 ||
        (h->previous != UINT32_MAX && !fg_vk_qwen2_step(&h->model, h->previous, h->logits))) return -1;
    uint32_t token;
    if (!fg_vk_qwen2_choose(h->logits, ignore_eog ? h->tokenizer : NULL, &token)) return -1;
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
    fprintf(stderr, "Vulkan Qwen2 HTTP listening on 127.0.0.1:%lld\n", (long long)port);
    fflush(stderr);
}

int fg_vk_http_run(const char *model_file, uint32_t capacity, uint32_t index, int cpu,
    uint32_t port, uint32_t requests) {
#ifdef _WIN32
    if (!fg_win_network_ready()) return 0;
#endif
    fg_vk_http *h = calloc(1, sizeof(*h));
    int ok = 0;
    if (h) {
        h->tokenizer = fg_vk_qwen2_tokenizer_open(model_file);
        if (h->tokenizer && fg_vk_qwen2_open(&h->model, model_file, capacity, index, cpu)) {
            ok = fg_vk_http_serve((uintptr_t)h, port, requests, capacity) >= 0;
        }
        fg_vk_qwen2_close(&h->model);
        fg_vk_tokenizer_close(h->tokenizer);
        free(h);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    if (!ok) fputs("Vulkan Qwen2 HTTP stopped with an error\n", stderr);
    return ok;
}
