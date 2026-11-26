#define _POSIX_C_SOURCE 200809L
#include "fixture.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { VOCAB = 248320 };
static uint8_t enabled;
extern uint8_t fg_q35_topk_scan_enabled(void);
uint8_t fg_bonsai_host_scan_enabled(void) { return enabled; }

static int64_t select_id(float *rows, int64_t len, int64_t count, uint8_t fast) {
    enabled = fast;
    return probe_greedy((sl_f32){rows, len}, count);
}

static void equivalent(float *rows, int64_t len, int64_t count) {
    assert(select_id(rows, len, count, 0) == select_id(rows, len, count, 1));
}

static int64_t now_ns(void) {
    struct timespec timestamp;
    assert(clock_gettime(CLOCK_MONOTONIC, &timestamp) == 0);
    return timestamp.tv_sec * INT64_C(1000000000) + timestamp.tv_nsec;
}

int main(int argc, char **argv) {
    assert(argc == 2 && fg_q35_topk_scan_enabled());
    goose_init(0, NULL);
    float *scores = malloc(VOCAB * sizeof(*scores));
    assert(scores);
    equivalent(scores, 0, 0);
    equivalent(scores, 0, 1);
    uint32_t rng = 2101;
    for (int i = 0; i < VOCAB; ++i) {
        rng = rng * 1664525u + 1013904223u;
        scores[i] = (float)(rng % 8192u) * (1.0f / 4096.0f);
    }
    for (int n = 1; n <= 1024; ++n) equivalent(scores, VOCAB, n);
    for (int i = 0; i < 80; ++i) scores[i] = -1;
    scores[31] = scores[32] = INFINITY;
    equivalent(scores, 80, 80);
    assert(select_id(scores, 80, 80, 1) == 31);
    scores[0] = NAN;
    equivalent(scores, 80, 80);
    assert(select_id(scores, 80, 80, 1) == 0);
    scores[0] = -INFINITY;
    scores[31] = -0.0f;
    scores[32] = 0.0f;
    equivalent(scores, 80, 80);
    assert(select_id(scores, 80, 80, 1) == 31);
    scores[30] = NAN;
    equivalent(scores, 80, 80);

    int64_t raw_ns[2] = {0, 0};
    unsigned samples = 0;
    FILE *fixture = fopen(argv[1], "rb");
    assert(fixture);
    uint32_t step;
    while (fread(&step, sizeof(step), 1, fixture) == 1) {
        assert(fread(scores, sizeof(*scores), VOCAB, fixture) == VOCAB);
        equivalent(scores, VOCAB, VOCAB);
        static const uint8_t order[] = {0, 1, 1, 0};
        for (size_t j = 0; j < sizeof(order); ++j) {
            uint8_t fast = order[j];
            const int64_t start = now_ns();
            const int64_t token = select_id(scores, VOCAB, VOCAB, fast);
            raw_ns[fast] += now_ns() - start;
            assert(token >= 0 && token < VOCAB);
        }
        samples += 2;
    }
    assert(feof(fixture) && samples > 0);
    fclose(fixture);
    free(scores);
    printf("{\"real_logit_rows\":%u,\"timings_per_arm\":%u,"
           "\"legacy_mean_us\":%.3f,\"scan_mean_us\":%.3f}\n",
           samples / 2, samples, raw_ns[0] / (1000.0 * samples),
           raw_ns[1] / (1000.0 * samples));
}
