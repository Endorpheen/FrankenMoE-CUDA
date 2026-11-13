// Bridge for the slot-selection decisions (EXP-272): primitives, default-on switch, wrappers, witness.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG272S validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static void g_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }
static float g_from_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

#define fg272s_fail      g_fail
#define fg272s_ld_i64    g_ld
#define fg272s_st_i64    g_st
#define fg272s_from_bits g_from_bits
#define fg272s_pick_similar fg272s_pick_similar_impl
#define fg272s_pick_lru     fg272s_pick_lru_impl
#define main fg272s_generated_main
#include "slot_selection-functions.c"
#undef main
#undef fg272s_pick_similar
#undef fg272s_pick_lru

static _Atomic uint64_t g_sim, g_lru;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG272S_SLOT_SELECT similar=%llu lru=%llu\n",
            (unsigned long long) atomic_load(&g_sim), (unsigned long long) atomic_load(&g_lru));
}
int64_t fg272s_pick_similar(uint64_t a, uint64_t b, int64_t c, int64_t d, int64_t e) {
    atomic_fetch_add(&g_sim, 1); return fg272s_pick_similar_impl(a, b, c, d, e);
}
int64_t fg272s_pick_lru(uint64_t a, uint64_t b, uint64_t c, int64_t d) {
    atomic_fetch_add(&g_lru, 1); return fg272s_pick_lru_impl(a, b, c, d);
}
int fg272s_enabled(void) {
    const char * f = getenv("GGML_GOOSE_SLOT_SELECT");
    return !(f && strcmp(f, "0") == 0);
}
