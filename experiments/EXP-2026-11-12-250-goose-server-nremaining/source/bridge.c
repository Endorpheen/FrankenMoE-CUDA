// Bridge for the Goosslot budget module (EXP250): scalars in, scalars out.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fg250s_fail(int64_t c) { fprintf(stderr, "FG250S_FAIL code=%lld\n", (long long) c); abort(); }
static void fg250s_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t) p)[k * 16 + i] = (int64_t) v; }
static uint64_t fg250s_get(int64_t p, int64_t k, int64_t i) { return (uint64_t) ((const int64_t *)(uintptr_t) p)[k * 16 + i]; }

#define main fg250ds_generated_main
#include "host-qualified.c"
#undef main

int64_t fg250s_remaining(int64_t n_predict_max, int64_t n_gen) {
    int64_t pkt[2 * 16] = {0};
    pkt[0] = n_predict_max;
    pkt[1] = n_gen;
    (void) fg250ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);
    return pkt[16 + 0];
}

int64_t fg250s_has_budget(int64_t n_predict_max, int64_t n_gen) {
    int64_t pkt[2 * 16] = {0};
    pkt[0] = n_predict_max;
    pkt[1] = n_gen;
    (void) fg250ds_gg_function_1((int64_t)(uintptr_t) pkt, 1);
    return pkt[16 + 1];
}
