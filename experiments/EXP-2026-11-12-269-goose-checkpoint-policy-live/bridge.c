// Bridge for the context-checkpoint policy (EXP-268): primitives, default-on switch, entry points, witness.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG268P validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static void g_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg268p_fail   g_fail
#define fg268p_ld_i64 g_ld
#define fg268p_st_i64 g_st
#define main fg268p_generated_main
#include "checkpoint_policy-functions.c"
#undef main

static _Atomic uint64_t g_near, g_room, g_super;
__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG268P_CTX_CHECKPOINTS near=%llu room=%llu supersede=%llu\n",
            (unsigned long long) atomic_load(&g_near), (unsigned long long) atomic_load(&g_room),
            (unsigned long long) atomic_load(&g_super));
}
int64_t fg268p_near_evict(uint64_t a, uint64_t b, int64_t c, int64_t d, int64_t e, int64_t f, uint64_t g) {
    atomic_fetch_add(&g_near, 1); return fg268p_near_evict_impl(a, b, c, d, e, f, g);
}
int64_t fg268p_make_room(int64_t a, int64_t b) { atomic_fetch_add(&g_room, 1); return fg268p_make_room_impl(a, b); }
int64_t fg268p_supersede(uint64_t a, int64_t b, int64_t c, uint64_t d) {
    atomic_fetch_add(&g_super, 1); return fg268p_supersede_impl(a, b, c, d);
}
int fg268p_enabled(void) {
    const char * f = getenv("GGML_GOOSE_CTX_CHECKPOINTS");
    return !(f && strcmp(f, "0") == 0);
}
