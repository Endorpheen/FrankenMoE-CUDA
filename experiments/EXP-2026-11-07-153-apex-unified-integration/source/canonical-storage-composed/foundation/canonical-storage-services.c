/* worker-8 APX030 canonical storage: raw effect primitives.
 *
 * No container, growth, probing, balancing or ownership policy is implemented
 * here.  This translation unit is deliberately small enough to audit: malloc/
 * free, memcpy/memzero, pointer addition, indexed i64/u8 access, one CAS and
 * diagnostics. */

#include "canonical-storage-primitives.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Atomic int64_t g_alloc_calls = 0;
static _Atomic int64_t g_fail_alloc_at = 0;
static _Atomic int64_t g_live_allocs = 0;
static _Atomic int64_t g_live_bytes = 0;

static void fgc_require(bool ok, const char * what) {
    if (!ok) {
        fprintf(stderr, "worker-8 canonical-storage primitive invariant failed: %s\n", what);
        abort();
    }
}

int64_t fgc_alloc(int64_t bytes) {
    int64_t call = atomic_fetch_add_explicit(&g_alloc_calls, 1, memory_order_relaxed) + 1;
    int64_t fail_at = atomic_load_explicit(&g_fail_alloc_at, memory_order_relaxed);
    if (fail_at != 0 && call == fail_at) return 0;
    if (bytes <= 0) return 0;
    void * p = malloc((size_t) bytes);
    if (!p) return 0;
    atomic_fetch_add_explicit(&g_live_allocs, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_live_bytes, bytes, memory_order_relaxed);
    return (int64_t)(intptr_t) p;
}

void fgc_free_ptr(int64_t ptr, int64_t bytes) {
    if (ptr == 0) return;
    fgc_require(bytes >= 0, "negative free size");
    atomic_fetch_sub_explicit(&g_live_allocs, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&g_live_bytes, bytes, memory_order_relaxed);
    free((void *)(intptr_t) ptr);
}

int64_t fgc_memcpy(int64_t dst, int64_t src, int64_t bytes) {
    fgc_require(bytes >= 0, "negative memcpy size");
    if (bytes == 0) return dst;
    fgc_require(dst != 0 && src != 0, "null memcpy");
    memcpy((void *)(intptr_t) dst, (const void *)(intptr_t) src, (size_t) bytes);
    return dst;
}

int64_t fgc_memzero(int64_t dst, int64_t bytes) {
    fgc_require(bytes >= 0, "negative memzero size");
    if (bytes == 0) return dst;
    fgc_require(dst != 0, "null memzero");
    memset((void *)(intptr_t) dst, 0, (size_t) bytes);
    return dst;
}

int64_t fgc_add_ptr(int64_t ptr, int64_t bytes) {
    return ptr + bytes;
}

int64_t fgc_load_i64(int64_t base, int64_t index) {
    fgc_require(base != 0, "null i64 load");
    fgc_require(index >= 0, "negative i64 index");
    return ((const int64_t *)(intptr_t) base)[index];
}

void fgc_store_i64(int64_t base, int64_t index, int64_t value) {
    fgc_require(base != 0, "null i64 store");
    fgc_require(index >= 0, "negative i64 index");
    ((int64_t *)(intptr_t) base)[index] = value;
}

int64_t fgc_load_u8(int64_t base, int64_t index) {
    fgc_require(base != 0, "null u8 load");
    fgc_require(index >= 0, "negative u8 index");
    return (int64_t) ((const unsigned char *)(intptr_t) base)[index];
}

void fgc_store_u8(int64_t base, int64_t index, int64_t value) {
    fgc_require(base != 0, "null u8 store");
    fgc_require(index >= 0 && value >= 0 && value <= 255, "bad u8 store");
    ((unsigned char *)(intptr_t) base)[index] = (unsigned char) value;
}

int64_t fgc_cas_i64(int64_t base, int64_t index, int64_t expected, int64_t desired) {
    fgc_require(base != 0, "null CAS");
    fgc_require(index >= 0, "negative CAS index");
    int64_t old = expected;
    __atomic_compare_exchange_n((int64_t *)(intptr_t) base + index, &old, desired, 0,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old;
}

void fgc_abort(int64_t code) {
    fprintf(stderr, "worker-8 canonical-storage invariant failed: code=%lld\n", (long long) code);
    abort();
}

#ifdef W8_WITNESS
static _Atomic int64_t g_modes[256];
#endif

void fgc_witness(int64_t mode) {
    fgc_require(mode >= 0 && mode < 256, "witness mode");
#ifdef W8_WITNESS
    atomic_fetch_add_explicit(&g_modes[mode], 1, memory_order_relaxed);
#endif
}

#ifdef W8_WITNESS
__attribute__((destructor)) static void fgc_report(void) {
    for (int i = 0; i < 256; ++i) {
        int64_t n = atomic_load_explicit(&g_modes[i], memory_order_relaxed);
        if (n) fprintf(stderr, "FGC_STORAGE mode=%d calls=%lld\n", i, (long long) n);
    }
}
#endif

void fgc_test_alloc_reset(void) {
    atomic_store_explicit(&g_alloc_calls, 0, memory_order_relaxed);
    atomic_store_explicit(&g_fail_alloc_at, 0, memory_order_relaxed);
}

void fgc_test_fail_alloc_at(int64_t call_index) {
    atomic_store_explicit(&g_fail_alloc_at, call_index, memory_order_relaxed);
}

int64_t fgc_live_allocs(void) {
    return atomic_load_explicit(&g_live_allocs, memory_order_relaxed);
}

int64_t fgc_live_bytes(void) {
    return atomic_load_explicit(&g_live_bytes, memory_order_relaxed);
}
