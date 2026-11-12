// Bridge for the Goose KV cell-copy module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;

__attribute__((destructor)) static void fg204s_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG204S_KV_SET cells_copies=%llu\n", atomic_load(&g_calls));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG204S validation code %lld\n", (long long)c);
    abort();
}
static int64_t g_ld_i64(uint64_t d, int64_t b) { return *(const int64_t *)(uintptr_t)(d + (uint64_t)b); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

#define fg204s_ld_i64 g_ld_i64
#define fg204s_st_i64 g_st_i64
#define fg204s_fail   g_fail
#define main fg204s_generated_main
#include "set-generated.c"
#undef main

// gg_function_1 is the only eight-argument body in this module.
int64_t fg204s_set_cells(uint64_t dst, int64_t dn, int64_t di,
                         uint64_t src, int64_t sn, int64_t sj, int64_t m) {
    atomic_fetch_add(&g_calls, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_1(dst, dn, di, src, sn, sj, m, 0);
}

int fg204s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_SET");
    return !(f && strcmp(f, "0") == 0);
}
