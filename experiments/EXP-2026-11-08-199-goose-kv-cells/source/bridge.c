// Bridge for the Goose KV cell-ownership module: supplies the arena load/store primitives and
// exposes the eight operations. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
