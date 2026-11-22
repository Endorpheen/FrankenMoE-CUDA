// The one host adapter of the merged Goose runtime (M1, P11-01).
//
// The composed unit (88 modules, goose-gen/main.c) declares 99 extern entry
// points. 34 of them are libc/pthread and the remaining 51 are here,
// including the 14 IQ table views: those dispatch through a descriptor that is
// installed at start-up, so this file and the unit hold no reference to ggml
// and the tables stay where they live.
//
// ONE start-up: the unit's runtime (data stacks) is allocated exactly once, by
// fg_goose_host_ensure_init(). Every bridge calls it instead of keeping its own
// std::call_once guard; the ten staged trees each had a guard of their own, so
// a process linking two of them could enter gs_rt_init twice.
//
// The IQ descriptor is installed inside that same start-up, before goose_init()
// runs, which is what makes the tables part of the init contract rather than of
// the link line.
//
// Bodies are lifted from the accepted fixtures, not rewritten:
//   quant primitives   EXP-2026-11-15-635 common/fg-goose-quant-bridge.c
//   gs_json_strtod     EXP-2026-11-14-610 / 544 fixtures
//   gs_json_fmt_e      EXP-2026-11-14-610 (%.*e)
//   gs_frexp_mant      EXP-2026-11-14-544 (frexp, mantissa only)
//   gs_to_string_f64   EXP-2026-11-14-544 (std::to_string == %f)
//   gs_strftime_epoch  EXP-2026-11-14-554 (localtime + strftime)
//   mt_fmt_g6          EXP-2026-11-15-618 (%.6g at an offset)
//   mp_fmt_f1          EXP-2026-11-15-650 (%.1f at an offset)
//   fg637_fmt2         EXP-2026-11-15-638 (%.2f)
//   fg458_st_i64       EXP-2026-11-13-458 (store i64 at object + offset)
//
// fp16 conversion is the tree's own algorithm copied from ggml-impl.h, so the
// module sees exactly the arithmetic ggml's exported helper produces without
// making this file depend on ggml.

#include "goose-unit.h"
#include "goose-iq-tables.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <limits.h>
#endif

// ---- the IQ table contract, installed at init ------------------------------
//
// The descriptor is data: six function pointers plus the provider's own ctx.
// The native CPU provider is defined in ggml/src/ggml-quants.c, next to the
// tables. The reference is weak, so this archive has no hard dependency on
// ggml and links into a host that owns its tables another way.

static const fg_goose_iq_tables * g_iq_tables = NULL;

extern const fg_goose_iq_tables * fg_goose_iq_tables_native(void) __attribute__((weak));

int fg_goose_host_bind_iq_tables(const fg_goose_iq_tables * tables) {
    if (tables == NULL) {
        return -1;
    }
    if (tables->abi != FG_GOOSE_IQ_TABLES_ABI || tables->size != sizeof(fg_goose_iq_tables)) {
        return -2;
    }
    if (tables->iq2_grid == NULL || tables->iq3_grid == NULL || tables->iq2_map == NULL ||
        tables->iq2_nb == NULL || tables->iq3_map == NULL || tables->iq3_nb == NULL) {
        return -3;
    }

    const fg_goose_iq_tables * previous = __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
    if (previous == tables) {
        return 0; // repeated bind of the same provider is a no-op
    }
    if (previous != NULL) {
        return -4; // one provider per process: a second one is a second copy
    }
    if (!__atomic_compare_exchange_n(&g_iq_tables, &previous, tables, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
        return previous == tables ? 0 : -4;
    }
    return 0;
}

int fg_goose_host_iq_tables_bound(void) {
    return __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE) != NULL;
}

// The one read path of the 14 unit entry points below. Reaching it without a
// provider is a programming error (a host that never bound one), so it stops
// with the name of the view instead of dereferencing NULL.
static const fg_goose_iq_tables * iq_tables_required(const char * what) {
    const fg_goose_iq_tables * tables = __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
    if (tables == NULL) {
        fprintf(stderr, "fg-goose: IQ tables (%s) requested before a provider was bound; "
                        "call fg_goose_host_bind_iq_tables() or link ggml-base\n", what);
        abort();
    }
    return tables;
}

// ---- one start-up for the whole runtime ------------------------------------

static pthread_once_t g_host_once = PTHREAD_ONCE_INIT;
static int64_t g_init_runs = 0;

static void fg_goose_host_init_once(void) {
    if (__atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE) == NULL && fg_goose_iq_tables_native != NULL) {
        (void) fg_goose_host_bind_iq_tables(fg_goose_iq_tables_native());
    }
    goose_init(0, NULL);
    __atomic_add_fetch(&g_init_runs, 1, __ATOMIC_SEQ_CST);
}

void fg_goose_host_ensure_init(void) {
    pthread_once(&g_host_once, fg_goose_host_init_once);
}

int fg_goose_host_init_with_tables(const fg_goose_iq_tables * tables) {
    const int rc = fg_goose_host_bind_iq_tables(tables);
    if (rc != 0) {
        return rc;
    }
    fg_goose_host_ensure_init();
    return 0;
}

// How many times goose_init() was actually entered. One is the only correct
// value; the two-thread gate in gates/m1/host_smoke.c asserts it.
int64_t fg_goose_host_init_runs(void) {
    return __atomic_load_n(&g_init_runs, __ATOMIC_SEQ_CST);
}

// ---- fp16, the ggml algorithm ---------------------------------------------

static uint32_t host_f32_bits(float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof(b));
    return b;
}

static float host_f32_from_bits(uint32_t b) {
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

static float host_fp16_to_fp32(uint16_t h16) {
    const uint32_t w = (uint32_t) h16 << 16;
    const uint32_t sign = w & UINT32_C(0x80000000);
    const uint32_t two_w = w + w;
    const uint32_t exp_offset = UINT32_C(0xE0) << 23;
    const float exp_scale = 0x1.0p-112f;
    const float normalized_value = host_f32_from_bits((two_w >> 4) + exp_offset) * exp_scale;
    const uint32_t magic_mask = UINT32_C(126) << 23;
    const float magic_bias = 0.5f;
    const float denormalized_value = host_f32_from_bits((two_w >> 17) | magic_mask) - magic_bias;
    const uint32_t denormalized_cutoff = UINT32_C(1) << 27;
    const uint32_t result = sign |
        (two_w < denormalized_cutoff ? host_f32_bits(denormalized_value) : host_f32_bits(normalized_value));
    return host_f32_from_bits(result);
}

static uint16_t host_fp32_to_fp16(float f) {
    const float scale_to_inf = 0x1.0p+112f;
    const float scale_to_zero = 0x1.0p-110f;
    float base = (fabsf(f) * scale_to_inf) * scale_to_zero;
    const uint32_t w = host_f32_bits(f);
    const uint32_t shl1_w = w + w;
    const uint32_t sign = w & UINT32_C(0x80000000);
    uint32_t bias = shl1_w & UINT32_C(0xFF000000);
    if (bias < UINT32_C(0x71000000)) {
        bias = UINT32_C(0x71000000);
    }
    base = host_f32_from_bits((bias >> 1) + UINT32_C(0x07800000)) + base;
    const uint32_t bits = host_f32_bits(base);
    const uint32_t exp_bits = (bits >> 13) & UINT32_C(0x00007C00);
    const uint32_t mantissa_bits = bits & UINT32_C(0x00000FFF);
    const uint32_t nonsign = exp_bits + mantissa_bits;
    return (uint16_t) ((sign >> 16) | (shl1_w > UINT32_C(0xFF000000) ? UINT16_C(0x7E00) : nonsign));
}

// ---- the shared bodies -----------------------------------------------------

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
    uint16_t h;
    memcpy(&h, (const void *) (uintptr_t) a, sizeof(h));
    return host_fp16_to_fp32(h);
}

static void host_st_f16(uint64_t a, float v) {
    const uint16_t h = host_fp32_to_fp16(v);
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
    fprintf(stderr, "fg-goose: module failure %lld\n", (long long) code);
    abort();
}

static int host_iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

static void host_sort16(uint64_t pairs, int64_t n) {
    qsort((void *) (uintptr_t) pairs, (size_t) n, 2 * sizeof(float), host_iq1_cmp);
}

static char * host_dupz(sl_u8 s) {
    char * p = (char *) malloc((size_t) s.len + 1);
    if (p == NULL) {
        return NULL;
    }
    if (s.len > 0) {
        memcpy(p, s.data, (size_t) s.len);
    }
    p[s.len] = '\0';
    return p;
}

// ---- the extern entry points the unit asks for -----------------------------

// EXP675: gguf-offsets.goose reads and writes through its own binding names.
// Same primitive as fg458_st_i64, bound under the name that module declares.
int64_t fg457_ld_i64(uint64_t p, uint64_t o) {
    return *(const int64_t *) ((const char *) (uintptr_t) p + o);
}

uint32_t fg446_ld_u8(uint64_t p, uint64_t o) {
    return *((const uint8_t *) (uintptr_t) p + o);
}

void fg446_st_i64(uint64_t p, uint64_t o, int64_t value) {
    memcpy((uint8_t *) (uintptr_t) p + o, &value, sizeof value);
}

// EXP684: goose-model-graph.goose reads the native per-layer flags array.
int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    uint32_t v = 0;
    memcpy(&v, (const char *) (uintptr_t) base + index * (int64_t) sizeof v, sizeof v);
    return (int64_t) v;
}

// EXP683: goose-tokenizer.goose binds its i64 store to fg202t_*.
void fg202t_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

void fg457_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

void fg458_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

int64_t fg637_fmt2(sl_u8 out, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.2f", x);
}

double gs_frexp_mant(double x) {
    int e = 0;
    return frexp(x, &e);
}

int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
}

double gs_json_strtod(sl_u8 s) {
    char * t = host_dupz(s);
    if (t == NULL) {
        return 0.0;
    }
    const double v = strtod(t, NULL);
    free(t);
    return v;
}

int64_t gs_strftime_epoch(sl_u8 out, int64_t epoch, sl_u8 fmt) {
    char * f = host_dupz(fmt);
    if (f == NULL) {
        return -1;
    }
    const time_t t = epoch < 0 ? time(NULL) : (time_t) epoch;
    const struct tm * tmv = localtime(&t);
    if (tmv == NULL) {
        free(f);
        return -1;
    }
    const size_t w = strftime((char *) out.data, (size_t) out.len, f, tmv);
    free(f);
    return (int64_t) w;
}

int64_t gs_to_string_f64(sl_u8 out, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%f", x);
}

int64_t mp_fmt_f1(sl_u8 out, int64_t at, double x) {
    if (at < 0 || at >= out.len) {
        return 0;
    }
    return (int64_t) snprintf((char *) out.data + at, (size_t) (out.len - at), "%.1f", x);
}

int64_t mt_fmt_g6(sl_u8 out, int64_t at, double x) {
    if (at < 0 || at >= out.len) {
        return 0;
    }
    return (int64_t) snprintf((char *) out.data + at, (size_t) (out.len - at), "%.6g", x);
}

int64_t qd_bits(float v) {
    return host_bits(v);
}

float qd_of_bits(int64_t b) {
    return host_of_bits(b);
}

float qz2_abs(float v) {
    return fabsf(v);
}

int64_t qz2_bits(float v) {
    return host_bits(v);
}

void qz2_fail(int64_t code) {
    host_fail(code);
}

float qz2_floorf(float v) {
    return floorf(v);
}

float qz2_ld_f16(uint64_t a) {
    return host_ld_f16(a);
}

float qz2_ld_f32(uint64_t a) {
    return host_ld_f32(a);
}

int64_t qz2_ld_u8(uint64_t a) {
    return host_ld_u8(a);
}

float qz2_ldexpf(float v, int64_t e) {
    return ldexpf(v, (int) e);
}

float qz2_log2f(float v) {
    return log2f(v);
}

int64_t qz2_lroundf(float v) {
    return (int64_t) lroundf(v);
}

int64_t qz2_nearest(float v) {
    return host_nearest(v);
}

float qz2_of_bits(int64_t b) {
    return host_of_bits(b);
}

void qz2_sort16(uint64_t pairs, int64_t n) {
    host_sort16(pairs, n);
}

float qz2_sqrtf(float v) {
    return sqrtf(v);
}

void qz2_st_f16(uint64_t a, float v) {
    host_st_f16(a, v);
}

void qz2_st_f32(uint64_t a, float v) {
    host_st_f32(a, v);
}

void qz2_st_u16(uint64_t a, int64_t v) {
    host_st_u16(a, v);
}

void qz2_st_u32(uint64_t a, int64_t v) {
    host_st_u32(a, v);
}

void qz2_st_u8(uint64_t a, int64_t v) {
    host_st_u8(a, v);
}

uint64_t qz2_tmp(int64_t off) {
    return host_tmp(off);
}

float qz_abs(float v) {
    return fabsf(v);
}

int64_t qz_bits(float v) {
    return host_bits(v);
}

void qz_fail(int64_t code) {
    host_fail(code);
}

float qz_floorf(float v) {
    return floorf(v);
}

float qz_ld_f16(uint64_t a) {
    return host_ld_f16(a);
}

float qz_ld_f32(uint64_t a) {
    return host_ld_f32(a);
}

int64_t qz_ld_u8(uint64_t a) {
    return host_ld_u8(a);
}

float qz_ldexpf(float v, int64_t e) {
    return ldexpf(v, (int) e);
}

float qz_log2f(float v) {
    return log2f(v);
}

int64_t qz_lroundf(float v) {
    return (int64_t) lroundf(v);
}

int64_t qz_nearest(float v) {
    return host_nearest(v);
}

float qz_of_bits(int64_t b) {
    return host_of_bits(b);
}

void qz_sort16(uint64_t pairs, int64_t n) {
    host_sort16(pairs, n);
}

float qz_sqrtf(float v) {
    return sqrtf(v);
}

void qz_st_f16(uint64_t a, float v) {
    host_st_f16(a, v);
}

void qz_st_f32(uint64_t a, float v) {
    host_st_f32(a, v);
}

void qz_st_u16(uint64_t a, int64_t v) {
    host_st_u16(a, v);
}

void qz_st_u32(uint64_t a, int64_t v) {
    host_st_u32(a, v);
}

void qz_st_u8(uint64_t a, int64_t v) {
    host_st_u8(a, v);
}

uint64_t qz_tmp(int64_t off) {
    return host_tmp(off);
}

// File-descriptor I/O. These exist because Goose cannot declare read/write/close
// itself: the generated unit includes unistd.h, which declares them with a
// different prototype, so an extern of that name is a compile error (compiler
// blocker 6). socket/bind/listen/accept/getsockname do NOT collide and are called
// directly from Goose. These three are substrate primitives, the same category as
// qz_st_u8 and qz_ld_u8 - they move bytes, they decide nothing.
int64_t qz_rd(int64_t fd, uint64_t buf, int64_t n) {
#ifdef _WIN32
    return n < 0 || n > INT_MAX ? -1 : recv((SOCKET)fd, (char *)(uintptr_t)buf, (int)n, 0);
#else
    return (int64_t) read((int) fd, (void *) (uintptr_t) buf, (size_t) n);
#endif
}

int64_t qz_wr(int64_t fd, uint64_t buf, int64_t n) {
#ifdef _WIN32
    return n < 0 || n > INT_MAX ? -1 : send((SOCKET)fd, (const char *)(uintptr_t)buf, (int)n, 0);
#else
    return (int64_t) write((int) fd, (const void *) (uintptr_t) buf, (size_t) n);
#endif
}

int64_t qz_cl(int64_t fd) {
#ifdef _WIN32
    return closesocket((SOCKET)fd);
#else
    return (int64_t) close((int) fd);
#endif
}
// ---- the 14 IQ table views the unit declares -------------------------------

// One dispatcher per view; the six provider entries of fg_goose_iq_tables do
// the work. EXP-635 defined these next to the tables; the M1 seam removal
// moved them here, where the descriptor is stored. The names and signatures
// are unchanged, so the composed unit did not have to move.
//
// A view is a (family, kind) pair: grid/qgrid read the compile-time grids,
// map/nb read the run-time tables built by ggml_quantize_init().

int64_t qd_iq2_grid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qd_iq2_grid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qd_iq3_grid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qd_iq3_grid");
    return tables->iq3_grid(tables->ctx, g, i);
}

int64_t qz2_iq2_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_map");
    return tables->iq2_map(tables->ctx, g, u);
}

int64_t qz2_iq2_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_nb");
    return tables->iq2_nb(tables->ctx, g, off);
}

int64_t qz2_iq2_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_qgrid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qz2_iq3_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_map");
    return tables->iq3_map(tables->ctx, g, u);
}

int64_t qz2_iq3_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_nb");
    return tables->iq3_nb(tables->ctx, g, off);
}

int64_t qz2_iq3_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_qgrid");
    return tables->iq3_grid(tables->ctx, g, i);
}

int64_t qz_iq2_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_map");
    return tables->iq2_map(tables->ctx, g, u);
}

int64_t qz_iq2_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_nb");
    return tables->iq2_nb(tables->ctx, g, off);
}

int64_t qz_iq2_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_qgrid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qz_iq3_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_map");
    return tables->iq3_map(tables->ctx, g, u);
}

int64_t qz_iq3_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_nb");
    return tables->iq3_nb(tables->ctx, g, off);
}

int64_t qz_iq3_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_qgrid");
    return tables->iq3_grid(tables->ctx, g, i);
}

// A store, and only a store: goose/server/shutdown-policy.goose writes its three
// decision words through this. It is host binding, not logic - the action, the exit
// code and the message flag are chosen in Goose.
void fg448_st_i64(uint64_t base, uint64_t offset, int64_t value) {
    *(int64_t *) (uintptr_t) (base + offset) = value;
}
