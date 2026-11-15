// KV cache against a full recompute.
//
// Three tokens are processed twice: once writing each token's k/v into a persistent cache
// and attending over the positions written so far, once attending over an explicitly
// built k/v matrix restricted to the same positions. If the slot arithmetic is right the
// two are bit-identical; if it is off by one or by a stride, the cached arm reads a
// neighbour's data and the outputs diverge.
//
// The slot arithmetic is goose/model/gdn-ops.goose::attn_kv_slot - the gate does not
// compute offsets itself.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);
extern int64_t attn_kv_slot(int64_t c, int64_t cache, int64_t layer, int64_t pos,
                            int64_t head_dim, int64_t n_ctx, int64_t n_kv_heads,
                            int64_t n_layers, int64_t n_tokens, int64_t elem);

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const int64_t head_dim = 8, n_ctx = 8, n_kv_heads = 1, n_layers = 1, n_tok = 3;
    const size_t elem = sizeof(float);

    struct ggml_init_params gp = { 32u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_KV_CACHE init failed\n"); return 2; }

    struct ggml_tensor * cache_k = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_dim, n_ctx, n_kv_heads, n_layers);
    struct ggml_tensor * cache_v = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_dim, n_ctx, n_kv_heads, n_layers);
    memset(cache_k->data, 0, (size_t) ggml_nbytes(cache_k));
    memset(cache_v->data, 0, (size_t) ggml_nbytes(cache_v));

    struct ggml_tensor * q  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, head_dim, n_tok);
    struct ggml_tensor * kf = ggml_new_tensor_2d(gc, GGML_TYPE_F32, head_dim, n_tok);
    struct ggml_tensor * vf = ggml_new_tensor_2d(gc, GGML_TYPE_F32, head_dim, n_tok);
    {
        struct { struct ggml_tensor * t; int seed; } ws[] = { { q, 7 }, { kf, 11 }, { vf, 13 } };
        for (size_t i = 0; i < 3; ++i) {
            float * d = (float *) ws[i].t->data;
            const int64_t n = ggml_nelements(ws[i].t);
            for (int64_t j = 0; j < n; ++j)
                d[j] = (float) ((j % ws[i].seed) - ws[i].seed / 2) / 16.0f;
        }
    }

    // A mask that keeps positions <= t and suppresses the rest.
    struct ggml_tensor * mask = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_ctx, 1);

    int bad = 0;
    for (int64_t t = 0; t < n_tok; ++t) {
        // write this token's k/v through the module's slot
        struct ggml_tensor * ks = (struct ggml_tensor *) (uintptr_t) attn_kv_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache_k, 0, t, head_dim, n_ctx,
            n_kv_heads, n_layers, 1, (int64_t) elem);
        struct ggml_tensor * vs = (struct ggml_tensor *) (uintptr_t) attn_kv_slot(
            (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cache_v, 0, t, head_dim, n_ctx,
            n_kv_heads, n_layers, 1, (int64_t) elem);
        if (!ks || !vs) { printf("FG_KV_CACHE slot returned null at %lld\n", (long long) t); return 1; }
        memcpy(ks->data, (char *) kf->data + (size_t) t * head_dim * elem, (size_t) head_dim * elem);
        memcpy(vs->data, (char *) vf->data + (size_t) t * head_dim * elem, (size_t) head_dim * elem);

        // cached attention over the whole cache, masked to positions <= t
        struct ggml_tensor * qt = ggml_view_2d(gc, q, head_dim, 1, (size_t) head_dim * elem,
                                               (size_t) t * head_dim * elem);
        // [head_dim, n_ctx] over the cache: the second dim steps by nb[1] = elem*head_dim.
        // Using n_ctx*head_dim*elem here skipped a whole cache row per position.
        struct ggml_tensor * kfull = ggml_view_2d(gc, cache_k, head_dim, n_ctx,
                                                  (size_t) head_dim * elem, 0);
        struct ggml_tensor * vfull = ggml_view_2d(gc, cache_v, head_dim, n_ctx,
                                                  (size_t) head_dim * elem, 0);
        struct ggml_tensor * sc = ggml_mul_mat(gc, kfull, qt);   // [n_ctx, 1]
        float * md = (float *) mask->data;
        for (int64_t i = 0; i < n_ctx; ++i) md[i] = (i <= t) ? 0.0f : -INFINITY;
        struct ggml_tensor * scm = ggml_add(gc, sc, mask);
        struct ggml_tensor * pr = ggml_soft_max(gc, scm);
        // ggml_mul_mat(a, b) reduces over a->ne[0], so the value matrix has to be in
        // [n_kv, head_dim] order - transposed relative to its storage layout.
        struct ggml_tensor * vT = ggml_cont(gc, ggml_transpose(gc, vfull));
        struct ggml_tensor * outA = ggml_mul_mat(gc, vT, pr);     // [head_dim, 1]

        // Reference: the SAME graph over a fresh buffer filled with positions 0..t.
        // Comparing against a narrower softmax instead would differ by rounding alone and
        // say nothing about the slot arithmetic; this compares persistent write-then-read
        // against writing everything again.
        struct ggml_tensor * kref_all = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_dim, n_ctx,
                                                           n_kv_heads, n_layers);
        struct ggml_tensor * vref_all = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_dim, n_ctx,
                                                           n_kv_heads, n_layers);
        memset(kref_all->data, 0, (size_t) ggml_nbytes(kref_all));
        memset(vref_all->data, 0, (size_t) ggml_nbytes(vref_all));
        for (int64_t p2 = 0; p2 <= t; ++p2) {
            struct ggml_tensor * ks2 = (struct ggml_tensor *) (uintptr_t) attn_kv_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kref_all, 0, p2, head_dim,
                n_ctx, n_kv_heads, n_layers, 1, (int64_t) elem);
            struct ggml_tensor * vs2 = (struct ggml_tensor *) (uintptr_t) attn_kv_slot(
                (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) vref_all, 0, p2, head_dim,
                n_ctx, n_kv_heads, n_layers, 1, (int64_t) elem);
            memcpy(ks2->data, (char *) kf->data + (size_t) p2 * head_dim * elem, (size_t) head_dim * elem);
            memcpy(vs2->data, (char *) vf->data + (size_t) p2 * head_dim * elem, (size_t) head_dim * elem);
        }
        struct ggml_tensor * kref = ggml_view_2d(gc, kref_all, head_dim, n_ctx,
                                                 (size_t) head_dim * elem, 0);
        struct ggml_tensor * vref = ggml_view_2d(gc, vref_all, head_dim, n_ctx,
                                                 (size_t) head_dim * elem, 0);
        struct ggml_tensor * scr = ggml_mul_mat(gc, kref, qt);
        struct ggml_tensor * scmr = ggml_add(gc, scr, mask);
        struct ggml_tensor * prr = ggml_soft_max(gc, scmr);
        struct ggml_tensor * vTref = ggml_cont(gc, ggml_transpose(gc, vref));
        struct ggml_tensor * outB = ggml_mul_mat(gc, vTref, prr);

        struct ggml_cgraph * g = ggml_new_graph(gc);
        ggml_build_forward_expand(g, outA);
        ggml_build_forward_expand(g, outB);
        ggml_graph_compute_with_ctx(gc, g, 1);

        int d = 0;
        for (int64_t i = 0; i < head_dim; ++i) {
            if (memcmp((char *) outA->data + i * elem, (char *) outB->data + i * elem, elem) != 0) ++d;
        }
        printf("  token %lld: differs=%d\n", (long long) t, d);
        if (d) ++bad;
    }

    printf("FG_KV_CACHE tokens=%lld head_dim=%lld n_ctx=%lld bad=%d %s\n",
           (long long) n_tok, (long long) head_dim, (long long) n_ctx, bad,
           bad == 0 ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
