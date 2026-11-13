// Bridge for the MoE cache copy decisions (EXP-263): primitives, the default-on switch, the four entry points, and a
// call witness so a run can show whether the decisions were executed.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG263C validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld32(uint64_t b, int64_t i) { int32_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 4); return v; }
static void g_st64(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg263c_fail   g_fail
#define fg263c_ld_i32 g_ld32
#define fg263c_st_i64 g_st64
#define fg263c_run_from_slot  fg263c_run_from_slot_impl
#define fg263c_run_from_fills fg263c_run_from_fills_impl
#define fg263c_run_from_pairs fg263c_run_from_pairs_impl
#define fg263c_stats_class    fg263c_stats_class_impl
#define fg263c_fill_bytes     fg263c_fill_bytes_impl
#define main fg263c_generated_main
#include "copy_decisions-functions.c"
#undef main
#undef fg263c_run_from_slot
#undef fg263c_run_from_fills
#undef fg263c_run_from_pairs
#undef fg263c_stats_class
#undef fg263c_fill_bytes

static _Atomic uint64_t g_runs, g_pairs, g_stats;

__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG263C_MOE_CACHE_COPY runs=%llu pairs=%llu stats=%llu\n",
            (unsigned long long) atomic_load(&g_runs),
            (unsigned long long) atomic_load(&g_pairs),
            (unsigned long long) atomic_load(&g_stats));
}

int64_t fg263c_run_from_slot(uint64_t slots, int64_t e, int64_t last) {
    atomic_fetch_add(&g_runs, 1);
    return fg263c_run_from_slot_impl(slots, e, last);
}
int64_t fg263c_run_from_pairs(uint64_t pairs, int64_t i, int64_t n) {
    atomic_fetch_add(&g_pairs, 1);
    return fg263c_run_from_pairs_impl(pairs, i, n);
}
int64_t fg263c_stats_class(int64_t n_ids, int64_t n_used) {
    atomic_fetch_add(&g_stats, 1);
    return fg263c_stats_class_impl(n_ids, n_used);
}

// Default on, per rule 10: the switch exists only to restore the native loops.
int fg263c_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MOE_CACHE_COPY");
    return !(f && strcmp(f, "0") == 0);
}
