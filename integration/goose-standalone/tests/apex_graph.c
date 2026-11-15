// Apex subgraph gate: a real matmul at the model's own dimensions, built by the
// Goose graph builder.
//
// This is a TEST. It takes the extents from the actual Apex weights file (the
// recurrent layer's fused qkv projection), creates the weight and activation
// tensors at those extents, asks `gb_common` for the node, and runs it on the CPU
// backend.
//
// What makes this different from EXP681: the shapes are not 4-element toys. The
// weight is [2048 x qkv_w] taken from the model, and qkv_w is the sum of the
// query, key and value widths the architecture builder computes. A graph that
// assembles at real Apex dimensions is the first thing that can be extended into
// a full layer.
//
// It does not load the weights and does not start a model: the buffers are filled
// with a deterministic pattern.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"

extern int64_t gb_common(int64_t p);
extern void fg_goose_host_ensure_init(void);

#define P_N_EMBD 0
#define P_N_VOCAB 1
#define P_N_LAYER 2
#define P_N_LAYER_ALL 3
#define P_N_HEAD 4
#define P_N_HEAD_KV 5
#define P_N_FF 6
#define P_N_EXPERT 7
#define P_N_EXPERT_USED 8
#define P_N_FF_EXP 9
#define P_N_FF_SHEXP 10
#define P_D_CONV 11
#define P_D_INNER 12
#define P_D_STATE 13
#define P_DT_RANK 14
#define P_N_GROUP 15
#define P_HEAD_K 16
#define P_INTERVAL 17
#define P_HAS_OUTPUT 18

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    switch (gguf_get_kv_type(c, (int) id)) {
        case GGUF_TYPE_UINT32: return (int64_t) gguf_get_val_u32(c, (int) id);
        case GGUF_TYPE_INT32:  return (int64_t) gguf_get_val_i32(c, (int) id);
        case GGUF_TYPE_UINT64: return (int64_t) gguf_get_val_u64(c, (int) id);
        case GGUF_TYPE_INT64:  return gguf_get_val_i64(c, (int) id);
        default: return dflt;
    }
}

// Bits of a float, which is how ggml stores its rope parameters.
static int32_t float_bits_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]);
        return 2;
    }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * gguf = gguf_init_from_file(argv[1], mp);
    if (gguf == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    // The weight extents come from the file, so the graph is built at the model's
    // dimensions rather than at invented ones.
    const int64_t wid = gguf_find_tensor(gguf, "blk.0.attn_qkv.weight");
    if (wid < 0) {
        fprintf(stderr, "blk.0.attn_qkv.weight not found\n");
        gguf_free(gguf);
        return 1;
    }
    const int64_t * wne = gguf_get_tensor_ne(gguf, (int) wid);
    const int64_t K = wne[0];   // 2048
    const int64_t N = wne[1];   // fused qkv width

    // The architecture builder must agree with the file on that width.
    int64_t p[32] = { 0 };
    p[P_N_EMBD]      = kv_u32(gguf, "qwen35moe.embedding_length", 0);
    p[P_N_VOCAB]     = kv_u32(gguf, "qwen35moe.vocab_size", 0);
    if (p[P_N_VOCAB] == 0) {
        const int64_t tk = gguf_find_key(gguf, "tokenizer.ggml.tokens");
        if (tk >= 0) p[P_N_VOCAB] = (int64_t) gguf_get_arr_n(gguf, (int) tk);
    }
    p[P_N_LAYER]       = kv_u32(gguf, "qwen35moe.block_count", 0);
    p[P_N_LAYER_ALL]   = p[P_N_LAYER] + 1;
    p[P_N_HEAD]        = kv_u32(gguf, "qwen35moe.attention.head_count", 0);
    p[P_N_HEAD_KV]     = kv_u32(gguf, "qwen35moe.attention.head_count_kv", 0);
    p[P_N_FF]          = kv_u32(gguf, "qwen35moe.feed_forward_length", 0);
    p[P_N_EXPERT]      = kv_u32(gguf, "qwen35moe.expert_count", 0);
    p[P_N_EXPERT_USED] = kv_u32(gguf, "qwen35moe.expert_used_count", 0);
    p[P_N_FF_EXP]      = kv_u32(gguf, "qwen35moe.expert_feed_forward_length", 0);
    p[P_N_FF_SHEXP]    = kv_u32(gguf, "qwen35moe.expert_shared_feed_forward_length", 0);
    p[P_D_CONV]        = kv_u32(gguf, "qwen35moe.ssm.conv_kernel", 0);
    p[P_D_INNER]       = kv_u32(gguf, "qwen35moe.ssm.inner_size", 0);
    p[P_D_STATE]       = kv_u32(gguf, "qwen35moe.ssm.state_size", 0);
    p[P_DT_RANK]       = kv_u32(gguf, "qwen35moe.ssm.time_step_rank", 0);
    p[P_N_GROUP]       = kv_u32(gguf, "qwen35moe.ssm.group_count", 0);
    p[P_HEAD_K]        = kv_u32(gguf, "qwen35moe.attention.key_length", 0);
    p[P_INTERVAL]      = kv_u32(gguf, "qwen35moe.full_attention_interval", 0);
    p[P_HAS_OUTPUT]    = 1;

    int64_t expect[3] = { 0, 0, 0 };
    // tid 9 = T_WQKV in the architecture builder; layer 0 is recurrent here.
    const int64_t declared = q35_shape((sl_i64) { p, 32 }, 1 /* K_RECR */, 9, (sl_i64) { expect, 3 });
    const int declared_ok = declared == 1 && expect[0] == K && expect[1] == N;

    // A graph at the model's own dimensions.
    struct ggml_init_params gp = { 256 * 1024 * 1024, NULL, /*.no_alloc =*/ true };
    struct ggml_context * gc = ggml_init(gp);
    ggml_backend_t backend = ggml_backend_cpu_init();
    if (gc == NULL || backend == NULL) {
        printf("FG_APEX_GRAPH init failed\n");
        return 2;
    }

    struct ggml_tensor * w = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, N);
    struct ggml_tensor * x = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, 1);
    if (!ggml_backend_alloc_ctx_tensors(gc, backend)) {
        printf("FG_APEX_GRAPH alloc failed\n");
        return 2;
    }

    // Deterministic fill; the point is the shape and the execution, not the values.
    float * wd = (float *) malloc((size_t) (K * N) * sizeof(float));
    float * xd = (float *) malloc((size_t) K * sizeof(float));
    if (wd == NULL || xd == NULL) {
        printf("FG_APEX_GRAPH host alloc failed\n");
        return 2;
    }
    for (int64_t i = 0; i < K * N; ++i) wd[i] = (float) ((i % 17) - 8) / 64.0f;
    for (int64_t i = 0; i < K; ++i)      xd[i] = (float) ((i % 5) + 1) / 8.0f;
    ggml_backend_tensor_set(w, wd, 0, (size_t) (K * N) * sizeof(float));
    ggml_backend_tensor_set(x, xd, 0, (size_t) K * sizeof(float));

    // A three-node prefix of an Apex layer, each node emitted by the Goose builder:
    //   mode 4  rms_norm on the activation      (GGML_OP_RMS_NORM, eps in param 0)
    //   mode 11 mul_mat against the real weight (GGML_OP_MUL_MAT)
    //   mode 3  silu on the projection          (unary, GGML_UNARY_OP_SILU)
    const float eps = 1e-6f;
    int32_t eps_bits;
    memcpy(&eps_bits, &eps, sizeof eps_bits);

    uint64_t a_norm[8] = { 4, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) x,
                           0, 0, (uint64_t) GGML_OP_RMS_NORM, 0, (uint64_t) (int64_t) eps_bits };
    struct ggml_tensor * normed =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_norm);
    if (normed == NULL) {
        printf("FG_APEX_GRAPH rms_norm returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    uint64_t a_mm[8] = { 11, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) w,
                         (uint64_t) (uintptr_t) normed, 0, (uint64_t) GGML_OP_MUL_MAT, 0, 0 };
    struct ggml_tensor * proj =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_mm);
    if (proj == NULL) {
        printf("FG_APEX_GRAPH mul_mat returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    uint64_t a_silu[8] = { 3, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) proj,
                           0, 0, (uint64_t) GGML_UNARY_OP_SILU, 0, 0 };
    struct ggml_tensor * gated =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_silu);
    if (gated == NULL) {
        printf("FG_APEX_GRAPH silu returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // mode 11 again: the output projection back to the embedding width.
    struct ggml_tensor * w2 = ggml_new_tensor_2d(gc, GGML_TYPE_F32, N, K);
    ggml_backend_alloc_ctx_tensors(gc, backend);
    float * w2d = (float *) malloc((size_t) (N * K) * sizeof(float));
    for (int64_t i = 0; i < N * K; ++i) w2d[i] = (float) ((i % 13) - 6) / 128.0f;
    ggml_backend_tensor_set(w2, w2d, 0, (size_t) (N * K) * sizeof(float));

    uint64_t a_mm2[8] = { 11, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) w2,
                          (uint64_t) (uintptr_t) gated, 0, (uint64_t) GGML_OP_MUL_MAT, 0, 0 };
    struct ggml_tensor * proj_out =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_mm2);
    if (proj_out == NULL) {
        printf("FG_APEX_GRAPH out projection returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // mode 0 with op ADD: the residual connection back to the activation.
    uint64_t a_add[8] = { 0, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) proj_out,
                          (uint64_t) (uintptr_t) x, 0, (uint64_t) GGML_OP_ADD, 0, 0 };
    struct ggml_tensor * out =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_add);
    if (out == NULL) {
        printf("FG_APEX_GRAPH residual add returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    struct ggml_cgraph * gf = ggml_new_graph(gc);
    ggml_build_forward_expand(gf, out);
    const enum ggml_status st = ggml_backend_graph_compute(backend, gf);

    // The output projection returns to the embedding width, so the result is [K, 1].
    const int shape_ok = out->ne[0] == K && out->ne[1] == 1;

    // The host computes the same prefix independently: rms_norm over K elements,
    // one dot product per output column, then silu on the first.
    double ss = 0.0;
    for (int64_t i = 0; i < K; ++i) ss += (double) xd[i] * (double) xd[i];
    const double rms = sqrt(ss / (double) K + (double) eps);
    double acc0 = 0.0;
    for (int64_t i = 0; i < K; ++i) acc0 += (double) wd[i] * ((double) xd[i] / rms);
    (void) acc0;
    // Five-node host reference: silu on all N columns, then one output projection
    // column, then the residual.
    double out_acc = 0.0;
    for (int64_t j = 0; j < N; ++j) {
        double pj = 0.0;
        for (int64_t i = 0; i < K; ++i) {
            pj += (double) wd[i + j * K] * ((double) xd[i] / rms);
        }
        const double sj = pj / (1.0 + exp(-pj));
        out_acc += (double) w2d[j] * sj;   // w2 is [N, K]: column 0 is j + 0*N
    }
    const double want0 = out_acc + (double) xd[0];

    float got0 = 0.0f;
    ggml_backend_tensor_get(out, &got0, 0, sizeof got0);
    const double rel = (want0 != 0.0) ? (fabs((double) got0 - want0) / fabs(want0)) : fabs((double) got0);

    // ---- the MoE expert matmul, common to all 42 layers ----
    // mode 12 = mul_mat_id. The module requires: ids are i32, a and b are 3D-free
    // (ne[3]==1), ids have ne[2]==ne[3]==1 and ne[1]==ne(b,2), a and b agree on
    // ne[0], and ids' width divides the expert count.
    {
        int64_t n_ff_exp = kv_u32(gguf, "qwen35moe.expert_feed_forward_length", 0);
        int64_t n_expert = p[P_N_EXPERT];
        int64_t n_used   = p[P_N_EXPERT_USED];
        if (n_ff_exp <= 0) n_ff_exp = 512;

        struct ggml_tensor * ew = ggml_new_tensor_3d(gc, GGML_TYPE_F32, K, n_ff_exp, n_expert);
        struct ggml_tensor * eb = ggml_new_tensor_3d(gc, GGML_TYPE_F32, K, 1, 1);
        struct ggml_tensor * ids = ggml_new_tensor_2d(gc, GGML_TYPE_I32, n_used, 1);
        ggml_backend_alloc_ctx_tensors(gc, backend);

        float * ewd = (float *) malloc((size_t) (K * n_ff_exp * n_expert) * sizeof(float));
        float * ebd = (float *) malloc((size_t) K * sizeof(float));
        int32_t * idd = (int32_t *) malloc((size_t) n_used * sizeof(int32_t));
        if (!ewd || !ebd || !idd) { printf("FG_APEX_GRAPH moe alloc failed\n"); return 2; }
        for (int64_t i = 0; i < K * n_ff_exp * n_expert; ++i) ewd[i] = (float) ((i % 11) - 5) / 64.0f;
        for (int64_t i = 0; i < K; ++i) ebd[i] = (float) ((i % 3) + 1) / 4.0f;
        for (int64_t i = 0; i < n_used; ++i) idd[i] = (int32_t) i;   // experts 0..n_used-1
        ggml_backend_tensor_set(ew, ewd, 0, (size_t) (K * n_ff_exp * n_expert) * sizeof(float));
        ggml_backend_tensor_set(eb, ebd, 0, (size_t) K * sizeof(float));
        ggml_backend_tensor_set(ids, idd, 0, (size_t) n_used * sizeof(int32_t));

        uint64_t a_moe[8] = { 12, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) ew,
                              (uint64_t) (uintptr_t) eb, (uint64_t) (uintptr_t) ids,
                              (uint64_t) GGML_OP_MUL_MAT_ID, 0, 0 };
        struct ggml_tensor * moe =
            (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) a_moe);
        if (moe == NULL) {
            printf("FG_APEX_GRAPH mul_mat_id returned null\n");
        } else {
            ggml_backend_alloc_ctx_tensors(gc, backend);
            struct ggml_cgraph * mf = ggml_new_graph(gc);
            ggml_build_forward_expand(mf, moe);
            const enum ggml_status mst = ggml_backend_graph_compute(backend, mf);

            // Expert 0, row 0: the host computes it from the same buffers.
            double want_e0 = 0.0;
            for (int64_t i = 0; i < K; ++i) want_e0 += (double) ewd[i] * (double) ebd[i];
            float got_e0 = 0.0f;
            ggml_backend_tensor_get(moe, &got_e0, 0, sizeof got_e0);
            const double erel = (want_e0 != 0.0) ? fabs((double) got_e0 - want_e0) / fabs(want_e0)
                                                 : fabs((double) got_e0);
            const int mshape_ok = moe->ne[0] == n_ff_exp && moe->ne[1] == n_used;
            printf("FG_APEX_MOE n_ff_exp=%lld n_expert=%lld n_used=%lld shape_ok=%d "
                   "status=%d first=%.6f want=%.6f rel=%.3g %s\n",
                   (long long) n_ff_exp, (long long) n_expert, (long long) n_used,
                   mshape_ok, (int) mst, got_e0, want_e0, erel,
                   (mshape_ok && mst == GGML_STATUS_SUCCESS && erel < 1e-4) ? "PASS" : "FAIL");
        }
        free(ewd); free(ebd); free(idd);
    }

    // rope is NOT tested here. mode 25 exists and the node builds, but the result
    // came out NaN: the parameter set the gate passed (freq_base, freq_scale,
    // ext_factor, attn_factor, beta_fast, beta_slow) does not match what the ggml
    // rope kernel reads, and guessing it further would be a false positive waiting
    // to happen. Recorded as "not proven" in EXP-2026-11-15-688 rather than shipped
    // as a passing test.

    printf("FG_APEX_GRAPH nodes=5 K=%lld N=%lld declared=%lld declared_ok=%d shape_ok=%d "
           "status=%d first=%.6f want=%.6f rel=%.3g %s\n",
           (long long) K, (long long) N, (long long) declared, declared_ok, shape_ok,
           (int) st, got0, want0, rel,
           (declared_ok && shape_ok && st == GGML_STATUS_SUCCESS && rel < 1e-4) ? "PASS" : "FAIL");

    free(wd);
    free(w2d);
    free(xd);
    ggml_backend_free(backend);
    ggml_free(gc);
    gguf_free(gguf);
    if (meta) ggml_free(meta);
    return 0;
}
