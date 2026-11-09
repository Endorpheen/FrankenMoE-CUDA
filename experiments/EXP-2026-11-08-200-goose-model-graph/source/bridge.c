// Bridge for the Goose Apex graph-decision module. Nothing here is algorithmic: it supplies the
// load/store primitives, exposes the decisions, and counts how often each runs.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_recr, g_recr_y, g_recr_native, g_moe_grp, g_moe_scale, g_lin;

__attribute__((destructor)) static void fg200g_report(void) {
    if (!atomic_load(&g_recr) && !atomic_load(&g_recr_native)) return;
    fprintf(stderr, "FG200G_MODEL_GRAPH is_recr=%llu recurrent=%llu native=%llu\n",
            atomic_load(&g_recr), atomic_load(&g_recr_y), atomic_load(&g_recr_native));
    fprintf(stderr, "FG200G_MOE group_select=%llu scale_check=%llu\n",
            atomic_load(&g_moe_grp), atomic_load(&g_moe_scale));
    fprintf(stderr, "FG200G_RECR geometry=%llu\n", atomic_load(&g_lin));
}

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

// --- decisions -------------------------------------------------------------------------------
int64_t fg200g_is_recr(int64_t il, int64_t n_layer_all, uint64_t flags) {
    const int64_t r = gg_function_1(il, n_layer_all, flags, 0);
    if (r >= 0) {
        atomic_fetch_add(&g_recr, 1);
        if (r != 0) atomic_fetch_add(&g_recr_y, 1);
    } else {
        atomic_fetch_add(&g_recr_native, 1);
    }
    return r;
}
int64_t fg200g_crop_before_nextn(int64_t a, int64_t b, int64_t c) { return gg_function_5(a, b, c, 0); }
int64_t fg200g_crop_after_nextn(int64_t a, int64_t b, int64_t c) { return gg_function_6(a, b, c, 0); }
int64_t fg200g_capture_layer_input(int64_t il, int64_t n_layer) { return gg_function_7(il, n_layer, 0); }
int64_t fg200g_shift_skip(int64_t shift) { return gg_function_8(shift, 0); }
int64_t fg200g_gating_kind(int64_t g) { return gg_function_9(g, 0); }

int64_t fg200g_group_select(int64_t g, int64_t ntok) {
    atomic_fetch_add(&g_moe_grp, 1);
    return gg_function_10(g, ntok, 0);
}
int64_t fg200g_weight_before_ffn(int64_t is4) { return gg_function_11(is4, 0); }
int64_t fg200g_weights_need_scale(int64_t bits) {
    atomic_fetch_add(&g_moe_scale, 1);
    return gg_function_12(bits, 0);
}
int64_t fg200g_kq_scale_auto(int64_t bits) { return gg_function_13(bits, 0); }
int64_t fg200g_gate_elem_offset(int64_t n) { return gg_function_14(n, 0); }
int64_t fg200g_q_full_width(int64_t n) { return gg_function_15(n, 0); }

int64_t fg200g_head_v_dim(int64_t d_inner, int64_t nvh) {
    atomic_fetch_add(&g_lin, 1);
    return gg_function_16(d_inner, nvh, 0);
}
int64_t fg200g_conv_channels(int64_t di, int64_t ng, int64_t ds) {
    atomic_fetch_add(&g_lin, 1);
    return gg_function_17(di, ng, ds, 0);
}
int64_t fg200g_qkv_dim(int64_t hk, int64_t nk, int64_t hv, int64_t nv) {
    atomic_fetch_add(&g_lin, 1);
    return gg_function_18(hk, nk, hv, nv, 0);
}
int64_t fg200g_k_view_offset(int64_t hk, int64_t nk) { return gg_function_19(hk, nk, 0); }
int64_t fg200g_v_view_offset(int64_t hk, int64_t nk) { return gg_function_20(hk, nk, 0); }
int64_t fg200g_need_repeat(int64_t nk, int64_t nv, int64_t ar, int64_t ch) { return gg_function_21(nk, nv, ar, ch, 0); }
int64_t fg200g_repeat_divisible(int64_t nk, int64_t nv) { return gg_function_22(nk, nv, 0); }
int64_t fg200g_final_width(int64_t hv, int64_t nv) { return gg_function_23(hv, nv, 0); }
int64_t fg200g_final_rows(int64_t nt, int64_t ns) { return gg_function_24(nt, ns, 0); }
int64_t fg200g_n_ff_exp(int64_t d, int64_t n_ff, int64_t nu) { return gg_function_25(d, n_ff, nu, 0); }
int64_t fg200g_n_ff_shexp(int64_t d, int64_t n_ff) { return gg_function_26(d, n_ff, 0); }
int64_t fg200g_key_dim(int64_t hk, int64_t nk) { return gg_function_27(hk, nk, 0); }
int64_t fg200g_value_dim(int64_t hv, int64_t nv) { return gg_function_28(hv, nv, 0); }
int64_t fg200g_conv_dim(int64_t kd, int64_t vd) { return gg_function_29(kd, vd, 0); }
int64_t fg200g_wqkv_width(int64_t kd, int64_t vd) { return gg_function_30(kd, vd, 0); }
int64_t fg200g_merged_half(int64_t ne0) { return gg_function_31(ne0, 0); }
int64_t fg200g_up_view_offset(int64_t n_ff) { return gg_function_32(n_ff, 0); }
int64_t fg200g_has_gate(int64_t a, int64_t b) { return gg_function_33(a, b, 0); }
int64_t fg200g_swiglu_limit_used(int64_t bits) { return gg_function_34(bits, 0); }
int64_t fg200g_fused_swiglu(int64_t m, int64_t d, int64_t g, int64_t f, int64_t hc, int64_t hy) {
    return gg_function_35(m, d, g, f, hc, hy, 0);
}

// --- integration switch ----------------------------------------------------------------------
int fg200g_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MODEL_GRAPH");
    return !(f && strcmp(f, "0") == 0);
}
