#define _POSIX_C_SOURCE 200112L
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fg006_integration_stats {
    uint64_t dispatcher_calls;
    uint64_t planner_calls;
    uint64_t planner_increases;
    uint64_t control_calls;
    uint64_t control_threads;
    size_t planner_old;
    size_t planner_need;
    size_t provider_workspace_n1;
    size_t provider_workspace_n8;
    int enabled;
};
extern struct fg006_integration_stats fg006_integration_stats __attribute__((weak));

static FILE * out;
static int failures, cases;
static uint32_t rng = 0x006a91u;
static uint32_t next_u32(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static float next_float(void) { return ((int32_t)(next_u32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static uint64_t fnv64(const void * p, size_t n) {
    const uint8_t * b = p; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
static struct ggml_context * ctx_new(void) {
    struct ggml_init_params p = { 32u * 1024u * 1024u, NULL, false };
    struct ggml_context * c = ggml_init(p); if (!c) abort(); return c;
}
static void fill_float(struct ggml_tensor * t, int edge) {
    int64_t n = ggml_nelements(t);
    if (t->type == GGML_TYPE_F32) {
        float * p = t->data;
        for (int64_t i = 0; i < n; ++i) {
            if (edge && i < 8) { const float e[] = {-0.0f,0.0f,60.f,-60.f,7.f,-7.f,1e-8f,-1e-8f}; p[i]=e[i]; }
            else p[i] = next_float() * 9.0f;
        }
    } else if (t->type == GGML_TYPE_F16) {
        ggml_fp16_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = ggml_fp32_to_fp16(next_float() * 9.0f);
    } else if (t->type == GGML_TYPE_BF16) {
        ggml_bf16_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = ggml_fp32_to_bf16(next_float() * 9.0f);
    } else if (t->type == GGML_TYPE_I32) {
        int32_t * p = t->data;
        for (int64_t i = 0; i < n; ++i) p[i] = (int32_t)((i * 13 + 5) % 97) - 48;
    } else abort();
}
static void fill_quant(struct ggml_tensor * t) {
    uint8_t * p = t->data; size_t n = ggml_nbytes(t);
    for (size_t i = 0; i < n; ++i) p[i] = (uint8_t)(next_u32() >> 24);
    size_t bs = ggml_type_size(t->type);
    for (size_t b = 0; b < n / bs; ++b) {
        uint8_t * q = p + b * bs;
        uint16_t d = 0x2c00u, dmin = 0x2800u;
        if (t->type == GGML_TYPE_Q4_K || t->type == GGML_TYPE_Q5_K) { memcpy(q,&d,2); memcpy(q+2,&dmin,2); }
        else if (t->type == GGML_TYPE_Q6_K) memcpy(q+208,&d,2);
        else abort();
    }
}
static struct ggml_tensor * nt(struct ggml_context * c, enum ggml_type type,
        int64_t a, int64_t b, int64_t cc, int64_t d, int edge) {
    struct ggml_tensor * t = ggml_new_tensor_4d(c, type, a, b, cc, d);
    if (type == GGML_TYPE_Q4_K || type == GGML_TYPE_Q5_K || type == GGML_TYPE_Q6_K) fill_quant(t);
    else fill_float(t, edge);
    return t;
}
static int check_backend(enum ggml_status s, const char * name) {
    if (s == GGML_STATUS_SUCCESS) return 1;
    fprintf(stderr, "BACKEND_FAILED %s status=%d\n", name, (int) s);
    ++failures; return 0;
}
static void emit(const char * name, int op, int threads, const void * data, size_t bytes) {
    ++cases;
    fprintf(out, "{\"name\":\"%s\",\"op\":%d,\"threads\":%d,\"bytes\":%zu,\"hash\":\"%016" PRIx64 "\"}\n",
            name, op, threads, bytes, fnv64(data, bytes));
    fflush(out);
}
static void run_one(struct ggml_context * ctx, struct ggml_backend * backend,
        const char * name, struct ggml_tensor * t, int threads) {
    struct ggml_cgraph * g = ggml_new_graph(ctx);
    ggml_build_forward_expand(g, t);
    if (!check_backend(ggml_backend_graph_compute(backend, g), name)) return;
    emit(name, (int) t->op, threads, t->data, ggml_nbytes(t));
    ggml_free(ctx);
}
static struct ggml_backend * backend_new(int threads) {
    struct ggml_backend * b = ggml_backend_cpu_init();
    if (!b) abort();
    ggml_backend_cpu_set_n_threads(b, threads);
    return b;
}
static void set_glu_params(struct ggml_tensor * t) {
    float alpha = 1.702f, limit = 7.0f;
    memcpy((char *) t->op_params + 8, &alpha, 4);
    memcpy((char *) t->op_params + 12, &limit, 4);
}
static void cases_getrows(void) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_backend * b = backend_new(threads);
        for (int variant = 0; variant < 5; ++variant) {
            struct ggml_context * c = ctx_new();
            enum ggml_type type = variant == 1 ? GGML_TYPE_Q6_K : variant == 2 ? GGML_TYPE_F32 : GGML_TYPE_Q4_K;
            int64_t W = variant == 3 ? 256 : 512, rows = variant == 3 ? 4 : 12;
            int stride = variant == 4;
            struct ggml_tensor * base = nt(c, type, W, stride ? rows * 2 : rows, 1, 1, 0);
            struct ggml_tensor * a = stride ? ggml_view_2d(c, base, W, rows, base->nb[1] * 2, 0) : base;
            struct ggml_tensor * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, 8);
            int32_t * p = ids->data;
            for (int i = 0; i < 8; ++i) p[i] = (i * 17 + 3) % rows;
            struct ggml_tensor * t = ggml_get_rows(c, a, ids);
            char name[80]; snprintf(name, sizeof(name), "backend-getrows-v%d-t%d", variant, threads);
            run_one(c, b, name, t, threads);
        }
        ggml_backend_free(b);
    }
}
static void cases_glu(void) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_backend * b = backend_new(threads);
        for (int op = 0; op < GGML_GLU_OP_COUNT; ++op) {
            for (int layout = 0; layout < 3; ++layout) {
                struct ggml_context * c = ctx_new();
                struct ggml_tensor * t;
                if (layout == 1) {
                    struct ggml_tensor * a = nt(c, GGML_TYPE_F32, 256, 5, 1, 1, 0);
                    struct ggml_tensor * g = nt(c, GGML_TYPE_F32, 256, 5, 1, 1, 0);
                    t = ggml_glu_split(c, a, g, (enum ggml_glu_op) op);
                } else {
                    struct ggml_tensor * a = nt(c, GGML_TYPE_F32, 512, 5, 1, 1, layout == 2);
                    t = ggml_glu(c, a, (enum ggml_glu_op) op, layout == 2);
                }
                set_glu_params(t);
                char name[80]; snprintf(name, sizeof(name), "backend-glu-op%d-l%d-t%d", op, layout, threads);
                run_one(c, b, name, t, threads);
            }
        }
        ggml_backend_free(b);
    }
}
static void cases_convert(void) {
    for (int threads = 1; threads <= 8; threads = threads == 1 ? 3 : threads == 3 ? 8 : 9) {
        struct ggml_backend * b = backend_new(threads);
        const enum ggml_type types[][2] = {
            {GGML_TYPE_Q4_K, GGML_TYPE_F32}, {GGML_TYPE_Q6_K, GGML_TYPE_F32},
            {GGML_TYPE_F32, GGML_TYPE_F16}, {GGML_TYPE_F32, GGML_TYPE_BF16},
            {GGML_TYPE_F16, GGML_TYPE_F32}, {GGML_TYPE_F16, GGML_TYPE_BF16},
            {GGML_TYPE_I32, GGML_TYPE_F32}, {GGML_TYPE_F32, GGML_TYPE_I32},
        };
        for (size_t k = 0; k < sizeof(types) / sizeof(*types); ++k) {
            struct ggml_context * c = ctx_new();
            struct ggml_tensor * src = nt(c, types[k][0], 512, 4, 1, 1, 0);
            struct ggml_tensor * t = ggml_cast(c, src, types[k][1]);
            char name[100]; snprintf(name, sizeof(name), "backend-cast-%d-%d-t%d", (int) types[k][0], (int) types[k][1], threads);
            run_one(c, b, name, t, threads);
        }
        {
            struct ggml_context * c = ctx_new();
            struct ggml_tensor * base = nt(c, GGML_TYPE_F32, 64, 8, 1, 1, 0);
            struct ggml_tensor * src = ggml_view_2d(c, base, 64, 4, base->nb[1] * 2, 0);
            struct ggml_tensor * dstbase = ggml_new_tensor_4d(c, GGML_TYPE_F32, 64, 8, 1, 1);
            memset(dstbase->data, 0, ggml_nbytes(dstbase));
            struct ggml_tensor * dst = ggml_view_2d(c, dstbase, 64, 4, dstbase->nb[1] * 2, 0);
            struct ggml_tensor * t = ggml_cpy(c, src, dst);
            char name[80]; snprintf(name, sizeof(name), "backend-cpy-strided-t%d", threads);
            run_one(c, b, name, t, threads);
        }
        ggml_backend_free(b);
    }
}
static void cases_workspace_stress(void) {
    const int threads = 8;
    struct ggml_backend * b = backend_new(threads);
    struct ggml_context * c = ctx_new();
    struct ggml_tensor * w = nt(c, GGML_TYPE_Q4_K, 4096, 8, 1, 1, 0);
    struct ggml_tensor * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, 4);
    int32_t * p = ids->data; p[0]=7; p[1]=0; p[2]=3; p[3]=5;
    struct ggml_tensor * rows = ggml_get_rows(c, w, ids);
    struct ggml_tensor * a = nt(c, GGML_TYPE_F32, 512, 4, 1, 1, 0);
    struct ggml_tensor * glu = ggml_glu(c, a, GGML_GLU_OP_SWIGLU, false);
    set_glu_params(glu);
    struct ggml_cgraph * g = ggml_new_graph(c);
    ggml_build_forward_expand(g, rows);
    ggml_build_forward_expand(g, glu);
    if (check_backend(ggml_backend_graph_compute(b, g), "workspace-stress")) {
        emit("backend-workspace-stress-rows", (int) rows->op, threads, rows->data, ggml_nbytes(rows));
        emit("backend-workspace-stress-glu", (int) glu->op, threads, glu->data, ggml_nbytes(glu));
    }
    ggml_free(c);
    ggml_backend_free(b);
}
int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    out = fopen(argv[1], "wb"); if (!out) return 2;
    cases_getrows();
    cases_glu();
    cases_convert();
    cases_workspace_stress();
    const struct fg006_integration_stats * st = &fg006_integration_stats;
    int has = (&fg006_integration_stats) != NULL;
    fprintf(out, "{\"summary\":\"%s\",\"cases\":%d,\"failures\":%d,\"integration_present\":%s,"
            "\"dispatcher\":%llu,\"planner\":%llu,\"planner_increases\":%llu,"
            "\"planner_old\":%zu,\"planner_need\":%zu,\"n1\":%zu,\"n8\":%zu,"
            "\"control\":%llu,\"control_threads\":%llu}\n",
            failures ? "FAILED" : "PASS", cases, failures, has ? "true" : "false",
            has ? (unsigned long long) st->dispatcher_calls : 0ull,
            has ? (unsigned long long) st->planner_calls : 0ull,
            has ? (unsigned long long) st->planner_increases : 0ull,
            has ? st->planner_old : 0u,
            has ? st->planner_need : 0u,
            has ? st->provider_workspace_n1 : 0u,
            has ? st->provider_workspace_n8 : 0u,
            has ? (unsigned long long) st->control_calls : 0ull,
            has ? (unsigned long long) st->control_threads : 0ull);
    fclose(out);
    if (failures) { fprintf(stderr, "BACKEND_CORPUS failures=%d cases=%d\n", failures, cases); return 1; }
    printf("BACKEND_CORPUS PASS cases=%d\n", cases);
    return 0;
}
