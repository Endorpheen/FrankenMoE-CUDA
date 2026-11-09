// Bridge for the Goose KV cell gather module.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_cells;

__attribute__((destructor)) static void fg206c_report(void) {
    if (!atomic_load(&g_cells)) return;
    fprintf(stderr, "FG206C_KV_CP cells=%llu\n", atomic_load(&g_cells));
}
static void g_fail(int64_t c) { fprintf(stderr, "FG206C validation code %lld\n", (long long)c); abort(); }
static int64_t g_ld_i64(uint64_t d, int64_t b) { return *(const int64_t *)(uintptr_t)(d + (uint64_t)b); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

#define fg206c_ld_i64 g_ld_i64
#define fg206c_st_i64 g_st_i64
#define fg206c_fail   g_fail
#define gg_init_globals fg206c_init_globals
#define main fg206c_generated_main
#include "cp-generated.c"
#undef main

// gg_function_1 is the eight-argument indexed gather; gg_function_6 the contiguous one.
int64_t fg206c_gather_cells(uint64_t dpos, uint64_t dseq, uint64_t spos, uint64_t sseq,
                            uint64_t sshift, uint64_t idxs, int64_t m, int64_t w) {
    atomic_fetch_add(&g_cells, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_1(dpos, dseq, spos, sseq, sshift, idxs, m, w, 0);
}
int64_t fg206c_gather_contig(uint64_t dpos, uint64_t dseq, uint64_t spos, uint64_t sseq,
                             uint64_t sshift, int64_t i, int64_t m, int64_t w) {
    atomic_fetch_add(&g_cells, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_6(dpos, dseq, spos, sseq, sshift, i, m, w, 0);
}
int fg206c_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_CP");
    return !(f && strcmp(f, "0") == 0);
}
