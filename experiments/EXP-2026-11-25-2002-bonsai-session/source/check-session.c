#include "session.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GENERATED = 8, PROMPT = 273 };

static int generate(struct bonsai_session *session, const uint32_t *prompt,
    unsigned count, float *rows, int inject_invalid) {
    const uint32_t invalid = BONSAI_SESSION_VOCAB;
    if (bonsai_session_step(session, prompt, count, rows)) return -1;
    if (inject_invalid && bonsai_session_step(session, &invalid, 1, rows) == 0) return -1;
    for (unsigned step = 0; step < GENERATED; ++step) {
        float *row = rows + step * BONSAI_SESSION_VOCAB;
        uint32_t best = BONSAI_SESSION_VOCAB;
        for (uint32_t i = 0; i < BONSAI_SESSION_VOCAB; ++i) {
            if (!isfinite(row[i])) return -1;
            if (bonsai_session_is_eog(session, i)) continue;
            if (best == BONSAI_SESSION_VOCAB || row[i] > row[best]) best = i;
        }
        printf("{\"step\":%u,\"token\":%u,\"logit\":%.9g}\n", step, best, row[best]);
        if (best == BONSAI_SESSION_VOCAB) return -1;
        if (step + 1 < GENERATED && bonsai_session_step(session, &best, 1, row + BONSAI_SESSION_VOCAB)) return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5) return 2;
    FILE *input = fopen(argv[4], "r");
    if (!input) return 2;
    uint32_t prompt[PROMPT];
    unsigned count = 0;
    while (count < PROMPT && fscanf(input, "%u", &prompt[count]) == 1) ++count;
    fclose(input);
    if (count != PROMPT) return 2;

    const size_t bytes = (size_t)GENERATED * BONSAI_SESSION_VOCAB * sizeof(float);
    float *a = malloc(bytes), *b = malloc(bytes);
    struct bonsai_session *session = bonsai_session_open(argv[1], argv[2], argv[3], 36864, 128);
    const uint32_t other[] = {1511, 17, 248046, 1511};
    int status = !a || !b || !session ||
        generate(session, prompt, count, a, 0) ||
        bonsai_session_reset(session) ||
        generate(session, other, sizeof(other) / sizeof(*other), b, 0) ||
        memcmp(a, b, bytes) == 0 ||
        bonsai_session_reset(session) ||
        generate(session, prompt, count, b, 1) ||
        memcmp(a, b, bytes) != 0;
    if (!status) printf("Identical A logits: %zu bytes\n", bytes);
    bonsai_session_close(session);
    free(a);
    free(b);
    return status;
}
