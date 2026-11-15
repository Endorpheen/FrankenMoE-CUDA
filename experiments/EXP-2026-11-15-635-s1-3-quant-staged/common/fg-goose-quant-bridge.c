// Staged S1.3: the host side of the Goose P7-04 quant stack.
//
// Two things live here, and nothing else:
//
//   1. the host primitives the composed Goose unit declares as `extern`
//      (float/byte load and store, fabsf/sqrtf/log2f/floorf/lroundf/ldexpf,
//      float <-> bit reinterpretation, the native nearest_int, a scratch
//      block, the IQ grid/map/neighbour view, the IQ1 sort);
//   2. the three dispatch entries the native wrappers call when
//      FG_GOOSE_QUANT is on.
//
// The primitives are the same set the accepted P7-04 gates used
// (experiments/EXP-2026-11-14-611, -613, -616, tests/fixture.cpp), so the
// module sees the same arithmetic environment it was accepted on. The IQ
// tables are read through fg_goose_iq*_* in ggml-quants.c: they are the one
// piece of data the module does not own.

#include "fg-goose-quant-bridge.h"

#ifdef FG_GOOSE_QUANT

#include "ggml.h"          // fp16 conversion, GGML_API
#include "quant-stack.h"   // the generated Goose unit: qz_*, sl_u8, sl_f32

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- module start-up -------------------------------------------------------
//
// The generated unit has its own runtime (data stacks); goose_init() allocates
// them and must run before the first export. It is done once, lazily, on the
// first dispatch, following the S1.1 bridge (EXP626).

static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

static void fg_goose_init_once(void) {
    goose_init(0, NULL);
}

static void fg_goose_ensure_init(void) {
    pthread_once(&g_init_once, fg_goose_init_once);
}

// ---- counters --------------------------------------------------------------

static int64_t g_row_calls  = 0;
static int64_t g_blck_calls = 0;

int64_t fg_goose_quant_row_calls(void)  { return g_row_calls; }
int64_t fg_goose_quant_blck_calls(void) { return g_blck_calls; }

// ---- host primitives: shared bodies ---------------------------------------

// 16 KiB of scratch, aligned; the module's largest request is a few hundred
// bytes of float pairs, and the P7-04 gates used the same block.
static unsigned char g_scratch[16384] __attribute__((aligned(16)));

static float host_ld_f32(uint64_t a) {
    float v;
    memcpy(&v, (const void *) (uintptr_t) a, sizeof(v));
    return v;
}

static void host_st_f32(uint64_t a, float v) {
    memcpy((void *) (uintptr_t) a, &v, sizeof(v));
}

static float host_ld_f16(uint64_t a) {
    ggml_fp16_t h;
    memcpy(&h, (const void *) (uintptr_t) a, sizeof(h));
    return ggml_fp16_to_fp32(h);
}

static void host_st_f16(uint64_t a, float v) {
    const ggml_fp16_t h = ggml_fp32_to_fp16(v);
    memcpy((void *) (uintptr_t) a, &h, sizeof(h));
}

static int64_t host_ld_u8(uint64_t a) {
    uint8_t b;
    memcpy(&b, (const void *) (uintptr_t) a, sizeof(b));
    return b;
}

static void host_st_u8(uint64_t a, int64_t v) {
    const uint8_t b = (uint8_t) v;
    memcpy((void *) (uintptr_t) a, &b, sizeof(b));
}

static void host_st_u16(uint64_t a, int64_t v) {
    const uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

static void host_st_u32(uint64_t a, int64_t v) {
    const uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

static int64_t host_bits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof(b));
    return (int64_t) b;
}

static float host_of_bits(int64_t b) {
    const uint32_t u = (uint32_t) b;
    float v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

// The native nearest_int without its debug-only range assert: the library is
// built with NDEBUG, so the module must see the same conversion.
static int64_t host_nearest(float fval) {
    const float val = fval + 12582912.0f;
    int32_t i;
    memcpy(&i, &val, sizeof(i));
    return (int64_t) ((i & 0x007fffff) - 0x00400000);
}

static uint64_t host_tmp(int64_t off) {
    return (uint64_t) (uintptr_t) (g_scratch + off);
}

static void host_fail(int64_t code) {
    fprintf(stderr, "fg-goose-quant: module failure %lld\n", (long long) code);
    abort();
}

// IQ1 sorts n {value, index} records by value only, as iq1_sort_helper does.
// Equal keys keep the C library's order, and the oracle makes the same call.
static int host_iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

static void host_sort16(uint64_t pairs, int64_t n) {
    qsort((void *) (uintptr_t) pairs, (size_t) n, 2 * sizeof(float), host_iq1_cmp);
}

// ---- host primitives: the C symbols the generated unit links against ------
//
// Three families exist because the three accepted module groups were written
// against three prefixes (qz_ for rows/dequant/weighted, qz2_ for rows2,
// qd_ for dequant2). They share the same implementation.

// The generated unit is C; these definitions get C linkage from the
// translation unit itself.

float qz_ld_f32(uint64_t a) { return host_ld_f32(a); }
void  qz_st_f32(uint64_t a, float v) { host_st_f32(a, v); }
float qz_ld_f16(uint64_t a) { return host_ld_f16(a); }
void  qz_st_f16(uint64_t a, float v) { host_st_f16(a, v); }
int64_t qz_ld_u8(uint64_t a) { return host_ld_u8(a); }
void  qz_st_u8(uint64_t a, int64_t v) { host_st_u8(a, v); }
void  qz_st_u16(uint64_t a, int64_t v) { host_st_u16(a, v); }
void  qz_st_u32(uint64_t a, int64_t v) { host_st_u32(a, v); }
float qz_abs(float v) { return fabsf(v); }
float qz_sqrtf(float v) { return sqrtf(v); }
float qz_log2f(float v) { return log2f(v); }
float qz_floorf(float v) { return floorf(v); }
int64_t qz_lroundf(float v) { return (int64_t) lroundf(v); }
float qz_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }
int64_t qz_bits(float v) { return host_bits(v); }
float qz_of_bits(int64_t b) { return host_of_bits(b); }
int64_t qz_nearest(float v) { return host_nearest(v); }
uint64_t qz_tmp(int64_t off) { return host_tmp(off); }
void qz_fail(int64_t code) { host_fail(code); }
void qz_sort16(uint64_t pairs, int64_t n) { host_sort16(pairs, n); }
int64_t qz_iq2_grid(int64_t g, int64_t i) { return fg_goose_iq2_grid(g, i); }
int64_t qz_iq2_map(int64_t g, int64_t u) { return fg_goose_iq2_map(g, u); }
int64_t qz_iq2_nb(int64_t g, int64_t off) { return fg_goose_iq2_nb(g, off); }
int64_t qz_iq3_grid(int64_t g, int64_t i) { return fg_goose_iq3_grid(g, i); }
int64_t qz_iq3_map(int64_t g, int64_t u) { return fg_goose_iq3_map(g, u); }
int64_t qz_iq3_nb(int64_t g, int64_t off) { return fg_goose_iq3_nb(g, off); }

float qz2_ld_f32(uint64_t a) { return host_ld_f32(a); }
void  qz2_st_f32(uint64_t a, float v) { host_st_f32(a, v); }
float qz2_ld_f16(uint64_t a) { return host_ld_f16(a); }
void  qz2_st_f16(uint64_t a, float v) { host_st_f16(a, v); }
int64_t qz2_ld_u8(uint64_t a) { return host_ld_u8(a); }
void  qz2_st_u8(uint64_t a, int64_t v) { host_st_u8(a, v); }
void  qz2_st_u16(uint64_t a, int64_t v) { host_st_u16(a, v); }
void  qz2_st_u32(uint64_t a, int64_t v) { host_st_u32(a, v); }
float qz2_abs(float v) { return fabsf(v); }
float qz2_sqrtf(float v) { return sqrtf(v); }
float qz2_log2f(float v) { return log2f(v); }
float qz2_floorf(float v) { return floorf(v); }
int64_t qz2_lroundf(float v) { return (int64_t) lroundf(v); }
float qz2_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }
int64_t qz2_bits(float v) { return host_bits(v); }
float qz2_of_bits(int64_t b) { return host_of_bits(b); }
int64_t qz2_nearest(float v) { return host_nearest(v); }
uint64_t qz2_tmp(int64_t off) { return host_tmp(off); }
void qz2_fail(int64_t code) { host_fail(code); }
void qz2_sort16(uint64_t pairs, int64_t n) { host_sort16(pairs, n); }
int64_t qz2_iq2_grid(int64_t g, int64_t i) { return fg_goose_iq2_grid(g, i); }
int64_t qz2_iq2_map(int64_t g, int64_t u) { return fg_goose_iq2_map(g, u); }
int64_t qz2_iq2_nb(int64_t g, int64_t off) { return fg_goose_iq2_nb(g, off); }
int64_t qz2_iq3_grid(int64_t g, int64_t i) { return fg_goose_iq3_grid(g, i); }
int64_t qz2_iq3_map(int64_t g, int64_t u) { return fg_goose_iq3_map(g, u); }
int64_t qz2_iq3_nb(int64_t g, int64_t off) { return fg_goose_iq3_nb(g, off); }

int64_t qd_bits(float v) { return host_bits(v); }
float qd_of_bits(int64_t b) { return host_of_bits(b); }
int64_t qd_iq2_grid(int64_t g, int64_t i) { return fg_goose_iq2_grid(g, i); }
int64_t qd_iq3_grid(int64_t g, int64_t i) { return fg_goose_iq3_grid(g, i); }

// ---- the three dispatch entries -------------------------------------------

// ggml type ids the module knows; kept next to the calls for readability.
#define FG_TYPE_Q4_0 2
#define FG_TYPE_Q8_0 8

#define FG_QK4_0 32
#define FG_QK8_0 32
#define FG_B_Q4_0 18
#define FG_B_Q8_0 34

int fg_goose_quant_dequant_row_q4_0(const void * src, float * dst, int64_t k) {
    if (src == NULL || dst == NULL || k <= 0 || (k % FG_QK4_0) != 0) {
        return -1;
    }
    fg_goose_ensure_init();

    const int64_t nb = k / FG_QK4_0;
    const sl_u8  s = { (uint8_t *) (uintptr_t) src, nb * FG_B_Q4_0 };
    const sl_f32 d = { dst, k };

    const int64_t r = qz_dequant(FG_TYPE_Q4_0, s, k, d);
    if (r != k) {
        return -1;
    }
    ++g_row_calls;
    return 0;
}

int fg_goose_quant_quantize_row_q8_0(const float * src, void * dst, int64_t k) {
    if (src == NULL || dst == NULL || k <= 0 || (k % FG_QK8_0) != 0) {
        return -1;
    }
    fg_goose_ensure_init();

    const int64_t r = qz_quantize_row(FG_TYPE_Q8_0,
                                      (uint64_t) (uintptr_t) src,
                                      (uint64_t) (uintptr_t) dst,
                                      k);
    if (r != (k / FG_QK8_0) * FG_B_Q8_0) {
        return -1;
    }
    ++g_row_calls;
    return 0;
}

int fg_goose_quant_blck_size(int type) {
    if (type < 0) {
        return -1;
    }
    fg_goose_ensure_init();
    const int64_t r = qz_blck(type);
    if (r < 1) {
        return -1;
    }
    ++g_blck_calls;
    return (int) r;
}

#endif // FG_GOOSE_QUANT
