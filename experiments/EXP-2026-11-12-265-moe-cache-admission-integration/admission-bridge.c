// Bridge for the MoE cache admission decision (EXP-262): primitives, default-on switch, entry point and a witness.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG262A validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld64(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
static void g_st64(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg262a_fail   g_fail
#define fg262a_ld_i64 g_ld64
#define fg262a_st_i64 g_st64
#define fg262a_run    fg262a_run_impl
#define main fg262a_generated_main
#include "admission-functions.c"
#undef main
#undef fg262a_run

static _Atomic uint64_t g_plans;

__attribute__((destructor)) static void report(void) {
    fprintf(stderr, "FG262A_MOE_CACHE_ADMIT plans=%llu\n", (unsigned long long) atomic_load(&g_plans));
}

uint8_t fg262a_run(int64_t n_layers, uint64_t n_experts, uint64_t device, uint64_t layout, uint64_t nbytes,
                   uint64_t group, uint64_t group_dev, uint64_t group_layout, uint64_t group_bytes,
                   uint64_t group_count, uint64_t n_groups, int64_t max_groups) {
    atomic_fetch_add(&g_plans, 1);
    return fg262a_run_impl(n_layers, n_experts, device, layout, nbytes, group, group_dev, group_layout,
                           group_bytes, group_count, n_groups, max_groups);
}

// Default on, per rule 10: the switch exists only to restore the native group loop.
int fg262a_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MOE_CACHE_ADMIT");
    return !(f && strcmp(f, "0") == 0);
}
