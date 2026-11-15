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
// them and must run before the first export. The start-up is shared with every
// other bridge through the one adapter guard, which also installs the IQ table
// descriptor before goose_init(); a private pthread_once here was a second path
// into gs_rt_init and skipped the descriptor.

#include "goose-host.h"

static void fg_goose_ensure_init(void) {
    fg_goose_host_ensure_init();
}

// ---- counters --------------------------------------------------------------

static int64_t g_row_calls  = 0;
static int64_t g_blck_calls = 0;

int64_t fg_goose_quant_row_calls(void)  { return g_row_calls; }
int64_t fg_goose_quant_blck_calls(void) { return g_blck_calls; }

// ---- the host primitives ------------------------------------------------
//
// In the tenth staged tree this file defined 54 primitives (qz_/qz2_/qd_:
// load, store, libm, bit views, the scratch block, the IQ1 sort). M1 moved all
// of them to host/goose_host.c, because they are shared by every stage and a
// second copy is a second definition at link time. What stays here is what the
// S1.3 seam itself owns: the three dispatch entries below.

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
