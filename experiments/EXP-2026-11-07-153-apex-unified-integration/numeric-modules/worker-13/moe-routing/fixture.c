// fixture.c - deterministic host differential for the Goose MoE routing port.
// The reference functions are scalar transcriptions of the native bitonic
// argsort/top-k and weighted-reduction kernels with identical stage/expert
// order.  The Goose side is the unmodified /compiler-emitted C driven one
// notional thread at a time.  No GPU execution is claimed here.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fg153-moe-abi.h"

int64_t fg153_goose_moe_argsort_host(int64_t cfg);
int64_t fg153_goose_moe_reduction_host(int64_t cfg);

int64_t fg153_h_tid;
int64_t fg153_h_bidx[3];
int64_t fg153_h_bdim;
int32_t *fg153_h_sh;
int64_t fg153_h_shcap;

static uint64_t g_rng = 0x243f6a8885a308d3ull;
static float frand(void) {
    g_rng = g_rng * 6364136223846793005ull + 1442695040888963407ull;
    return ((float)(uint32_t)(g_rng >> 33) / 2147483648.0f - 1.0f) * 3.0f;
}
static float special_val(uint32_t r) {
    switch (r % 17u) {
        case 0: return -0.0f;
        case 1: return 0.0f;
        case 2: return (float)NAN;
        case 3: return (float)INFINITY;
        case 4: return (float)-INFINITY;
        case 5: return 1.0f;
        case 6: return 1.0f;
        default: return frand();
    }
}
static int g_fail;
static void cmp_i32(const char *name, const char *field, const int32_t *a, const int32_t *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            fprintf(stderr, "FG153_FAIL %s %s i=%zu ref=%d goose=%d\n", name, field, i, a[i], b[i]);
            g_fail = 1;
            return;
        }
    }
    printf("FG153_TRACE case=%s field=%s bits=exact n=%zu PASS\n", name, field, n);
}
static void cmp_f32(const char *name, const char *field, const float *a, const float *b, size_t n) {
    size_t i;
    double maxd = 0.0;
    for (i = 0; i < n; i++) {
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0) {
            double d = fabs((double)a[i] - (double)b[i]);
            if (isnan(a[i]) != isnan(b[i])) d = 1e30;
            if (d > maxd) maxd = d;
        }
    }
    if (maxd == 0.0) {
        printf("FG153_TRACE case=%s field=%s bits=exact n=%zu PASS\n", name, field, n);
        return;
    }
    if (maxd <= 1e-6) {
        printf("FG153_TRACE case=%s field=%s max_abs_diff=%.9g n=%zu PASS\n", name, field, maxd, n);
        return;
    }
    fprintf(stderr, "FG153_FAIL %s %s max_abs_diff=%.9g n=%zu\n", name, field, maxd, n);
    g_fail = 1;
}

static int64_t next_pow2(int64_t x) {
    int64_t n = 1;
    while (n < x) n *= 2;
    return n;
}
static void swap_i32(int32_t *a, int32_t *b) { int32_t t = *a; *a = *b; *b = t; }

static void bitonic_step_ref(const float *x, int32_t ncols, int32_t *sh, int32_t col,
                             int32_t k, int32_t j, int order) {
    int32_t ixj = col ^ j;
    if (ixj <= col) return;
    if ((col & k) == 0) {
        if (sh[col] >= ncols ||
            (sh[ixj] < ncols && (order == 0 ? x[sh[col]] > x[sh[ixj]] : x[sh[col]] < x[sh[ixj]]))) {
            swap_i32(&sh[col], &sh[ixj]);
        }
    } else {
        if (sh[ixj] >= ncols ||
            (sh[col] < ncols && (order == 0 ? x[sh[col]] < x[sh[ixj]] : x[sh[col]] > x[sh[ixj]]))) {
            swap_i32(&sh[col], &sh[ixj]);
        }
    }
}

/* Native k_argsort_f32_i32 bitonic semantics. */
static void sort_ref(const fg153_moe_sort_cfg *c) {
    const float *x = (const float *)(uintptr_t)c->v[FG153_MOE_X];
    int32_t *dst = (int32_t *)(uintptr_t)c->v[FG153_MOE_DST];
    int32_t ncols = (int32_t)c->v[FG153_MOE_NCOLS];
    int32_t npad = (int32_t)c->v[FG153_MOE_NPAD];
    int32_t rows = (int32_t)c->v[FG153_MOE_ROWS];
    int32_t outcols = (int32_t)c->v[FG153_MOE_OUTCOLS];
    int order = (int)c->v[FG153_MOE_ORDER];
    int32_t *sh = (int32_t *)malloc((size_t)npad * sizeof(int32_t));
    int32_t row, col, k, j;
    for (row = 0; row < rows; row++) {
        const float *xr = x + (size_t)row * ncols;
        for (col = 0; col < npad; col++) sh[col] = col;
        for (k = 2; k <= npad; k *= 2) {
            for (j = k / 2; j > 0; j /= 2) {
                for (col = 0; col < npad; col++) bitonic_step_ref(xr, ncols, sh, col, k, j, order);
            }
        }
        for (col = 0; col < outcols; col++) dst[(size_t)row * outcols + col] = sh[col];
    }
    free(sh);
}

static void run_sort_goose(fg153_moe_sort_cfg *c) {
    int32_t npad = (int32_t)c->v[FG153_MOE_NPAD];
    int32_t rows = (int32_t)c->v[FG153_MOE_ROWS];
    int64_t bd = npad < 1024 ? npad : 1024;
    int64_t row, k, j, tid;
    for (row = 0; row < rows; row++) {
        fg153_h_bidx[0] = row; fg153_h_bidx[1] = 0; fg153_h_bidx[2] = 0;
        fg153_h_bdim = bd;
        c->v[FG153_MOE_PHASE] = 0;
        for (tid = 0; tid < bd; tid++) { fg153_h_tid = tid; (void)fg153_goose_moe_argsort_host((int64_t)(uintptr_t)c); }
        for (k = 2; k <= npad; k *= 2) {
            for (j = k / 2; j > 0; j /= 2) {
                c->v[FG153_MOE_PHASE] = 1;
                c->v[FG153_MOE_KK] = k;
                c->v[FG153_MOE_JJ] = j;
                for (tid = 0; tid < bd; tid++) { fg153_h_tid = tid; (void)fg153_goose_moe_argsort_host((int64_t)(uintptr_t)c); }
            }
        }
        c->v[FG153_MOE_PHASE] = 2;
        for (tid = 0; tid < bd; tid++) { fg153_h_tid = tid; (void)fg153_goose_moe_argsort_host((int64_t)(uintptr_t)c); }
    }
}

static float *xmalloc(size_t n) {
    float *p = (float *)malloc(n * sizeof(float));
    if (!p) { fprintf(stderr, "OOM\n"); exit(2); }
    return p;
}

static void sort_case(const char *name, int32_t ncols, int32_t rows, int order, int kind, int32_t outcols) {
    fg153_moe_sort_cfg c;
    int32_t npad = (int32_t)next_pow2(ncols);
    float *x = xmalloc((size_t)ncols * (size_t)rows);
    int32_t *ref = (int32_t *)malloc((size_t)ncols * (size_t)rows * sizeof(int32_t));
    int32_t *got = (int32_t *)malloc((size_t)ncols * (size_t)rows * sizeof(int32_t));
    size_t i, total = (size_t)ncols * (size_t)rows;
    for (i = 0; i < total; i++) { x[i] = special_val((uint32_t)i * 2654435761u + (uint32_t)ncols); ref[i] = got[i] = -777; }
    memset(&c, 0, sizeof(c));
    c.v[FG153_MOE_X] = (int64_t)(uintptr_t)x;
    c.v[FG153_MOE_NCOLS] = ncols;
    c.v[FG153_MOE_NPAD] = npad;
    c.v[FG153_MOE_ROWS] = rows;
    c.v[FG153_MOE_OUTCOLS] = outcols;
    c.v[FG153_MOE_ORDER] = order;
    c.v[FG153_MOE_KIND] = kind;
    c.v[FG153_MOE_DSTSTRIDE] = outcols;
    c.v[FG153_MOE_DST] = (int64_t)(uintptr_t)ref;
    sort_ref(&c);
    c.v[FG153_MOE_DST] = (int64_t)(uintptr_t)got;
    run_sort_goose(&c);
    cmp_i32(name, kind ? "topk" : "argsort", ref, got, (size_t)outcols * (size_t)rows);
    free(x); free(ref); free(got);
}

/* Native moe_weighted_reduction_f32 semantics. */
static void red_ref(const fg153_moe_reduce_cfg *c) {
    const float *exp = (const float *)(uintptr_t)c->v[FG153_MR_EXPERTS];
    const float *scale = c->v[FG153_MR_SCALE] ? (const float *)(uintptr_t)c->v[FG153_MR_SCALE] : NULL;
    const float *w = (const float *)(uintptr_t)c->v[FG153_MR_WEIGHTS];
    float *dst = (float *)(uintptr_t)c->v[FG153_MR_DST];
    int64_t ne = c->v[FG153_MR_NEMBD];
    int64_t nt = c->v[FG153_MR_NTOK];
    int64_t nu = c->v[FG153_MR_NUSED];
    int64_t token, col, e;
    for (token = 0; token < nt; token++) {
        for (col = 0; col < ne; col++) {
            const uint64_t first = (uint64_t)token * (uint64_t)nu;
            const float s0 = scale ? scale[first] : 1.0f;
            float sum = (exp[first * (uint64_t)ne + (uint64_t)col] * s0) * w[first];
            for (e = 1; e < nu; e++) {
                const uint64_t row = first + (uint64_t)e;
                const float sc = scale ? scale[row] : 1.0f;
                sum += (exp[row * (uint64_t)ne + (uint64_t)col] * sc) * w[row];
            }
            dst[(uint64_t)token * (uint64_t)ne + (uint64_t)col] = sum;
        }
    }
}

static void run_reduce_goose(fg153_moe_reduce_cfg *c) {
    int64_t ne = c->v[FG153_MR_NEMBD];
    int64_t nt = c->v[FG153_MR_NTOK];
    int64_t token, by, tid;
    for (token = 0; token < nt; token++) {
        for (by = 0; by * 256 < ne; by++) {
            fg153_h_bidx[0] = token; fg153_h_bidx[1] = by; fg153_h_bidx[2] = 0;
            fg153_h_bdim = 256;
            for (tid = 0; tid < 256; tid++) { fg153_h_tid = tid; (void)fg153_goose_moe_reduction_host((int64_t)(uintptr_t)c); }
        }
    }
}

static void red_case(const char *name, int64_t ne, int64_t nt, int64_t nu, int with_scale) {
    fg153_moe_reduce_cfg c;
    size_t esz = (size_t)(ne * nt * nu);
    size_t wsz = (size_t)(nt * nu);
    size_t dsz = (size_t)(ne * nt);
    float *exp = xmalloc(esz), *w = xmalloc(wsz), *sc = xmalloc(wsz);
    float *ref = xmalloc(dsz), *got = xmalloc(dsz);
    size_t i;
    for (i = 0; i < esz; i++) exp[i] = special_val((uint32_t)i * 2246822519u);
    for (i = 0; i < wsz; i++) { w[i] = frand(); sc[i] = 0.5f + 0.25f * frand(); }
    for (i = 0; i < dsz; i++) ref[i] = got[i] = 999.0f;
    memset(&c, 0, sizeof(c));
    c.v[FG153_MR_EXPERTS] = (int64_t)(uintptr_t)exp;
    c.v[FG153_MR_WEIGHTS] = (int64_t)(uintptr_t)w;
    c.v[FG153_MR_SCALE] = with_scale ? (int64_t)(uintptr_t)sc : 0;
    c.v[FG153_MR_NEMBD] = ne;
    c.v[FG153_MR_NTOK] = nt;
    c.v[FG153_MR_NUSED] = nu;
    c.v[FG153_MR_DST] = (int64_t)(uintptr_t)ref;
    red_ref(&c);
    c.v[FG153_MR_DST] = (int64_t)(uintptr_t)got;
    run_reduce_goose(&c);
    cmp_f32(name, "dst", ref, got, dsz);
    free(exp); free(w); free(sc); free(ref); free(got);
}

static void negatives(void) {
    fg153_moe_sort_cfg s;
    fg153_moe_reduce_cfg r;
    float canary[64];
    int32_t sh[16];
    int64_t i;
    int32_t *saved_sh = fg153_h_sh;
    int64_t saved_cap = fg153_h_shcap;
    struct { int field; int64_t value; int64_t code; const char *name; } bads[] = {
        {FG153_MOE_X, 0, 1, "xnull"},
        {FG153_MOE_NPAD, 7, 2, "pad"},
        {FG153_MOE_OUTCOLS, 99, 3, "outcols"},
        {FG153_MOE_ORDER, 2, 5, "order"},
        {FG153_MOE_KIND, 7, 6, "kind"},
        {FG153_MOE_OUTCOLS, 8, 7, "argsort-outcols-mismatch"}
    };
    for (i = 0; i < (int64_t)(sizeof(bads) / sizeof(bads[0])); i++) {
        int k;
        for (k = 0; k < 64; k++) canary[k] = 321.0f;
        for (k = 0; k < 16; k++) sh[k] = 7;
        memset(&s, 0, sizeof(s));
        s.v[FG153_MOE_X] = (int64_t)(uintptr_t)canary;
        s.v[FG153_MOE_DST] = (int64_t)(uintptr_t)canary;
        s.v[FG153_MOE_NCOLS] = 16;
        s.v[FG153_MOE_NPAD] = 16;
        s.v[FG153_MOE_ROWS] = 1;
        s.v[FG153_MOE_OUTCOLS] = 16;
        s.v[FG153_MOE_ORDER] = 0;
        s.v[FG153_MOE_KIND] = 0;
        s.v[FG153_MOE_DSTSTRIDE] = 16;
        s.v[FG153_MOE_PHASE] = 0;
        s.v[bads[i].field] = bads[i].value;
        fg153_h_sh = sh; fg153_h_shcap = 16;
        fg153_h_tid = 0; fg153_h_bidx[0] = 0; fg153_h_bidx[1] = 0; fg153_h_bidx[2] = 0; fg153_h_bdim = 16;
        int64_t rc = fg153_goose_moe_argsort_host((int64_t)(uintptr_t)&s);
        int ok = rc == -1 && s.v[FG153_MOE_ERROR] == bads[i].code && canary[0] == 321.0f;
        printf("FG153_TRACE case=neg-sort-%s rc=%lld err=%lld canary=%.1f %s\n",
               bads[i].name, (long long)rc, (long long)s.v[FG153_MOE_ERROR], canary[0], ok ? "PASS" : "FAIL");
        if (!ok) g_fail = 1;
    }
    struct { int field; int64_t value; int64_t code; const char *name; } badr[] = {
        {FG153_MR_EXPERTS, 0, 1, "expnull"},
        {FG153_MR_NEMBD, 0, 2, "nembd0"},
        {FG153_MR_NTOK, -1, 2, "ntok-neg"},
        {FG153_MR_NUSED, 0, 2, "nused0"}
    };
    for (i = 0; i < (int64_t)(sizeof(badr) / sizeof(badr[0])); i++) {
        int k;
        for (k = 0; k < 64; k++) canary[k] = 654.0f;
        memset(&r, 0, sizeof(r));
        r.v[FG153_MR_EXPERTS] = (int64_t)(uintptr_t)canary;
        r.v[FG153_MR_WEIGHTS] = (int64_t)(uintptr_t)canary;
        r.v[FG153_MR_DST] = (int64_t)(uintptr_t)canary;
        r.v[FG153_MR_NEMBD] = 1; r.v[FG153_MR_NTOK] = 1; r.v[FG153_MR_NUSED] = 1;
        r.v[badr[i].field] = badr[i].value;
        fg153_h_tid = 0; fg153_h_bidx[0] = 0; fg153_h_bidx[1] = 0; fg153_h_bidx[2] = 0; fg153_h_bdim = 256;
        int64_t rc = fg153_goose_moe_reduction_host((int64_t)(uintptr_t)&r);
        int ok = rc == -1 && r.v[FG153_MR_ERROR] == badr[i].code && canary[0] == 654.0f;
        printf("FG153_TRACE case=neg-reduce-%s rc=%lld err=%lld canary=%.1f %s\n",
               badr[i].name, (long long)rc, (long long)r.v[FG153_MR_ERROR], canary[0], ok ? "PASS" : "FAIL");
        if (!ok) g_fail = 1;
    }
    fg153_h_sh = saved_sh;
    fg153_h_shcap = saved_cap;
}

int main(void) {
    fg153_h_shcap = 12288;
    fg153_h_sh = (int32_t *)malloc((size_t)fg153_h_shcap * sizeof(int32_t));
    if (!fg153_h_sh) return 2;
    printf("FG153_STAGE moe-routing-host-differential\n");
    sort_case("argsort-desc-1", 1, 1, 1, 0, 1);
    sort_case("argsort-desc-2", 2, 3, 1, 0, 2);
    sort_case("argsort-desc-3", 3, 2, 1, 0, 3);
    sort_case("argsort-desc-7", 7, 3, 1, 0, 7);
    sort_case("argsort-desc-8", 8, 2, 1, 0, 8);
    sort_case("argsort-desc-16", 16, 3, 1, 0, 16);
    sort_case("argsort-desc-17", 17, 2, 1, 0, 17);
    sort_case("argsort-desc-256", 256, 3, 1, 0, 256);
    sort_case("argsort-desc-1000", 1000, 1, 1, 0, 1000);
    sort_case("argsort-asc-17", 17, 2, 0, 0, 17);
    sort_case("argsort-asc-256", 256, 1, 0, 0, 256);
    sort_case("topk-256-k8", 256, 3, 1, 1, 8);
    sort_case("topk-17-k2", 17, 2, 1, 1, 2);
    sort_case("topk-256-k1", 256, 4, 1, 1, 1);
    red_case("reduce-ne1", 1, 1, 1, 0);
    red_case("reduce-ne7", 7, 3, 2, 0);
    red_case("reduce-ne256", 256, 2, 8, 1);
    red_case("reduce-ne1000", 1000, 1, 8, 0);
    red_case("reduce-ne1000-scale", 1000, 3, 4, 1);
    negatives();
    free(fg153_h_sh);
    if (g_fail) { fprintf(stderr, "FG153_FIXTURE_FAIL\n"); return 1; }
    printf("FG153_FIXTURE_PASS\n");
    return 0;
}
