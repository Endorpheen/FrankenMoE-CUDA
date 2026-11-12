// LD_PRELOAD shim: route quantize_row_q8_K_ref through the Goose module.
//
// quantize_row_q8_K in libggml-cpu.so is a three-instruction trampoline into
// quantize_row_q8_K_ref, which lives in libggml-base.so and goes through the PLT. That
// makes the reference symbol the clean interposition point: overriding it here swaps the
// algorithm for every caller without touching either library.
//
// GGML_GOOSE_CPU_Q8K=0 restores the native reference for the paired comparison.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define QK_K 256
typedef struct {
    float   d;
    int8_t  qs[QK_K];
    int16_t bsums[QK_K/16];
} block_q8_K;

// The Goose entry produced by the compiler for this stage.
uint8_t fg190q_enter(int64_t k, const float * src, void * dst);

static _Atomic uint64_t g_calls, g_elements, g_native;

static void report(void) {
    fprintf(stderr, "FG190Q_CPU_Q8K calls=%llu elements=%llu native=%llu\n",
            (unsigned long long)atomic_load(&g_calls),
            (unsigned long long)atomic_load(&g_elements),
            (unsigned long long)atomic_load(&g_native));
}

void quantize_row_q8_K_ref(const float * x, void * y, int64_t k) {
    static _Atomic int once;
    if (!atomic_exchange(&once, 1)) atexit(report);

    static void (*native)(const float *, void *, int64_t);
    if (!native) native = (void (*)(const float *, void *, int64_t))dlsym(RTLD_NEXT, "quantize_row_q8_K_ref");
    if (!native) {
        fprintf(stderr, "FG190Q_CPU_Q8K fatal: native reference not found\n");
        abort();
    }

    const char * flag = getenv("GGML_GOOSE_CPU_Q8K");
    if (flag && strcmp(flag, "0") == 0) {
        atomic_fetch_add(&g_native, 1);
        native(x, y, k);
        return;
    }
    // The Goose module validates k itself; anything it cannot handle goes to native so a
    // shape change can never silently produce wrong quants.
    if (k <= 0 || k % QK_K != 0) {
        atomic_fetch_add(&g_native, 1);
        native(x, y, k);
        return;
    }
    atomic_fetch_add(&g_calls, 1);
    atomic_fetch_add(&g_elements, (uint64_t)k);
    fg190q_enter(k, x, y);
}
