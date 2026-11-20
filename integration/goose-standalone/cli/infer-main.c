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

int64_t fg_http_complete(sl_u8 prompt, int64_t max_tokens, sl_i64 ids,
                         sl_u8 text, sl_i64 result) {
    if (max_tokens <= 0 || max_tokens > ids.len || result.len < 1) return -1;
    const int64_t count = hose_generate(prompt.data, prompt.len, max_tokens,
                                        ids.data, NULL, 0, NULL);
    if (count <= 0) return -1;
    result.data[0] = count;
    return hose_detokenize(ids.data, count, text.data, text.len);
}

void fg_http_stats(sl_i64 stats) {
    if (stats.len >= 5) hose_stats(stats.data);
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

static int serve(const char * model, int64_t port, int64_t limit) {
    if (port <= 0 || port > 65535 || limit < 0) {
        fputs("invalid port or connection limit\n", stderr);
        return 2;
    }
    static unsigned char request[262144], output[65536], response[524288];
    static int64_t ids[2048];
    fg_goose_host_ensure_init();
    hose_host_load(model);
    const int64_t served = ih_serve(port, limit, (sl_u8) { request, sizeof request },
        (sl_u8) { output, sizeof output }, (sl_u8) { response, sizeof response },
        (sl_i64) { ids, sizeof ids / sizeof *ids });
    if (served >= 0) return 0;
    fprintf(stderr, "HTTP listener failed (%" PRId64 ")\n", served);
    return 1;
}

int main(int argc, char ** argv) {
    const int server_mode = argc >= 3 && strcmp(argv[2], "--serve") == 0;
    if (server_mode && (argc == 4 || argc == 5)) {
        return serve(argv[1], positive_integer(argv[3]), argc == 5 ? positive_integer(argv[4]) : 0);
    }
    if (server_mode || (argc != 3 && argc != 4)) {
        fprintf(stderr, "usage: %s MODEL.gguf PROMPT [MAX_TOKENS]\n", argv[0]);
        fprintf(stderr, "       %s MODEL.gguf --serve PORT [MAX_CONNECTIONS]\n", argv[0]);
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
    hose_host_load(argv[1]);
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
