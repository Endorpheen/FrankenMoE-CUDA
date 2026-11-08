/* worker-8: typed primitives for allocator-closure.goose.
 *
 * These are field accessors only.  They must never grow into an implementation
 * of reserve, reuse, coalescing or lifetime policy: all of that stays in Goose
 * (allocator-closure.goose) or in the already-qualified parent kernels.
 *
 * The struct mirror is validated by allocator-abi.h static assertions and by
 * the executable differential against the qualified runtime library. */

#include "allocator-abi.h"
#include "allocator-closure-bridge.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static bool w8_closure_enabled = true;

__attribute__((constructor)) static void w8_closure_config(void) {
    const char * on = getenv("GGML_GOOSE_ALLOC_CLOSURE");
    if (on && atoi(on) == 0) w8_closure_enabled = false;
}

int fg153_alloc_closure_enabled(void) { return w8_closure_enabled ? 1 : 0; }

static void w8_require(bool ok) {
    if (!ok) {
        fprintf(stderr, "worker-8 allocator closure: invariant failed\n");
        abort();
    }
}

int64_t fg153_ac_nbuffers(int64_t galloc) {
    const struct w8_gallocr_layout * g = (const void *)(intptr_t)galloc;
    w8_require(g != NULL);
    w8_require(g->n_buffers >= 0 && g->n_buffers <= 64);
    return g->n_buffers;
}

int64_t fg153_ac_talloc(int64_t galloc, int64_t index) {
    const struct w8_gallocr_layout * g = (const void *)(intptr_t)galloc;
    w8_require(g != NULL);
    w8_require(index >= 0 && index < g->n_buffers);
    w8_require(g->buf_tallocs != NULL);
    w8_require(g->buf_tallocs[index] != NULL);
    return (intptr_t)g->buf_tallocs[index];
}

int64_t fg153_ac_nchunks(int64_t talloc) {
    const struct w8_dyn_tallocr_layout * a = (const void *)(intptr_t)talloc;
    w8_require(a != NULL);
    w8_require(a->n_chunks >= 0 && a->n_chunks <= W8_VBUFFER_MAX_CHUNKS);
    return a->n_chunks;
}

int64_t fg153_ac_sizeget(int64_t sizes, int64_t index) {
    w8_require(sizes != 0);
    w8_require(index >= 0);
    return (int64_t)((const size_t *)(intptr_t)sizes)[index];
}

void fg153_ac_sizeset(int64_t sizes, int64_t index, int64_t value) {
    w8_require(sizes != 0);
    w8_require(index >= 0);
    w8_require(value >= 0);
    ((size_t *)(intptr_t)sizes)[index] = (size_t)value;
}

#ifdef W8_WITNESS
static atomic_uint_fast64_t w8_modes[256];
#endif

void fg153_ac_witness(int64_t mode) {
    w8_require(mode >= 0 && mode < 256);
#ifdef W8_WITNESS
    atomic_fetch_add_explicit(&w8_modes[mode], 1, memory_order_relaxed);
#endif
}

#ifdef W8_WITNESS
__attribute__((destructor)) static void w8_closure_report(void) {
    for (int i = 0; i < 256; ++i) {
        unsigned long long n = atomic_load_explicit(&w8_modes[i], memory_order_relaxed);
        if (n) fprintf(stderr, "FG153_ALLOC_CLOSURE mode=%d calls=%llu\n", i, n);
    }
}
#endif
