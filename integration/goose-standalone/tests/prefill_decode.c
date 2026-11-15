// Prefill vs decode, one chain, differing only in the query token count.
//
// The previous attempt wrote two chains by hand and they disagreed - which says nothing
// about prefill, only that two hand-written chains differ. This version builds the k/v
// ONCE and shares every node; the two arms differ in exactly two numbers: n_q and the mask
// width.
//
//   prefill: n_q = 4, q positions 0..3, causal mask [4, 4]
//   decode:  n_q = 1, q position 3,   mask [4, 1] all zeros (last row of the same causal mask)
//
// The expected relationship is equality up to f32 accumulation order: flash attention over
// 4 queries and over 1 query select different internal tilings, so the products are summed
// in a different order. That is a real difference and not a wiring bug, so it is stated
// before the run and bounded rather than assumed away.
//
// The check is therefore two-sided:
//   - max |prefill_last - decode| must be within f32 rounding;
//   - ropeing the decode query at the WRONG position must exceed that by orders of
//     magnitude, or the comparison is insensitive to the thing under test.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

#define T4 4
#define K 32
#define HD 8

// The shared chain. `xq` is [K, n_q], `kp`/`vp` are already roped and permuted to
// [HD, n_kv, 1, 1], the mask is [n_kv, n_q, 1, 1] F16.
static struct ggml_tensor * chain(struct ggml_context * gc, struct ggml_tensor * xq,
                                  struct ggml_tensor * wq, struct ggml_tensor * wo,
                                  struct ggml_tensor * kp, struct ggml_tensor * vp,
                                  struct ggml_tensor * pos, struct ggml_tensor * mask,
                                  int64_t n_q, float eps, float scale) {
    struct ggml_tensor * nm = ggml_rms_norm(gc, xq, eps);
    struct ggml_tensor * q = ggml_mul_mat(gc, wq, nm);
    struct ggml_tensor * q3 = ggml_reshape_4d(gc, q, HD, 1, n_q, 1);
    struct ggml_tensor * qr = ggml_rope_ext(gc, q3, pos, NULL, HD, GGML_ROPE_TYPE_NORMAL,
                                            4096, 1e7f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f);
    struct ggml_tensor * qp = ggml_permute(gc, qr, 0, 2, 1, 3);
    struct ggml_tensor * a = ggml_flash_attn_ext(gc, qp, kp, vp, mask, scale, 0.0f, 0.0f);
    struct ggml_tensor * a2 = ggml_reshape_2d(gc, a, HD, n_q);
    return ggml_mul_mat(gc, wo, a2);
}

static void fill_mask(ggml_fp16_t * m, int64_t n_kv, int64_t n_q, int64_t offset) {
    for (int64_t i = 0; i < n_q; ++i) {
        for (int64_t j = 0; j < n_kv; ++j) {
            const int64_t qi = offset + i;
            m[i * n_kv + j] = ggml_fp32_to_fp16((j <= qi) ? 0.0f : -INFINITY);
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const float eps = 1e-6f;
    const float scale = 1.0f / sqrtf((float) HD);

    float wq_d[K * HD], wk_d[K * HD], wv_d[K * HD], wo_d[HD * K], x_d[T4 * K];
    for (int i = 0; i < K * HD; ++i) {
        wq_d[i] = (float) ((i * 7 % 19) - 9) / 16.0f;
        wk_d[i] = (float) ((i * 11 % 23) - 11) / 16.0f;
        wv_d[i] = (float) ((i * 13 % 29) - 14) / 16.0f;
    }
    for (int i = 0; i < HD * K; ++i) wo_d[i] = (float) ((i * 17 % 31) - 15) / 16.0f;
    for (int i = 0; i < T4 * K; ++i) x_d[i] = (float) ((i * 5 % 37) - 18) / 32.0f;

    // One context so k/v TENSORS are literally shared between the arms.
    struct ggml_init_params gp = { 128u * 1024u * 1024u, NULL, false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_PREFILL_DECODE init failed\n"); return 2; }

    struct ggml_tensor * x  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, T4);
    struct ggml_tensor * wq = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, HD);
    struct ggml_tensor * wk = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, HD);
    struct ggml_tensor * wv = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, HD);
    struct ggml_tensor * wo = ggml_new_tensor_2d(gc, GGML_TYPE_F32, HD, K);
    memcpy(x->data, x_d, sizeof x_d);
    memcpy(wq->data, wq_d, sizeof wq_d);
    memcpy(wk->data, wk_d, sizeof wk_d);
    memcpy(wv->data, wv_d, sizeof wv_d);
    memcpy(wo->data, wo_d, sizeof wo_d);

    // k/v over all four positions, roped at 0..3, permuted - built once.
    struct ggml_tensor * pos4 = ggml_new_tensor_1d(gc, GGML_TYPE_I32, T4);
    for (int i = 0; i < T4; ++i) ((int32_t *) pos4->data)[i] = i;
    struct ggml_tensor * nm = ggml_rms_norm(gc, x, eps);
    struct ggml_tensor * k = ggml_mul_mat(gc, wk, nm);
    struct ggml_tensor * v = ggml_mul_mat(gc, wv, nm);
    struct ggml_tensor * k3 = ggml_reshape_4d(gc, k, HD, 1, T4, 1);
    struct ggml_tensor * v3 = ggml_reshape_4d(gc, v, HD, 1, T4, 1);
    struct ggml_tensor * kr = ggml_rope_ext(gc, k3, pos4, NULL, HD, GGML_ROPE_TYPE_NORMAL,
                                            4096, 1e7f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f);
    struct ggml_tensor * kp = ggml_permute(gc, kr, 0, 2, 1, 3);
    struct ggml_tensor * vp = ggml_permute(gc, v3, 0, 2, 1, 3);

    // ---- prefill: 4 queries, causal mask ----
    struct ggml_tensor * m4 = ggml_new_tensor_4d(gc, GGML_TYPE_F16, T4, T4, 1, 1);
    fill_mask((ggml_fp16_t *) m4->data, T4, T4, 0);
    struct ggml_tensor * y4 = chain(gc, x, wq, wo, kp, vp, pos4, m4, T4, eps, scale);

    // ---- decode: 1 query at position 3, same k/v, mask is the last row of the causal one ----
    struct ggml_tensor * pos1 = ggml_new_tensor_1d(gc, GGML_TYPE_I32, 1);
    ((int32_t *) pos1->data)[0] = T4 - 1;
    struct ggml_tensor * m1 = ggml_new_tensor_4d(gc, GGML_TYPE_F16, T4, 1, 1, 1);
    fill_mask((ggml_fp16_t *) m1->data, T4, 1, T4 - 1);
    struct ggml_tensor * xq = ggml_view_2d(gc, x, K, 1, (size_t) K * sizeof(float),
                                           (size_t) (T4 - 1) * K * sizeof(float));
    struct ggml_tensor * y1 = chain(gc, xq, wq, wo, kp, vp, pos1, m1, 1, eps, scale);

    // ---- the control: identical decode with the WRONG rope position ----
    struct ggml_tensor * pos0 = ggml_new_tensor_1d(gc, GGML_TYPE_I32, 1);
    ((int32_t *) pos0->data)[0] = 0;
    struct ggml_tensor * y0 = chain(gc, xq, wq, wo, kp, vp, pos0, m1, 1, eps, scale);

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, y4);
    ggml_build_forward_expand(g, y1);
    ggml_build_forward_expand(g, y0);
    ggml_graph_compute_with_ctx(gc, g, 1);

    const float * p4 = (const float *) y4->data + (T4 - 1) * K;
    const float * p1 = (const float *) y1->data;
    const float * p0 = (const float *) y0->data;

    double max_ok = 0.0, max_wrong = 0.0, scale_ref = 0.0;
    int bit_diff = 0;
    for (int i = 0; i < K; ++i) {
        const double d_ok = fabs((double) p4[i] - (double) p1[i]);
        const double d_bad = fabs((double) p4[i] - (double) p0[i]);
        if (d_ok > max_ok) max_ok = d_ok;
        if (d_bad > max_wrong) max_wrong = d_bad;
        if (fabs((double) p4[i]) > scale_ref) scale_ref = fabs((double) p4[i]);
        if (memcmp(&p4[i], &p1[i], sizeof(float)) != 0) ++bit_diff;
    }

    const double rel = (scale_ref > 0.0) ? max_ok / scale_ref : max_ok;
    // Sensitivity: a wrong rope position must be far larger than the rounding difference.
    const int sensitive = max_wrong > 100.0 * (max_ok > 1e-9 ? max_ok : 1e-9);

    printf("FG_PREFILL_DECODE tokens=%d bit_diff=%d max_abs=%.9g rel=%.3g "
           "wrong_pos_max=%.9g sensitive=%d %s\n",
           T4, bit_diff, max_ok, rel, max_wrong, sensitive,
           (rel < 1e-5 && sensitive) ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
