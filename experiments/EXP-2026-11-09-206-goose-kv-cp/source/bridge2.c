// Bridge for the compact Goose KV gather.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_cells;
__attribute__((destructor)) static void fg206d_report(void) {
    if (!atomic_load(&g_cells)) return;
    fprintf(stderr, "FG206D_KV_CP cells=%llu calls=%llu\n", atomic_load(&g_cells), atomic_load(&g_cells));
}
static void g_fail(int64_t c) { fprintf(stderr, "FG206D validation code %lld\n", (long long)c); abort(); }
static int64_t g_ld_i64(uint64_t d, int64_t b) { return *(const int64_t *)(uintptr_t)(d + (uint64_t)b); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

#define fg206d_ld_i64 g_ld_i64
#define fg206d_st_i64 g_st_i64
#define fg206d_fail   g_fail
#define gg_init_globals fg206d_init_globals
#define main fg206d_generated_main
#include "cp2-generated.c"
#undef main

int64_t fg206d_gather(uint64_t dpos, uint64_t dseq, uint64_t spos, uint64_t sseq,
                      uint64_t sshift, int64_t m, int64_t w) {
    atomic_fetch_add(&g_cells, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_1(dpos, dseq, spos, sseq, sshift, m, w, 0);
}
int fg206d_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_CP");
    return !(f && strcmp(f, "0") == 0);
}
