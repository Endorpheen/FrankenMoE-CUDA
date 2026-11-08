// Executable native-vs-Goose corpus for the APX013 worker-6 provider block.
// One process builds real ggml tensors, runs the runtime CPU backend for the
// native oracle, then the generated Goose unit directly and through the
// default-on provider dispatcher. Every record is self-describing and hashed.
#define _POSIX_C_SOURCE 200112L
#include "provider.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE * out;
static int failures;
static int recorded_mismatches;
static int strict = 1;
static int cases;
static uint32_t rng = 0x006a91u;

static uint32_t next_u32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float next_float(void) { return ((int32_t)(next_u32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static uint64_t fnv64(const void * data, size_t n) {
    const uint8_t * p = data;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static struct ggml_context * ctx_new(void) {
    struct ggml_init_params p = { 32u * 1024u * 1024u, NULL, false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) abort();
    return ctx;
}

static float sample_float(int64_t i, int edge) {
    if (edge) {
        static const float edge_values[] = { -0.0f, 0.0f, 60.0f, -60.0f, 7.0f, -7.0f, 1e-8f, -1e-8f };
        if (i < (int64_t)(sizeof(edge_values) / sizeof(*edge_values))) return edge_values[i];
    }
    return next_float() * 9.0f;
}
static void fill_float(struct ggml_tensor * t, int edge) {
    int64_t n = ggml_nelements(t);
    if (t->type == GGML_TYPE_F32) {
        float * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = sample_float(i, edge);
    } else if (t->type == GGML_TYPE_F16) {
        ggml_fp16_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = ggml_fp32_to_fp16(sample_float(i, edge));
    } else if (t->type == GGML_TYPE_BF16) {
        ggml_bf16_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = ggml_fp32_to_bf16(sample_float(i, edge));
    } else if (t->type == GGML_TYPE_I32) {
        int32_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = (int32_t)((i * 13 + 5) % 97) - 48;
    } else {
        abort();
    }
}

static void fill_quant(struct ggml_tensor * t) {
    uint8_t * p = t->data;
    size_t n = ggml_nbytes(t);
    for (size_t i = 0; i < n; ++i) p[i] = (uint8_t)(next_u32() >> 24);
    size_t bs = ggml_type_size(t->type);
    uint16_t d = 0x2c00u;      // 0.0625
    uint16_t dmin = 0x2800u;   // 0.03125
    for (size_t b = 0; b < n / bs; ++b) {
        uint8_t * q = p + b * bs;
        if (t->type == GGML_TYPE_Q4_K || t->type == GGML_TYPE_Q5_K) {
            memcpy(q, &d, 2);
            memcpy(q + 2, &dmin, 2);
        } else if (t->type == GGML_TYPE_Q6_K) {
            memcpy(q + 208, &d, 2);
        } else {
            abort();
        }
    }
}

static struct ggml_tensor * new_tensor(struct ggml_context * ctx, enum ggml_type type,
        int64_t a, int64_t b, int64_t c, int64_t d, int edge) {
    struct ggml_tensor * t = ggml_new_tensor_4d(ctx, type, a, b, c, d);
    if (type == GGML_TYPE_Q4_K || type == GGML_TYPE_Q5_K || type == GGML_TYPE_Q6_K) fill_quant(t);
    else fill_float(t, edge);
    return t;
}

static int run_native(struct ggml_context * ctx, struct ggml_tensor * t, int threads) {
    struct ggml_cgraph * g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, t);
    return ggml_graph_compute_with_ctx(ctx, g, threads) == GGML_STATUS_SUCCESS;
}

static void * alloc_workspace(size_t n) {
    void * p = NULL;
    if (!n) return NULL;
    if (posix_memalign(&p, 64, n) != 0 || !p) abort();
    return p;
}

static int run_direct(struct ggml_tensor * t, int threads, void * dst) {
    size_t n = ggml_nbytes(t);
    memset(dst, 0, n);
    struct fg006_node base = fg006_describe(t, 0, threads);
    int64_t info[4];
    fg006_goose_workspace(&base, threads, info);
    if (info[3] < 0) abort();
    void * work = alloc_workspace((size_t) info[3]);
    float * scratch = info[0] ? (float *) work : NULL;
    uint8_t * raw = info[1] ? (uint8_t *) work + info[2] : NULL;
    for (int ith = 0; ith < threads; ++ith) {
        struct fg006_node p = fg006_describe(t, ith, threads);
        uint8_t ok = fg006_goose_compute(&p,
                t->src[0] ? t->src[0]->data : NULL,
                t->src[1] ? t->src[1]->data : NULL,
                t->src[2] ? t->src[2]->data : NULL,
                dst, scratch, info[0], raw, info[1], NULL, 0);
        if (!ok) { free(work); return 0; }
    }
    free(work);
    return 1;
}

static int run_binding(struct ggml_tensor * t, int threads) {
    size_t wsize = fg006_extra_workspace(t, threads);
    void * work = alloc_workspace(wsize);
    for (int ith = 0; ith < threads; ++ith) {
        if (!fg006_try_raw(ith, threads, wsize, work, 0, t)) { free(work); return 0; }
    }
    free(work);
    return 1;
}

static void emit(const char * name, struct ggml_tensor * t, int threads, size_t bytes,
        const void * native, const void * direct, const void * binding) {
    int nd = memcmp(native, direct, bytes) == 0;
    int nb = binding && memcmp(native, binding, bytes) == 0;
    if (!nd || !nb) {
        if (strict) ++failures; else ++recorded_mismatches;
        size_t at = 0;
        const uint8_t * a = native, * b = nd ? binding : direct;
        while (at < bytes && a[at] == b[at]) ++at;
        fprintf(stderr, "MISMATCH %s op=%d threads=%d at=%zu/%zu direct=%d binding=%d\n",
                name, (int) t->op, threads, at, bytes, nd, nb);
    }
    ++cases;
    fprintf(out, "{\"name\":\"%s\",\"op\":%d,\"threads\":%d,\"bytes\":%zu,"
            "\"native\":\"%016" PRIx64 "\",\"direct\":\"%016" PRIx64 "\","
            "\"binding\":\"%016" PRIx64 "\",\"native_direct\":%s,\"native_binding\":%s}\n",
            name, (int) t->op, threads, bytes,
            fnv64(native, bytes), fnv64(direct, bytes),
            binding ? fnv64(binding, bytes) : 0,
            nd ? "true" : "false", nb ? "true" : "false");
    fflush(out);
}

static void run_case(struct ggml_context * ctx, const char * name, struct ggml_tensor * t, int threads) {
    size_t n = ggml_nbytes(t);
    if (!n) { fprintf(stderr, "EMPTY_CASE %s\n", name); ++failures; return; }
    uint8_t * native = malloc(n);
    uint8_t * direct = malloc(n);
    if (!native || !direct) abort();
    memset(native, 0, n);
    if (!run_native(ctx, t, threads)) { fprintf(stderr, "NATIVE_FAILED %s\n", name); ++failures; free(native); free(direct); return; }
    memcpy(native, t->data, n);
    if (!run_direct(t, threads, direct)) { fprintf(stderr, "GOOSE_REFUSED %s t=%d\n", name, threads); ++failures; }
    memset(t->data, 0, n);
    int ok = run_binding(t, threads);
    if (!ok) fprintf(stderr, "BINDING_REFUSED %s t=%d\n", name, threads);
    emit(name, t, threads, n, native, direct, ok ? t->data : NULL);
    free(native);
    free(direct);
}

static int64_t id_value(int64_t i, int64_t rows) { return (i * 17 + 3) % rows; }

static void case_get_rows(const char * name, enum ggml_type type, int64_t width, int64_t rows,
        int64_t ids_n, int stride, int planes, int edge) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_context * ctx = ctx_new();
        int64_t alloc_rows = stride ? rows * 2 : rows;
        struct ggml_tensor * base = new_tensor(ctx, type, width, alloc_rows, planes, 1, edge);
        struct ggml_tensor * a = base;
        if (stride) a = ggml_view_3d(ctx, base, width, rows, planes, base->nb[1] * 2, base->nb[2], 0);
        struct ggml_tensor * ids;
        if (planes > 1) {
            ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, ids_n, planes);
            int32_t * p = ids->data;
            for (int64_t i = 0; i < ids_n * planes; ++i) p[i] = (int32_t) ((i * 11 + 1) % rows);
        } else {
            ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ids_n);
            int32_t * p = ids->data;
            for (int64_t i = 0; i < ids_n; ++i) p[i] = (int32_t) id_value(i, rows);
        }
        struct ggml_tensor * t = ggml_get_rows(ctx, a, ids);
        char label[160];
        snprintf(label, sizeof(label), "%s-t%d", name, threads);
        run_case(ctx, label, t, threads);
        ggml_free(ctx);
    }
}

static void set_glu_params(struct ggml_tensor * t) {
    float alpha = 1.702f;
    float limit = 7.0f;
    memcpy((char *) t->op_params + 8, &alpha, 4);
    memcpy((char *) t->op_params + 12, &limit, 4);
}

static struct ggml_tensor * build_glu(struct ggml_context * ctx, int op, int swapped, int split,
        int stride, int edge, enum ggml_type type) {
    const int64_t W = 256, R = 5;
    struct ggml_tensor * a;
    struct ggml_tensor * b = NULL;
    struct ggml_tensor * t;
    if (split) {
        int64_t rows = stride ? R * 2 : R;
        a = new_tensor(ctx, type, W, rows, 1, 1, edge);
        b = new_tensor(ctx, type, W, rows, 1, 1, edge);
        struct ggml_tensor * av = stride ? ggml_view_2d(ctx, a, W, R, a->nb[1] * 2, 0) : a;
        struct ggml_tensor * bv = stride ? ggml_view_2d(ctx, b, W, R, b->nb[1] * 2, 0) : b;
        t = ggml_glu_split(ctx, av, bv, (enum ggml_glu_op) op);
    } else {
        int64_t rows = stride ? R * 2 : R;
        a = new_tensor(ctx, type, 2 * W, rows, 1, 1, edge);
        struct ggml_tensor * av = stride ? ggml_view_2d(ctx, a, 2 * W, R, a->nb[1] * 2, 0) : a;
        t = ggml_glu(ctx, av, (enum ggml_glu_op) op, swapped != 0);
    }
    set_glu_params(t);
    return t;
}

static void cases_glu(void) {
    for (int op = 0; op < GGML_GLU_OP_COUNT; ++op) {
        for (int layout = 0; layout < 3; ++layout) {
            for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
                struct ggml_context * ctx = ctx_new();
                struct ggml_tensor * t = build_glu(ctx, op, layout == 2, layout == 1,
                        layout == 2, 0, GGML_TYPE_F32);
                char label[128];
                snprintf(label, sizeof(label), "glu-op%d-layout%d-t%d", op, layout, threads);
                run_case(ctx, label, t, threads);
                ggml_free(ctx);
            }
        }
    }
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        {
            struct ggml_context * ctx = ctx_new();
            struct ggml_tensor * t = build_glu(ctx, GGML_GLU_OP_SWIGLU, 0, 0, 1, 1, GGML_TYPE_F32);
            char label[80];
            snprintf(label, sizeof(label), "glu-edge-stride-t%d", threads);
            run_case(ctx, label, t, threads);
            ggml_free(ctx);
        }
        {
            struct ggml_context * ctx = ctx_new();
            struct ggml_tensor * t = build_glu(ctx, GGML_GLU_OP_SWIGLU, 0, 1, 0, 1, GGML_TYPE_F32);
            char label[80];
            snprintf(label, sizeof(label), "glu-edge-split-t%d", threads);
            run_case(ctx, label, t, threads);
            ggml_free(ctx);
        }
        {
            struct ggml_context * ctx = ctx_new();
            struct ggml_tensor * t = build_glu(ctx, GGML_GLU_OP_GEGLU, 0, 0, 0, 0, GGML_TYPE_F16);
            char label[80];
            snprintf(label, sizeof(label), "glu-f16-geglu-t%d", threads);
            run_case(ctx, label, t, threads);
            ggml_free(ctx);
        }
        {
            struct ggml_context * ctx = ctx_new();
            struct ggml_tensor * t = build_glu(ctx, GGML_GLU_OP_SWIGLU, 0, 0, 0, 0, GGML_TYPE_BF16);
            char label[80];
            snprintf(label, sizeof(label), "glu-bf16-swiglu-t%d", threads);
            run_case(ctx, label, t, threads);
            ggml_free(ctx);
        }
    }
}

static void case_convert(const char * name, enum ggml_type st, enum ggml_type dt,
        int64_t width, int64_t rows, int stride, int edge) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_context * ctx = ctx_new();
        int64_t source_rows = stride ? rows * 2 : rows;
        struct ggml_tensor * sbase = new_tensor(ctx, st, width, source_rows, 1, 1, edge);
        struct ggml_tensor * src = stride ? ggml_view_2d(ctx, sbase, width, rows, sbase->nb[1] * 2, 0) : sbase;
        struct ggml_tensor * t;
        if (st == dt) {
            int64_t target_rows = stride ? rows * 2 : rows;
            struct ggml_tensor * dbase = ggml_new_tensor_4d(ctx, dt, width, target_rows, 1, 1);
            memset(dbase->data, 0, ggml_nbytes(dbase));
            struct ggml_tensor * dst = stride ? ggml_view_2d(ctx, dbase, width, rows, dbase->nb[1] * 2, 0) : dbase;
            t = ggml_cpy(ctx, src, dst);
        } else {
            t = ggml_cast(ctx, src, dt);
        }
        char label[160];
        snprintf(label, sizeof(label), "%s-t%d", name, threads);
        run_case(ctx, label, t, threads);
        ggml_free(ctx);
    }
}

static void case_dup_cont(const char * name, enum ggml_type type, int64_t width, int64_t rows, int cont) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_context * ctx = ctx_new();
        struct ggml_tensor * base = new_tensor(ctx, type, width, rows * 2, 1, 1, 0);
        struct ggml_tensor * view = ggml_view_2d(ctx, base, width, rows, base->nb[1] * 2, 0);
        struct ggml_tensor * t = cont ? ggml_cont(ctx, view) : ggml_dup(ctx, view);
        char label[160];
        snprintf(label, sizeof(label), "%s-t%d", name, threads);
        run_case(ctx, label, t, threads);
        ggml_free(ctx);
    }
}

static void fake_get_rows(struct fg006_node * p, int xkind, int width, int rows, int idkind, int ids, int dkind) {
    memset(p, 0, sizeof(*p));
    p->op = GGML_OP_GET_ROWS;
    p->nth = 1;
    p->x.kind = xkind;
    p->x.ne[0] = width; p->x.ne[1] = rows; p->x.ne[2] = 1; p->x.ne[3] = 1;
    int64_t blck = (xkind == GGML_TYPE_Q4_K || xkind == GGML_TYPE_Q5_K || xkind == GGML_TYPE_Q6_K)
        ? 256 : ggml_blck_size((enum ggml_type) xkind);
    p->x.nb[0] = (int64_t) ggml_type_size((enum ggml_type) xkind);
    p->x.nb[1] = p->x.nb[0] * ((width + blck - 1) / blck);
    p->x.nb[2] = p->x.nb[1] * rows;
    p->x.nb[3] = p->x.nb[2];
    p->x.bytes = p->x.nb[1] * rows;
    p->y.kind = idkind;
    p->y.ne[0] = ids; p->y.ne[1] = 1; p->y.ne[2] = 1; p->y.ne[3] = 1;
    p->y.nb[0] = 4; p->y.nb[1] = 4 * ids; p->y.nb[2] = p->y.nb[1]; p->y.nb[3] = p->y.nb[1];
    p->y.bytes = ids * 4;
    p->d.kind = dkind;
    p->d.ne[0] = width; p->d.ne[1] = ids; p->d.ne[2] = 1; p->d.ne[3] = 1;
    p->d.nb[0] = (int64_t) ggml_type_size((enum ggml_type) dkind);
    p->d.nb[1] = p->d.nb[0] * width;
    p->d.nb[2] = p->d.nb[1] * ids;
    p->d.nb[3] = p->d.nb[2];
    p->d.bytes = p->d.nb[1] * ids;
}

static void emit_negative(const char * name, int accepted, int expected) {
    ++cases;
    if (accepted != expected) ++failures;
    fprintf(out, "{\"name\":\"%s\",\"negative\":true,\"accepted\":%s,\"expected\":%s,\"pass\":%s}\n",
            name, accepted ? "true" : "false", expected ? "true" : "false",
            accepted == expected ? "true" : "false");
    fflush(out);
}

static void cases_negative(void) {
    struct fg006_node p;
    fake_get_rows(&p, GGML_TYPE_Q4_K, 512, 8, GGML_TYPE_I32, 6, GGML_TYPE_F32);
    emit_negative("support-getrows-q4k", fg006_supported_node(&p), 1);
    fake_get_rows(&p, GGML_TYPE_Q6_K, 512, 8, GGML_TYPE_I32, 6, GGML_TYPE_F32);
    emit_negative("support-getrows-q6k", fg006_supported_node(&p), 1);
    fake_get_rows(&p, GGML_TYPE_F32, 512, 8, GGML_TYPE_I32, 6, GGML_TYPE_F32);
    emit_negative("support-getrows-f32", fg006_supported_node(&p), 1);
    fake_get_rows(&p, GGML_TYPE_Q4_K, 384, 8, GGML_TYPE_I32, 6, GGML_TYPE_F32);
    emit_negative("reject-getrows-nonmultiple256", fg006_supported_node(&p), 0);
    fake_get_rows(&p, GGML_TYPE_Q4_K, 512, 8, GGML_TYPE_I64, 6, GGML_TYPE_F32);
    emit_negative("reject-getrows-i64-ids", fg006_supported_node(&p), 0);
    fake_get_rows(&p, GGML_TYPE_F32, 512, 8, GGML_TYPE_I32, 6, GGML_TYPE_I32);
    emit_negative("reject-getrows-f32-to-i32", fg006_supported_node(&p), 0);
    fake_get_rows(&p, GGML_TYPE_Q4_K, 512, 8, GGML_TYPE_I32, 6, GGML_TYPE_I32);
    emit_negative("reject-getrows-q4k-to-i32", fg006_supported_node(&p), 0);

    memset(&p, 0, sizeof(p));
    p.op = GGML_OP_GLU; p.nth = 1;
    p.x.kind = GGML_TYPE_F32; p.x.ne[0] = 512; p.x.ne[1] = 4;
    p.x.nb[0] = 4; p.x.nb[1] = 2048; p.x.bytes = 8192;
    p.y.kind = GGML_TYPE_F32; p.y.ne[0] = 256; p.y.ne[1] = 4;
    p.y.nb[0] = 4; p.y.nb[1] = 1024; p.y.bytes = 4096;
    p.d.kind = GGML_TYPE_F32; p.d.ne[0] = 256; p.d.ne[1] = 4;
    p.d.nb[0] = 4; p.d.nb[1] = 1024; p.d.bytes = 4096;
    p.ip[0] = GGML_GLU_OP_SWIGLU;
    emit_negative("support-glu-swiglu", fg006_supported_node(&p), 1);
    p.ip[0] = GGML_GLU_OP_COUNT;
    emit_negative("reject-glu-op-count", fg006_supported_node(&p), 0);
    p.ip[0] = GGML_GLU_OP_SWIGLU;
    p.x.kind = GGML_TYPE_F64; p.d.kind = GGML_TYPE_F64;
    emit_negative("reject-glu-f64", fg006_supported_node(&p), 0);

    memset(&p, 0, sizeof(p));
    p.op = GGML_OP_CPY; p.nth = 1;
    p.x.kind = GGML_TYPE_Q4_K; p.x.ne[0] = 512; p.x.ne[1] = 4;
    p.x.nb[0] = 144; p.x.nb[1] = 288; p.x.bytes = 1152;
    p.d.kind = GGML_TYPE_F16; p.d.ne[0] = 512; p.d.ne[1] = 4;
    p.d.nb[0] = 2; p.d.nb[1] = 1024; p.d.bytes = 4096;
    emit_negative("reject-cpy-q4k-to-f16", fg006_supported_node(&p), 0);
    p.d.kind = GGML_TYPE_F32; p.d.nb[0] = 4; p.d.nb[1] = 2048; p.d.bytes = 8192;
    emit_negative("support-cpy-q4k-to-f32", fg006_supported_node(&p), 1);
    p.x.kind = GGML_TYPE_F32; p.x.nb[0] = 4; p.x.nb[1] = 2048; p.x.bytes = 8192;
    p.d.kind = GGML_TYPE_Q4_K; p.d.nb[0] = 144; p.d.nb[1] = 288; p.d.bytes = 1152;
    emit_negative("reject-cpy-f32-to-q4k", fg006_supported_node(&p), 0);

    // ids bounds are checked before dispatch, without dereferencing.
    struct fg006_node q;
    fake_get_rows(&q, GGML_TYPE_Q4_K, 512, 4, GGML_TYPE_I32, 4, GGML_TYPE_F32);
    int32_t good[4] = {0, 1, 2, 3};
    int32_t bad_high[4] = {0, 1, 2, 4};
    int32_t bad_low[4] = {0, 1, -1, 3};
    emit_negative("ids-bounds-good", fg006_validate_ids_node(&q, good), 1);
    emit_negative("ids-bounds-high", fg006_validate_ids_node(&q, bad_high), 0);
    emit_negative("ids-bounds-low", fg006_validate_ids_node(&q, bad_low), 0);
}

static void cases_workspace(void) {
    struct fg006_node p;
    memset(&p, 0, sizeof(p));
    p.op = GGML_OP_GET_ROWS;
    p.nth = 1;
    p.x.kind = GGML_TYPE_Q4_K;
    p.x.ne[0] = 4096; p.x.ne[1] = 4;
    p.x.nb[0] = 144; p.x.nb[1] = 2304; p.x.bytes = 9216;
    p.y.kind = GGML_TYPE_I32;
    p.y.ne[0] = 4; p.y.nb[0] = 4; p.y.bytes = 16;
    p.d.kind = GGML_TYPE_F32;
    p.d.ne[0] = 4096; p.d.ne[1] = 4;
    p.d.nb[0] = 4; p.d.nb[1] = 16384; p.d.bytes = 65536;
    int64_t info[4];
    fg006_goose_workspace(&p, 8, info);
    size_t corrected = info[3] < 0 ? 0 : (size_t) info[3];
    fg006_goose_workspace(&p, 1, info);
    size_t legacy = info[3] < 0 ? 0 : (size_t) info[3];
    ++cases;
    if (!(corrected > legacy)) ++failures;
    fprintf(out, "{\"name\":\"workspace-callback\",\"op\":40,\"threads\":8,"
            "\"corrected\":%zu,\"legacy\":%zu,\"legacy_insufficient\":%s}\n",
            corrected, legacy, corrected > legacy ? "true" : "false");
    fflush(out);
}

static void cases_use_ref(void) {
    struct ggml_context * ctx = ctx_new();
    struct ggml_tensor * w = new_tensor(ctx, GGML_TYPE_Q4_K, 512, 4, 1, 1, 0);
    struct ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 2);
    ((int32_t *) ids->data)[0] = 1; ((int32_t *) ids->data)[1] = 3;
    struct ggml_tensor * t = ggml_get_rows(ctx, w, ids);
    size_t n = ggml_nbytes(t);
    uint8_t * before = malloc(n);
    memcpy(before, t->data, n);
    size_t wsize = fg006_extra_workspace(t, 3);
    void * work = alloc_workspace(wsize);
    int r = fg006_try_raw(0, 3, wsize, work, 1, t);
    int unchanged = memcmp(before, t->data, n) == 0;
    free(work); free(before); ggml_free(ctx);
    ++cases;
    if (r != 0 || !unchanged) ++failures;
    fprintf(out, "{\"name\":\"use-ref-fallback\",\"op\":40,\"threads\":3,\"returned\":%d,"
            "\"unchanged\":%s,\"pass\":%s}\n", r, unchanged ? "true" : "false",
            (r == 0 && unchanged) ? "true" : "false");
    fflush(out);
}

static void fill_q4k_bytes(uint8_t * p, size_t blocks) {
    for (size_t i = 0; i < blocks * 144; ++i) p[i] = (uint8_t)(next_u32() >> 24);
    for (size_t b = 0; b < blocks; ++b) {
        uint16_t d = 0x2c00u, dmin = 0x2800u;
        memcpy(p + b * 144, &d, 2);
        memcpy(p + b * 144 + 2, &dmin, 2);
    }
}
static void emit_internal(const char * name, int pass) {
    ++cases;
    if (!pass) ++failures;
    fprintf(out, "{\"name\":\"%s\",\"internal\":true,\"pass\":%s}\n", name, pass ? "true" : "false");
    fflush(out);
}
static void cases_internal_stride(void) {
    const int64_t W = 512, rows = 4, blocks_per_row = 2, rowbytes = 288;
    uint8_t src[4 * 288];
    fill_q4k_bytes(src, rows * blocks_per_row);
    uint8_t padded[4 * 320];
    for (int64_t r = 0; r < rows; ++r) {
        for (int64_t b = 0; b < blocks_per_row; ++b) {
            memcpy(padded + r * 320 + b * 160, src + (r * blocks_per_row + b) * 144, 144);
        }
    }
    int32_t ids[4] = {3, 1, 0, 2};
    float scratch[512];
    uint8_t raw[288];
    float a[W * rows], b[W * rows];
    struct fg006_node p;
    memset(&p, 0, sizeof(p));
    p.op = GGML_OP_GET_ROWS;
    p.nth = 1;
    p.x.kind = GGML_TYPE_Q4_K;
    p.x.ne[0] = W; p.x.ne[1] = rows; p.x.ne[2] = 1; p.x.ne[3] = 1;
    p.x.nb[0] = 144; p.x.nb[1] = 288; p.x.nb[2] = 1152; p.x.nb[3] = 1152;
    p.x.bytes = 1152;
    p.y.kind = GGML_TYPE_I32;
    p.y.ne[0] = 4; p.y.ne[1] = 1; p.y.ne[2] = 1; p.y.ne[3] = 1;
    p.y.nb[0] = 4; p.y.nb[1] = 16; p.y.nb[2] = 16; p.y.nb[3] = 16;
    p.y.bytes = 16;
    p.d.kind = GGML_TYPE_F32;
    p.d.ne[0] = W; p.d.ne[1] = rows; p.d.ne[2] = 1; p.d.ne[3] = 1;
    p.d.nb[0] = 4; p.d.nb[1] = W * 4; p.d.nb[2] = W * 4 * rows; p.d.nb[3] = p.d.nb[2];
    p.d.bytes = W * 4 * rows;
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    int ok_a = fg006_goose_compute(&p, src, ids, NULL, a, scratch, 512, raw, 288, NULL, 0);
    struct fg006_node q = p;
    q.x.nb[0] = 160; q.x.nb[1] = 320; q.x.nb[2] = 1280; q.x.nb[3] = 1280; q.x.bytes = 1280;
    int ok_b = fg006_goose_compute(&q, padded, ids, NULL, b, scratch, 512, raw, 288, NULL, 0);
    emit_internal("internal-getrows-block-stride", ok_a && ok_b && memcmp(a, b, sizeof(a)) == 0);

    memset(&p, 0, sizeof(p));
    p.op = GGML_OP_CPY;
    p.nth = 1;
    p.x.kind = GGML_TYPE_Q4_K;
    p.x.ne[0] = W; p.x.ne[1] = rows; p.x.ne[2] = 1; p.x.ne[3] = 1;
    p.x.nb[0] = 144; p.x.nb[1] = 288; p.x.nb[2] = 1152; p.x.nb[3] = 1152;
    p.x.bytes = 1152;
    p.d.kind = GGML_TYPE_F32;
    p.d.ne[0] = W; p.d.ne[1] = rows; p.d.ne[2] = 1; p.d.ne[3] = 1;
    p.d.nb[0] = 4; p.d.nb[1] = W * 4; p.d.nb[2] = W * 4 * rows; p.d.nb[3] = p.d.nb[2];
    p.d.bytes = W * 4 * rows;
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    ok_a = fg006_goose_compute(&p, src, NULL, NULL, a, scratch, 256, raw, 144, NULL, 0);
    q = p;
    q.x.nb[0] = 160; q.x.nb[1] = 320; q.x.nb[2] = 1280; q.x.nb[3] = 1280; q.x.bytes = 1280;
    ok_b = fg006_goose_compute(&q, padded, NULL, NULL, b, scratch, 256, raw, 144, NULL, 0);
    emit_internal("internal-convert-block-stride", ok_a && ok_b && memcmp(a, b, sizeof(a)) == 0);
}

int main(int argc, char ** argv) {
    if (argc != 3) return 2;
    out = fopen(argv[1], "wb");
    if (!out) return 2;
    strict = strncmp(argv[2], "bridge", 6) != 0;
    fprintf(out, "{\"header\":\"fg006.native-vs-goose\",\"tag\":\"%s\",\"provider_enabled\":%d}\n",
            argv[2], fg006_provider_enabled());

    cases_use_ref();
    case_get_rows("getrows-q4k-basic", GGML_TYPE_Q4_K, 512, 12, 8, 0, 1, 0);
    case_get_rows("getrows-q4k-stride", GGML_TYPE_Q4_K, 512, 6, 6, 1, 1, 0);
    case_get_rows("getrows-q5k-basic", GGML_TYPE_Q5_K, 512, 10, 7, 0, 1, 0);
    case_get_rows("getrows-q6k-basic", GGML_TYPE_Q6_K, 512, 10, 7, 0, 1, 0);
    case_get_rows("getrows-q6k-stride", GGML_TYPE_Q6_K, 512, 6, 6, 1, 1, 0);
    case_get_rows("getrows-q4k-planar", GGML_TYPE_Q4_K, 256, 8, 4, 0, 2, 0);
    case_get_rows("getrows-q4k-one-block", GGML_TYPE_Q4_K, 256, 4, 8, 0, 1, 0);
    case_get_rows("getrows-f32-basic", GGML_TYPE_F32, 512, 12, 8, 0, 1, 0);
    case_get_rows("getrows-f32-stride", GGML_TYPE_F32, 512, 6, 6, 1, 1, 0);
    case_get_rows("getrows-f32-planar", GGML_TYPE_F32, 256, 8, 4, 0, 2, 0);
    case_get_rows("getrows-f16-basic", GGML_TYPE_F16, 512, 12, 8, 0, 1, 0);
    case_get_rows("getrows-bf16-basic", GGML_TYPE_BF16, 512, 12, 8, 0, 1, 0);
    case_get_rows("getrows-i32-basic", GGML_TYPE_I32, 512, 12, 8, 0, 1, 0);
    cases_glu();
    case_convert("cpy-f32-f32-strided", GGML_TYPE_F32, GGML_TYPE_F32, 64, 4, 1, 0);
    case_convert("cpy-f32-f16", GGML_TYPE_F32, GGML_TYPE_F16, 64, 4, 0, 0);
    case_convert("cpy-f32-bf16", GGML_TYPE_F32, GGML_TYPE_BF16, 64, 4, 0, 0);
    case_convert("cpy-f16-f32", GGML_TYPE_F16, GGML_TYPE_F32, 64, 4, 0, 0);
    case_convert("cpy-f16-bf16", GGML_TYPE_F16, GGML_TYPE_BF16, 64, 4, 0, 0);
    case_convert("cpy-bf16-f32", GGML_TYPE_BF16, GGML_TYPE_F32, 64, 4, 0, 0);
    case_convert("cpy-bf16-f16", GGML_TYPE_BF16, GGML_TYPE_F16, 64, 4, 0, 0);
    case_convert("cpy-i32-f32", GGML_TYPE_I32, GGML_TYPE_F32, 64, 4, 0, 0);
    case_convert("cpy-f32-i32", GGML_TYPE_F32, GGML_TYPE_I32, 64, 4, 0, 0);
    case_convert("cpy-q4k-f32", GGML_TYPE_Q4_K, GGML_TYPE_F32, 512, 6, 0, 0);
    case_convert("cpy-q4k-f32-stride", GGML_TYPE_Q4_K, GGML_TYPE_F32, 512, 6, 1, 0);
    case_convert("cpy-q5k-f32", GGML_TYPE_Q5_K, GGML_TYPE_F32, 512, 6, 0, 0);
    case_convert("cpy-q6k-f32", GGML_TYPE_Q6_K, GGML_TYPE_F32, 512, 6, 0, 0);
    case_convert("cpy-q6k-f32-stride", GGML_TYPE_Q6_K, GGML_TYPE_F32, 512, 6, 1, 0);
    case_dup_cont("dup-f32-strided", GGML_TYPE_F32, 64, 4, 0);
    case_dup_cont("cont-f32-strided", GGML_TYPE_F32, 64, 4, 1);
    case_dup_cont("dup-f16-strided", GGML_TYPE_F16, 64, 4, 0);
    case_dup_cont("cont-bf16-strided", GGML_TYPE_BF16, 64, 4, 1);
    cases_negative();
    cases_workspace();
    cases_internal_stride();

    fprintf(out, "{\"summary\":\"%s\",\"cases\":%d,\"failures\":%d,\"recorded_mismatches\":%d}\n",
            strict ? (failures ? "FAILED" : "PASS") : "RECORDED", cases, failures, recorded_mismatches);
    fclose(out);
    if (failures) {
        fprintf(stderr, "FG006_CORPUS failures=%d cases=%d\n", failures, cases);
        return 1;
    }
    printf("FG006_CORPUS %s cases=%d recorded_mismatches=%d\n",
            strict ? "PASS" : "RECORDED", cases, recorded_mismatches);
    return 0;
}
