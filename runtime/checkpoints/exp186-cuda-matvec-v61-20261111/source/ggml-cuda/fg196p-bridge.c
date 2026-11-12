// Bridge for the Goose pinned-memory policy module: supplies the store/fail primitives,
// exposes the six decision entry points and counts how often each is taken. Nothing here is
// algorithmic -- the decisions live in host.goose.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}
static void g_fail(int64_t c) {
    fprintf(stderr, "FG196P validation code %lld\n", (long long)c);
    abort();
}

#define fg196p_st_i64 g_st_i64
#define fg196p_fail   g_fail
#define main fg196p_generated_main
#include "fg196p-host-generated.c"
#undef main

static _Atomic uint64_t g_alloc_plan, g_alloc_fail, g_fallback, g_reg_plan, g_reg_fail, g_unreg, g_native;

__attribute__((destructor)) static void fg196p_report(void) {
    fprintf(stderr,
            "FG196P_CUDA_PINNED host_plan=%llu host_fail=%llu fallback=%llu "
            "reg_plan=%llu reg_fail=%llu unreg=%llu native=%llu\n",
            (unsigned long long)atomic_load(&g_alloc_plan), (unsigned long long)atomic_load(&g_alloc_fail),
            (unsigned long long)atomic_load(&g_fallback), (unsigned long long)atomic_load(&g_reg_plan),
            (unsigned long long)atomic_load(&g_reg_fail), (unsigned long long)atomic_load(&g_unreg),
            (unsigned long long)atomic_load(&g_native));
}

// Rollback switch: GGML_GOOSE_CUDA_PINNED=0 selects the native decision code that the core
// file keeps inline. Any other value, including unset, uses the Goose path.
int fg196p_enabled(void) {
    const char * f = getenv("GGML_GOOSE_CUDA_PINNED");
    return !(f && strcmp(f, "0") == 0);
}
// Called from the core file's native branch so the report shows which path ran.
void fg196p_note_native(void) { atomic_fetch_add(&g_native, 1); }

void fg196p_plan_host_alloc(int64_t disabled, int64_t size, uint64_t out) {
    gg_function_1(disabled, size, out, 0);
    atomic_fetch_add(&g_alloc_plan, 1);
}
void fg196p_plan_host_alloc_result(int64_t err, uint64_t out) {
    gg_function_9(err, out, 0);
    if (err != 0) atomic_fetch_add(&g_alloc_fail, 1);
}
void fg196p_plan_fallback(int64_t ptr_zero, uint64_t out) {
    gg_function_7(ptr_zero, out, 0);
    if (ptr_zero) atomic_fetch_add(&g_fallback, 1);
}
void fg196p_plan_register(int64_t enabled, int64_t size, uint64_t out) {
    gg_function_8(enabled, size, out, 0);
    atomic_fetch_add(&g_reg_plan, 1);
}
void fg196p_plan_register_result(int64_t err, uint64_t out) {
    gg_function_9(err, out, 0);
    if (err != 0) atomic_fetch_add(&g_reg_fail, 1);
}
void fg196p_plan_unregister(int64_t enabled, int64_t err, uint64_t out) {
    gg_function_10(enabled, err, out, 0);
    atomic_fetch_add(&g_unreg, 1);
}
