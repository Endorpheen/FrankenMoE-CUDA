// GDN ops gate: the two operations goose/model/gdn-ops.goose decides.
//
// Same oracle discipline as everything else here: call the module, call ggml's own
// constructor with the same inputs, compare bit for bit. The two are l2_norm (with
// the epsilon the module passes as f32 bits) and repeat-to-value-heads (which the
// module skips when the head counts already match, exactly as the native builder
// skips it).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

static int32_t float_bits_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

int main(void) {
    fg_goose_host_ensure_init();

    // Recurrent geometry taken from the Apex weights: conv channels 8192, value head
    // width 128, 4 value heads and 2 key heads (so the repeat arm runs).
    const int64_t chan = 8192, head_v = 128, n_v = 4, n_k = 2, tokens = 1;

    struct ggml_init_params gp = { 256 * 1024 * 1024, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) {
        printf("FG_GDN_OPS init failed\n");
        return 2;
    }

    const float eps = 1e-6f;

    // ---- l2_norm ----
    struct ggml_tensor * a = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_v, n_k, tokens, 1);
    float * ad = (float *) a->data;
    const int64_t na = ggml_nelements(a);
    for (int64_t i = 0; i < na; ++i) ad[i] = (float) ((i % 13) - 6) / 32.0f;

    const int64_t ra = gdn_l2norm((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) a,
                                  (int64_t) float_bits_of(eps));
    struct ggml_tensor * rn = ggml_l2_norm(gc, a, eps);

    int diff_l2 = 0;
    if (ra == 0) {
        printf("FG_GDN_OPS l2norm returned null\n");
        diff_l2 = -1;
    } else {
        struct ggml_tensor * rm = (struct ggml_tensor *) (uintptr_t) ra;
        const int64_t n = ggml_nelements(rm);
        if (n != ggml_nelements(rn)) {
            printf("FG_GDN_OPS l2norm element count %lld vs %lld\n",
                   (long long) n, (long long) ggml_nelements(rn));
            diff_l2 = -1;
        } else {
            struct ggml_cgraph * f1 = ggml_new_graph(gc);
            ggml_build_forward_expand(f1, rm);
            struct ggml_cgraph * f2 = ggml_new_graph(gc);
            ggml_build_forward_expand(f2, rn);
            ggml_backend_t be = ggml_backend_cpu_init();
            ggml_backend_graph_compute(be, f1);
            ggml_backend_graph_compute(be, f2);
            float * p1 = (float *) malloc((size_t) n * sizeof(float));
            float * p2 = (float *) malloc((size_t) n * sizeof(float));
            // The context is host-allocated (no_alloc=false), so the data is already
            // in memory; tensor_get would need a backend buffer that does not exist.
            memcpy(p1, rm->data, (size_t) n * sizeof(float));
            memcpy(p2, rn->data, (size_t) n * sizeof(float));
            for (int64_t i = 0; i < n; ++i) {
                if (memcmp(&p1[i], &p2[i], sizeof(float)) != 0) ++diff_l2;
            }
            printf("FG_GDN_OPS l2norm elements=%lld eps=%.0e differs=%d\n",
                   (long long) n, (double) eps, diff_l2);
            free(p1);
            free(p2);
            ggml_backend_free(be);
        }
    }

    // ---- repeat to value heads ----
    const int64_t rr = gdn_repeat_to_v((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) a,
                                       head_v, n_k, n_v, tokens, 1);
    struct ggml_tensor * rqn = ggml_repeat_4d(gc, a, head_v, n_v, tokens, 1);
    int diff_rep = 0;
    if (rr == 0) {
        printf("FG_GDN_OPS repeat returned null\n");
        diff_rep = -1;
    } else {
        struct ggml_tensor * rp = (struct ggml_tensor *) (uintptr_t) rr;
        const int64_t n = ggml_nelements(rp);
        if (n != ggml_nelements(rqn)) {
            printf("FG_GDN_OPS repeat element count %lld vs %lld\n",
                   (long long) n, (long long) ggml_nelements(rqn));
            diff_rep = -1;
        } else {
            struct ggml_cgraph * f1 = ggml_new_graph(gc);
            ggml_build_forward_expand(f1, rp);
            struct ggml_cgraph * f2 = ggml_new_graph(gc);
            ggml_build_forward_expand(f2, rqn);
            ggml_backend_t be = ggml_backend_cpu_init();
            ggml_backend_graph_compute(be, f1);
            ggml_backend_graph_compute(be, f2);
            float * p1 = (float *) malloc((size_t) n * sizeof(float));
            float * p2 = (float *) malloc((size_t) n * sizeof(float));
            memcpy(p1, rp->data, (size_t) n * sizeof(float));
            memcpy(p2, rqn->data, (size_t) n * sizeof(float));
            for (int64_t i = 0; i < n; ++i) {
                if (memcmp(&p1[i], &p2[i], sizeof(float)) != 0) ++diff_rep;
            }
            printf("FG_GDN_OPS repeat elements=%lld n_k=%lld n_v=%lld differs=%d\n",
                   (long long) n, (long long) n_k, (long long) n_v, diff_rep);
            free(p1);
            free(p2);
            ggml_backend_free(be);
        }
    }

    // ---- the module must skip the repeat when the heads already match ----
    const int64_t same = gdn_repeat_to_v((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) a,
                                         head_v, n_v, n_v, tokens, 1);
    const int skip_ok = (same == (int64_t) (uintptr_t) a);
    printf("FG_GDN_OPS skip_when_equal=%d %s\n", skip_ok,
           (diff_l2 == 0 && diff_rep == 0 && skip_ok) ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
