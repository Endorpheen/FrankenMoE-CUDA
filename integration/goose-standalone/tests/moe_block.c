// The MoE block's expert path, at a reduced scale.
//
// Arm A takes the selection from goose/cpu/goose-moe-router.goose; arm B uses ggml's own
// softmax + argsort_top_k. Everything after the selection is ggml in both arms, because
// the expert arithmetic is substrate, not a decision. What this checks is the wiring
// around it: which expert matrix receives the silu, how the ids index the expert axis,
// and that the per-expert outputs are combined with the router weights.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const int64_t n_embd = 32, n_ff = 16, n_expert = 8, n_used = 2;
    const float scale = 2.5f;

    struct ggml_init_params gp = { 64u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_MOE_BLOCK init failed\n"); return 2; }

    struct ggml_tensor * cur  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, 1);
    struct ggml_tensor * rin  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, n_expert);
    struct ggml_tensor * wup  = ggml_new_tensor_3d(gc, GGML_TYPE_F32, n_embd, n_ff, n_expert);
    struct ggml_tensor * wgt  = ggml_new_tensor_3d(gc, GGML_TYPE_F32, n_embd, n_ff, n_expert);
    struct ggml_tensor * wdn  = ggml_new_tensor_3d(gc, GGML_TYPE_F32, n_ff, n_embd, n_expert);
    {
        struct { struct ggml_tensor * t; int seed; } ws[] = {
            { cur, 13 }, { rin, 7 }, { wup, 3 }, { wgt, 5 }, { wdn, 11 },
        };
        for (size_t i = 0; i < sizeof ws / sizeof ws[0]; ++i) {
            float * d = (float *) ws[i].t->data;
            const int64_t n = ggml_nelements(ws[i].t);
            for (int64_t j = 0; j < n; ++j)
                d[j] = (float) ((j % ws[i].seed) - ws[i].seed / 2) / 16.0f;
        }
    }

    // ---- selection: ggml logits, then the Goose router and ggml's own argsort ----
    struct ggml_tensor * logits = ggml_mul_mat(gc, rin, cur);   // [n_expert, 1]
    struct ggml_tensor * pB = ggml_soft_max(gc, logits);
    struct ggml_tensor * idsB = ggml_argsort_top_k(gc, pB, n_used);
    struct ggml_cgraph * gsel = ggml_new_graph(gc);
    ggml_build_forward_expand(gsel, logits);
    ggml_build_forward_expand(gsel, pB);
    ggml_build_forward_expand(gsel, idsB);
    ggml_graph_compute_with_ctx(gc, gsel, 1);

    int64_t idsA[16] = { 0 };
    float wA[16] = { 0 };
    float probs[256] = { 0 };
    sl_f32 s_logits = { (float *) logits->data, n_expert };
    sl_i64 s_ids = { idsA, n_used };
    sl_f32 s_w = { wA, n_used };
    sl_f32 s_probs = { probs, n_expert };
    const int64_t got = moe_route(s_logits, n_used, 1, scale, s_ids, s_w, s_probs);
    if (got != n_used) {
        printf("FG_MOE_BLOCK router returned %lld\n", (long long) got);
        return 1;
    }

    const int32_t * ib = (const int32_t *) idsB->data;
    int ids_match = 1;
    for (int64_t i = 0; i < n_used; ++i) {
        if ((int64_t) ib[i] != idsA[i]) {
            ids_match = 0;
            printf("  id[%lld] goose=%lld ggml=%d\n", (long long) i, (long long) idsA[i], ib[i]);
        }
    }

    // ---- the expert path ----
    struct ggml_tensor * ids = ggml_new_tensor_2d(gc, GGML_TYPE_I32, n_used, 1);
    struct ggml_tensor * wt  = ggml_new_tensor_3d(gc, GGML_TYPE_F32, 1, n_used, 1);

    struct ggml_tensor * gate_e = ggml_mul_mat_id(gc, wgt, cur, ids);   // [n_ff, n_used, 1]
    struct ggml_tensor * up_e   = ggml_mul_mat_id(gc, wup, cur, ids);
    struct ggml_tensor * act    = ggml_mul(gc, ggml_silu(gc, gate_e), up_e);
    struct ggml_tensor * down_e = ggml_mul_mat_id(gc, wdn, act, ids);   // [n_embd, n_used, 1]
    struct ggml_tensor * wmul = ggml_mul(gc, down_e, wt);   // [n_embd, n_used, 1]
    // The sum is over the EXPERT axis, which is dim 1. ggml_sum_rows sums over dim 0, so
    // the tensor is transposed, summed and reshaped back - summing dim 0 directly gives a
    // [1, n_used, 1] result, which is what the first version of this gate produced.
    struct ggml_tensor * tr = ggml_cont(gc, ggml_transpose(gc, wmul));   // [n_used, n_embd, 1]
    struct ggml_tensor * sm = ggml_sum_rows(gc, tr);                     // [1, n_embd, 1]
    struct ggml_tensor * moe_out = ggml_reshape_2d(gc, sm, n_embd, 1);

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, moe_out);

    memcpy(ids->data, ib, (size_t) n_used * sizeof(int32_t));
    {
        float * wd = (float *) wt->data;
        for (int64_t i = 0; i < n_used; ++i) wd[i] = wA[i];
    }
    ggml_graph_compute_with_ctx(gc, g, 1);

    const int64_t n_out = ggml_nelements(moe_out);
    float * out = (float *) malloc((size_t) n_out * sizeof(float));
    memcpy(out, moe_out->data, (size_t) n_out * sizeof(float));

    int nonzero = 0;
    for (int64_t i = 0; i < n_out; ++i) {
        if (out[i] != 0.0f) { nonzero = 1; break; }
    }

    // The weighted sum must differ from an unweighted one: if the weights never reached
    // the multiply, both would be equal and this would catch it.
    float wsum = 0.0f;
    for (int64_t i = 0; i < n_used; ++i) wsum += wA[i];
    const int weights_applied = (wsum > 0.0f);

    printf("FG_MOE_BLOCK n_embd=%lld n_ff=%lld n_expert=%lld n_used=%lld ids_match=%d "
           "out_n=%lld nonzero=%d wsum=%.6f router=%s\n",
           (long long) n_embd, (long long) n_ff, (long long) n_expert, (long long) n_used,
           ids_match, (long long) n_out, nonzero, (double) wsum,
           (ids_match && nonzero && weights_applied && n_out == n_embd) ? "PASS" : "FAIL");

    free(out);
    ggml_free(gc);
    return 0;
}
