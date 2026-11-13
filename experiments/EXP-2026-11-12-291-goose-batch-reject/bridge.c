#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG291B validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static void g_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg291b_fail   g_fail
#define fg291b_ld_i64 g_ld
#define fg291b_ld_u64 g_ld
#define fg291b_st_i64 g_st
#include "batch_reject-functions.c"

static _Atomic uint64_t g_cont, g_walk, g_mrope, g_refused;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG291B_BATCH_REJECT continuity=%llu walk=%llu mrope=%llu refused=%llu\n",
            (unsigned long long) atomic_load(&g_cont), (unsigned long long) atomic_load(&g_walk),
            (unsigned long long) atomic_load(&g_mrope), (unsigned long long) atomic_load(&g_refused));
}
int64_t fg291b_check_continuity(int64_t n, uint64_t pos, uint64_t mn, uint64_t mx, uint64_t cnt, int64_t has_mem,
                                uint64_t mem, int64_t n_seq) {
    atomic_fetch_add(&g_cont, 1);
    const int64_t r = fg291b_check_continuity_impl(n, pos, mn, mx, cnt, has_mem, mem, n_seq);
    if (r) atomic_fetch_add(&g_refused, 1);
    return r;
}
int64_t fg291b_check_walk(int64_t n, uint64_t pos, uint64_t mask, int64_t n_seq, uint64_t cs, uint64_t cp) {
    atomic_fetch_add(&g_walk, 1);
    const int64_t r = fg291b_check_walk_impl(n, pos, mask, n_seq, cs, cp);
    if (r) atomic_fetch_add(&g_refused, 1);
    return r;
}
int64_t fg291b_check_mrope(int64_t n, uint64_t mask, uint64_t embd, int64_t mixed, int64_t has_token, int64_t n_seq,
                           uint64_t mn, uint64_t cnt, int64_t has_mem, uint64_t mem, uint64_t sf, uint64_t ss) {
    atomic_fetch_add(&g_mrope, 1);
    const int64_t r = fg291b_check_mrope_impl(n, mask, embd, mixed, has_token, n_seq, mn, cnt, has_mem, mem, sf, ss);
    if (r) atomic_fetch_add(&g_refused, 1);
    return r;
}
int fg291b_enabled(void) {
    const char * f = getenv("GGML_GOOSE_BATCH_REJECT");
    return !(f && strcmp(f, "0") == 0);
}
