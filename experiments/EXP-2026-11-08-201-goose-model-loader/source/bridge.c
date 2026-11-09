// Bridge for the Goose model-loader decision module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_shape, g_native;

__attribute__((destructor)) static void fg201l_report(void) {
    if (!atomic_load(&g_shape) && !atomic_load(&g_native)) return;
    fprintf(stderr, "FG201L_MODEL_LOADER shape_checks=%llu direct=%llu\n",
            atomic_load(&g_shape), atomic_load(&g_native));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG201L validation code %lld\n", (long long)c);
    abort();
}
static int64_t g_ld_ne(uint64_t desc, int64_t i) {
    return *(const int64_t *)(uintptr_t)(desc + (uint64_t)i * 8);
}
static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}

#define fg201l_ld_ne  g_ld_ne
#define fg201l_st_i64 g_st_i64
#define fg201l_fail   g_fail
#define main fg201l_generated_main
#include "loader-generated.c"
#undef main

int64_t fg201l_nelements(uint64_t desc, int64_t n) { return gg_function_1(desc, n, 0); }
int64_t fg201l_dims_ok(uint64_t desc, int64_t nreq, int64_t r0, int64_t r1, int64_t r2, int64_t r3) {
    return gg_function_5(desc, nreq, r0, r1, r2, r3, 0);
}
int64_t fg201l_shape_ok(int64_t allow_reshape, uint64_t desc, int64_t nreq, int64_t total,
                        int64_t r0, int64_t r1, int64_t r2, int64_t r3) {
    return gg_function_6(allow_reshape, desc, nreq, total, r0, r1, r2, r3, 0);
}

// Integration entry point. Records the decision when the caller routes one through Goose.
int fg201l_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MODEL_LOADER");
    return !(f && strcmp(f, "0") == 0);
}
int fg201l_shape_check(int allow_reshape, const int64_t * dims, int64_t nreq, int64_t total,
                       const int64_t * req) {
    if (!fg201l_enabled()) { atomic_fetch_add(&g_native, 1); return -1; }
    const int64_t r = fg201l_shape_ok(allow_reshape, (uint64_t)(uintptr_t)dims, nreq, total,
                                      req[0], req[1], req[2], req[3]);
    atomic_fetch_add(&g_shape, 1);
    return (int)r;
}
