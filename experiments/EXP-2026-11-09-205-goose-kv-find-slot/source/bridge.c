// Bridge for the Goose KV slot-search module.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;

__attribute__((destructor)) static void fg205f_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG205F_KV_FIND_SLOTS calls=%llu\n", atomic_load(&g_calls));
}

static void g_fail(int64_t c) { fprintf(stderr, "FG205F validation code %lld\n", (long long)c); abort(); }
static int64_t g_ld_i64(uint64_t d, int64_t b) { return *(const int64_t *)(uintptr_t)(d + (uint64_t)b); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }
static int64_t g_ld_u8(uint64_t d, int64_t b) { return (int64_t) *(const uint8_t *)(uintptr_t)(d + (uint64_t)b); }

#define fg205f_ld_i64 g_ld_i64
#define fg205f_st_i64 g_st_i64
#define fg205f_ld_u8  g_ld_u8
#define fg205f_fail   g_fail
#define gg_init_globals fg205f_init_globals
#define main fg205f_generated_main
#include "find_slot-generated.c"
#undef main

int64_t fg205f_head_start(int64_t a, int64_t b, int64_t c) { return gg_function_1(a, b, c, 0); }

int64_t fg205f_find_slots(uint64_t can_use, int64_t size, int64_t head0,
                          int64_t n_tokens, int64_t cont, uint64_t out) {
    atomic_fetch_add(&g_calls, 1);
    return gg_function_4(can_use, size, head0, n_tokens, cont, out, 0);
}

int fg205f_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_FIND_SLOT");
    return !(f && strcmp(f, "0") == 0);
}
