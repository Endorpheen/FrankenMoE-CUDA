// CRT formatting and reference interfaces for the host runtime.

#include "goose-unit.h"
#include "goose-iq-tables.h"

#include <math.h>
#include <stddef.h>
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

#ifdef GOOSEINFER_GOOSE_HOST_QUANT
_Static_assert(FG_GOOSE_IQ_TABLES_ABI == 1 && sizeof(fg_goose_iq_tables) == 64 &&
    offsetof(fg_goose_iq_tables, ctx) == 8 && offsetof(fg_goose_iq_tables, iq2_grid) == 16 &&
    offsetof(fg_goose_iq_tables, iq3_nb) == 56, "Goose IQ table ABI");
#else
#include "host-init-reference.inc"
#endif

static int host_iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

static void host_sort16(uint64_t pairs, int64_t n) {
    qsort((void *) (uintptr_t) pairs, (size_t) n, 2 * sizeof(float), host_iq1_cmp);
}

// ---- the extern entry points the unit asks for -----------------------------

#ifndef GOOSEINFER_GOOSE_HOST_QUANT
#include "host-conversion-reference.inc"
#endif

int64_t fg637_fmt2(sl_u8 out, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.2f", x);
}

int64_t gs_json_fmt_e(sl_u8 out, int64_t prec, double x) {
    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);
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
