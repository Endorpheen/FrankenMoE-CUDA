/* worker-10 host differential corpus: generated Goose vs native CPU reference.
 * No model, no GPU, no benchmark.  Covers the reachable Apex movement paths:
 * F32/F16 strided/contiguous/transposed copy, get_rows, set_rows, Q4_0 KV
 * quantize/dequantize block layout, alias/self-copy, bounds and negative cases.
 */
#include "apex-move-bridge.h"
#include "ggml-quants.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int type;
    int64_t ne[4];
    int64_t nb[4];
    uint8_t * data;
    size_t cap;
} TT;

static int failures;
static int cases;
static uint64_t hash = 1469598103934665603ULL;

static void mix(const void * p, size_t n) {
    const uint8_t * b = p;
    for (size_t i = 0; i < n; ++i) { hash ^= b[i]; hash *= 1099511628211ULL; }
}
static void check(int ok, const char * what) {
    ++cases;
    if (!ok) { ++failures; fprintf(stderr, "FAIL %s\n", what); }
}
static void * xcalloc(size_t n) { void * p = calloc(1, n); if (!p) abort(); return p; }

static size_t tsize(int type) {
    switch (type) {
        case GGML_TYPE_F32: return 4;
        case GGML_TYPE_F16: return 2;
        case GGML_TYPE_I32: return 4;
        case GGML_TYPE_I64: return 8;
        case GGML_TYPE_Q4_0: return sizeof(block_q4_0);
        default: abort();
    }
}
static size_t trow(int type, int64_t ne0) {
    if (type == GGML_TYPE_Q4_0) return (size_t) (ne0 / 32) * sizeof(block_q4_0);
    return (size_t) ne0 * tsize(type);
}
static void tt_alloc(TT * t, int type, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3) {
    memset(t, 0, sizeof *t);
    t->type = type; t->ne[0] = ne0; t->ne[1] = ne1; t->ne[2] = ne2; t->ne[3] = ne3;
    t->nb[0] = (int64_t) tsize(type);
    t->nb[1] = (int64_t) trow(type, ne0);
    t->nb[2] = t->nb[1] * ne1;
    t->nb[3] = t->nb[2] * ne2;
    t->cap = (size_t) (t->nb[3] * ne3 + 64);
    t->data = xcalloc(t->cap);
}
static void tt_free(TT * t) { free(t->data); memset(t, 0, sizeof *t); }

static float f16v(uint16_t b) { return ggml_fp16_to_fp32((ggml_fp16_t) b); }
static uint16_t f16b(float v) { return (uint16_t) ggml_fp32_to_fp16(v); }

static float ref_load(int type, const uint8_t * base, int64_t off) {
    if (type == GGML_TYPE_F32) { float v; memcpy(&v, base + off, 4); return v; }
    if (type == GGML_TYPE_F16) { uint16_t v; memcpy(&v, base + off, 2); return f16v(v); }
    abort();
}
static void ref_store(int type, uint8_t * base, int64_t off, float v) {
    if (type == GGML_TYPE_F32) { memcpy(base + off, &v, 4); return; }
    if (type == GGML_TYPE_F16) { uint16_t b = f16b(v); memcpy(base + off, &b, 2); return; }
    abort();
}
static void ref_store_bits(int type, uint8_t * base, int64_t off, uint16_t bits) {
    if (type == GGML_TYPE_F16) { memcpy(base + off, &bits, 2); return; }
    ref_store(type, base, off, f16v(bits));
}
static uint16_t ref_load_bits(int type, const uint8_t * base, int64_t off) {
    if (type == GGML_TYPE_F16) { uint16_t b; memcpy(&b, base + off, 2); return b; }
    return f16b(ref_load(type, base, off));
}

static void decompose(int64_t i, int64_t ne0, int64_t ne1, int64_t ne2,
                      int64_t * i0, int64_t * i1, int64_t * i2, int64_t * i3) {
    int64_t m = ne0 * ne1 * ne2;
    *i3 = i / m; int64_t r = i - *i3 * m;
    *i2 = r / (ne0 * ne1); r -= *i2 * ne0 * ne1;
    *i1 = r / ne0; *i0 = r - *i1 * ne0;
}
static int64_t off_of(const int64_t * idx, const int64_t * nb) {
    return idx[0]*nb[0] + idx[1]*nb[1] + idx[2]*nb[2] + idx[3]*nb[3];
}

static void ref_copy_scalar(const TT * a, const TT * b) {
    const int64_t n = a->ne[0]*a->ne[1]*a->ne[2]*a->ne[3];
    for (int64_t i = 0; i < n; ++i) {
        int64_t ia[4], ib[4];
        decompose(i, a->ne[0], a->ne[1], a->ne[2], &ia[0], &ia[1], &ia[2], &ia[3]);
        decompose(i, b->ne[0], b->ne[1], b->ne[2], &ib[0], &ib[1], &ib[2], &ib[3]);
        int64_t so = off_of(ia, a->nb), dof = off_of(ib, b->nb);
        if (a->type == GGML_TYPE_F16 && b->type == GGML_TYPE_F16) {
            ref_store_bits(b->type, b->data, dof, ref_load_bits(a->type, a->data, so));
        } else {
            ref_store(b->type, b->data, dof, ref_load(a->type, a->data, so));
        }
    }
}
static void ref_copy_contig(const TT * a, const TT * b) {
    const int64_t n = a->ne[0]*a->ne[1]*a->ne[2]*a->ne[3];
    for (int64_t i = 0; i < n; ++i) {
        int64_t so = i * (int64_t) tsize(a->type), dof = i * (int64_t) tsize(b->type);
        if (a->type == GGML_TYPE_F16 && b->type == GGML_TYPE_F16)
            ref_store_bits(b->type, b->data, dof, ref_load_bits(a->type, a->data, so));
        else ref_store(b->type, b->data, dof, ref_load(a->type, a->data, so));
    }
}
static void ref_transpose(const TT * a, const TT * b) {
    const int64_t ne0 = a->ne[0], ne1 = a->ne[1], nmat = a->ne[2];
    for (int64_t imat = 0; imat < nmat; ++imat) {
        for (int64_t row = 0; row < ne1; ++row) {
            for (int64_t col = 0; col < ne0; ++col) {
                int64_t so = imat*(ne0*ne1) + col*ne1 + row;
                int64_t dof = imat*(ne0*ne1) + row*ne0 + col;
                if (a->type == GGML_TYPE_F16 && b->type == GGML_TYPE_F16)
                    ref_store_bits(b->type, b->data, dof*2, ref_load_bits(a->type, a->data, so*2));
                else ref_store(b->type, b->data, dof*(int64_t)tsize(b->type),
                               ref_load(a->type, a->data, so*(int64_t)tsize(a->type)));
            }
        }
    }
}

static void ref_getrows(const TT * a, const TT * idx, const TT * b, int64_t nrows) {
    const int64_t ne0 = a->ne[0], ne1 = a->ne[1], ne2 = a->ne[2];
    for (int64_t i10 = 0; i10 < nrows; ++i10) {
        for (int64_t z = 0; z < idx->ne[1]*idx->ne[2]; ++z) {
            int64_t i11 = z / idx->ne[2], i12 = z - i11*idx->ne[2];
            int32_t i01;
            memcpy(&i01, idx->data + i10*idx->nb[0] + i11*idx->nb[1] + i12*idx->nb[2], 4);
            int64_t drow = i10*b->nb[1] + i11*b->nb[2] + i12*b->nb[3];
            if (a->type == GGML_TYPE_Q4_0) {
                float tmp[1024];
                dequantize_row_q4_0((const block_q4_0 *)(a->data + i01*a->nb[1] + i11*a->nb[2] + i12*a->nb[3]), tmp, ne0);
                for (int64_t i0 = 0; i0 < ne0; ++i0) {
                    if (b->type == GGML_TYPE_F16) { uint16_t v = f16b(tmp[i0]); memcpy(b->data + drow + i0*2, &v, 2); }
                    else memcpy(b->data + drow + i0*4, &tmp[i0], 4);
                }
            } else {
                const uint8_t * srow = a->data + i01*a->nb[1] + i11*a->nb[2] + i12*a->nb[3];
                for (int64_t i0 = 0; i0 < ne0; ++i0) {
                    if (a->type == GGML_TYPE_F16 && b->type == GGML_TYPE_F16)
                        ref_store_bits(b->type, b->data, drow + i0*2, ref_load_bits(a->type, srow, i0*2));
                    else ref_store(b->type, b->data, drow + i0*(int64_t)tsize(b->type),
                                   ref_load(a->type, srow, i0*(int64_t)tsize(a->type)));
                }
            }
        }
    }
}

static void ref_setrows(const TT * a, const TT * idx, const TT * b, int quant) {
    const int64_t ne0=a->ne[0], ne1=a->ne[1], ne2=a->ne[2], ne3=a->ne[3];
    const int64_t units = quant ? (ne0*ne1*ne2*ne3)/32 : ne0*ne1*ne2*ne3;
    for (int64_t u = 0; u < units; ++u) {
        int64_t ii = quant ? u*32 : u;
        int64_t i0,i1,i2,i3; decompose(ii, ne0, ne1, ne2, &i0,&i1,&i2,&i3);
        int64_t i11 = i2 % idx->ne[1], i12 = i3 % idx->ne[2], i10 = i1;
        int64_t iv;
        if (idx->type == GGML_TYPE_I32) { int32_t v; memcpy(&v, idx->data+i10*idx->nb[0]+i11*idx->nb[1]+i12*idx->nb[2],4); iv=v; }
        else { int64_t v; memcpy(&v, idx->data+i10*idx->nb[0]+i11*idx->nb[1]+i12*idx->nb[2],8); iv=v; }
        int64_t so = i1*a->nb[1] + i2*a->nb[2] + i3*a->nb[3] + i0*(int64_t)tsize(a->type);
        int64_t dof = iv*b->nb[1] + i2*b->nb[2] + i3*b->nb[3];
        if (quant) {
            quantize_row_q4_0_ref((const float *)(a->data+so),
                (block_q4_0 *)(b->data + dof + (i0/32)*sizeof(block_q4_0)), 32);
        } else {
            dof += i0*(int64_t)tsize(b->type);
            if (a->type == GGML_TYPE_F16 && b->type == GGML_TYPE_F16)
                ref_store_bits(b->type, b->data, dof, ref_load_bits(a->type, a->data, so));
            else ref_store(b->type, b->data, dof, ref_load(a->type, a->data, so));
        }
    }
}

static void fill_copy_params(fg153_am_params * p, const TT * a, const TT * b, int op) {
    memset(p, 0, sizeof *p);
    p->v[FG153_AM_P_OP] = op;
    p->v[FG153_AM_P_SRC] = (int64_t)(uintptr_t)a->data;
    p->v[FG153_AM_P_DST] = (int64_t)(uintptr_t)b->data;
    p->v[FG153_AM_P_ST] = a->type; p->v[FG153_AM_P_DT] = b->type;
    for (int i = 0; i < 4; ++i) {
        p->v[FG153_AM_P_NE00+i] = a->ne[i];
        p->v[FG153_AM_P_NB00+i] = a->nb[i];
        p->v[FG153_AM_P_NE10+i] = b->ne[i];
        p->v[FG153_AM_P_NB10+i] = b->nb[i];
    }
    int64_t n = a->ne[0]*a->ne[1]*a->ne[2]*a->ne[3];
    p->v[FG153_AM_P_N] = n;
}
static void fill_getrows_params(fg153_am_params * p, const TT * a, const TT * idx, const TT * b) {
    memset(p, 0, sizeof *p);
    p->v[FG153_AM_P_OP] = FG153_AM_OP_GETROWS;
    p->v[FG153_AM_P_SRC] = (int64_t)(uintptr_t)a->data;
    p->v[FG153_AM_P_DST] = (int64_t)(uintptr_t)b->data;
    p->v[FG153_AM_P_IDX] = (int64_t)(uintptr_t)idx->data;
    p->v[FG153_AM_P_ST] = a->type; p->v[FG153_AM_P_DT] = b->type; p->v[FG153_AM_P_IT] = idx->type;
    for (int i = 0; i < 4; ++i) {
        p->v[FG153_AM_P_NE00+i] = a->ne[i];
        p->v[FG153_AM_P_NB00+i] = a->nb[i];
        p->v[FG153_AM_P_NE10+i] = idx->ne[i];
        p->v[FG153_AM_P_NB10+i] = idx->nb[i];
    }
    p->v[FG153_AM_P_NB1] = b->nb[1]; p->v[FG153_AM_P_NB2] = b->nb[2]; p->v[FG153_AM_P_NB3] = b->nb[3];
    p->v[FG153_AM_P_AUX0] = idx->ne[0];
    p->v[FG153_AM_P_AUX2] = a->type == GGML_TYPE_Q4_0 ? 1 : 0;
}
static void fill_setrows_params(fg153_am_params * p, const TT * a, const TT * idx, const TT * b) {
    memset(p, 0, sizeof *p);
    p->v[FG153_AM_P_OP] = FG153_AM_OP_SETROWS;
    p->v[FG153_AM_P_SRC] = (int64_t)(uintptr_t)a->data;
    p->v[FG153_AM_P_DST] = (int64_t)(uintptr_t)b->data;
    p->v[FG153_AM_P_IDX] = (int64_t)(uintptr_t)idx->data;
    p->v[FG153_AM_P_ST] = a->type; p->v[FG153_AM_P_DT] = b->type; p->v[FG153_AM_P_IT] = idx->type;
    for (int i = 0; i < 4; ++i) {
        p->v[FG153_AM_P_NE00+i] = a->ne[i];
        p->v[FG153_AM_P_NB01+i] = a->nb[i+1];
        p->v[FG153_AM_P_NE10+i] = idx->ne[i];
        p->v[FG153_AM_P_NB10+i] = idx->nb[i];
    }
    p->v[FG153_AM_P_NB1] = b->nb[1]; p->v[FG153_AM_P_NB2] = b->nb[2]; p->v[FG153_AM_P_NB3] = b->nb[3];
    int64_t n = a->ne[0]*a->ne[1]*a->ne[2]*a->ne[3];
    p->v[FG153_AM_P_N] = b->type == GGML_TYPE_Q4_0 ? n/32 : n;
}
static void fill_random_f32(float * x, int64_t n, uint32_t seed) {
    for (int64_t i = 0; i < n; ++i) {
        seed = seed*1664525u + 1013904223u;
        int32_t s = (int32_t)(seed >> 8);
        x[i] = (float)(s % 20000) / 337.0f;
    }
}

static void run_copy_case(const char * name, int st, int dt, int mode) {
    int64_t ne0 = 17, ne1 = 5, ne2 = 3, ne3 = 2;
    if (mode == 1) { ne0 = 40; ne1 = 3; ne2 = 2; ne3 = 1; }
    if (mode == 2) { ne0 = 6; ne1 = 6; ne2 = 1; ne3 = 1; }
    TT a, b, r;
    tt_alloc(&a, st, ne0, ne1, ne2, ne3);
    tt_alloc(&b, dt, ne0, ne1, ne2, ne3);
    const int64_t n = ne0*ne1*ne2*ne3;
    if (st == GGML_TYPE_F32) {
        fill_random_f32((float *)a.data, n, 7u + (uint32_t)mode);
    } else {
        float * tmp = xcalloc((size_t)n * sizeof(float));
        fill_random_f32(tmp, n, 11u + (uint32_t)mode);
        for (int64_t i = 0; i < n; ++i) { uint16_t v = f16b(tmp[i]); memcpy(a.data + i*2, &v, 2); }
        free(tmp);
    }
    if (mode == 0) {
        b.nb[0] = (int64_t)tsize(dt);
        b.nb[1] = (int64_t)trow(dt, b.ne[0]) + (int64_t)tsize(dt);
        b.nb[2] = b.nb[1]*b.ne[1] + (int64_t)tsize(dt);
        b.nb[3] = b.nb[2]*b.ne[2];
        b.cap = (size_t)(b.nb[3]*b.ne[3] + 64);
        b.data = realloc(b.data, b.cap);
        memset(b.data, 0x5a, b.cap);
    }
    if (mode == 2) {
        /* transposed view: nb0 is the physical row stride, nb1 is element size */
        a.nb[0] = (int64_t)trow(st, ne1);
        a.nb[1] = (int64_t)tsize(st);
        a.nb[2] = a.nb[0]*ne1;
        a.nb[3] = a.nb[2]*ne2;
        a.cap = (size_t)(a.nb[3]*ne3 + 64);
        a.data = realloc(a.data, a.cap);
    }
    tt_alloc(&r, dt, b.ne[0], b.ne[1], b.ne[2], b.ne[3]);
    r.nb[0]=b.nb[0]; r.nb[1]=b.nb[1]; r.nb[2]=b.nb[2]; r.nb[3]=b.nb[3];
    r.cap=b.cap; free(r.data); r.data=xcalloc(r.cap); memcpy(r.data,b.data,b.cap);

    fg153_am_params p;
    const int op = mode == 0 ? FG153_AM_OP_COPY_SCALAR :
                   mode == 1 ? FG153_AM_OP_COPY_CONTIG : FG153_AM_OP_TRANSPOSE;
    fill_copy_params(&p, &a, &b, op);
    if (mode == 2) ref_transpose(&a, &r);
    else if (mode == 1) ref_copy_contig(&a, &r);
    else ref_copy_scalar(&a, &r);
    check(fg153_am_can_run((int64_t)p.v[FG153_AM_P_OP], &p) == 1, name);
    fg153_am_call((int64_t)p.v[FG153_AM_P_OP], 1, (int64_t)(uintptr_t)&p);
    check(memcmp(b.data, r.data, b.cap) == 0, name);
    mix(b.data, b.cap);
    tt_free(&a); tt_free(&b); tt_free(&r);
}

static void run_getrows_case(const char * name, int st, int dt) {
    TT a, idx, b, r;
    const int64_t ne0 = st == GGML_TYPE_Q4_0 ? 64 : 19;
    tt_alloc(&a, st, ne0, 9, 3, 2);
    tt_alloc(&idx, GGML_TYPE_I32, 4, 3, 2, 1);
    tt_alloc(&b, dt, ne0, idx.ne[0], idx.ne[1], idx.ne[2]);
    int64_t n0 = a.ne[0]*a.ne[1]*a.ne[2]*a.ne[3];
    if (st == GGML_TYPE_Q4_0) {
        float * tmp = xcalloc((size_t)n0*sizeof(float));
        fill_random_f32(tmp, n0, 21u);
        quantize_row_q4_0_ref(tmp, (block_q4_0 *)a.data, n0);
        free(tmp);
    } else if (st == GGML_TYPE_F32) {
        fill_random_f32((float *)a.data, n0, 22u);
    } else {
        float * tmp = xcalloc((size_t)n0*sizeof(float));
        fill_random_f32(tmp, n0, 23u);
        for (int64_t i=0;i<n0;++i){ uint16_t v=f16b(tmp[i]); memcpy(a.data+i*2,&v,2);} free(tmp);
    }
    int32_t * ids = (int32_t *)idx.data;
    for (int64_t i=0;i<4;++i) for (int64_t j=0;j<3;++j) for (int64_t k=0;k<2;++k) {
        int64_t lin = (i*3+j)*2+k;
        ids[lin] = (int32_t)((lin*5 + 1) % a.ne[1]);
    }
    tt_alloc(&r, dt, b.ne[0], b.ne[1], b.ne[2], b.ne[3]);
    r.nb[0]=b.nb[0]; r.nb[1]=b.nb[1]; r.nb[2]=b.nb[2]; r.nb[3]=b.nb[3];
    r.cap=b.cap; free(r.data); r.data=xcalloc(r.cap); memcpy(r.data,b.data,b.cap);

    fg153_am_params p;
    fill_getrows_params(&p, &a, &idx, &b);
    ref_getrows(&a, &idx, &r, idx.ne[0]);
    check(fg153_am_can_run(FG153_AM_OP_GETROWS, &p) == 1, name);
    fg153_am_call(FG153_AM_OP_GETROWS, 1, (int64_t)(uintptr_t)&p);
    check(memcmp(b.data, r.data, b.cap) == 0, name);
    mix(b.data, b.cap);
    tt_free(&a); tt_free(&idx); tt_free(&b); tt_free(&r);
}

static void run_setrows_case(const char * name, int dt, int it) {
    TT a, idx, b, r;
    const int64_t ne0 = dt == GGML_TYPE_Q4_0 ? 64 : 20;
    tt_alloc(&a, GGML_TYPE_F32, ne0, 6, 1, 1);
    tt_alloc(&idx, it, 6, 1, 1, 1);
    int64_t nrows = 11;
    if (dt == GGML_TYPE_Q4_0) {
        tt_alloc(&b, dt, ne0, nrows, 1, 1);
        b.nb[1]=(int64_t)trow(dt,ne0); b.nb[2]=b.nb[1]*nrows; b.nb[3]=b.nb[2];
    } else {
        tt_alloc(&b, dt, ne0, nrows, 1, 1);
    }
    int64_t n0=a.ne[0]*a.ne[1]*a.ne[2]*a.ne[3];
    fill_random_f32((float *)a.data, n0, 31u + (uint32_t)dt*3u);
    for (int64_t i=0;i<idx.ne[0];++i) for (int64_t j=0;j<idx.ne[1];++j) {
        int64_t v = (i*7 + j*3) % nrows;
        if (it == GGML_TYPE_I32) ((int32_t*)idx.data)[i*idx.ne[1]+j]=(int32_t)v;
        else ((int64_t*)idx.data)[i*idx.ne[1]+j]=v;
    }
    tt_alloc(&r, dt, b.ne[0], b.ne[1], b.ne[2], b.ne[3]);
    r.nb[0]=b.nb[0]; r.nb[1]=b.nb[1]; r.nb[2]=b.nb[2]; r.nb[3]=b.nb[3];
    r.cap=b.cap; free(r.data); r.data=xcalloc(r.cap); memcpy(r.data,b.data,b.cap);

    fg153_am_params p;
    fill_setrows_params(&p, &a, &idx, &b);
    ref_setrows(&a, &idx, &r, dt == GGML_TYPE_Q4_0);
    check(fg153_am_can_run(FG153_AM_OP_SETROWS, &p) == 1, name);
    fg153_am_call(FG153_AM_OP_SETROWS, 1, (int64_t)(uintptr_t)&p);
    check(memcmp(b.data, r.data, b.cap) == 0, name);
    mix(b.data, b.cap);
    tt_free(&a); tt_free(&idx); tt_free(&b); tt_free(&r);
}

static void run_q4_block_cases(void) {
    float x[32], y[32], z[32];
    block_q4_0 q, qr;
    for (int mode = 0; mode < 6; ++mode) {
        if (mode == 0) { for (int i=0;i<32;++i) x[i]=0.0f; }
        else if (mode == 1) { for (int i=0;i<32;++i) x[i]=(float)(i-16); }
        else if (mode == 2) { for (int i=0;i<32;++i) x[i]=(i==7)?-17.5f:(float)((i*13)%29-14); }
        else fill_random_f32(x, 32, 41u + (uint32_t)mode);
        quantize_row_q4_0_ref(x, &qr, 32);
        TT a,b;
        tt_alloc(&a, GGML_TYPE_F32, 32, 1, 1, 1);
        tt_alloc(&b, GGML_TYPE_Q4_0, 32, 1, 1, 1);
        memcpy(a.data, x, 128);
        fg153_am_params p;
        fill_copy_params(&p, &a, &b, FG153_AM_OP_COPY_BLOCKS);
        p.v[FG153_AM_P_N] = 32;
        check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 1, "q4 quant can_run");
        fg153_am_call(FG153_AM_OP_COPY_BLOCKS, 1, (int64_t)(uintptr_t)&p);
        memcpy(&q, b.data, sizeof q);
        check(memcmp(&q, &qr, sizeof q) == 0, "q4 quant byte-exact vs native ref");
        dequantize_row_q4_0(&q, y, 32);
        p.v[FG153_AM_P_ST] = GGML_TYPE_Q4_0; p.v[FG153_AM_P_DT] = GGML_TYPE_F32;
        p.v[FG153_AM_P_SRC] = (int64_t)(uintptr_t)b.data; p.v[FG153_AM_P_DST] = (int64_t)(uintptr_t)a.data;
        memset(a.data, 0, a.cap);
        check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 1, "q4 dequant can_run");
        fg153_am_call(FG153_AM_OP_COPY_BLOCKS, 1, (int64_t)(uintptr_t)&p);
        memcpy(z, a.data, 128);
        check(memcmp(y, z, 128) == 0, "q4 dequant match native ref");
        mix(&q, sizeof q); mix(z, sizeof z);
        tt_free(&a); tt_free(&b);
    }
}

static void run_negative_cases(void) {
    TT a,b; fg153_am_params p;
    tt_alloc(&a, GGML_TYPE_F32, 32, 1, 1, 1);
    tt_alloc(&b, GGML_TYPE_Q4_0, 32, 1, 1, 1);
    fill_copy_params(&p, &a, &b, FG153_AM_OP_COPY_SCALAR);
    check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 1, "negative base valid");
    p.v[FG153_AM_P_N] = 31;
    check(fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p) == 0, "reject non-block Q4 length");
    fill_copy_params(&p, &a, &b, FG153_AM_OP_COPY_SCALAR);
    p.v[FG153_AM_P_NE10] = 31;
    check(fg153_am_can_run(FG153_AM_OP_COPY_SCALAR, &p) == 0, "reject mismatched shapes");
    fill_getrows_params(&p, &a, &a, &b);
    p.v[FG153_AM_P_IT] = GGML_TYPE_I64;
    check(fg153_am_can_run(FG153_AM_OP_GETROWS, &p) == 0, "reject getrows non-I32 index");
    fill_setrows_params(&p, &a, &a, &b);
    p.v[FG153_AM_P_NE11] = 0;
    check(fg153_am_can_run(FG153_AM_OP_SETROWS, &p) == 0, "reject setrows empty index dim");
    tt_free(&a); tt_free(&b);
}

static void run_alias_cases(void) {
    TT a; fg153_am_params p;
    tt_alloc(&a, GGML_TYPE_F32, 8, 3, 1, 1);
    fill_random_f32((float *)a.data, 24, 51u);
    TT before; memset(&before,0,sizeof before); before.cap=a.cap; before.data=xcalloc(a.cap); memcpy(before.data,a.data,a.cap);
    fill_copy_params(&p, &a, &a, FG153_AM_OP_COPY_SCALAR);
    fg153_am_call(FG153_AM_OP_COPY_SCALAR, 1, (int64_t)(uintptr_t)&p);
    check(memcmp(a.data, before.data, a.cap) == 0, "self-copy alias preserves bytes");
    mix(a.data, a.cap);
    free(before.data); tt_free(&a);
}

int main(void) {
    run_copy_case("copy f32->f32 strided", GGML_TYPE_F32, GGML_TYPE_F32, 0);
    run_copy_case("copy f32->f16 strided", GGML_TYPE_F32, GGML_TYPE_F16, 0);
    run_copy_case("copy f16->f32 strided", GGML_TYPE_F16, GGML_TYPE_F32, 0);
    run_copy_case("copy f16->f16 strided", GGML_TYPE_F16, GGML_TYPE_F16, 0);
    run_copy_case("contig f32->f16", GGML_TYPE_F32, GGML_TYPE_F16, 1);
    run_copy_case("contig f16->f32", GGML_TYPE_F16, GGML_TYPE_F32, 1);
    run_copy_case("transpose f32", GGML_TYPE_F32, GGML_TYPE_F32, 2);
    run_copy_case("transpose f16", GGML_TYPE_F16, GGML_TYPE_F16, 2);
    run_getrows_case("getrows f32->f32", GGML_TYPE_F32, GGML_TYPE_F32);
    run_getrows_case("getrows f32->f16", GGML_TYPE_F32, GGML_TYPE_F16);
    run_getrows_case("getrows f16->f32", GGML_TYPE_F16, GGML_TYPE_F32);
    run_getrows_case("getrows q4->f32", GGML_TYPE_Q4_0, GGML_TYPE_F32);
    run_getrows_case("getrows q4->f16", GGML_TYPE_Q4_0, GGML_TYPE_F16);
    run_setrows_case("setrows f32->f32 i32", GGML_TYPE_F32, GGML_TYPE_I32);
    run_setrows_case("setrows f32->f16 i64", GGML_TYPE_F16, GGML_TYPE_I64);
    run_setrows_case("setrows f32->q4 i32", GGML_TYPE_Q4_0, GGML_TYPE_I32);
    run_setrows_case("setrows f32->q4 i64", GGML_TYPE_Q4_0, GGML_TYPE_I64);
    run_q4_block_cases();
    run_negative_cases();
    run_alias_cases();
    check(fg153_am_witness_count(0) > 0, "copy_scalar entry reachable");
    check(fg153_am_witness_count(1) > 0, "copy_contig entry reachable");
    check(fg153_am_witness_count(2) > 0, "copy_blocks entry reachable");
    check(fg153_am_witness_count(3) > 0, "transpose entry reachable");
    check(fg153_am_witness_count(4) > 0, "getrows entry reachable");
    check(fg153_am_witness_count(5) > 0, "setrows entry reachable");
    printf("{\"status\":\"%s\",\"cases\":%d,\"failures\":%d,\"hash\":\"%016llx\"}\n",
           failures ? "FAIL" : "PASS_APEX_MOVE_DIFFERENTIAL", cases, failures,
           (unsigned long long) hash);
    return failures ? 1 : 0;
}
