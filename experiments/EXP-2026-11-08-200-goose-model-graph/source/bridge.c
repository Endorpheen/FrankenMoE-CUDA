// Bridge for the Goose Apex graph-decision module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static void g_fail(int64_t c) {
    fprintf(stderr, "FG200G validation code %lld\n", (long long)c);
    abort();
}
static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}
static int64_t g_ld_u32(uint64_t base, int64_t index) {
    return (int64_t) * (const uint32_t *)(uintptr_t)(base + (uint64_t)index * 4);
}

#define fg200g_ld_u32 g_ld_u32
#define fg200g_st_i64 g_st_i64
#define fg200g_fail   g_fail
#define main fg200g_generated_main
#include "graph-generated.c"
#undef main

static _Atomic unsigned long long g_recr, g_recr_y, g_native, g_moe_grp, g_moe_scale;
__attribute__((destructor)) static void fg200g_report(void) {
    if (!atomic_load(&g_recr) && !atomic_load(&g_native)) return;
    fprintf(stderr, "FG200G_MODEL_GRAPH is_recr=%llu recurrent=%llu native=%llu\n",
            atomic_load(&g_recr), atomic_load(&g_recr_y), atomic_load(&g_native));
    fprintf(stderr, "FG200G_MOE group_select=%llu scale_check=%llu\n",
            atomic_load(&g_moe_grp), atomic_load(&g_moe_scale));
}
int64_t fg200g_is_recr(int64_t il, int64_t n_layer_all, uint64_t flags) {
    const int64_t r = gg_function_1(il, n_layer_all, flags, 0);
    if (r >= 0) {
        atomic_fetch_add(&g_recr, 1);
        if (r != 0) atomic_fetch_add(&g_recr_y, 1);
    } else {
        atomic_fetch_add(&g_native, 1);
    }
    return r;
}
int64_t fg200g_crop_before_nextn(int64_t a, int64_t b, int64_t c) { return gg_function_5(a, b, c, 0); }
int64_t fg200g_crop_after_nextn(int64_t a, int64_t b, int64_t c) { return gg_function_6(a, b, c, 0); }
int64_t fg200g_capture_layer_input(int64_t il, int64_t n_layer) { return gg_function_7(il, n_layer, 0); }
int64_t fg200g_shift_skip(int64_t shift) { return gg_function_8(shift, 0); }
int64_t fg200g_gating_kind(int64_t gating_op) { return gg_function_9(gating_op, 0); }
int64_t fg200g_group_select(int64_t g, int64_t ntok) { atomic_fetch_add(&g_moe_grp, 1); return gg_function_10(g, ntok, 0); }
int64_t fg200g_weight_before_ffn(int64_t is4) { return gg_function_11(is4, 0); }
int64_t fg200g_weights_need_scale(int64_t bits) { atomic_fetch_add(&g_moe_scale, 1); return gg_function_12(bits, 0); }
int64_t fg200g_kq_scale_auto(int64_t bits) { return gg_function_13(bits, 0); }
int64_t fg200g_gate_elem_offset(int64_t n) { return gg_function_14(n, 0); }
int64_t fg200g_q_full_width(int64_t n) { return gg_function_15(n, 0); }

// Integration entry points. The switch is opt-out so a new build has the feature on by default.
int fg200g_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MODEL_GRAPH");
    return !(f && strcmp(f, "0") == 0);
}
