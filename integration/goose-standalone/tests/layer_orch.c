// Layer orchestration: the two residuals attach to different tensors.
//
// qwen35moe.cpp:186-232:
//
//   cur     = rms_norm(inpL, attn_norm)
//   cur     = attn(cur)                    (or the recurrent builder)
//   cur     = cur + inpSA                  <- residual against the LAYER INPUT
//   ffn_res = cur                          <- saved BEFORE the post norm
//   post    = rms_norm(cur, attn_post_norm)
//   cur     = moe(post)
//   cur     = cur + ffn_res                <- residual against the PRE-NORM value
//
// The blocks themselves are already proven (attention EXP-694, recurrent EXP-696, MoE
// EXP-698). What this checks is the wiring between them: two residuals attached to two
// different tensors, and a post-norm that only the FFN branch sees.
//
// The blocks are replaced by a cheap stand-in here on purpose - the question is where the
// results are added, not what they contain. A stand-in makes a wrong attachment visible as
// a large difference instead of a subtle one.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

static int32_t fb_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

// Stand-in for an attention or MoE block: a fixed linear map, identical in both arms.
static struct ggml_tensor * blk(struct ggml_context * gc, struct ggml_tensor * x,
                                struct ggml_tensor * w) {
    return ggml_mul_mat(gc, w, x);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fg_goose_host_ensure_init();

    const int64_t K = 32;
    const float eps = 1e-6f;
    const size_t elem = sizeof(float);

    struct ggml_init_params gp = { 256u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_LAYER_ORCH init failed\n"); return 2; }

    struct ggml_tensor * x   = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, 1);
    struct ggml_tensor * wa  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, K);   // attention map
    struct ggml_tensor * wm  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, K);   // moe map
    {
        struct { struct ggml_tensor * t; int seed; } ws[] = { { x, 13 }, { wa, 7 }, { wm, 11 } };
        for (size_t i = 0; i < 3; ++i) {
            float * d = (float *) ws[i].t->data;
            const int64_t n = ggml_nelements(ws[i].t);
            for (int64_t j = 0; j < n; ++j)
                d[j] = (float) ((j % ws[i].seed) - ws[i].seed / 2) / 16.0f;
        }
    }

    // ---- arm A: the native order, node by node ----
    struct ggml_tensor * inpSA = x;
    struct ggml_tensor * cur = ggml_rms_norm(gc, x, eps);
    cur = blk(gc, cur, wa);
    struct ggml_tensor * after_attn_res = ggml_add(gc, cur, inpSA);   // residual 1
    struct ggml_tensor * ffn_res = after_attn_res;                    // saved pre-norm
    struct ggml_tensor * post = ggml_rms_norm(gc, after_attn_res, eps);
    struct ggml_tensor * ffn = blk(gc, post, wm);
    struct ggml_tensor * outA = ggml_add(gc, ffn, ffn_res);           // residual 2

    // ---- arm B: the same arithmetic written as one expression ----
    struct ggml_tensor * b1 = ggml_rms_norm(gc, x, eps);
    struct ggml_tensor * b2 = ggml_add(gc, ggml_mul_mat(gc, wa, b1), x);
    struct ggml_tensor * b3 = ggml_rms_norm(gc, b2, eps);
    struct ggml_tensor * outB = ggml_add(gc, ggml_mul_mat(gc, wm, b3), b2);

    // ---- the two wrong variants a plausible mistake would produce ----
    // W1: the second residual attached to the POST-NORM value instead of the pre-norm one.
    struct ggml_tensor * w1 = ggml_add(gc, ggml_mul_mat(gc, wm, b3), b3);
    // W2: the second residual attached to the layer INPUT instead of the pre-norm value.
    struct ggml_tensor * w2 = ggml_add(gc, ggml_mul_mat(gc, wm, b3), x);

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, outA);
    ggml_build_forward_expand(g, outB);
    ggml_build_forward_expand(g, w1);
    ggml_build_forward_expand(g, w2);
    ggml_graph_compute_with_ctx(gc, g, 1);

    int d_ab = 0, d_w1 = 0, d_w2 = 0;
    for (int64_t i = 0; i < K; ++i) {
        if (memcmp((char *) outA->data + i * elem, (char *) outB->data + i * elem, elem) != 0) ++d_ab;
        if (memcmp((char *) outA->data + i * elem, (char *) w1->data + i * elem, elem) != 0) ++d_w1;
        if (memcmp((char *) outA->data + i * elem, (char *) w2->data + i * elem, elem) != 0) ++d_w2;
    }

    // The wrong variants must be distinguishable, or the check would pass on any wiring.
    const int distinguishable = (d_w1 > 0 && d_w2 > 0);
    printf("FG_LAYER_ORCH k=%lld differs_ab=%d differs_wrong_postnorm=%d differs_wrong_input=%d "
           "distinguishable=%d %s\n",
           (long long) K, d_ab, d_w1, d_w2, distinguishable,
           (d_ab == 0 && distinguishable) ? "PASS" : "FAIL");

    ggml_free(gc);
    return 0;
}
