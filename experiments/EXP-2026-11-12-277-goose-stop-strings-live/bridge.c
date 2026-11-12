#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG276T validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static int64_t g_ld8(uint64_t b, int64_t i) { return *(const uint8_t *) (uintptr_t) (b + (uint64_t) i); }
static void g_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg276t_fail   g_fail
#define fg276t_ld_i64 g_ld
#define fg276t_ld_u8  g_ld8
#define fg276t_st_i64 g_st
#define fg276t_find_full    fg276t_find_full_impl
#define fg276t_find_partial fg276t_find_partial_impl
#define main fg276t_generated_main
#include "stop_strings-functions.c"
#undef main
#undef fg276t_find_full
#undef fg276t_find_partial

static _Atomic uint64_t g_calls, g_hits;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG276T_STOP_STRINGS full=%llu hits=%llu\n",
            (unsigned long long) atomic_load(&g_calls), (unsigned long long) atomic_load(&g_hits));
}
int64_t fg276t_find_full(uint64_t t, int64_t l, uint64_t w, uint64_t o, uint64_t ln, int64_t n, int64_t lts, uint64_t ow) {
    atomic_fetch_add(&g_calls, 1);
    const int64_t r = fg276t_find_full_impl(t, l, w, o, ln, n, lts, ow);
    if (r >= 0) atomic_fetch_add(&g_hits, 1);
    return r;
}
int fg276t_enabled(void) {
    const char * f = getenv("GGML_GOOSE_STOP_STRINGS");
    return !(f && strcmp(f, "0") == 0);
}
