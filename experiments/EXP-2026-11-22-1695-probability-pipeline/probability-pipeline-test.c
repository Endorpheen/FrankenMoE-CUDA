#define _POSIX_C_SOURCE 200809L
#include "probability.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern void fg_probability_init(int, char **);
extern uint8_t fg_prob_pipeline_start(int64_t, int64_t, int64_t);
extern uint8_t fg_prob_pipeline_submit(sl_f32, int64_t);
extern uint8_t fg_prob_pipeline_finish(int64_t, sl_i64, sl_f32);

enum { ROWS = 31, TOP = 5 };

static void make_logits(float * out, int64_t n, int64_t row) {
    for (int64_t i = 0; i < n; ++i) out[i] = (float) ((row * 13 + i * 9973) % 1021) * 0x1p-5f - 20.0f;
}

static void compare_rows(int64_t n) {
    int64_t ids[ROWS][TOP+1], expected_ids[ROWS][TOP+1], * order = malloc((size_t) n * sizeof(int64_t));
    float logp[ROWS][TOP+1], expected_logp[ROWS][TOP+1], * logits = malloc((size_t) n * sizeof(float));
    assert(order && logits);
    for (int64_t row = 0; row < ROWS; ++row) {
        make_logits(logits, n, row);
        assert(fg_probability_worker((sl_f32) {logits,n}, (sl_i64) {order,n}, row % n, TOP,
            (sl_i64) {expected_ids[row],TOP+1}, (sl_f32) {expected_logp[row],TOP+1}));
    }
    assert(fg_prob_pipeline_start(n, TOP, ROWS));
    assert(!fg_prob_pipeline_submit((sl_f32) {logits,n}, n));
    for (int64_t row = 0; row < ROWS; ++row) {
        make_logits(logits, n, row);
        assert(fg_prob_pipeline_submit((sl_f32) {logits,n}, row % n));
    }
    assert(fg_prob_pipeline_finish(ROWS, (sl_i64) {ids[0],ROWS*(TOP+1)},
        (sl_f32) {logp[0],ROWS*(TOP+1)}));
    assert(memcmp(ids, expected_ids, sizeof(ids)) == 0);
    assert(memcmp(logp, expected_logp, sizeof(logp)) == 0);
    free(order);
    free(logits);
}

int main(void) {
    assert(setenv("GOOSE_CPU_PROBABILITY_PIPELINE", "1", 1) == 0);
    fg_probability_init(0, NULL);
    compare_rows(37);
    assert(fg_prob_pipeline_start(37, TOP, ROWS));
    assert(!fg_prob_pipeline_finish(-1, (sl_i64) {0}, (sl_f32) {0}));
    compare_rows(248320);
    return 0;
}
