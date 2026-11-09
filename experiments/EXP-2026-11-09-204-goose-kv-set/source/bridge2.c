// Bridge for the Goose KV cell-copy module, real container shape.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_cells;

__attribute__((destructor)) static void fg204w_report(void) {
    if (!atomic_load(&g_cells)) return;
    fprintf(stderr, "FG204W_KV_SET cells=%llu\n", atomic_load(&g_cells));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG204W validation code %lld\n", (long long)c);
    abort();
}
static int64_t g_ld_i64(uint64_t d, int64_t b) { return *(const int64_t *)(uintptr_t)(d + (uint64_t)b); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

#define fg204w_ld_i64 g_ld_i64
#define fg204w_st_i64 g_st_i64
#define fg204w_fail   g_fail
#define main fg204w_generated_main
#include "set2-generated.c"
#undef main

// gg_function_1 is the eight-argument indexed form; gg_function_7 is the ten-argument contiguous
// form. Both are unique arities in this module.
int64_t fg204w_set_indexed(uint64_t dpos, uint64_t dseq, uint64_t dused,
                           uint64_t spos, uint64_t sseq,
                           uint64_t idxs, int64_t m, int64_t w) {
    atomic_fetch_add(&g_cells, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_1(dpos, dseq, dused, spos, sseq, idxs, m, w, 0);
}

int64_t fg204w_set_contig(uint64_t dpos, uint64_t dseq, uint64_t dused, int64_t dn, int64_t di,
                          uint64_t spos, uint64_t sseq, int64_t m, int64_t w) {
    atomic_fetch_add(&g_cells, m > 0 ? (unsigned long long)m : 1ULL);
    return gg_function_7(dpos, dseq, dused, dn, di, spos, sseq, m, w, 0);
}

int fg204w_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_SET");
    return !(f && strcmp(f, "0") == 0);
}
