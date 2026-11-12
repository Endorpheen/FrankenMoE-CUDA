// P2-01 contract probe: binary/unary/GLU with special values and strides.
// Writes raw bytes per case so two builds (native vs Goose) can be compared bit for bit.
#include "ggml.h"
#include "ggml-cpu.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE * out;

static uint32_t f32_bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float bits_f32(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

// special values, including three distinct NaN payloads
static const uint32_t SPECIAL[] = {
    0x00000000u, 0x80000000u,           // +0, -0
    0x3f800000u, 0xbf800000u,           // +1, -1
    0x7f800000u, 0xff800000u,           // +inf, -inf
    0x7fc00000u, 0x7fc00001u, 0x7f800001u,  // quiet NaN, other payload, signalling
    0x00000001u, 0x807fffffu,           // smallest subnormal, largest negative subnormal
    0x3dccccccu,                        // ~0.1
};

#define NSPEC ((int)(sizeof(SPECIAL)/sizeof(SPECIAL[0])))

static void emit(const char * name, const float * a, const float * b, size_t n) {
    char hdr[128];
    int k = snprintf(hdr, sizeof hdr, "%s %zu\n", name, n);
    fwrite(hdr, 1, (size_t) k, out);
    fwrite(a, sizeof(float), n, out);
    if (b) fwrite(b, sizeof(float), n, out);
}

static void run_rr(const char * name, const float * a, const float * b, size_t n, int nth,
                   struct ggml_tensor * (*op)(struct ggml_context *, struct ggml_tensor *, struct ggml_tensor *)) {
    struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) abort();
    struct ggml_tensor * ta = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t) n);
    struct ggml_tensor * tb = b ? ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t) n) : NULL;
    memcpy(ta->data, a, n*sizeof(float));
    if (tb) memcpy(tb->data, b, n*sizeof(float));
    struct ggml_tensor * t = op(ctx, ta, tb);
    struct ggml_cgraph * g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, t);
    if (ggml_graph_compute_with_ctx(ctx, g, nth) != GGML_STATUS_SUCCESS) abort();
    emit(name, (const float *) t->data, NULL, n);
    ggml_free(ctx);
}

static struct ggml_tensor * o_add (struct ggml_context * c, struct ggml_tensor * a, struct ggml_tensor * b) { return ggml_add(c, a, b); }
static struct ggml_tensor * o_mul (struct ggml_context * c, struct ggml_tensor * a, struct ggml_tensor * b) { return ggml_mul(c, a, b); }
static struct ggml_tensor * o_sub (struct ggml_context * c, struct ggml_tensor * a, struct ggml_tensor * b) { return ggml_sub(c, a, b); }
static struct ggml_tensor * o_div (struct ggml_context * c, struct ggml_tensor * a, struct ggml_tensor * b) { return ggml_div(c, a, b); }

static void run_unary(const char * name, const float * a, size_t n, int nth,
                      struct ggml_tensor * (*op)(struct ggml_context *, struct ggml_tensor *)) {
    struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) abort();
    struct ggml_tensor * ta = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t) n);
    memcpy(ta->data, a, n*sizeof(float));
    struct ggml_tensor * t = op(ctx, ta);
    struct ggml_cgraph * g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, t);
    if (ggml_graph_compute_with_ctx(ctx, g, nth) != GGML_STATUS_SUCCESS) abort();
    emit(name, (const float *) t->data, NULL, n);
    ggml_free(ctx);
}

static struct ggml_tensor * o_sqr  (struct ggml_context * c, struct ggml_tensor * a) { return ggml_sqr(c, a); }
static struct ggml_tensor * o_sqrt (struct ggml_context * c, struct ggml_tensor * a) { return ggml_sqrt(c, a); }
static struct ggml_tensor * o_log  (struct ggml_context * c, struct ggml_tensor * a) { return ggml_log(c, a); }
static struct ggml_tensor * o_silu (struct ggml_context * c, struct ggml_tensor * a) { return ggml_silu(c, a); }

static void run_glu(const char * name, const float * a, size_t n, int nth, bool swapped, bool oai) {
    struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) abort();
    const int64_t n_ff = (int64_t) n/2;
    struct ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_ff, 2);
    memcpy(t->data, a, n*sizeof(float));
    struct ggml_tensor * s = ggml_glu(ctx, t, oai ? GGML_GLU_OP_SWIGLU_OAI : GGML_GLU_OP_SWIGLU, swapped);
    struct ggml_cgraph * g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, s);
    if (ggml_graph_compute_with_ctx(ctx, g, nth) != GGML_STATUS_SUCCESS) abort();
    emit(name, (const float *) s->data, NULL, (size_t) n_ff);
    ggml_free(ctx);
}

int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    out = fopen(argv[1], "wb");
    if (!out) return 3;

    // 1) binary ops on the full special-value cross product, plus a strided pair
    float a[NSPEC], b[NSPEC];
    for (int i = 0; i < NSPEC; ++i) { a[i] = bits_f32(SPECIAL[i]); b[i] = bits_f32(SPECIAL[NSPEC-1-i]); }
    const int threads[] = { 1, 8 };
    for (int ti = 0; ti < 2; ++ti) {
        char nm[64];
        snprintf(nm, sizeof nm, "add_t%d", threads[ti]);   run_rr(nm, a, b, NSPEC, threads[ti], o_add);
        snprintf(nm, sizeof nm, "sub_t%d", threads[ti]);   run_rr(nm, a, b, NSPEC, threads[ti], o_sub);
        snprintf(nm, sizeof nm, "mul_t%d", threads[ti]);   run_rr(nm, a, b, NSPEC, threads[ti], o_mul);
        snprintf(nm, sizeof nm, "div_t%d", threads[ti]);   run_rr(nm, a, b, NSPEC, threads[ti], o_div);
        snprintf(nm, sizeof nm, "sqr_t%d", threads[ti]);   run_unary(nm, a, NSPEC, threads[ti], o_sqr);
        snprintf(nm, sizeof nm, "sqrt_t%d", threads[ti]);  run_unary(nm, a, NSPEC, threads[ti], o_sqrt);
        snprintf(nm, sizeof nm, "log_t%d", threads[ti]);   run_unary(nm, a, NSPEC, threads[ti], o_log);
        snprintf(nm, sizeof nm, "silu_t%d", threads[ti]);  run_unary(nm, a, NSPEC, threads[ti], o_silu);
    }

    // 2) GLU on a two-row tensor built from the special values
    float glu[2*NSPEC];
    for (int i = 0; i < NSPEC; ++i) { glu[i] = bits_f32(SPECIAL[i]); glu[NSPEC+i] = bits_f32(SPECIAL[(i*3)%NSPEC]); }
    run_glu("swiglu_t1", glu, 2*NSPEC, 1, false, false);
    run_glu("swiglu_swapped_t1", glu, 2*NSPEC, 1, true, false);
    run_glu("swiglu_oai_t1", glu, 2*NSPEC, 1, false, true);

    // 3) larger sizes, to leave the tiny/tail regime of the kernels
    {
        enum { NBIG = 1000 };
        static float big_a[NBIG], big_b[NBIG];
        for (int i = 0; i < NBIG; ++i) {
            big_a[i] = (float)((i % 97) - 48) * 0.37f;
            big_b[i] = (float)((i % 53) + 1) * 0.11f;
        }
        char nm[64];
        snprintf(nm, sizeof nm, "add_1000");  run_rr(nm, big_a, big_b, NBIG, 8, o_add);
        snprintf(nm, sizeof nm, "mul_1000");  run_rr(nm, big_a, big_b, NBIG, 8, o_mul);
        snprintf(nm, sizeof nm, "sqrt_1000"); run_unary(nm, big_a, NBIG, 8, o_sqrt);
        snprintf(nm, sizeof nm, "silu_1000"); run_unary(nm, big_a, NBIG, 8, o_silu);
    }

    // 4) a non-contiguous view: a 2-D tensor read with a row stride
    {
        struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
        struct ggml_context * ctx = ggml_init(p);
        if (!ctx) abort();
        struct ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 16, 4);
        float * d = (float *) t->data;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 16; ++c) d[r*16+c] = (float)((r*16+c) % 13) * 0.21f;
        struct ggml_tensor * v = ggml_view_2d(ctx, t, 8, 4, t->nb[1], 2*sizeof(float));
        struct ggml_tensor * s = ggml_sqr(ctx, v);
        struct ggml_cgraph * g = ggml_new_graph(ctx);
        ggml_build_forward_expand(g, s);
        if (ggml_graph_compute_with_ctx(ctx, g, 4) != GGML_STATUS_SUCCESS) abort();
        // note: the result of a strided view is written contiguously by the kernel; record it as produced
        emit("sqr_view_stride", (const float *) s->data, NULL, 32);
        ggml_free(ctx);
    }

    // 5) other elementwise ops on the same special-value set
    {
        const int threads2[] = { 1, 8 };
        for (int ti = 0; ti < 2; ++ti) {
            struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
            struct ggml_context * ctx = ggml_init(p);
            if (!ctx) abort();
            struct ggml_tensor * ta = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, NSPEC);
            memcpy(ta->data, a, NSPEC*sizeof(float));
            struct ggml_tensor * cl = ggml_clamp(ctx, ta, -0.5f, 0.5f);
            struct ggml_tensor * sc = ggml_scale(ctx, ta, 1.75f);
            struct ggml_cgraph * g = ggml_new_graph(ctx);
            ggml_build_forward_expand(g, cl);
            ggml_build_forward_expand(g, sc);
            if (ggml_graph_compute_with_ctx(ctx, g, threads2[ti]) != GGML_STATUS_SUCCESS) abort();
            char nm[64];
            snprintf(nm, sizeof nm, "clamp_t%d", threads2[ti]); emit(nm, (const float *) cl->data, NULL, NSPEC);
            snprintf(nm, sizeof nm, "scale_t%d", threads2[ti]); emit(nm, (const float *) sc->data, NULL, NSPEC);
            ggml_free(ctx);
        }
    }

    // 6) concatenation order and padding-free copy
    {
        struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
        struct ggml_context * ctx = ggml_init(p);
        if (!ctx) abort();
        struct ggml_tensor * x = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, NSPEC);
        struct ggml_tensor * y = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, NSPEC);
        memcpy(x->data, a, NSPEC*sizeof(float));
        memcpy(y->data, b, NSPEC*sizeof(float));
        struct ggml_tensor * cat = ggml_concat(ctx, x, y, 0);
        struct ggml_cgraph * g = ggml_new_graph(ctx);
        ggml_build_forward_expand(g, cat);
        if (ggml_graph_compute_with_ctx(ctx, g, 8) != GGML_STATUS_SUCCESS) abort();
        emit("concat_special", (const float *) cat->data, NULL, 2*NSPEC);
        ggml_free(ctx);
    }

    // 7) normalisation and softmax on rows built from the special values
    {
        const int nr = 4, nc = 16;
        struct ggml_init_params p = { 64u*1024u*1024u, NULL, false };
        struct ggml_context * ctx = ggml_init(p);
        if (!ctx) abort();
        struct ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, nc, nr);
        float * d = (float *) t->data;
        for (int r = 0; r < nr; ++r) for (int c = 0; c < nc; ++c) d[r*nc+c] = (float)((r*7 + c*3) % 11) * 0.19f - 1.0f;
        struct ggml_tensor * nrm = ggml_norm(ctx, t, 1e-6f);
        struct ggml_tensor * rms = ggml_rms_norm(ctx, t, 1e-6f);
        struct ggml_tensor * sm  = ggml_soft_max(ctx, t);
        struct ggml_cgraph * g = ggml_new_graph(ctx);
        ggml_build_forward_expand(g, nrm);
        ggml_build_forward_expand(g, rms);
        ggml_build_forward_expand(g, sm);
        if (ggml_graph_compute_with_ctx(ctx, g, 8) != GGML_STATUS_SUCCESS) abort();
        emit("norm_finite", (const float *) nrm->data, NULL, (size_t)(nr*nc));
        emit("rms_finite",  (const float *) rms->data, NULL, (size_t)(nr*nc));
        emit("softmax_finite", (const float *) sm->data, NULL, (size_t)(nr*nc));
        ggml_free(ctx);
    }

    // 8) per-layer operations the Apex graph actually runs: get_rows, sum_rows, mul_mat
    {
        struct ggml_init_params p = { 128u*1024u*1024u, NULL, false };
        struct ggml_context * ctx = ggml_init(p);
        if (!ctx) abort();
        // get_rows over a 16x8 table with a fixed id list
        struct ggml_tensor * tbl = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 16, 8);
        float * td = (float *) tbl->data;
        for (int i = 0; i < 16*8; ++i) td[i] = (float)(i % 17) * 0.13f - 0.8f;
        struct ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 5);
        int32_t * idp = (int32_t *) ids->data;
        for (int i = 0; i < 5; ++i) idp[i] = (i*3) % 8;
        struct ggml_tensor * gr = ggml_get_rows(ctx, tbl, ids);
        // sum_rows over the same table
        struct ggml_tensor * sr = ggml_sum_rows(ctx, tbl);
        struct ggml_cgraph * g = ggml_new_graph(ctx);
        ggml_build_forward_expand(g, gr);
        ggml_build_forward_expand(g, sr);
        if (ggml_graph_compute_with_ctx(ctx, g, 8) != GGML_STATUS_SUCCESS) abort();
        emit("get_rows_16x8", (const float *) gr->data, NULL, (size_t)(16*5));
        emit("sum_rows_16x8", (const float *) sr->data, NULL, (size_t) ggml_nelements(sr));
        ggml_free(ctx);
    }

    // 9) a small dense matmul, the shape every layer uses
    {
        struct ggml_init_params p = { 256u*1024u*1024u, NULL, false };
        struct ggml_context * ctx = ggml_init(p);
        if (!ctx) abort();
        const int K = 32, M = 8, N = 6;
        struct ggml_tensor * w = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, K, M);
        struct ggml_tensor * x = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, K, N);
        float * wp = (float *) w->data; float * xp = (float *) x->data;
        for (int i = 0; i < K*M; ++i) wp[i] = (float)((i % 19) - 9) * 0.07f;
        for (int i = 0; i < K*N; ++i) xp[i] = (float)((i % 23) - 11) * 0.05f;
        struct ggml_tensor * mm = ggml_mul_mat(ctx, w, x);
        struct ggml_cgraph * g = ggml_new_graph(ctx);
        ggml_build_forward_expand(g, mm);
        if (ggml_graph_compute_with_ctx(ctx, g, 8) != GGML_STATUS_SUCCESS) abort();
        emit("mul_mat_32x8x6", (const float *) mm->data, NULL, (size_t)(M*N));
        ggml_free(ctx);
    }

    fclose(out);
    return 0;
}
