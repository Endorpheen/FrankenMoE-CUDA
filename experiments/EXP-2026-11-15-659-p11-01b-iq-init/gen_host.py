#!/usr/bin/env python3
"""Emit the one host adapter for the composed Goose unit (M1, P11-01).

Inputs, both taken from the unit just composed at base 050354e9:
  unit/main.c     the extern prototype block (the authoritative signatures)
  unit/extern.txt the 99 undefined symbols of main.o

The adapter owns all 65 Goose helpers, including the 14 IQ table views. Those
views no longer live next to the file-static tables in ggml-quants.c: they
dispatch through fg_goose_iq_tables, a descriptor installed at start-up by the
host that owns the tables (see goose-iq-tables.h). The generator therefore
emits no fragment for ggml-quants.c; the provider there is a fixed six-entry
descriptor and is not generated code.
"""

import sys
from pathlib import Path

WORK = Path("/tmp/opencode/sub-661")
UNIT = WORK / "unit"
TREE = WORK / "tree"

LIBC = {
    "calloc", "_exit", "_Exit", "exp", "fflush", "fmod", "__fprintf_chk", "free",
    "fwrite", "log", "madvise", "malloc", "memcmp", "memcpy", "__memcpy_chk",
    "mmap", "mprotect", "munmap", "pause", "pthread_attr_destroy",
    "pthread_attr_getstack", "pthread_getattr_np", "pthread_self", "setvbuf",
    "sigaction", "sigaltstack", "__snprintf_chk", "sqrt", "__stack_chk_fail",
    "stderr", "stdout", "strlen", "__vsnprintf_chk", "write",
}

# The IQ table views: the tables are static in ggml-quants.c.
IQ = {
    "qd_iq2_grid", "qd_iq3_grid",
    "qz_iq2_map", "qz_iq2_nb", "qz_iq2_qgrid", "qz_iq3_map", "qz_iq3_nb", "qz_iq3_qgrid",
    "qz2_iq2_map", "qz2_iq2_nb", "qz2_iq2_qgrid", "qz2_iq3_map", "qz2_iq3_nb", "qz2_iq3_qgrid",
}

# suffix -> body, for the qz_/qz2_/qd_ primitives
SUFFIX = {
    "ld_f32": "return host_ld_f32(a);",
    "st_f32": "host_st_f32(a, v);",
    "ld_f16": "return host_ld_f16(a);",
    "st_f16": "host_st_f16(a, v);",
    "ld_u8": "return host_ld_u8(a);",
    "st_u8": "host_st_u8(a, v);",
    "st_u16": "host_st_u16(a, v);",
    "st_u32": "host_st_u32(a, v);",
    "abs": "return fabsf(v);",
    "sqrtf": "return sqrtf(v);",
    "log2f": "return log2f(v);",
    "floorf": "return floorf(v);",
    "lroundf": "return (int64_t) lroundf(v);",
    "ldexpf": "return ldexpf(v, (int) e);",
    "bits": "return host_bits(v);",
    "of_bits": "return host_of_bits(b);",
    "nearest": "return host_nearest(v);",
    "tmp": "return host_tmp(off);",
    "fail": "host_fail(code);",
    "sort16": "host_sort16(pairs, n);",
}

# Symbols whose parameter names differ from the suffix table's: the table's
# names are only used in the body, so key on position instead.
ARGNAMES = {
    "ldexpf": ["v", "e"],
}

NAMED = {
    "gs_json_strtod": """    char * t = host_dupz(s);
    if (t == NULL) {
        return 0.0;
    }
    const double v = strtod(t, NULL);
    free(t);
    return v;""",
    "gs_json_fmt_e": '    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.*e", (int) prec, x);',
    "gs_frexp_mant": "    int e = 0;\n    return frexp(x, &e);",
    "gs_to_string_f64": '    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%f", x);',
    "gs_strftime_epoch": """    char * f = host_dupz(fmt);
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
    return (int64_t) w;""",
    "mt_fmt_g6": """    if (at < 0 || at >= out.len) {
        return 0;
    }
    return (int64_t) snprintf((char *) out.data + at, (size_t) (out.len - at), "%.6g", x);""",
    "mp_fmt_f1": """    if (at < 0 || at >= out.len) {
        return 0;
    }
    return (int64_t) snprintf((char *) out.data + at, (size_t) (out.len - at), "%.1f", x);""",
    "fg637_fmt2": '    return (int64_t) snprintf((char *) out.data, (size_t) out.len, "%.2f", x);',
    "fg458_st_i64": "    *(int64_t *) ((char *) (uintptr_t) p + o) = v;",
}

PREAMBLE = '''// The one host adapter of the merged Goose runtime (M1, P11-01).
//
// The composed unit (88 modules, goose-gen/main.c) declares @N_ALL@ extern entry
// points. @N_LIBC@ of them are libc/pthread and the remaining @N_OWN@ are here,
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
#include <time.h>

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
                        "call fg_goose_host_bind_iq_tables() or link ggml-base\\n", what);
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
    fprintf(stderr, "fg-goose: module failure %lld\\n", (long long) code);
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
    p[s.len] = '\\0';
    return p;
}

// ---- the extern entry points the unit asks for -----------------------------
'''

HEADER = '''// The merged runtime's one host seam (M1, P11-01).
#pragma once

#include <stdint.h>

#include "goose-iq-tables.h"

#ifdef __cplusplus
extern "C" {
#endif

// Allocates the composed unit's runtime exactly once per process, whichever
// bridge is the first to need it. If the CPU table provider is linked, its
// descriptor is installed first, so the tables enter the runtime at init.
void fg_goose_host_ensure_init(void);

// The explicit embedding path: install this descriptor, then initialise.
// Returns 0 on success, the bind error otherwise (the runtime is then left
// uninitialised). Use this when the caller owns the tables, or to pin the
// provider instead of relying on the weak native reference.
int fg_goose_host_init_with_tables(const fg_goose_iq_tables * tables);

// How many times goose_init() was entered; one is the only correct value.
int64_t fg_goose_host_init_runs(void);

#ifdef __cplusplus
}
#endif
'''

IQ_SECTION = '''// ---- the 14 IQ table views the unit declares -------------------------------

// One dispatcher per view; the six provider entries of fg_goose_iq_tables do
// the work. EXP-635 defined these next to the tables; the M1 seam removal
// moved them here, where the descriptor is stored. The names and signatures
// are unchanged, so the composed unit did not have to move.
//
// A view is a (family, kind) pair: grid/qgrid read the compile-time grids,
// map/nb read the run-time tables built by ggml_quantize_init().
'''


def iq_field(name):
    fam = "iq2" if "iq2" in name else "iq3"
    kind = "grid" if ("qgrid" in name or "grid" in name) else "map" if "map" in name else "nb"
    return "%s_%s" % (fam, kind)


PARAMS = {
    "gs_json_strtod": ["s"],
    "gs_json_fmt_e": ["out", "prec", "x"],
    "gs_frexp_mant": ["x"],
    "gs_to_string_f64": ["out", "x"],
    "gs_strftime_epoch": ["out", "epoch", "fmt"],
    "mt_fmt_g6": ["out", "at", "x"],
    "mp_fmt_f1": ["out", "at", "x"],
    "fg637_fmt2": ["out", "x"],
    "fg458_st_i64": ["p", "o", "v"],
}

SUFFIX_ARGS = {
    "ld_f32": ["a"], "st_f32": ["a", "v"],
    "ld_f16": ["a"], "st_f16": ["a", "v"],
    "ld_u8": ["a"], "st_u8": ["a", "v"],
    "st_u16": ["a", "v"], "st_u32": ["a", "v"],
    "abs": ["v"], "sqrtf": ["v"], "log2f": ["v"], "floorf": ["v"],
    "lroundf": ["v"], "ldexpf": ["v", "e"], "bits": ["v"], "of_bits": ["b"],
    "nearest": ["v"], "tmp": ["off"], "fail": ["code"], "sort16": ["pairs", "n"],
}


def define(proto, names):
    """Turn `ret name(types...);` into a definition head with parameter names."""
    head, rest = proto.split("(", 1)
    types = [t.strip() for t in rest.rstrip(";").rstrip(")").split(",")]
    if len(types) != len(names):
        raise SystemExit("arity mismatch: %s" % proto)
    return "%s(%s)" % (head, ", ".join("%s %s" % (t, n) for t, n in zip(types, names)))


def prototypes(text):
    block = text.split("/* ---- extern prototypes ---- */")[1].split("/* ---- prototypes ---- */")[0]
    out = {}
    for line in block.splitlines():
        line = line.strip()
        if not line.endswith(";") or "(" not in line:
            continue
        out.setdefault(line.split("(")[0].split()[-1], line)
    return out


def main():
    externs = [l.strip() for l in (UNIT / "extern.txt").read_text().splitlines() if l.strip()]
    protos = prototypes((UNIT / "main.c").read_text())
    helpers = [e for e in externs if e not in LIBC]
    missing = [h for h in helpers if h not in protos]
    if missing:
        print("no prototype for: %s" % ", ".join(missing), file=sys.stderr)
        return 1

    own = [h for h in helpers if h not in IQ]
    iq = [h for h in helpers if h in IQ]

    body = [PREAMBLE.replace("@N_ALL@", str(len(externs)))
                     .replace("@N_LIBC@", str(len(LIBC)))
                     .replace("@N_OWN@", str(len(own)))]
    for name in sorted(own):
        proto = protos[name]
        if name in NAMED:
            body.append("\n%s {\n%s\n}\n" % (define(proto, PARAMS[name]), NAMED[name]))
            continue
        prefix = "qz2_" if name.startswith("qz2_") else "qz_" if name.startswith("qz_") else "qd_"
        suffix = name[len(prefix):]
        if suffix not in SUFFIX:
            print("no body for %s" % name, file=sys.stderr)
            return 1
        body.append("\n%s {\n    %s\n}\n" % (define(proto, SUFFIX_ARGS[suffix]), SUFFIX[suffix]))

    (TREE / "host").mkdir(parents=True, exist_ok=True)
    (TREE / "host" / "goose-host.h").write_text(HEADER)

    iq_body = [IQ_SECTION]
    for name in sorted(iq):
        proto = protos[name]
        second = "i" if "grid" in name else "u" if "map" in name else "off"
        iq_body.append("\n%s {\n"
                       "    const fg_goose_iq_tables * tables = iq_tables_required(\"%s\");\n"
                       "    return tables->%s(tables->ctx, g, %s);\n"
                       "}\n"
                       % (define(proto, ["g", second]), name, iq_field(name), second))

    (TREE / "host" / "goose_host.c").write_text("".join(body) + "".join(iq_body))

    print("extern %d = libc %d + helpers %d (adapter %d, iq views %d)"
          % (len(externs), len(LIBC), len(helpers), len(own), len(iq)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
