// Bridge for the server-queue decisions (EXP-270): primitives, default-on switch, wrappers and witness.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG270Q validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static void g_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg270q_fail   g_fail
#define fg270q_ld_i64 g_ld
#define fg270q_st_i64 g_st
#define fg270q_pick_deferred fg270q_pick_deferred_impl
#define fg270q_cleanup_mask  fg270q_cleanup_mask_impl
#define main fg270q_generated_main
#include "queue_decisions-functions.c"
#undef main
#undef fg270q_pick_deferred
#undef fg270q_cleanup_mask

static _Atomic uint64_t g_pick, g_clean;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG270Q_SERVER_QUEUE pick=%llu cleanup=%llu\n",
            (unsigned long long) atomic_load(&g_pick), (unsigned long long) atomic_load(&g_clean));
}
int64_t fg270q_pick_deferred(uint64_t a, int64_t b, int64_t c) {
    atomic_fetch_add(&g_pick, 1); return fg270q_pick_deferred_impl(a, b, c);
}
int64_t fg270q_cleanup_mask(uint64_t a, int64_t b, int64_t c, uint64_t d) {
    atomic_fetch_add(&g_clean, 1); return fg270q_cleanup_mask_impl(a, b, c, d);
}
int fg270q_enabled(void) {
    const char * f = getenv("GGML_GOOSE_SERVER_QUEUE");
    return !(f && strcmp(f, "0") == 0);
}
