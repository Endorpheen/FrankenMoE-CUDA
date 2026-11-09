// Bridge for the Goose VMM pool module: supplies the store/fail primitives and exposes the
// two entry points. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}
static void g_fail(int64_t c) {
    fprintf(stderr, "FG195V validation code %lld\n", (long long)c);
    abort();
}

#define fg195v_st_i64 g_st_i64
#define fg195v_fail   g_fail
#define main fg195v_generated_main
#include "device-generated.c"
#undef main

void fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                       int64_t granularity, uint64_t out) {
    gg_function_1(size, pool_used, pool_size, granularity, out, 0);
}

int64_t fg195v_plan_free(int64_t ptr_off, int64_t pool_used, int64_t size) {
    return gg_function_5(ptr_off, pool_used, size, 0);
}
