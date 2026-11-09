// C side: wraps the generated module and exposes it with a predicate callback.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls, g_tested;
typedef int64_t (*fg_pred_fn)(int64_t);
static fg_pred_fn g_pred;

__attribute__((destructor)) static void fg205g_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG205G_KV_FIND_SLOTS calls=%llu tested=%llu\n",
            atomic_load(&g_calls), atomic_load(&g_tested));
}

static void g_fail(int64_t c) { fprintf(stderr, "FG205G validation code %lld\n", (long long)c); abort(); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }
static int64_t g_can_use(int64_t idx) {
    atomic_fetch_add(&g_tested, 1);
    return g_pred ? g_pred(idx) : 0;
}

#define fg205g_fail    g_fail
#define fg205g_st_i64  g_st_i64
#define fg205g_can_use g_can_use
#define gg_init_globals fg205g_init_globals
#define main fg205g_generated_main
#include "../source/find_slot2-generated.c"
#undef main

int64_t fg205g_run(int64_t size, int64_t head0, int64_t n_tokens, int64_t cont,
                   int64_t * out, fg_pred_fn pred) {
    g_pred = pred;
    atomic_fetch_add(&g_calls, 1);
    int64_t r = gg_function_4(size, head0, n_tokens, cont, (uint64_t)(uintptr_t) out, 0);
    return r;
}
