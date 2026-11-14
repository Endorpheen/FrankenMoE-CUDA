// Host primitives the canonical quant set borrows through `extern "qz_*"`,
// `extern "qz2_*"` and `extern "qd_*"`. The accepted gates implement them in
// their C++ fixtures (see experiments/EXP-2026-11-14-611-p7-04-quant-rows2);
// a shared runtime needs one small library instead.
//
// The IQ grid/map/neighbour tables are not literals in the port: they are built
// by native ggml_quantize_init code. build.sh extracts that code with the same
// script the gate uses (make-tables.py) into gen/native-tables.c and links it
// here, so the tables the port reads are the ones the reference builds.
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The extracted native table code keeps ggml's assert macros: GGML_ABORT calls
   ggml_abort. Providing it here keeps the tables free of the ggml library. */
void ggml_abort(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fflush(stdout);
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    abort();
}

/* native table constructors, generated from ggml-quants.c */
void tbl_init_iq2(int type);
void tbl_init_iq3(int grid_size);
uint64_t tbl_iq2_grid(int g, int i);
int tbl_iq2_map(int g, int u);
int tbl_iq2_nb(int g, int off);
uint64_t tbl_iq3_grid(int g, int i);
int tbl_iq3_map(int g, int u);
int tbl_iq3_nb(int g, int off);

/* ggml_type values the four IQ2 grids and the two IQ3 grids come from */
#define GGML_TYPE_IQ2_XXS 16
#define GGML_TYPE_IQ2_XS 17
#define GGML_TYPE_IQ1_S 19
#define GGML_TYPE_IQ2_S 22

static uint8_t g_scratch[4096] __attribute__((aligned(16)));
static int g_tables_ready = 0;

static void tables_init(void) {
    if (g_tables_ready) return;
    g_tables_ready = 1;
    tbl_init_iq2(GGML_TYPE_IQ2_XXS);
    tbl_init_iq2(GGML_TYPE_IQ2_XS);
    tbl_init_iq2(GGML_TYPE_IQ1_S);
    tbl_init_iq2(GGML_TYPE_IQ2_S);
    tbl_init_iq3(256);
    tbl_init_iq3(512);
}

/* ---- memory primitives ---------------------------------------------------- */

float qz_ld_f32(uint64_t a) {
    float v;
    memcpy(&v, (const void *)(uintptr_t)a, sizeof v);
    return v;
}

void qz_st_f16(uint64_t a, float v) {
    _Float16 h = (_Float16)v;
    memcpy((void *)(uintptr_t)a, &h, sizeof h);
}

void qz_st_u8(uint64_t a, int64_t v) {
    uint8_t b = (uint8_t)v;
    memcpy((void *)(uintptr_t)a, &b, sizeof b);
}

void qz_st_u32(uint64_t a, int64_t v) {
    uint32_t w = (uint32_t)v;
    memcpy((void *)(uintptr_t)a, &w, sizeof w);
}

int64_t qz_ld_u8(uint64_t a) {
    uint8_t b;
    memcpy(&b, (const void *)(uintptr_t)a, sizeof b);
    return b;
}

float qz_abs(float v) { return fabsf(v); }

float qz2_ld_f32(uint64_t a) { return qz_ld_f32(a); }

void qz2_st_f32(uint64_t a, float v) {
    memcpy((void *)(uintptr_t)a, &v, sizeof v);
}

float qz2_ld_f16(uint64_t a) {
    _Float16 h;
    memcpy(&h, (const void *)(uintptr_t)a, sizeof h);
    return (float)h;
}

void qz2_st_f16(uint64_t a, float v) { qz_st_f16(a, v); }
void qz2_st_u8(uint64_t a, int64_t v) { qz_st_u8(a, v); }
void qz2_st_u32(uint64_t a, int64_t v) { qz_st_u32(a, v); }

int64_t qz2_ld_u8(uint64_t a) { return qz_ld_u8(a); }

void qz2_st_u16(uint64_t a, int64_t v) {
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    memcpy((void *)(uintptr_t)a, b, sizeof b);
}

float qz2_abs(float v) { return fabsf(v); }
float qz2_sqrtf(float v) { return sqrtf(v); }
float qz2_log2f(float v) { return log2f(v); }
float qz2_floorf(float v) { return floorf(v); }
int64_t qz2_lroundf(float v) { return (int64_t)lroundf(v); }
float qz2_ldexpf(float v, int64_t e) { return ldexpf(v, (int)e); }

int64_t qz2_bits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof b);
    return (int64_t)b;
}

float qz2_of_bits(int64_t b) {
    uint32_t u = (uint32_t)b;
    float v;
    memcpy(&v, &u, sizeof v);
    return v;
}

/* ggml's nearest_int, without its debug-only range assert */
int64_t qz2_nearest(float fval) {
    float val = fval + 12582912.0f;
    int32_t i;
    memcpy(&i, &val, sizeof i);
    return (int64_t)((i & 0x007fffff) - 0x00400000);
}

uint64_t qz2_tmp(int64_t off) {
    return (uint64_t)(uintptr_t)(g_scratch + off);
}

void qz2_fail(int64_t code) {
    fprintf(stderr, "goose quant port failure %lld\n", (long long)code);
    abort();
}

static int iq1_cmp(const void *l, const void *r) {
    const float *a = (const float *)l;
    const float *b = (const float *)r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

void qz2_sort16(uint64_t pairs, int64_t n) {
    qsort((void *)(uintptr_t)pairs, (size_t)n, 2 * sizeof(float), iq1_cmp);
}

/* ---- IQ grid tables ------------------------------------------------------- */

int64_t qz2_iq2_grid(int64_t g, int64_t i) {
    tables_init();
    return (int64_t)tbl_iq2_grid((int)g, (int)i);
}

int64_t qz2_iq2_map(int64_t g, int64_t u) {
    tables_init();
    return tbl_iq2_map((int)g, (int)u);
}

int64_t qz2_iq2_nb(int64_t g, int64_t off) {
    tables_init();
    return tbl_iq2_nb((int)g, (int)off);
}

int64_t qz2_iq3_grid(int64_t g, int64_t i) {
    tables_init();
    return (int64_t)tbl_iq3_grid((int)g, (int)i);
}

int64_t qz2_iq3_map(int64_t g, int64_t u) {
    tables_init();
    return tbl_iq3_map((int)g, (int)u);
}

int64_t qz2_iq3_nb(int64_t g, int64_t off) {
    tables_init();
    return tbl_iq3_nb((int)g, (int)off);
}

/* ---- dequant2 primitives -------------------------------------------------- */

int64_t qd_bits(float v) { return qz2_bits(v); }

float qd_of_bits(int64_t b) { return qz2_of_bits(b); }

int64_t qd_iq2_grid(int64_t g, int64_t i) { return qz2_iq2_grid(g, i); }

int64_t qd_iq3_grid(int64_t g, int64_t i) { return qz2_iq3_grid(g, i); }
