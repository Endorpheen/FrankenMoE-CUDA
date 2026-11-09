// Bridge for the Goose model-loader decision module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_shape, g_native, g_mode, g_key, g_merge, g_sub, g_sort;

__attribute__((destructor)) static void fg201l_report(void) {
    if (!atomic_load(&g_shape) && !atomic_load(&g_native)) return;
    fprintf(stderr, "FG201L_MODEL_LOADER shape_checks=%llu direct=%llu\n",
            atomic_load(&g_shape), atomic_load(&g_native));
    fprintf(stderr, "FG201L_KEYS load_mode=%llu or_arr=%llu\n",
            atomic_load(&g_mode), atomic_load(&g_key));
    fprintf(stderr, "FG201L_RANGES merge=%llu sub=%llu sort=%llu\n",
            atomic_load(&g_merge), atomic_load(&g_sub), atomic_load(&g_sort));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG201L validation code %lld\n", (long long)c);
    abort();
}
static int64_t g_ld_ne(uint64_t desc, int64_t i) {
    return *(const int64_t *)(uintptr_t)(desc + (uint64_t)i * 8);
}
static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}

#define fg201l_ld_ne  g_ld_ne
#define fg201l_st_i64 g_st_i64
#define fg201l_fail   g_fail
#define main fg201l_generated_main
#include "loader-generated.c"
#undef main

// Bounded arrays make the ordering Goose's too; the caller owns the pair buffer.
int64_t fg201l_sort_pairs(uint64_t base, int64_t n) {
    atomic_fetch_add(&g_sort, 1);
    return gg_function_24(base, n, 0);
}
int64_t fg201l_nelements(uint64_t desc, int64_t n) { return gg_function_1(desc, n, 0); }
int64_t fg201l_dims_ok(uint64_t desc, int64_t nreq, int64_t r0, int64_t r1, int64_t r2, int64_t r3) {
    return gg_function_5(desc, nreq, r0, r1, r2, r3, 0);
}
int64_t fg201l_or_arr_too_many(int64_t n, int64_t n_max) { atomic_fetch_add(&g_key, 1); return gg_function_7(n, n_max, 0); }
int64_t fg201l_is_array_kv(int64_t kv_type, int64_t array_type) { return gg_function_8(kv_type, array_type, 0); }
int64_t fg201l_arr_length_ok(int64_t n, int64_t length) { return gg_function_9(n, length, 0); }
int64_t fg201l_use_mmap_mode(int64_t load_mode) { atomic_fetch_add(&g_mode, 1); return gg_function_10(load_mode, 0); }
int64_t fg201l_use_direct_io_mode(int64_t load_mode) { return gg_function_11(load_mode, 0); }
int64_t fg201l_merge_touches(int64_t a, int64_t b) { return gg_function_12(a, b, 0); }
int64_t fg201l_merged_last(int64_t a, int64_t b) { return gg_function_13(a, b, 0); }
int64_t fg201l_sub_skip(int64_t a, int64_t b, int64_t c, int64_t d) { return gg_function_14(a, b, c, d, 0); }
int64_t fg201l_sub_emit_gap(int64_t a, int64_t b) { return gg_function_15(a, b, 0); }
int64_t fg201l_sub_advance(int64_t a, int64_t b) { return gg_function_16(a, b, 0); }
int64_t fg201l_sub_emit_tail(int64_t a, int64_t b) { return gg_function_17(a, b, 0); }
int64_t fg201l_merge_ranges(uint64_t src, int64_t n, uint64_t dst, int64_t cap) {
    atomic_fetch_add(&g_merge, 1);
    return gg_function_18(src, n, dst, cap, 0);
}
int64_t fg201l_sub_ranges(uint64_t a, int64_t na, uint64_t sub, int64_t ns, uint64_t dst, int64_t cap) {
    atomic_fetch_add(&g_sub, 1);
    return gg_function_23(a, na, sub, ns, dst, cap, 0);
}
int64_t fg201l_shape_ok(int64_t allow_reshape, uint64_t desc, int64_t nreq, int64_t total,
                        int64_t r0, int64_t r1, int64_t r2, int64_t r3) {
    return gg_function_6(allow_reshape, desc, nreq, total, r0, r1, r2, r3, 0);
}

// Integration entry point. Records the decision when the caller routes one through Goose.
int fg201l_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MODEL_LOADER");
    return !(f && strcmp(f, "0") == 0);
}
int fg201l_shape_check(int allow_reshape, const int64_t * dims, int64_t nreq, int64_t total,
                       const int64_t * req) {
    if (!fg201l_enabled()) { atomic_fetch_add(&g_native, 1); return -1; }
    const int64_t r = fg201l_shape_ok(allow_reshape, (uint64_t)(uintptr_t)dims, nreq, total,
                                      req[0], req[1], req[2], req[3]);
    atomic_fetch_add(&g_shape, 1);
    return (int)r;
}
