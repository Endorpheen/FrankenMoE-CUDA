#include "goose-host-llm.h"
#include "goose-host.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s MODEL.gguf PROMPT [MAX_TOKENS]\n", argv[0]);
        return 2;
    }
    int64_t max_tokens = 1;
    if (argc == 4) {
        char * end;
        errno = 0;
        const intmax_t value = strtoimax(argv[3], &end, 10);
        if (errno || end == argv[3] || *end || value <= 0 || value > INT64_MAX ||
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
