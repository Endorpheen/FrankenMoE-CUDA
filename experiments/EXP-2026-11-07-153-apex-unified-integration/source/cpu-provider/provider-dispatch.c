// Default-on CPU provider dispatcher for the APX013 GetRows/GLU/conversion
// block. Numerical work is the generated Goose unit in provider-unit.c.
// The workspace callback is deliberately sized with the real worker count,
// not the native n_tasks value: the Goose kernels split by params.nth.
#include "provider.h"
#include "ggml.h"
#include "ggml-cpu-impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fg006_enabled = 1;
__attribute__((constructor)) static void fg006_config(void) {
    const char * v = getenv("GGML_GOOSE_CPU_PROVIDER");
    fg006_enabled = !v || strcmp(v, "0");
}
int fg006_provider_enabled(void) { return fg006_enabled; }

static float fg006_f32(const struct ggml_tensor * t, int at) {
    float f;
    memcpy(&f, (const char *) t->op_params + at * 4, 4);
    return f;
}
static void fill_tensor(struct fg006_tensor * s, const struct ggml_tensor * t) {
    memset(s, 0, sizeof(*s));
    if (!t) return;
    for (int i = 0; i < 4; ++i) { s->ne[i] = t->ne[i]; s->nb[i] = t->nb[i]; }
    s->kind = (int64_t) t->type;
    s->bytes = (int64_t) ggml_nbytes(t);
}
struct fg006_node fg006_describe(const struct ggml_tensor * t, int ith, int nth) {
    struct fg006_node p;
    memset(&p, 0, sizeof(p));
    fill_tensor(&p.x, t->src[0]);
    fill_tensor(&p.y, t->src[1]);
    fill_tensor(&p.z, t->src[2]);
    fill_tensor(&p.d, t);
    p.op = (int64_t) t->op;
    p.ith = ith;
    p.nth = nth;
    for (int i = 0; i < 16; ++i) p.ip[i] = t->op_params[i];
    for (int i = 0; i < 8; ++i) p.fp[i] = fg006_f32(t, i);
    p.ip[13] = 1;
#if defined(__AVX2__) && defined(__FMA__)
    p.ip[12] = 1;
#else
    p.ip[12] = 0;
#endif
    if (t->op == GGML_OP_GLU) {
        p.ip[2] = t->src[1] != NULL;
        p.fp[0] = fg006_f32(t, 2);
        p.fp[1] = fg006_f32(t, 3);
    }
    return p;
}

static int64_t tensor_elements(const struct fg006_tensor * t) {
    int64_t n = 1;
    for (int i = 0; i < 4; ++i) n *= t->ne[i];
    return n;
}
static int64_t tensor_rows(const struct fg006_tensor * t) {
    return t->ne[1] * t->ne[2] * t->ne[3];
}
static int same_shape(const struct fg006_tensor * a, const struct fg006_tensor * b) {
    for (int i = 0; i < 4; ++i) if (a->ne[i] != b->ne[i]) return 0;
    return 1;
}
static int is_scalar_kind(int k) {
    return k == GGML_TYPE_F32 || k == GGML_TYPE_F16 || k == GGML_TYPE_BF16 ||
           k == GGML_TYPE_I8 || k == GGML_TYPE_I16 || k == GGML_TYPE_I32 ||
           k == GGML_TYPE_I64 || k == GGML_TYPE_F64;
}
static int is_apex_quant(int k) {
    return k == GGML_TYPE_Q4_K || k == GGML_TYPE_Q5_K || k == GGML_TYPE_Q6_K;
}
static int native_cast_supported(int s, int d) {
    if (s == d) return is_scalar_kind(s);
    if (s == GGML_TYPE_F32 && (d == GGML_TYPE_F16 || d == GGML_TYPE_BF16 || d == GGML_TYPE_I32)) return 1;
    if (s == GGML_TYPE_F16 && (d == GGML_TYPE_F32 || d == GGML_TYPE_BF16)) return 1;
    if (s == GGML_TYPE_BF16 && (d == GGML_TYPE_F32 || d == GGML_TYPE_F16)) return 1;
    if (s == GGML_TYPE_I32 && d == GGML_TYPE_F32) return 1;
    return 0;
}

int fg006_supported_node(const struct fg006_node * p) {
    if (p->nth <= 0 || p->ith < 0 || p->ith >= p->nth) return 0;
    if (p->op == GGML_OP_GET_ROWS) {
        if (p->d.kind != GGML_TYPE_F32 && p->d.kind != GGML_TYPE_I32) return 0;
        if (p->y.kind != GGML_TYPE_I32) return 0;
        if (p->d.ne[0] <= 0 || p->d.ne[0] != p->x.ne[0]) return 0;
        if (p->x.ne[2] != p->y.ne[1] || p->x.ne[3] != p->y.ne[2] || p->y.ne[3] != 1) return 0;
        if (tensor_elements(&p->y) != tensor_rows(&p->d)) return 0;
        if (p->d.kind == GGML_TYPE_I32)
            return p->x.kind == GGML_TYPE_I32 && p->x.nb[0] == 4 && p->d.nb[0] == 4;
        if (p->x.kind == GGML_TYPE_F32 || p->x.kind == GGML_TYPE_F16 || p->x.kind == GGML_TYPE_BF16) {
            int64_t bytes = ggml_type_size((enum ggml_type) p->x.kind);
            return p->x.nb[0] == bytes;
        }
        return is_apex_quant((int) p->x.kind) && p->x.ne[0] > 0 && p->x.ne[0] % 256 == 0;
    }
    if (p->op == GGML_OP_GLU) {
        if (p->d.kind != GGML_TYPE_F32 && p->d.kind != GGML_TYPE_F16 && p->d.kind != GGML_TYPE_BF16) return 0;
        if (p->x.kind != p->d.kind) return 0;
        int64_t bytes = ggml_type_size((enum ggml_type) p->x.kind);
        if (p->x.nb[0] != bytes || p->d.nb[0] != bytes) return 0;
        if (p->d.ne[0] <= 0 || tensor_rows(&p->x) != tensor_rows(&p->d)) return 0;
        if (p->ip[0] < 0 || p->ip[0] >= GGML_GLU_OP_COUNT) return 0;
        if (p->ip[2] != 0) {
            if (p->y.kind != p->x.kind || p->y.nb[0] != bytes) return 0;
            if (!same_shape(&p->x, &p->y) || !same_shape(&p->x, &p->d)) return 0;
        } else if (p->x.ne[0] != 2 * p->d.ne[0]) return 0;
        return 1;
    }
    if (p->op == GGML_OP_DUP || p->op == GGML_OP_CPY || p->op == GGML_OP_CONT) {
        if (tensor_elements(&p->x) != tensor_elements(&p->d)) return 0;
        if (native_cast_supported((int) p->x.kind, (int) p->d.kind)) return 1;
        if (p->d.kind == GGML_TYPE_F32 && is_apex_quant((int) p->x.kind)) {
            return p->x.ne[0] > 0 && p->x.ne[0] % 256 == 0 && p->d.nb[0] == 4 &&
                   tensor_elements(&p->d) % 256 == 0;
        }
        return 0;
    }
    return 0;
}

void fg006_workspace_node(const struct fg006_node * p, int64_t nth, int64_t info[4]) {
    info[0] = info[1] = info[2] = info[3] = 0;
    if (!fg006_supported_node(p)) return;
    fg006_goose_workspace(p, nth, info);
}

size_t fg006_extra_workspace(const struct ggml_tensor * t, int n_threads) {
    if (!fg006_enabled || n_threads <= 0) return 0;
    struct fg006_node p = fg006_describe(t, 0, n_threads);
    if (!fg006_supported_node(&p)) return 0;
    int64_t info[4];
    fg006_goose_workspace(&p, n_threads, info);
    if (info[3] < 0 || (uint64_t) info[3] > SIZE_MAX) abort();
    return (size_t) info[3];
}

// Reproduces the pre-fix graph-plan request, which passed the native n_tasks
// value (GET_ROWS = 1) while the Goose kernel runs with params.nth workers.
size_t fg006_extra_workspace_legacy(const struct ggml_tensor * t, int n_tasks) {
    if (!fg006_enabled || n_tasks <= 0) return 0;
    struct fg006_node p = fg006_describe(t, 0, n_tasks);
    if (!fg006_supported_node(&p)) return 0;
    int64_t info[4];
    fg006_goose_workspace(&p, n_tasks, info);
    if (info[3] < 0 || (uint64_t) info[3] > SIZE_MAX) abort();
    return (size_t) info[3];
}

int fg006_validate_ids_node(const struct fg006_node * p, const void * ids) {
    if (p->op != GGML_OP_GET_ROWS || p->y.kind != GGML_TYPE_I32) return 0;
    return fg006_goose_validate(p, ids) ? 1 : 0;
}

uint8_t fg006_try_raw(int ith, int nth, size_t wsize, void * wdata, int use_ref,
        struct ggml_tensor * t) {
    if (!fg006_enabled || use_ref) return 0;
    struct fg006_node p = fg006_describe(t, ith, nth);
    if (!fg006_supported_node(&p)) return 0;
    int64_t info[4];
    fg006_goose_workspace(&p, nth, info);
    if (info[3] < 0 || (uint64_t) info[3] > SIZE_MAX) abort();
    if ((size_t) info[3] > wsize) {
        fprintf(stderr, "FG006_WORKSPACE_OVERFLOW op=%lld need=%llu have=%zu threads=%d\n",
                (long long) p.op, (unsigned long long) info[3], wsize, nth);
        abort();
    }
    uint8_t * work = (uint8_t *) wdata;
    float * scratch = info[0] ? (float *) work : NULL;
    uint8_t * raw = info[1] ? work + info[2] : NULL;
    uint8_t ok = fg006_goose_compute(&p,
            t->src[0] ? t->src[0]->data : NULL,
            t->src[1] ? t->src[1]->data : NULL,
            t->src[2] ? t->src[2]->data : NULL,
            t->data, scratch, info[0], raw, info[1], NULL, 0);
    if (!ok) {
        fprintf(stderr, "FG006_GOOSE_REFUSED op=%lld\n", (long long) p.op);
        abort();
    }
    return 1;
}

uint8_t fg006_try(const struct ggml_compute_params * params, struct ggml_tensor * t) {
    return fg006_try_raw(params->ith, params->nth, params->wsize, params->wdata,
            params->use_ref ? 1 : 0, t);
}
