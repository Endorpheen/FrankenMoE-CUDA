// Bridge for the Goose KV slot-search module, lazy per-cell predicate.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls, g_tested;
static const uint8_t * g_can;
static int64_t g_can_n;

__attribute__((destructor)) static void fg205g_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG205G_KV_FIND_SLOTS calls=%llu tested=%llu\n",
            atomic_load(&g_calls), atomic_load(&g_tested));
}

static void g_fail(int64_t c) { fprintf(stderr, "FG205G validation code %lld\n", (long long)c); abort(); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

// the extern primitive Goose calls for exactly the cells it tests
static int64_t g_can_use(int64_t idx) {
    atomic_fetch_add(&g_tested, 1);
    if (idx < 0 || idx >= g_can_n) return 0;
    return g_can[idx] ? 1 : 0;
}

#define fg205g_fail    g_fail
#define fg205g_st_i64  g_st_i64
#define fg205g_can_use g_can_use
#define gg_init_globals fg205g_init_globals
#define main fg205g_generated_main
#include "find_slot2-generated.c"
#undef main

int64_t fg205g_head_start(int64_t a, int64_t b, int64_t c) { return gg_function_1(a, b, c, 0); }

int64_t fg205g_find_slots(const uint8_t * can, int64_t size, int64_t head0,
                          int64_t n_tokens, int64_t cont, int64_t * out) {
    g_can = can; g_can_n = size;
    atomic_fetch_add(&g_calls, 1);
    return gg_function_4(size, head0, n_tokens, cont, (uint64_t)(uintptr_t) out, 0);
}

int fg205g_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_FIND_SLOT");
    return !(f && strcmp(f, "0") == 0);
}
