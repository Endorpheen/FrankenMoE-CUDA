#include "goose-host-llm.h"
#include "goose-host.h"
#include "main.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

extern int64_t fg_cuda_dense_bytes(void);
extern int64_t fg_cuda_expert_bytes(void);
extern void fg_cuda_expert_stats(sl_i64 stats);
extern int64_t fg_cuda_model_state_bytes(void);

int64_t fg_http_tokenize(sl_u8 prompt, sl_i64 ids) {
    return hose_tokenize(prompt.data, prompt.len, ids.data, ids.len);
}

int64_t fg_http_detokenize(sl_i64 ids, sl_u8 text) {
    return hose_detokenize(ids.data, ids.len, text.data, text.len);
}

void fg_http_stats(sl_i64 stats) {
    if (stats.len >= 5) hose_stats(stats.data);
    if (stats.len >= 6) stats.data[5] = hose_context_capacity();
    if (stats.len >= 10 && hose_model_residency(stats.data + 6) != 0) stats.data[6] = -1;
    if (stats.len >= 11) stats.data[10] = fg_cuda_dense_bytes();
    if (stats.len >= 12) stats.data[11] = fg_cuda_expert_bytes();
    if (stats.len >= 13) stats.data[12] = fg_cuda_model_state_bytes();
    if (stats.len >= 15) hose_prefill_profile(stats.data + 13);
    if (stats.len >= 21) fg_cuda_expert_stats((sl_i64) { stats.data + 15, 6 });
    if (stats.len >= 22) stats.data[21] = hose_expert_cache_limit();
}

void fg_http_listening(int64_t port) {
    fprintf(stderr, "listening on 127.0.0.1:%" PRId64 "\n", port);
    fflush(stderr);
}

int64_t fg_http_set_timeout(int64_t fd) {
    const struct timeval timeout = { .tv_sec = 30 };
    if (setsockopt((int) fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) != 0) return -1;
    return setsockopt((int) fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
}

static int64_t positive_integer(const char * text) {
    char * end;
    errno = 0;
    const intmax_t value = strtoimax(text, &end, 10);
    return errno || end == text || *end || value <= 0 || value > INT64_MAX ? -1 : (int64_t) value;
}

static int serve(const char * model, int64_t port, int64_t limit, int64_t capacity) {
    if (port <= 0 || port > 65535 || limit < 0) {
        fputs("invalid port or connection limit\n", stderr);
        return 2;
    }
    static unsigned char request[262144], output[65536], response[524288];
    static int64_t ids[2048];
    fg_goose_host_ensure_init();
    hose_host_load_context(model, capacity);
    const int64_t served = ih_serve(port, limit, (sl_u8) { request, sizeof request },
        (sl_u8) { output, sizeof output }, (sl_u8) { response, sizeof response },
        (sl_i64) { ids, sizeof ids / sizeof *ids });
    if (served >= 0) return 0;
    fprintf(stderr, "HTTP listener failed (%" PRId64 ")\n", served);
    return 1;
}

int main(int argc, char ** argv) {
    const char * program = argv[0];
    int64_t context = HOSE_DEFAULT_CONTEXT;
    int64_t batch = HOSE_DEFAULT_PREFILL_BATCH;
    int64_t ubatch = 0;
    while (argc >= 2) {
        if (strcmp(argv[1], "--cache-type-k") == 0 || strcmp(argv[1], "--cache-type-v") == 0) {
            if (argc < 3 || strcmp(argv[2], "q4_0") != 0) {
                fprintf(stderr, "%s requires q4_0 in the standalone Apex runtime\n", argv[1]);
                return 2;
            }
            argc -= 2;
            argv += 2;
            continue;
        }
        if (strcmp(argv[1], "--expert-cache-mib") == 0) {
            if (argc < 3 || hose_configure_expert_cache(positive_integer(argv[2])) != 0) {
                fputs("--expert-cache-mib requires a positive MiB value that fits in signed 64-bit bytes\n", stderr);
                return 2;
            }
            argc -= 2;
            argv += 2;
            continue;
        }
        int64_t * value = NULL;
        if (strcmp(argv[1], "--ctx-size") == 0) value = &context;
        if (strcmp(argv[1], "--batch-size") == 0) value = &batch;
        if (strcmp(argv[1], "--ubatch-size") == 0) value = &ubatch;
        if (!value) break;
        if (argc < 3 || (*value = positive_integer(argv[2])) <= 0) {
            fprintf(stderr, "%s requires a positive integer\n", argv[1]);
            return 2;
        }
        argc -= 2;
        argv += 2;
    }
    if (context > INT32_MAX || context % 32 != 0) {
        fputs("--ctx-size must be a positive multiple of 32 within the model context length\n", stderr);
        return 2;
    }
    if (ubatch == 0) ubatch = batch < HOSE_DEFAULT_PREFILL_BATCH ? batch : HOSE_DEFAULT_PREFILL_BATCH;
    if (hose_configure_prefill(batch, ubatch) != 0) {
        fputs("require 1 <= --ubatch-size <= --batch-size <= 4096\n", stderr);
        return 2;
    }
    const int server_mode = argc >= 3 && strcmp(argv[2], "--serve") == 0;
    if (server_mode && (argc == 4 || argc == 5)) {
        return serve(argv[1], positive_integer(argv[3]), argc == 5 ? positive_integer(argv[4]) : 0, context);
    }
    if (server_mode || (argc != 3 && argc != 4)) {
        fputs("options: --ctx-size N --batch-size N --ubatch-size N --cache-type-k q4_0 "
              "--cache-type-v q4_0 --expert-cache-mib N\n", stderr);
        fprintf(stderr, "usage: %s [OPTIONS] MODEL.gguf PROMPT [MAX_TOKENS]\n", program);
        fprintf(stderr, "       %s [OPTIONS] MODEL.gguf --serve PORT [MAX_CONNECTIONS]\n", program);
        return 2;
    }
    int64_t max_tokens = 1;
    if (argc == 4) {
        const int64_t value = positive_integer(argv[3]);
        if (value <= 0 ||
            (uintmax_t) value > SIZE_MAX / sizeof(int64_t)) {
            fputs("invalid maximum token count\n", stderr);
            return 2;
        }
        max_tokens = (int64_t) value;
    }

    int64_t * ids = malloc((size_t) max_tokens * sizeof(*ids));
    size_t capacity = 256;
    unsigned char * text = malloc(capacity);
    int status = 1;
    if (!ids || !text) goto done;

    fg_goose_host_ensure_init();
    hose_host_load_context(argv[1], context);
    const int64_t count = hose_generate((const unsigned char *) argv[2], (int64_t) strlen(argv[2]),
                                        max_tokens, ids, NULL, 0, NULL);
    if (count <= 0) {
        fputs("inference failed\n", stderr);
        goto done;
    }
    for (int64_t i = 0; i < count; ++i) {
        int64_t length = hose_detokenize(ids + i, 1, text, (int64_t) capacity);
        while (length < 0) {
            if (capacity > SIZE_MAX / 2 || capacity > INT64_MAX / 2) goto done;
            unsigned char * grown = realloc(text, capacity * 2);
            if (!grown) goto done;
            text = grown;
            capacity *= 2;
            length = hose_detokenize(ids + i, 1, text, (int64_t) capacity);
        }
        if (fwrite(text, 1, (size_t) length, stdout) != (size_t) length) goto done;
    }
    if (putchar('\n') == EOF || fflush(stdout) == EOF) goto done;
    status = 0;

done:
    free(text);
    free(ids);
    return status;
}
