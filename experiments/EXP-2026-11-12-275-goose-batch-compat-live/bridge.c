#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG274B validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static float g_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

#define fg274b_fail      g_fail
#define fg274b_ld_i64    g_ld
#define fg274b_from_bits g_from_bits
#define fg274b_can_batch fg274b_can_batch_impl
#define main fg274b_generated_main
#include "batch_compat-functions.c"
#undef main
#undef fg274b_can_batch

static _Atomic uint64_t g_calls, g_yes;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG274B_BATCH_COMPAT calls=%llu yes=%llu\n",
            (unsigned long long) atomic_load(&g_calls), (unsigned long long) atomic_load(&g_yes));
}
uint8_t fg274b_can_batch(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f,
                         uint64_t g, uint64_t h, int64_t i, uint64_t j, uint64_t k, int64_t l) {
    atomic_fetch_add(&g_calls, 1);
    const uint8_t r = fg274b_can_batch_impl(a, b, c, d, e, f, g, h, i, j, k, l);
    if (r) atomic_fetch_add(&g_yes, 1);
    return r;
}
int fg274b_enabled(void) {
    const char * f = getenv("GGML_GOOSE_BATCH_COMPAT");
    return !(f && strcmp(f, "0") == 0);
}
