// Bridge for the Goose KV cell-ownership module: supplies the arena load/store primitives and
// exposes the eight operations. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls, g_cleared, g_native, g_purge, g_head, g_slot, g_swa, g_posin, g_rmhead, g_cp, g_shift;

static void g_fail(int64_t c) {
    fprintf(stderr, "FG199K validation code %lld\n", (long long)c);
    abort();
}
static int64_t g_ld_i64(uint64_t base, int64_t byte) {
    return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte);
}
static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}

#define fg199k_ld_i64 g_ld_i64
#define fg199k_st_i64 g_st_i64
#define fg199k_fail   g_fail
#define main fg199k_generated_main
#include "cells-generated.c"
#undef main

// Function numbering is read off the generated file by arity; it shifts whenever the Goose
// source changes, so it is re-checked on every rebuild rather than assumed.
int fg199k_is_empty(int64_t n, int64_t pos) { return gg_function_1(n, pos, 0) != 0; }
void fg199k_rm(uint64_t a, int64_t n, int64_t i) { gg_function_4(a, n, i, 0); }
int64_t fg199k_seq_rm(uint64_t a, int64_t n, int64_t i, int64_t seq_id, int64_t widx, int64_t bit) {
    return gg_function_12(a, n, i, seq_id, widx, bit, 0);
}
int64_t fg199k_seq_keep(uint64_t a, int64_t n, int64_t i, int64_t seq_id, int64_t widx, int64_t bit) {
    return gg_function_14(a, n, i, seq_id, widx, bit, 0);
}
void fg199k_seq_add(uint64_t a, int64_t n, int64_t i, int64_t widx, int64_t bit) {
    gg_function_15(a, n, i, widx, bit, 0);
}
void fg199k_pos_set(uint64_t a, int64_t n, int64_t i, int64_t pos) { gg_function_16(a, n, i, pos, 0); }
int64_t fg199k_seq_count(uint64_t a, int64_t n, int64_t i) { return gg_function_17(a, n, i, 0); }
int64_t fg199k_used_max_p1(uint64_t a, int64_t n) { return gg_function_18(a, n, 0); }
int64_t fg199k_place_cell(uint64_t a, int64_t n, int64_t idx, int64_t pos, int64_t seq_n, uint64_t seqs) {
    return gg_function_20(a, n, idx, pos, seq_n, seqs, 0);
}
int64_t fg199k_head_after_place(int64_t last_idx) { return gg_function_21(last_idx, 0); }

// Integration entry point: answers apply_ubatch's question and reports whether the Goose path ran.
#include <stdbool.h>
int fg199k_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_CELLS");
    return !(f && strcmp(f, "0") == 0);
}
int64_t fg199k_should_purge_c(int64_t min_pos, int64_t max_rm) {
    const int64_t r = gg_function_23(min_pos, max_rm, 0);
    atomic_fetch_add(&g_purge, 1);
    return r;
}
int64_t fg199k_should_purge(int64_t min_pos, int64_t max_rm) { return fg199k_should_purge_c(min_pos, max_rm); }
int64_t fg199k_seq_add_skip(int64_t shift, int64_t p0, int64_t p1) {
    atomic_fetch_add(&g_shift, 1);
    return gg_function_33(shift, p0, p1, 0);
}
int64_t fg199k_pos_add(int64_t pos, int64_t shift, int64_t d, uint64_t out) {
    return gg_function_34(pos, shift, d, out, 0);
}
void fg199k_pos_div(int64_t pos, int64_t shift, int64_t d, uint64_t out) {
    gg_function_35(pos, shift, d, out, 0);
}
int64_t fg199k_seq_add_head(int64_t freed, int64_t new_head) { return gg_function_36(freed, new_head, 0); }
int64_t fg199k_seq_div_skip(int64_t d, int64_t p0, int64_t p1) { return gg_function_37(d, p0, p1, 0); }
int64_t fg199k_same_stream(int64_t s0, int64_t s1) {
    atomic_fetch_add(&g_cp, 1);
    return gg_function_30(s0, s1, 0);
}
int64_t fg199k_seq_cp_self(int64_t src, int64_t dst) { return gg_function_31(src, dst, 0); }
int64_t fg199k_seq_cp_is_full(int64_t p0, int64_t p1, int64_t size) { return gg_function_32(p0, p1, size, 0); }
int64_t fg199k_pos_in(int64_t pos, int64_t p0, int64_t p1) {
    atomic_fetch_add(&g_posin, 1);
    return gg_function_27(pos, p0, p1, 0);
}
int64_t fg199k_norm_range(int64_t p0, int64_t p1, uint64_t out) { return gg_function_28(p0, p1, out, 0); }
int64_t fg199k_head_after_rm(int64_t freed, int64_t new_head, int64_t head) {
    atomic_fetch_add(&g_rmhead, 1);
    return gg_function_29(freed, new_head, head, 0);
}
int64_t fg199k_head_start(int64_t head_cur, int64_t used, int64_t n_tokens) {
    atomic_fetch_add(&g_slot, 1);
    return gg_function_24(head_cur, used, n_tokens, 0);
}
int64_t fg199k_can_use(int64_t occupied, int64_t seq_count, int64_t pos_cell,
                       int64_t seq_pos_max_1, int64_t n_swa, int64_t swa_type) {
    atomic_fetch_add(&g_swa, 1);
    return gg_function_26(occupied, seq_count, pos_cell, seq_pos_max_1, n_swa, swa_type, 0);
}
int64_t fg199k_head_after(int64_t last_idx) {
    atomic_fetch_add(&g_head, 1);
    return gg_function_21(last_idx, 0);
}

__attribute__((destructor)) static void fg199k_report(void) {
    if (!atomic_load(&g_calls) && !atomic_load(&g_native)) return;
    fprintf(stderr, "FG199K_KV_CELLS goose=%llu cleared=%llu native=%llu purge=%llu head=%llu\n",
            atomic_load(&g_calls), atomic_load(&g_cleared), atomic_load(&g_native),
            atomic_load(&g_purge), atomic_load(&g_head));
    fprintf(stderr, "FG199K_KV_SLOT head_start=%llu can_use=%llu\n",
            atomic_load(&g_slot), atomic_load(&g_swa));
    fprintf(stderr, "FG199K_KV_SEQRM pos_in=%llu head_after_rm=%llu\n",
            atomic_load(&g_posin), atomic_load(&g_rmhead));
    fprintf(stderr, "FG199K_KV_SEQCP cp_decisions=%llu\n", atomic_load(&g_cp));
    fprintf(stderr, "FG199K_KV_SHIFT add=%llu div=%llu\n", atomic_load(&g_shift), 0ULL);
}
int fg199k_plan_cell(int enabled, int occupied, int64_t pos, bool * cleared) {
    if (!enabled) { atomic_fetch_add(&g_native, 1); return 0; }
    int64_t r[2] = {0, 0};
    gg_function_22(occupied, pos, (uint64_t)(uintptr_t)r, 0);
    *cleared = r[0] != 0;
    atomic_fetch_add(&g_calls, 1);
    if (*cleared) atomic_fetch_add(&g_cleared, 1);
    return 1;
}
