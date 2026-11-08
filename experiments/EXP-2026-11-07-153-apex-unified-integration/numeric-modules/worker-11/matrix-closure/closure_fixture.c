/* Host differential fixture for the matrix-closure Goose unit.
 *
 * Checks:
 *   - MMVQ q8_1 block packing against the qualified native
 *     quantize_row_q8_1_ref and an independent reference,
 *   - MMQ q8_1 packing (DS4 for Q4_K, D4 for Q6_K) against an independent
 *     reference implementing the quantize_mmq_q8_1 arithmetic,
 *   - MMQ tiled dot against native dequantize_row_q4_K/q6_K plus the q8_1
 *     MMQ header semantics,
 *   - launch plan (J_best, shared bytes, tiles, stream-k, q8 strides) against
 *     an independent C restatement of the frozen host formulas,
 *   - route, tile staging, shared bound and GEMM row arithmetic.
 * No GPU is touched.
 */
#include <immintrin.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emitter_ids.h"

void quantize_row_q8_1_ref(const float * x, void * y, int64_t k);
void dequantize_row_q4_K(const void * x, float * y, int64_t k);
void dequantize_row_q6_K(const void * x, float * y, int64_t k);

static uint32_t fgc_bits(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
static float fgc_from_bits(uint32_t v) { float x; memcpy(&x, &v, 4); return x; }
static float fgc_half(uint16_t v) { return _cvtsh_ss(v); }
static uint16_t fgc_to_half(float v) { return _cvtss_sh(v, _MM_FROUND_TO_NEAREST_INT); }
static float fgc_roundf(float x) { return roundf(x); }
static uint8_t fgc_f2i8(float x) { return (uint8_t)(int8_t)(int)roundf(x); }
static void fgc_witness(int64_t op, int64_t value) { (void)op; (void)value; }

#define main fgc_generated_unused_main
#include "closure_generated.c"
#undef main

static uint32_t rng;
static uint32_t next_u32(void) { rng = rng*1664525u + 1013904223u; return rng; }
static float next_f32(void) { return ((int)(next_u32()%2001u) - 1000) / 256.0f; }
static void reset(uint32_t s) { rng = s; }

static int failures;
static void fail(const char * what) { printf("FAIL %s\n", what); failures++; }

static uint16_t rd16(const uint8_t * p) { uint16_t v; memcpy(&v, p, 2); return v; }
static float rdf32(const uint8_t * p) { float v; memcpy(&v, p, 4); return v; }

// ----------------------- independent q8_1 references ------------------------
static void ref_mmvq(const float * x, uint8_t * y) {
    float amax = 0.0f, sum = 0.0f;
    for (int i = 0; i < 32; i++) { amax = fmaxf(amax, fabsf(x[i])); sum += x[i]; }
    float d = amax / 127.0f;
    for (int i = 0; i < 32; i++) y[4+i] = fgc_f2i8(amax == 0.0f ? 0.0f : roundf(x[i]/d));
    uint16_t hd = fgc_to_half(d), hs = fgc_to_half(sum);
    memcpy(y+0, &hd, 2); memcpy(y+2, &hs, 2);
}
static void ref_mmq(const float * x, uint8_t * y, int layout) {
    for (int g = 0; g < 4; g++) {
        const float * xs = x + g*32;
        float amax = 0.0f, sum = 0.0f;
        for (int i = 0; i < 32; i++) { amax = fmaxf(amax, fabsf(xs[i])); sum += xs[i]; }
        float dinv = 127.0f / amax;
        float d = 1.0f / dinv;
        if (amax == 0.0f) { dinv = 0.0f; d = 0.0f; }
        for (int i = 0; i < 32; i++) y[16+g*32+i] = fgc_f2i8(amax == 0.0f ? 0.0f : roundf(xs[i]*dinv));
        if (layout == 0) {
            uint16_t hd = fgc_to_half(d), hs = fgc_to_half(sum);
            memcpy(y+g*4+0, &hd, 2); memcpy(y+g*4+2, &hs, 2);
        } else {
            memcpy(y+g*4, &d, 4);
        }
    }
}
static void check_q8(void) {
    float x[256];
    uint8_t g[144], r[144], n[36], nr[36];
    reset(0x71384b51u);
    for (int i = 0; i < 256; i++) x[i] = next_f32();
    memset(g, 0, sizeof(g)); memset(r, 0, sizeof(r));
    if (FGC_FN_MMVQ_Q8_BLOCK(gg_view(x, 32), gg_view(g, 36), 0) != 1) fail("mmvq block status");
    ref_mmvq(x, r);
    if (memcmp(g, r, 36) != 0) fail("mmvq q8 block");
    // The CPU reference stores s = sum(quantized)*d, while the CUDA
    // quantize.cu kernel replaced here stores s = sum(original x).  The CUDA
    // semantics are the contract for this path, so the independent ref_mmvq
    // above is the reference; q/d still have to agree byte-for-byte with the
    // CPU ref, checked below for the header's d field.
    memset(n, 0, sizeof(n)); memset(nr, 0, sizeof(nr));
    quantize_row_q8_1_ref(x, n, 32);
    if (memcmp(g+4, n+4, 32) != 0 || memcmp(g+0, n+0, 2) != 0) {
        fail("mmvq q/d vs native quantize_row_q8_1_ref");
    }
    (void) nr;
    for (int layout = 0; layout < 2; layout++) {
        memset(g, 0, sizeof(g)); memset(r, 0, sizeof(r));
        if (FGC_FN_MMQ_Q8_BLOCK(gg_view(x, 128), gg_view(g, 144), layout, 0) != 1) fail("mmq block status");
        ref_mmq(x, r, layout);
        if (memcmp(g, r, 144) != 0) fail(layout == 0 ? "mmq q8 DS4" : "mmq q8 D4");
    }
}

// --------------------------- independent MMQ dot ----------------------------
static void ref_scale_min(const uint8_t * q, int j, int * d, int * m) {
    if (j < 4) { *d = q[j] & 63; *m = q[j+4] & 63; }
    else {
        *d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4);
        *m = (q[j+4] >> 4) | ((q[j-0] >> 6) << 4);
    }
}
static int ref_q4_nib(const uint8_t * w, int g, int r) {
    int il = g/2, hi = g%2;
    int b = w[16 + 32*il + r];
    return hi ? (b >> 4) : (b & 15);
}
static int ref_q6_raw(const uint8_t * w, int lane) {
    int ip = lane/128, within = lane%128, il = within%32, ch = within/32;
    int qo = (ch%2) ? 32 : 0;
    int ql = w[64*ip+il+qo];
    int lo = (ch < 2) ? (ql & 15) : (ql >> 4);
    int qh = w[128+32*ip+il];
    return lo | (((qh >> (2*ch)) & 3) << 4);
}
static float ref_dot_q4(const uint8_t * w, const uint8_t * q8) {
    float d = fgc_half(rd16(w)), dmin = fgc_half(rd16(w+2));
    float sumd = 0.0f, summ = 0.0f;
    for (int g = 0; g < 8; g++) {
        int blk = g/4, gi = g%4;
        float d8 = fgc_half(rd16(q8 + blk*144 + gi*4));
        float s  = fgc_half(rd16(q8 + blk*144 + gi*4 + 2));
        const int8_t * qs = (const int8_t *)(q8 + blk*144 + 16 + gi*32);
        long sumi = 0;
        for (int r = 0; r < 32; r++) sumi += (long) ref_q4_nib(w, g, r) * qs[r];
        int sc, m; ref_scale_min(w+4, g, &sc, &m);
        sumd += d8 * ((float)sumi * (float)sc);
        summ += s * (float)m;
    }
    return d*sumd - dmin*summ;
}
static float ref_dot_q6(const uint8_t * w, const uint8_t * q8) {
    float d6 = fgc_half(rd16(w+208));
    float sum = 0.0f;
    for (int g = 0; g < 16; g++) {
        int t = g/2, blk = t/4, gi = t%4;
        float d8 = rdf32(q8 + blk*144 + gi*4);
        const int8_t * qs = (const int8_t *)(q8 + blk*144 + 16 + gi*32 + (g%2)*16);
        int sc = (int8_t) w[192+g];
        long sumi = 0;
        for (int r = 0; r < 16; r++) sumi += (long)(ref_q6_raw(w, g*16+r) - 32) * qs[r];
        sum += d8 * ((float)sumi * (float)sc);
    }
    return d6*sum;
}
static void make_w4(uint8_t * w) {
    reset(0x4b344b51u);
    for (int i = 0; i < 128; i++) w[16+i] = (uint8_t) next_u32();
    for (int i = 0; i < 12; i++) w[4+i] = (uint8_t) next_u32();
    uint16_t d = fgc_to_half(0.5f), dm = fgc_to_half(0.25f);
    memcpy(w, &d, 2); memcpy(w+2, &dm, 2);
}
static void make_w6(uint8_t * w) {
    reset(0x4b364b51u);
    for (int i = 0; i < 128; i++) w[i] = (uint8_t) next_u32();
    for (int i = 0; i < 64; i++) w[128+i] = (uint8_t) next_u32();
    for (int i = 0; i < 16; i++) w[192+i] = (uint8_t) next_u32();
    uint16_t d = fgc_to_half(0.125f);
    memcpy(w+208, &d, 2);
}
static float rel(float got, float ref) {
    float diff = fabsf(got-ref);
    float scale = fabsf(ref) > 1.0f ? fabsf(ref) : 1.0f;
    return diff/scale;
}
static void check_mmq_dot(void) {
    uint8_t w4[144], w6[210], q8[288];
    float x[256], out1[1], ref[256];
    reset(0x71384b51u);
    for (int i = 0; i < 256; i++) x[i] = next_f32();
    make_w4(w4);
    memset(q8, 0, sizeof(q8));
    if (FGC_FN_MMQ_Q8_BLOCK(gg_view(x, 128), gg_view(q8, 144), 0, 0) != 1) fail("q4 q8 block");
    if (FGC_FN_MMQ_Q8_BLOCK(gg_view(x+128, 128), gg_view(q8+144, 144), 0, 0) != 1) fail("q4 q8 block hi");
    if (FGC_FN_MMQ_DOT(12, gg_view(w4, 144), 0, gg_view(q8, 288), 0, gg_view(out1, 1), 0) != 1) fail("mmq_dot q4");
    dequantize_row_q4_K(w4, ref, 256);
    float truth = 0.0f;
    for (int g = 0; g < 8; g++) {
        int blk = g/4, gi = g%4;
        float d8 = fgc_half(rd16(q8+blk*144+gi*4));
        const int8_t * qs = (const int8_t *)(q8+blk*144+16+gi*32);
        for (int r = 0; r < 32; r++) truth += ref[g*32+r] * d8 * qs[r];
    }
    float rq4 = ref_dot_q4(w4, q8);
    if (rel(out1[0], rq4) > 1e-5) { printf("FAIL mmq dot q4 got %.9g ref %.9g\n", out1[0], rq4); failures++; }
    if (rel(out1[0], truth) > 1e-3) { printf("FAIL mmq dot q4 truth got %.9g ref %.9g\n", out1[0], truth); failures++; }

    make_w6(w6);
    memset(q8, 0, sizeof(q8));
    if (FGC_FN_MMQ_Q8_BLOCK(gg_view(x, 128), gg_view(q8, 144), 1, 0) != 1) fail("q6 q8 block");
    if (FGC_FN_MMQ_Q8_BLOCK(gg_view(x+128, 128), gg_view(q8+144, 144), 1, 0) != 1) fail("q6 q8 block hi");
    if (FGC_FN_MMQ_DOT(14, gg_view(w6, 210), 0, gg_view(q8, 288), 0, gg_view(out1, 1), 0) != 1) fail("mmq_dot q6");
    dequantize_row_q6_K(w6, ref, 256);
    truth = 0.0f;
    for (int g = 0; g < 16; g++) {
        int t = g/2, blk = t/4, gi = t%4;
        float d8 = rdf32(q8+blk*144+gi*4);
        const int8_t * qs = (const int8_t *)(q8+blk*144+16+gi*32+(g%2)*16);
        for (int r = 0; r < 16; r++) truth += ref[g*16+r] * d8 * qs[r];
    }
    float rq6 = ref_dot_q6(w6, q8);
    if (rel(out1[0], rq6) > 1e-5) { printf("FAIL mmq dot q6 got %.9g ref %.9g\n", out1[0], rq6); failures++; }
    if (rel(out1[0], truth) > 1e-3) { printf("FAIL mmq dot q6 truth got %.9g ref %.9g\n", out1[0], truth); failures++; }
}

// ------------------------------ launch plan ---------------------------------
static long align_up(long x, long a) { return ((x+a-1)/a)*a; }
static long ref_nbytes_shared(long J, long I, long stride, long nthreads) {
    return J*4 + I*stride*4 + align_up(J*144, nthreads*4);
}
static int ref_plan(long kind, long ne00, long ne01, long ne11, long smpbo, long cc, long nsm, long out[24]) {
    for (int i = 0; i < 24; i++) out[i] = 0;
    if (kind != 12 && kind != 14) return 0;
    long I = 128, nthreads = 256;
    long layout = kind == 12 ? 0 : 1;
    long sramlayout = kind == 12 ? 1 : 4;
    long stride = kind == 12 ? 84 : 76;
    long fallback = ne01 % 128 != 0;
    long ne10p = align_up(ne00, 512);
    long Jbest = 0, ntbest = 0x7fffffffffffffffL;
    for (long J = 8; J <= 128 && ntbest > 1; J += 8) {
        if (ref_nbytes_shared(J, I, stride, nthreads) <= smpbo) {
            long ntx = (ne11 + J - 1) / J;
            if (ntx < ntbest) { Jbest = J; ntbest = ntx; }
        }
    }
    if (Jbest == 0) Jbest = 8;
    long nty = (ne01 + I - 1) / I, ntx = (ne11 + Jbest - 1) / Jbest;
    long ntiles = nty * ntx;
    long nsm_n = nsm > 0 ? nsm : 1;
    long waves = (ntiles + nsm_n - 1) / nsm_n;
    long eff = nsm_n*waves > 0 ? (100*ntiles)/(nsm_n*waves) : 0;
    long nwork = ntiles * ((ne00 + 255) / 256);
    long skblocks = (eff >= 90 && cc >= 750) ? ntiles : (nsm_n < nwork ? nsm_n : nwork);
    long fixup = (skblocks > 0 && ntiles % skblocks != 0);
    long q8pad = align_up(Jbest*144, nthreads*4);
    long gi = 32, gj = 8;
    long gnty = (ne01 + gi - 1) / gi, gntx = (ne11 + gj - 1) / gj;
    long rs = kind == 12 ? 160 : 224;
    long gshared = gi*rs + gj*288;
    out[0] = 1; out[1] = Jbest; out[2] = nthreads; out[3] = I;
    out[4] = layout; out[5] = sramlayout; out[6] = stride;
    out[7] = ref_nbytes_shared(Jbest, I, stride, nthreads);
    out[8] = nty; out[9] = ntx; out[10] = ntiles;
    out[11] = gnty; out[12] = gntx; out[13] = gi; out[14] = gj;
    out[15] = gshared; out[16] = fallback; out[17] = ne10p; out[18] = ne10p/128;
    out[19] = q8pad; out[20] = skblocks; out[21] = fixup; out[22] = 2; out[23] = 0;
    return 1;
}
static void check_plan(void) {
    struct { long kind, ne00, ne01, ne11, smpbo, cc, nsm; } cases[] = {
        {12, 4096, 4096, 512, 49152, 800, 108},
        {14, 4096, 4096, 512, 49152, 800, 108},
        {12, 2048, 256, 64,  49152, 890, 84},
        {14, 8192, 1024, 1024, 101376, 900, 132},
    };
    for (unsigned c = 0; c < sizeof(cases)/sizeof(cases[0]); c++) {
        long want[24];
        int ok = ref_plan(cases[c].kind, cases[c].ne00, cases[c].ne01, cases[c].ne11,
                          cases[c].smpbo, cases[c].cc, cases[c].nsm, want);
        float got[24];
        int gok = (int) FGC_FN_PLAN_MMQ(cases[c].kind, cases[c].ne00, cases[c].ne01,
                    cases[c].ne11, cases[c].smpbo, cases[c].cc, cases[c].nsm, gg_view(got, 24), 0);
        if (gok != ok) { fail("plan status"); continue; }
        for (int i = 0; i < 24; i++) {
            if ((long) got[i] != want[i]) {
                printf("FAIL plan case %u field %d got %ld want %ld\n", c, i, (long)got[i], want[i]);
                failures++;
            }
        }
    }
}

// -------------------------------- routing -----------------------------------
static void check_route(void) {
    struct { long kind, batch, dense, want; } cases[] = {
        {12, 1, 1, 1}, {12, 8, 1, 1}, {12, 512, 1, 3}, {14, 512, 1, 3},
        {0, 512, 1, 2}, {1, 512, 1, 2}, {0, 1, 1, 4}, {12, 512, 0, 10},
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        float out[2];
        long r = (long) FGC_FN_ROUTE_MATMUL(cases[i].kind, cases[i].batch, cases[i].dense,
                    800, 49152, 4096, 512, 108, gg_view(out, 2), 0);
        if (r != cases[i].want) { printf("FAIL route %u got %ld want %ld\n", i, r, cases[i].want); failures++; }
    }
}

// ------------------------------- staging ------------------------------------
static void check_staging(void) {
    uint8_t w[4*3*144], dst[2*160], q8[4*5*144], qdst[2*288];
    reset(0x12345678u);
    for (unsigned i = 0; i < sizeof(w); i++) w[i] = (uint8_t) next_u32();
    memset(dst, 0xAA, sizeof(dst));
    // rows 2 and 3 at kbx0=1, row stride s01=3 blocks
    if (FGC_FN_STAGE_WEIGHTS(12, gg_view(w, sizeof(w)), 3, 1, 2, 2, gg_view(dst, sizeof(dst)), 160, 0) != 2) fail("stage_weights status");
    for (int i = 0; i < 2; i++) for (int b = 0; b < 144; b++)
        if (dst[i*160+b] != w[((2+i)*3+1)*144+b]) fail("stage_weights bytes");
    for (unsigned i = 0; i < sizeof(q8); i++) q8[i] = (uint8_t) next_u32();
    memset(qdst, 0, sizeof(qdst));
    if (FGC_FN_STAGE_Q8(gg_view(q8, sizeof(q8)), 5, 1, 2, 2, gg_view(qdst, sizeof(qdst)), 0) != 2) fail("stage_q8 status");
    for (int c = 0; c < 2; c++) for (int kk = 0; kk < 2; kk++)
        for (int b = 0; b < 144; b++)
            if (qdst[c*288+kk*144+b] != q8[((1+kk)*5+(2+c))*144+b]) fail("stage_q8 bytes");
    float sb[3];
    if (FGC_FN_SRAM_BOUND(12, 32, 8, gg_view(sb, 3), 0) != 1) fail("sram_bound q4");
    if ((long)sb[0] != 32*160+8*288 || (long)sb[1] != 9472 || (long)sb[2] != 1) fail("sram_bound q4 fields");
    if (FGC_FN_SRAM_BOUND(14, 32, 8, gg_view(sb, 3), 0) != 1) fail("sram_bound q6");
    if ((long)sb[0] != 32*224+8*288 || (long)sb[1] != 9472) fail("sram_bound q6 fields");
}

// --------------------------------- GEMM -------------------------------------
static float ref_gemm(int kind, const uint8_t * a, const float * b, long m, long n, long k, long row, long j) {
    float s = 0.0f;
    for (long t = 0; t < k; t++) {
        float av = kind == 1 ? fgc_half(((const uint16_t*)a)[row*k+t]) : ((const float*)a)[row*k+t];
        s = s + av * b[j*k+t];   // ggml src1: column-major, k contiguous
    }
    return s;
}
static void check_gemm(void) {
    const long m = 3, n = 4, k = 40;
    uint8_t abuf[3*40*4];
    float bbuf[4*40], cbuf[4*3];
    reset(0x6e6d32u);
    for (long i = 0; i < m*k; i++) ((float*)abuf)[i] = next_f32();
    reset(0xb00b1e5u);
    for (long i = 0; i < n*k; i++) bbuf[i] = next_f32();
    for (long row = 0; row < m; row++) {
        memset(cbuf, 0, sizeof(cbuf));
        if (FGC_FN_GEMM_ROW(0, gg_view(abuf, sizeof(abuf)), gg_view(bbuf, sizeof(bbuf)),
                                        gg_view(cbuf + row, (n-1)*m + 1), m, n, k, row, m, 0) != 1) fail("gemm status");
        for (long j = 0; j < n; j++) {
            float ref = ref_gemm(0, abuf, bbuf, m, n, k, row, j);
            if (cbuf[j*m + row] != ref) { printf("FAIL gemm %ld %ld got %.9g ref %.9g\n", row, j, cbuf[j*m+row], ref); failures++; }
        }
    }
    // F16 weights: same reference path.
    reset(0x6e6d16u);
    for (long i = 0; i < m*k; i++) ((uint16_t*)abuf)[i] = fgc_to_half(next_f32());
    for (long row = 0; row < m; row++) {
        memset(cbuf, 0, sizeof(cbuf));
        if (FGC_FN_GEMM_ROW(1, gg_view(abuf, sizeof(abuf)), gg_view(bbuf, sizeof(bbuf)),
                                        gg_view(cbuf + row, (n-1)*m + 1), m, n, k, row, m, 0) != 1) fail("gemm f16 status");
        for (long j = 0; j < n; j++) {
            float ref = ref_gemm(1, abuf, bbuf, m, n, k, row, j);
            if (cbuf[j*m + row] != ref) fail("gemm f16 value");
        }
    }
}

int main(void) {
    check_q8();
    check_mmq_dot();
    check_plan();
    check_route();
    check_staging();
    check_gemm();
    printf("{\"status\":\"%s\",\"failures\":%d}\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
