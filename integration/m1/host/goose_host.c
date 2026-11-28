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

#ifdef GOOSEINFER_GOOSE_HOST_QUANT
uint64_t fg_goose_host_iq_tables_pointer(void) {
    return (uintptr_t)__atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
}
#endif

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

#ifndef GOOSEINFER_GOOSE_HOST_QUANT
#include "host-quant-reference.inc"
#endif

void qz2_sort16(uint64_t pairs, int64_t n) {
    host_sort16(pairs, n);
}

void qz_sort16(uint64_t pairs, int64_t n) {
    host_sort16(pairs, n);
}

#ifndef GOOSEINFER_GOOSE_HOST_QUANT
#include "host-runtime-reference.inc"
#endif
