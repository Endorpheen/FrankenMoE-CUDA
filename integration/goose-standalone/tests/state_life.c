// A state buffer that survives a graph rebuild.
//
// Every gate so far allocated its cache inside the same ggml context as the graph, which
// means the cache dies with the graph. A real decode loop rebuilds the graph per token, so
// the cache has to live in its own context and be handed in as a tensor handle.
//
// The division is deliberate: this file owns the LIFETIME (one context per cache, freed
// once), while the offset arithmetic stays in goose/model/gdn-ops.goose::state_slot. No
// layout knowledge is duplicated here.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);
extern int64_t state_slot(int64_t c, int64_t cache, int64_t layout, int64_t layer,
                          int64_t pos, int64_t a, int64_t b, int64_t c2, int64_t n_layers,
                          int64_t n_tokens, int64_t elem);

// One persistent cache: its own context, its own tensor, alive across graphs.
typedef struct {
    struct ggml_context * ctx;
    struct ggml_tensor  * t;
} state_buf;

static state_buf state_alloc(size_t bytes, int64_t dim0, int64_t dim1, int64_t dim2,
                             int64_t dim3) {
    state_buf b = { NULL, NULL };
    struct ggml_init_params p = { bytes, NULL, /*.no_alloc =*/ false };
    b.ctx = ggml_init(p);
    if (b.ctx == NULL) return b;
    if (dim3 > 1 || dim2 > 1) {
        b.t = ggml_new_tensor_4d(b.ctx, GGML_TYPE_F32, dim0, dim1, dim2, dim3);
    } else if (dim1 > 1) {
        b.t = ggml_new_tensor_2d(b.ctx, GGML_TYPE_F32, dim0, dim1);
    } else {
        b.t = ggml_new_tensor_1d(b.ctx, GGML_TYPE_F32, dim0);
    }
    if (b.t != NULL) memset(b.t->data, 0, (size_t) ggml_nbytes(b.t));
    return b;
}

static void state_free(state_buf * b) {
    if (b->ctx) ggml_free(b->ctx);
    b->ctx = NULL;
    b->t = NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const size_t elem = sizeof(float);
    const int64_t head_dim = 16, n_ctx = 8, n_kv_heads = 1, n_layers = 1;

    // The cache outlives every graph below.
    state_buf cache = state_alloc(16u * 1024u * 1024u, head_dim, n_ctx, n_kv_heads, n_layers);
    if (cache.t == NULL) { printf("FG_STATE_LIFE alloc failed\n"); return 2; }

    int bad = 0;

    // Six tokens, each in its OWN context: the graph is rebuilt every time and the cache
    // is not. This is what a decode loop does.
    for (int64_t t = 0; t < 6; ++t) {
        struct ggml_init_params gp = { 4u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
        struct ggml_context * gc = ggml_init(gp);
        if (gc == NULL) { printf("FG_STATE_LIFE graph ctx failed\n"); return 2; }

        // A tensor in THIS graph, written through the persistent slot.
        struct ggml_tensor * ring = ggml_new_tensor_1d(gc, GGML_TYPE_F32, head_dim);
        for (int64_t i = 0; i < head_dim; ++i) {
            ((float *) ring->data)[i] = (float) (t * 100 + i);
        }

        struct ggml_tensor * slot = (struct ggml_tensor *) (uintptr_t) state_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache.t, 0, 0, t, head_dim,
            n_ctx, n_kv_heads, n_layers, 1, (int64_t) elem);
        if (!slot) { printf("FG_STATE_LIFE slot null at %lld\n", (long long) t); return 1; }
        memcpy(slot->data, ring->data, (size_t) head_dim * elem);

        ggml_free(gc);   // the graph is gone; the cache must not be
    }

    // Read back through a fresh context and confirm every position still holds its token.
    struct ggml_init_params gp = { 4u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    for (int64_t t = 0; t < 6; ++t) {
        struct ggml_tensor * slot = (struct ggml_tensor *) (uintptr_t) state_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache.t, 0, 0, t, head_dim,
            n_ctx, n_kv_heads, n_layers, 1, (int64_t) elem);
        const float * d = (const float *) slot->data;
        int ok = 1;
        for (int64_t i = 0; i < head_dim; ++i) {
            if (d[i] != (float) (t * 100 + i)) ok = 0;
        }
        if (!ok) { ++bad; printf("  token %lld lost\n", (long long) t); }
    }
    ggml_free(gc);

    // And the untouched positions must still be zero: a slot that writes past its own row
    // would corrupt a neighbour, which the per-token check above would not notice.
    int stray = 0;
    const float * all = (const float *) cache.t->data;
    const int64_t n = ggml_nelements(cache.t);
    for (int64_t p = 6; p < n_ctx; ++p) {
        for (int64_t i = 0; i < head_dim; ++i) {
            if (all[p * head_dim + i] != 0.0f) ++stray;
        }
    }

    printf("FG_STATE_LIFE tokens=6 rebuilt=6 lost=%d stray=%lld %s\n", bad,
           (long long) stray, (bad == 0 && stray == 0) ? "PASS" : "FAIL");

    state_free(&cache);
    return 0;
}
