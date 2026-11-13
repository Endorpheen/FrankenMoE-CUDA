// Bridge for the Goose VMM pool module: supplies the store/fail primitives and exposes the
// two entry points. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}
static void g_fail(int64_t c) {
    fprintf(stderr, "FG195V validation code %lld\n", (long long)c);
    abort();
}

#define fg195v_st_i64 g_st_i64
#define fg195v_fail   g_fail
#define gg_init_globals fg195v_init_globals
#define main fg195v_generated_main
#include "device-generated.c"
#undef main

static _Atomic uint64_t g_alloc, g_free, g_grow, g_native;

__attribute__((destructor)) static void fg195v_report(void) {
    fprintf(stderr, "FG195V_CUDA_VMM_POOL alloc=%llu free=%llu grow=%llu native=%llu\n",
            (unsigned long long)atomic_load(&g_alloc), (unsigned long long)atomic_load(&g_free),
            (unsigned long long)atomic_load(&g_grow), (unsigned long long)atomic_load(&g_native));
}

void fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                       int64_t granularity, uint64_t out) {
    gg_function_1(size, pool_used, pool_size, granularity, out, 0);
    atomic_fetch_add(&g_alloc, 1);
    if (((int64_t *)(uintptr_t)out)[0] != 0) atomic_fetch_add(&g_grow, 1);
}

int64_t fg195v_plan_free(int64_t ptr_off, int64_t pool_used, int64_t size) {
    atomic_fetch_add(&g_free, 1);
    return gg_function_5(ptr_off, pool_used, size, 0);
}

// Rollback switch: GGML_GOOSE_CUDA_VMM_POOL=0 selects the native pool arithmetic that the
// core file keeps inline. Any other value, including unset, uses the Goose path.
int fg195v_enabled(void) {
    const char * f = getenv("GGML_GOOSE_CUDA_VMM_POOL");
    return !(f && strcmp(f, "0") == 0);
}
