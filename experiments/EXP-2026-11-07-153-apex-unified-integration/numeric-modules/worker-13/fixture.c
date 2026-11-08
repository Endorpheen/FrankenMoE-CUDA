// fixture.c - deterministic host differential for the Goose CUDA GDN and
// ssm_conv ports.  The reference functions are scalar transcriptions of the
// native ggml-cuda kernels with the same row/lane reduction order; the Goose
// side is the unmodified /compiler-emitted C run one notional thread at a
// time.  No GPU is used or claimed here.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fg153-gdn-abi.h"
#include "fg153-host-state.h"

int64_t fg153_goose_gdn_host(int64_t cfg);
int64_t fg153_goose_conv_host(int64_t cfg);

int64_t fg153_h_tid;
int64_t fg153_h_bidx[3];
float  *fg153_h_smem;
int64_t fg153_h_smem_cap;

static uint64_t g_rng = 0x9e3779b97f4a7c15ull;
static float frand(void) {
    g_rng = g_rng * 6364136223846793005ull + 1442695040888963407ull;
    uint32_t u = (uint32_t)(g_rng >> 33);
    return ((float)u / 2147483648.0f - 1.0f) * 1.5f;
}
static int g_fail;
static void failf(const char *what, const char *detail) {
    fprintf(stderr, "FG153_FAIL %s %s\n", what, detail);
    g_fail = 1;
}
static int cmp_f32(const char *case_name, const char *field,
                   const float *a, const float *b, size_t n) {
    size_t i;
    int exact = 1;
    double maxd = 0.0;
    for (i = 0; i < n; i++) {
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0) exact = 0;
        double d = fabs((double)a[i] - (double)b[i]);
        if (d > maxd) maxd = d;
        if (isnan(a[i]) != isnan(b[i])) maxd = 1e30;
    }
    if (exact) {
        printf("FG153_TRACE case=%s field=%s bits=exact n=%zu PASS\n", case_name, field, n);
        return 0;
    }
    if (maxd <= 1e-6) {
        printf("FG153_TRACE case=%s field=%s max_abs_diff=%.9g n=%zu PASS\n", case_name, field, maxd, n);
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0) {
            fprintf(stderr, "FG153_DIFF %s %s i=%zu ref=%.9g goose=%.9g\n", case_name, field, i, (double)a[i], (double)b[i]);
            break;
        }
    }
    fprintf(stderr, "FG153_FAIL %s field=%s max_abs_diff=%.9g n=%zu\n", case_name, field, maxd, n);
    g_fail = 1;
    return -1;
}

static float reduce_xor(float *part, int width) {
    int off, i;
    for (off = width / 2; off > 0; off >>= 1) {
        for (i = 0; i < width; i++) {
            if ((i & off) == 0) {
                float v = part[i] + part[i + off];
                part[i] = v;
                part[i + off] = v;
            }
        }
    }
    return part[0];
}

/* Scalar transcription of gated_delta_net_cuda (KDA and non-KDA, keep_rs_t). */
static void gdn_ref(const fg153_gdn_cfg *c) {
    const int64_t Sv = c->v[FG153_GDN_SV];
    const int64_t H = c->v[FG153_GDN_H];
    const int64_t nt = c->v[FG153_GDN_NTOK];
    const int64_t ns = c->v[FG153_GDN_NSEQ];
    const int64_t neq = c->v[FG153_GDN_NEQK1];
    const int64_t rq3 = c->v[FG153_GDN_RQ3];
    const int64_t K = c->v[FG153_GDN_K_SLOTS];
    const int64_t kda = c->v[FG153_GDN_KDA];
    const int64_t keep = c->v[FG153_GDN_KEEP_RS];
    const int64_t stride = c->v[FG153_GDN_STATE_SLOT_STRIDE];
    const float scale = c->f[0];
    const float *q = (const float *)(uintptr_t)c->v[FG153_GDN_Q];
    const float *k = (const float *)(uintptr_t)c->v[FG153_GDN_K];
    const float *v = (const float *)(uintptr_t)c->v[FG153_GDN_V];
    const float *g = (const float *)(uintptr_t)c->v[FG153_GDN_G];
    const float *beta = (const float *)(uintptr_t)c->v[FG153_GDN_BETA];
    const float *sin = (const float *)(uintptr_t)c->v[FG153_GDN_STATE_IN];
    float *dst = (float *)(uintptr_t)c->v[FG153_GDN_DST];
    float *sout = (float *)(uintptr_t)c->v[FG153_GDN_STATE_OUT];
    int64_t seq, h, col, t, i, lane, r, si;
    (void)si;
    for (seq = 0; seq < ns; seq++) {
        for (h = 0; h < H; h++) {
            for (col = 0; col < Sv; col++) {
                const int64_t L = Sv >= 32 ? 8 : Sv / 4;
                const int64_t rows = Sv / L;
                const int64_t inbase = seq * H * Sv * Sv + h * Sv * Sv + col * Sv;
                float s[128], kreg[128], qreg[128], part[8];
                for (i = 0; i < Sv; i++) s[i] = sin[inbase + i];
                for (t = 0; t < nt; t++) {
                    const int64_t qb = (seq / rq3) * c->v[FG153_GDN_SQ3] + t * c->v[FG153_GDN_SQ2] + (h % neq) * c->v[FG153_GDN_SQ1];
                    const int64_t vb = seq * c->v[FG153_GDN_SV3] + t * c->v[FG153_GDN_SV2] + h * c->v[FG153_GDN_SV1];
                    const int64_t gb = seq * c->v[FG153_GDN_SB3] + t * c->v[FG153_GDN_SB2] + h * c->v[FG153_GDN_SB1];
                    const float bv = beta[gb];
                    for (i = 0; i < Sv; i++) { kreg[i] = k[qb + i]; qreg[i] = q[qb + i]; }
                    if (!kda) {
                        const float gv = expf(g[gb]);
                        float kv, delta, attn;
                        for (lane = 0; lane < L; lane++) {
                            float acc = 0.0f;
                            for (r = 0; r < rows; r++) { i = r * L + lane; acc = acc + s[i] * kreg[i]; }
                            part[lane] = acc;
                        }
                        kv = reduce_xor(part, (int)L);
                        delta = (v[vb + col] - gv * kv) * bv;
                        for (lane = 0; lane < L; lane++) {
                            float acc = 0.0f;
                            for (r = 0; r < rows; r++) {
                                i = r * L + lane;
                                s[i] = gv * s[i] + kreg[i] * delta;
                                acc = acc + s[i] * qreg[i];
                            }
                            part[lane] = acc;
                        }
                        attn = reduce_xor(part, (int)L);
                        dst[((seq * nt + t) * H + h) * Sv + col] = attn * scale;
                    } else {
                        float kv, delta, attn;
                        for (lane = 0; lane < L; lane++) {
                            float acc = 0.0f;
                            for (r = 0; r < rows; r++) { i = r * L + lane; acc = acc + expf(g[gb * Sv + i]) * s[i] * kreg[i]; }
                            part[lane] = acc;
                        }
                        kv = reduce_xor(part, (int)L);
                        delta = (v[vb + col] - kv) * bv;
                        for (lane = 0; lane < L; lane++) {
                            float acc = 0.0f;
                            for (r = 0; r < rows; r++) {
                                i = r * L + lane;
                                s[i] = expf(g[gb * Sv + i]) * s[i] + kreg[i] * delta;
                                acc = acc + s[i] * qreg[i];
                            }
                            part[lane] = acc;
                        }
                        attn = reduce_xor(part, (int)L);
                        dst[((seq * nt + t) * H + h) * Sv + col] = attn * scale;
                    }
                    if (keep) {
                        int64_t slot = nt - 1 - t;
                        if (slot >= 0 && slot < K) {
                            int64_t ob = slot * stride + (seq * H + h) * Sv * Sv + col * Sv;
                            for (i = 0; i < Sv; i++) sout[ob + i] = s[i];
                        }
                    }
                }
                if (!keep) {
                    int64_t ob = (seq * H + h) * Sv * Sv + col * Sv;
                    for (i = 0; i < Sv; i++) sout[ob + i] = s[i];
                }
            }
        }
    }
}

static void run_gdn_goose(fg153_gdn_cfg *c) {
    const int64_t Sv = c->v[FG153_GDN_SV];
    const int64_t H = c->v[FG153_GDN_H];
    const int64_t ns = c->v[FG153_GDN_NSEQ];
    const int64_t cta = c->v[FG153_GDN_COLS_PER_CTA];
    int64_t seq, h, cb, lane;
    for (seq = 0; seq < ns; seq++) {
        for (h = 0; h < H; h++) {
            for (cb = 0; cb * cta < Sv; cb++) {
                for (lane = 0; lane < cta; lane++) {
                    fg153_h_bidx[0] = h;
                    fg153_h_bidx[1] = seq;
                    fg153_h_bidx[2] = cb;
                    fg153_h_tid = lane;
                    (void)fg153_goose_gdn_host((int64_t)(uintptr_t)c);
                }
            }
        }
    }
}

static float *xmalloc(size_t n) {
    float *p = (float *)malloc(n * sizeof(float));
    if (!p) { fprintf(stderr, "OOM\n"); exit(2); }
    return p;
}

static void gdn_case(const char *name, int64_t Sv, int64_t H, int64_t nt, int64_t ns,
                     int64_t neq, int64_t rq3, int64_t kda, int64_t keep, int64_t K,
                     int64_t inplace) {
    fg153_gdn_cfg c;
    memset(&c, 0, sizeof(c));
    const int64_t qne3 = ns / rq3;
    const int64_t qsz = Sv * neq * nt * qne3;
    const int64_t vsz = Sv * H * nt * ns;
    const int64_t bsz = H * nt * ns;
    const int64_t ssz = Sv * Sv * H * ns;
    const int64_t slot_stride = Sv * Sv * H * ns;
    const int64_t osz = keep ? slot_stride * K : ssz;
    float *q = xmalloc((size_t)qsz), *k = xmalloc((size_t)qsz), *v = xmalloc((size_t)vsz);
    float *g = xmalloc((size_t)(kda ? Sv : 1) * (size_t)bsz);
    float *b = xmalloc((size_t)bsz), *sin = xmalloc((size_t)ssz);
    float *dg = xmalloc((size_t)vsz), *dn = xmalloc((size_t)vsz);
    float *sg = inplace ? sin : xmalloc((size_t)osz), *sn = xmalloc((size_t)osz);
    int64_t i;
    for (i = 0; i < qsz; i++) { q[i] = frand(); k[i] = frand(); }
    for (i = 0; i < vsz; i++) { v[i] = frand(); dg[i] = dn[i] = 12345.0f; }
    for (i = 0; i < (kda ? Sv : 1) * bsz; i++) g[i] = frand() * 0.25f;
    for (i = 0; i < bsz; i++) b[i] = 0.5f + 0.4f * frand();
    for (i = 0; i < ssz; i++) sin[i] = frand();
    for (i = 0; i < osz; i++) { sn[i] = inplace ? sin[i] : 54321.0f; if (!inplace) sg[i] = 54321.0f; }
    c.v[FG153_GDN_Q] = (int64_t)(uintptr_t)q;
    c.v[FG153_GDN_K] = (int64_t)(uintptr_t)k;
    c.v[FG153_GDN_V] = (int64_t)(uintptr_t)v;
    c.v[FG153_GDN_G] = (int64_t)(uintptr_t)g;
    c.v[FG153_GDN_BETA] = (int64_t)(uintptr_t)b;
    c.v[FG153_GDN_STATE_IN] = (int64_t)(uintptr_t)sin;
    c.v[FG153_GDN_H] = H;
    c.v[FG153_GDN_NTOK] = nt;
    c.v[FG153_GDN_NSEQ] = ns;
    c.v[FG153_GDN_SQ1] = 1; c.v[FG153_GDN_SQ2] = Sv; c.v[FG153_GDN_SQ3] = Sv * neq * nt;
    c.v[FG153_GDN_SV1] = 1; c.v[FG153_GDN_SV2] = Sv; c.v[FG153_GDN_SV3] = Sv * H * nt;
    c.v[FG153_GDN_SB1] = 1; c.v[FG153_GDN_SB2] = H; c.v[FG153_GDN_SB3] = H * nt;
    c.v[FG153_GDN_NEQK1] = neq;
    c.v[FG153_GDN_RQ3] = rq3;
    c.v[FG153_GDN_STATE_SLOT_STRIDE] = keep ? slot_stride : 0;
    c.v[FG153_GDN_K_SLOTS] = K;
    c.v[FG153_GDN_KDA] = kda;
    c.v[FG153_GDN_KEEP_RS] = keep;
    c.v[FG153_GDN_COLS_PER_CTA] = 8;
    c.v[FG153_GDN_SV] = Sv;
    c.f[0] = 1.0f / sqrtf((float)Sv);
    c.v[FG153_GDN_DST] = (int64_t)(uintptr_t)dn;
    c.v[FG153_GDN_STATE_OUT] = (int64_t)(uintptr_t)sn;
    gdn_ref(&c);
    c.v[FG153_GDN_DST] = (int64_t)(uintptr_t)dg;
    c.v[FG153_GDN_STATE_OUT] = (int64_t)(uintptr_t)(inplace ? sin : sg);
    run_gdn_goose(&c);
    cmp_f32(name, "dst", dn, dg, (size_t)vsz);
    cmp_f32(name, "state", sn, inplace ? sin : sg, (size_t)osz);
    free(q); free(k); free(v); free(g); free(b); free(sin);
    free(dg); free(dn); if (!inplace) free(sg); free(sn);
}

/* Scalar transcription of ssm_conv_f32 / ssm_conv_long_token_f32. */
static void conv_ref(const fg153_conv_cfg *c) {
    const int64_t nc = c->v[FG153_CONV_NC];
    const int64_t nr = c->v[FG153_CONV_NR];
    const int64_t nt = c->v[FG153_CONV_NT];
    const int64_t ns = c->v[FG153_CONV_NS];
    const int64_t mode = c->v[FG153_CONV_MODE];
    const int64_t split = c->v[FG153_CONV_SPLIT];
    const int64_t silu_on = c->v[FG153_CONV_SILU];
    const float *x = (const float *)(uintptr_t)c->v[FG153_CONV_X];
    const float *w = (const float *)(uintptr_t)c->v[FG153_CONV_W];
    const float *bias = c->v[FG153_CONV_BIAS] ? (const float *)(uintptr_t)c->v[FG153_CONV_BIAS] : NULL;
    float *y = (float *)(uintptr_t)c->v[FG153_CONV_Y];
    int64_t seq, ch, t, j, chunk, local, base;
    const int64_t chunk_step = mode == 1 ? split : (nt > 0 ? nt : 1);
    for (seq = 0; seq < ns; seq++) {
        for (ch = 0; ch < nr; ch++) {
            const float bv = bias ? bias[ch] : 0.0f;
            for (chunk = 0; chunk < nt; chunk += chunk_step) {
                local = chunk_step;
                if (chunk + local > nt) local = nt - chunk;
                base = c->v[FG153_CONV_XSS] * seq + ch * c->v[FG153_CONV_XSC];
                for (t = 0; t < local; t++) {
                    float sum = 0.0f;
                    for (j = 0; j < nc; j++) sum = sum + x[base + chunk + t + j] * w[ch * c->v[FG153_CONV_WSC] + j];
                    sum = sum + bv;
                    y[c->v[FG153_CONV_YSS] * seq + (chunk + t) * c->v[FG153_CONV_YST] + ch * c->v[FG153_CONV_YSC]] =
                        silu_on ? sum / (1.0f + expf(-sum)) : sum;
                }
            }
        }
    }
}

static void run_conv_goose(fg153_conv_cfg *c) {
    const int64_t nr = c->v[FG153_CONV_NR];
    const int64_t nt = c->v[FG153_CONV_NT];
    const int64_t ns = c->v[FG153_CONV_NS];
    const int64_t mode = c->v[FG153_CONV_MODE];
    const int64_t split = c->v[FG153_CONV_SPLIT];
    const int64_t nth = c->v[FG153_CONV_NTHREADS];
    int64_t seq, by, bz, lane;
    int64_t nz = mode == 0 ? 1 : (nt + split - 1) / split;
    if (nz < 1) nz = 1;
    for (seq = 0; seq < ns; seq++) {
        for (by = 0; by * nth < nr; by++) {
            for (bz = 0; bz < nz; bz++) {
                for (lane = 0; lane < nth; lane++) {
                    fg153_h_bidx[0] = seq;
                    fg153_h_bidx[1] = by;
                    fg153_h_bidx[2] = bz;
                    fg153_h_tid = lane;
                    (void)fg153_goose_conv_host((int64_t)(uintptr_t)c);
                }
            }
        }
    }
}

static void conv_case(const char *name, int64_t nc, int64_t nr, int64_t nt, int64_t ns,
                      int64_t mode, int64_t split, int64_t silu_on, int64_t with_bias) {
    fg153_conv_cfg c;
    const int64_t nth = 128;
    const int64_t xsc = nt + nc - 1;
    const int64_t xsz = ns * xsc * nr + 64;
    const int64_t wsz = nc * nr;
    const int64_t ysz = ns * nt * nr;
    float *x = xmalloc((size_t)xsz), *w = xmalloc((size_t)wsz), *b = xmalloc((size_t)nr);
    float *yg = xmalloc((size_t)ysz), *yn = xmalloc((size_t)ysz);
    int64_t i;
    memset(&c, 0, sizeof(c));
    for (i = 0; i < xsz; i++) x[i] = frand();
    for (i = 0; i < wsz; i++) w[i] = frand();
    for (i = 0; i < nr; i++) b[i] = frand();
    for (i = 0; i < ysz; i++) { yg[i] = 777.0f; yn[i] = 777.0f; }
    c.v[FG153_CONV_X] = (int64_t)(uintptr_t)x;
    c.v[FG153_CONV_W] = (int64_t)(uintptr_t)w;
    c.v[FG153_CONV_BIAS] = with_bias ? (int64_t)(uintptr_t)b : 0;
    c.v[FG153_CONV_NC] = nc;
    c.v[FG153_CONV_NR] = nr;
    c.v[FG153_CONV_NT] = nt;
    c.v[FG153_CONV_NS] = ns;
    c.v[FG153_CONV_XSC] = xsc;
    c.v[FG153_CONV_XSS] = xsc * nr;
    c.v[FG153_CONV_WSC] = nc;
    c.v[FG153_CONV_YSC] = 1;
    c.v[FG153_CONV_YST] = nr;
    c.v[FG153_CONV_YSS] = nr * nt;
    c.v[FG153_CONV_SILU] = silu_on;
    c.v[FG153_CONV_SPLIT] = split;
    c.v[FG153_CONV_MODE] = mode;
    c.v[FG153_CONV_NTHREADS] = nth;
    c.v[FG153_CONV_SMEM_CAP] = 12288;
    c.v[FG153_CONV_Y] = (int64_t)(uintptr_t)yn;
    conv_ref(&c);
    c.v[FG153_CONV_Y] = (int64_t)(uintptr_t)yg;
    run_conv_goose(&c);
    cmp_f32(name, "y", yn, yg, (size_t)ysz);
    free(x); free(w); free(b); free(yg); free(yn);
}

static void negative_gdn(void) {
    float canary[64];
    fg153_gdn_cfg c;
    int64_t i;
    struct { int field; int64_t value; int64_t code; const char *name; } bad[] = {
        {FG153_GDN_SV, 17, 1, "sv17"},
        {FG153_GDN_H, 0, 2, "h0"},
        {FG153_GDN_NTOK, -1, 3, "nt-neg"},
        {FG153_GDN_NSEQ, 0, 4, "ns0"},
        {FG153_GDN_NEQK1, 0, 5, "neq0"},
        {FG153_GDN_RQ3, 0, 6, "rq0"},
        {FG153_GDN_K_SLOTS, 0, 7, "k0"},
        {FG153_GDN_COLS_PER_CTA, 129, 8, "cta"},
        {FG153_GDN_Q, 0, 9, "qnull"},
        {FG153_GDN_STATE_IN, 0, 10, "statenull"},
        {FG153_GDN_KEEP_RS, 2, 13, "keep2"}
    };
    for (i = 0; i < (int64_t)(sizeof(bad) / sizeof(bad[0])); i++) {
        int k;
        memset(&c, 0, sizeof(c));
        for (k = 0; k < 64; k++) canary[k] = 999.0f;
        c.v[FG153_GDN_Q] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_K] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_V] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_G] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_BETA] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_STATE_IN] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_DST] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_STATE_OUT] = (int64_t)(uintptr_t)canary;
        c.v[FG153_GDN_H] = 1; c.v[FG153_GDN_NTOK] = 1; c.v[FG153_GDN_NSEQ] = 1;
        c.v[FG153_GDN_NEQK1] = 1; c.v[FG153_GDN_RQ3] = 1; c.v[FG153_GDN_K_SLOTS] = 1;
        c.v[FG153_GDN_COLS_PER_CTA] = 8; c.v[FG153_GDN_SV] = 16;
        c.v[FG153_GDN_KEEP_RS] = 0; c.v[FG153_GDN_KDA] = 0;
        c.v[bad[i].field] = bad[i].value;
        fg153_h_bidx[0] = 0; fg153_h_bidx[1] = 0; fg153_h_bidx[2] = 0; fg153_h_tid = 0;
        int64_t rc = fg153_goose_gdn_host((int64_t)(uintptr_t)&c);
        int ok = rc == -1 && c.v[FG153_GDN_ERROR] == bad[i].code && canary[0] == 999.0f;
        printf("FG153_TRACE case=neg-gdn-%s rc=%lld err=%lld canary=%.1f %s\n",
               bad[i].name, (long long)rc, (long long)c.v[FG153_GDN_ERROR], canary[0], ok ? "PASS" : "FAIL");
        if (!ok) g_fail = 1;
    }
}

static void negative_conv(void) {
    float canary[256];
    fg153_conv_cfg c;
    int64_t i;
    struct { int field; int64_t value; int64_t code; const char *name; } bad[] = {
        {FG153_CONV_NC, 6, 1, "nc6"},
        {FG153_CONV_NR, 0, 2, "nr0"},
        {FG153_CONV_NT, -1, 3, "nt-neg"},
        {FG153_CONV_NS, 0, 4, "ns0"},
        {FG153_CONV_NTHREADS, 0, 5, "nth0"},
        {FG153_CONV_X, 0, 6, "xnull"},
        {FG153_CONV_SILU, 2, 7, "silu2"},
        {FG153_CONV_MODE, 2, 11, "mode2"},
        {FG153_CONV_SPLIT, 0, 9, "split0"},
        {FG153_CONV_SPLIT, 64, 10, "smem-over"}
    };
    for (i = 0; i < (int64_t)(sizeof(bad) / sizeof(bad[0])); i++) {
        int k;
        memset(&c, 0, sizeof(c));
        for (k = 0; k < 256; k++) canary[k] = 111.0f;
        c.v[FG153_CONV_X] = (int64_t)(uintptr_t)canary;
        c.v[FG153_CONV_W] = (int64_t)(uintptr_t)canary;
        c.v[FG153_CONV_BIAS] = 0;
        c.v[FG153_CONV_Y] = (int64_t)(uintptr_t)canary;
        c.v[FG153_CONV_NC] = 4; c.v[FG153_CONV_NR] = 128; c.v[FG153_CONV_NT] = 8;
        c.v[FG153_CONV_NS] = 1; c.v[FG153_CONV_NTHREADS] = 128; c.v[FG153_CONV_SMEM_CAP] = 4096;
        c.v[FG153_CONV_MODE] = 1; c.v[FG153_CONV_SPLIT] = 32; c.v[FG153_CONV_SILU] = 0;
        c.v[bad[i].field] = bad[i].value;
        fg153_h_bidx[0] = 0; fg153_h_bidx[1] = 0; fg153_h_bidx[2] = 0; fg153_h_tid = 0;
        int64_t rc = fg153_goose_conv_host((int64_t)(uintptr_t)&c);
        int ok = rc == -1 && c.v[FG153_CONV_ERROR] == bad[i].code && canary[0] == 111.0f;
        printf("FG153_TRACE case=neg-conv-%s rc=%lld err=%lld canary=%.1f %s\n",
               bad[i].name, (long long)rc, (long long)c.v[FG153_CONV_ERROR], canary[0], ok ? "PASS" : "FAIL");
        if (!ok) g_fail = 1;
    }
}

int main(void) {
    fg153_h_smem_cap = 16384;
    fg153_h_smem = xmalloc((size_t)fg153_h_smem_cap);
    printf("FG153_STAGE host-differential\n");
    gdn_case("gdn-f32-s16", 16, 1, 1, 1, 1, 1, 0, 0, 1, 0);
    gdn_case("gdn-f32-s16-kda", 16, 2, 5, 2, 2, 1, 1, 0, 1, 0);
    gdn_case("gdn-f32-s32", 32, 1, 7, 1, 1, 1, 0, 0, 1, 0);
    gdn_case("gdn-f32-s32-kda", 32, 2, 33, 1, 2, 1, 1, 0, 1, 0);
    gdn_case("gdn-f32-s64", 64, 2, 9, 1, 2, 1, 0, 0, 1, 0);
    gdn_case("gdn-f32-s64-kda", 64, 1, 3, 2, 1, 2, 1, 0, 1, 0);
    gdn_case("gdn-f32-s128", 128, 2, 11, 1, 2, 1, 0, 0, 1, 0);
    gdn_case("gdn-f32-s128-kda", 128, 4, 4, 1, 4, 1, 1, 0, 1, 0);
    gdn_case("gdn-snap-s16", 16, 1, 5, 1, 1, 1, 0, 1, 3, 0);
    gdn_case("gdn-snap-s128-kda", 128, 2, 6, 2, 2, 1, 1, 1, 3, 0);
    gdn_case("gdn-snap-short", 32, 1, 2, 1, 1, 1, 0, 1, 4, 0);
    gdn_case("gdn-zero-token", 16, 1, 0, 1, 1, 1, 0, 0, 1, 0);
    gdn_case("gdn-multihead-neq2", 128, 4, 6, 2, 2, 2, 0, 0, 1, 0);
    gdn_case("gdn-multihead-kda", 64, 4, 5, 2, 1, 1, 1, 0, 1, 0);
    gdn_case("gdn-inplace-state", 32, 2, 3, 1, 2, 1, 0, 0, 1, 1);
    conv_case("conv-short-3", 3, 128, 8, 1, 0, 0, 0, 0);
    conv_case("conv-short-4-silu-bias", 4, 128, 8, 2, 0, 0, 1, 1);
    conv_case("conv-short-5", 5, 256, 32, 1, 0, 0, 1, 0);
    conv_case("conv-short-7", 7, 128, 1, 1, 0, 0, 0, 0);
    conv_case("conv-short-9", 9, 128, 16, 2, 0, 0, 1, 1);
    conv_case("conv-short-15", 15, 128, 3, 1, 0, 0, 0, 0);
    conv_case("conv-long-3", 3, 128, 64, 1, 1, 32, 0, 0);
    conv_case("conv-long-4-silu-bias", 4, 128, 70, 2, 1, 32, 1, 1);
    conv_case("conv-long-5", 5, 256, 33, 1, 1, 32, 1, 0);
    conv_case("conv-long-7", 7, 128, 31, 1, 1, 16, 0, 0);
    conv_case("conv-long-9", 9, 128, 65, 2, 1, 64, 1, 1);
    conv_case("conv-long-15", 15, 128, 40, 1, 1, 32, 0, 0);
    negative_gdn();
    negative_conv();
    free(fg153_h_smem);
    if (g_fail) {
        fprintf(stderr, "FG153_FIXTURE_FAIL\n");
        return 1;
    }
    printf("FG153_FIXTURE_PASS\n");
    return 0;
}
