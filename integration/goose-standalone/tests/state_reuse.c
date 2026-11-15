// Reuse from n_past: two calls instead of one, same result as a single pass.
//
// A decode loop does not recompute what the cache already holds. The correctness statement
// is therefore: processing positions [0, n_past) and then [n_past, n) through the
// persistent cache must give, at position n-1, exactly what one pass over [0, n) gives.
//
// That is stronger than the per-token check in EXP-701, which rebuilt the reference for
// every token. Here the split point is the thing under test.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);
extern int64_t state_slot(int64_t c, int64_t cache, int64_t layout, int64_t layer,
                          int64_t pos, int64_t a, int64_t b, int64_t c2, int64_t n_layers,
                          int64_t n_tokens, int64_t elem);

#define N_CTX 8
#define HEAD_DIM 8

// Writes k/v for positions [from, to) into a cache owned by the caller and returns the
// attention output at the last written position, computed over everything in the cache.
static void run_range(struct ggml_tensor * cache_k, struct ggml_tensor * cache_v,
                      const float * ksrc, const float * vsrc, const float * qsrc,
                      int64_t from, int64_t to, float * out) {
    const size_t elem = sizeof(float);

    for (int64_t t = from; t < to; ++t) {
        struct ggml_init_params gp = { 4u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
        struct ggml_context * gc = ggml_init(gp);

        struct ggml_tensor * ks = (struct ggml_tensor *) (uintptr_t) state_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache_k, 0, 0, t, HEAD_DIM,
            N_CTX, 1, 1, 1, (int64_t) elem);
        struct ggml_tensor * vs = (struct ggml_tensor *) (uintptr_t) state_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache_v, 0, 0, t, HEAD_DIM,
            N_CTX, 1, 1, 1, (int64_t) elem);
        memcpy(ks->data, ksrc + t * HEAD_DIM, (size_t) HEAD_DIM * elem);
        memcpy(vs->data, vsrc + t * HEAD_DIM, (size_t) HEAD_DIM * elem);
        ggml_free(gc);
    }

    // Attention at the last position, over the whole cache with a mask at `to - 1`.
    struct ggml_init_params gp = { 8u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    struct ggml_tensor * q = ggml_new_tensor_1d(gc, GGML_TYPE_F32, HEAD_DIM);
    memcpy(q->data, qsrc + (to - 1) * HEAD_DIM, (size_t) HEAD_DIM * elem);

    struct ggml_tensor * kfull = ggml_view_2d(gc, cache_k, HEAD_DIM, N_CTX,
                                              (size_t) HEAD_DIM * elem, 0);
    struct ggml_tensor * vfull = ggml_view_2d(gc, cache_v, HEAD_DIM, N_CTX,
                                              (size_t) HEAD_DIM * elem, 0);
    struct ggml_tensor * mask = ggml_new_tensor_1d(gc, GGML_TYPE_F32, N_CTX);
    for (int64_t i = 0; i < N_CTX; ++i) {
        ((float *) mask->data)[i] = (i <= to - 1) ? 0.0f : -INFINITY;
    }

    struct ggml_tensor * sc = ggml_add(gc, ggml_mul_mat(gc, kfull, q), mask);
    struct ggml_tensor * pr = ggml_soft_max(gc, sc);
    struct ggml_tensor * vT = ggml_cont(gc, ggml_transpose(gc, vfull));
    struct ggml_tensor * o = ggml_mul_mat(gc, vT, pr);

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, o);
    ggml_graph_compute_with_ctx(gc, g, 1);
    memcpy(out, o->data, (size_t) HEAD_DIM * elem);
    ggml_free(gc);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    float q[8 * HEAD_DIM], k[8 * HEAD_DIM], v[8 * HEAD_DIM];
    for (int i = 0; i < 8 * HEAD_DIM; ++i) {
        q[i] = (float) ((i * 7 % 23) - 11) / 8.0f;
        k[i] = (float) ((i * 11 % 29) - 14) / 8.0f;
        v[i] = (float) ((i * 13 % 31) - 15) / 8.0f;
    }

    float out_one[HEAD_DIM], out_split[HEAD_DIM];

    // one pass over 0..4
    {
        struct ggml_init_params gp = { 16u * 1024u * 1024u, NULL, false };
        struct ggml_context * c1 = ggml_init(gp);
        struct ggml_tensor * a = ggml_new_tensor_4d(c1, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        struct ggml_tensor * b = ggml_new_tensor_4d(c1, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        memset(a->data, 0, (size_t) ggml_nbytes(a));
        memset(b->data, 0, (size_t) ggml_nbytes(b));
        run_range(a, b, k, v, q, 0, 4, out_one);
        ggml_free(c1);
    }

    // two calls: 0..2 then 2..4
    {
        struct ggml_init_params gp = { 16u * 1024u * 1024u, NULL, false };
        struct ggml_context * c2 = ggml_init(gp);
        struct ggml_tensor * a = ggml_new_tensor_4d(c2, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        struct ggml_tensor * b = ggml_new_tensor_4d(c2, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        memset(a->data, 0, (size_t) ggml_nbytes(a));
        memset(b->data, 0, (size_t) ggml_nbytes(b));
        float scratch[HEAD_DIM];
        run_range(a, b, k, v, q, 0, 2, scratch);
        run_range(a, b, k, v, q, 2, 4, out_split);
        ggml_free(c2);
    }

    int d = 0;
    for (int i = 0; i < HEAD_DIM; ++i) {
        if (memcmp(&out_one[i], &out_split[i], sizeof(float)) != 0) ++d;
    }

    // The split must differ from the one-pass result at 0..2, otherwise both are trivially
    // the same and the check proves nothing about which positions were reused.
    float out_first[HEAD_DIM];
    {
        struct ggml_init_params gp = { 16u * 1024u * 1024u, NULL, false };
        struct ggml_context * c3 = ggml_init(gp);
        struct ggml_tensor * a = ggml_new_tensor_4d(c3, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        struct ggml_tensor * b = ggml_new_tensor_4d(c3, GGML_TYPE_F32, HEAD_DIM, N_CTX, 1, 1);
        memset(a->data, 0, (size_t) ggml_nbytes(a));
        memset(b->data, 0, (size_t) ggml_nbytes(b));
        run_range(a, b, k, v, q, 0, 2, out_first);
        ggml_free(c3);
    }
    int differs_at_1 = 0;
    for (int i = 0; i < HEAD_DIM; ++i) {
        if (memcmp(&out_first[i], &out_one[i], sizeof(float)) != 0) { differs_at_1 = 1; break; }
    }

    printf("FG_STATE_REUSE split_at=2 differs=%d distinguishable=%d %s\n",
           d, differs_at_1, (d == 0 && differs_at_1) ? "PASS" : "FAIL");
    return 0;
}
