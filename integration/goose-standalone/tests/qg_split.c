// QG split gate: the interleaved query/gate split, checked against ggml's own view.
//
// The native builder (qwen35moe.cpp:293-300) creates the query view with a head
// stride of 2*n_embd_head and offset 0; the gate is the same view shifted one head
// width. goose/model/qgate-split.goose computes those numbers; this gate proves the
// module's views equal ggml_view_3d called with the native parameters, element for
// element.
//
// It also proves the two views are NOT the halves of the tensor: a half-split would
// produce a graph that runs and computes nonsense, so a comparison that only checked
// "the split returns two tensors" would pass on a wrong implementation.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    fg_goose_host_ensure_init();

    // Apex attention geometry, from the model file.
    const int64_t K = 2048, H = 16;
    // n_embd_head is the ATTENTION head width, not the q projection width divided by
    // the head count. The projection emits 2 * n_embd_head per head, so 8192 with 16
    // heads means n_embd_head = 256 and a head stride of 512. Using 512 here made the
    // test agree with itself while describing a layout the model does not have.
    const int64_t head_dim_q = 256;          // n_embd_head
    const int64_t elem = 4;                  // f32

    struct ggml_init_params gp = { 256 * 1024 * 1024, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) {
        printf("FG_QG_SPLIT init failed\n");
        return 2;
    }

    const int64_t qfull_n = head_dim_q * H;   // 8192
    struct ggml_tensor * qfull = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, qfull_n);
    float * d = (float *) qfull->data;
    // A distinguishing pattern: q slots get +1 range, gate slots get -1 range, so a
    // half-split and an interleaved split differ in every element.
    for (int64_t h = 0; h < H; ++h) {
        for (int64_t i = 0; i < head_dim_q; ++i) {
            const int64_t at = (2 * h) * head_dim_q + i;   // column-major: index = i + col*K
            const float val = (i < head_dim_q / 2)
                ? (float) (h * 100 + i) / 1024.0f
                : (float) (-(h * 100 + i)) / 1024.0f;
            d[i + (int64_t) at * K] = val;
        }
    }

    // The module's views.
    int64_t out[2] = { 0, 0 };
    const int64_t rc = qg_split((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) qfull,
                               head_dim_q, H, 1, elem, (sl_i64) { out, 2 });
    if (rc != 2) {
        printf("FG_QG_SPLIT module refused (rc=%lld)\n", (long long) rc);
        return 1;
    }
    struct ggml_tensor * qv = (struct ggml_tensor *) (uintptr_t) out[0];
    struct ggml_tensor * gv = (struct ggml_tensor *) (uintptr_t) out[1];

    // ggml's own views with the native parameters.
    struct ggml_tensor * qn = ggml_view_3d(gc, qfull, head_dim_q, H, 1,
        elem * head_dim_q * 2, elem * head_dim_q * 2 * H, 0);
    struct ggml_tensor * gn = ggml_view_3d(gc, qfull, head_dim_q, H, 1,
        elem * head_dim_q * 2, elem * head_dim_q * 2 * H, elem * head_dim_q);

    int diff_q = 0, diff_g = 0;
    const float * a = (const float *) qv->data;
    const float * b = (const float *) qn->data;
    for (int64_t i = 0; i < head_dim_q * H; ++i) {
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0) ++diff_q;
    }
    a = (const float *) gv->data;
    b = (const float *) gn->data;
    for (int64_t i = 0; i < head_dim_q * H; ++i) {
        if (memcmp(&a[i], &b[i], sizeof(float)) != 0) ++diff_g;
    }

    // And the check that catches a half-split: q's first element must be the first
    // element of the tensor, while gate's must be head_dim/2 elements further on.
    const float * flat = (const float *) qfull->data;
    const int q_starts_at_zero = (((const float *) qv->data)[0] == flat[0]);
    const int g_offset_right   = (((const float *) gv->data)[0] == flat[head_dim_q * K]);

    printf("FG_QG_SPLIT head_dim=%lld n_head=%lld elements=%lld "
           "q_differs=%d gate_differs=%d q_at_zero=%d gate_at_head=%d %s\n",
           (long long) head_dim_q, (long long) H, (long long) (head_dim_q * H),
           diff_q, diff_g, q_starts_at_zero, g_offset_right,
           (diff_q == 0 && diff_g == 0 && q_starts_at_zero && g_offset_right)
               ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
