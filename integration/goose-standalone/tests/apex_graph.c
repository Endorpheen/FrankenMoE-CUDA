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

    // mode 11 = mul_mat; the module validates can_mul_mat and derives the result
    // shape from the operand extents.
    uint64_t args[8] = { 11, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) w,
                         (uint64_t) (uintptr_t) x, 0, (uint64_t) GGML_OP_MUL_MAT, 0, 0 };
    struct ggml_tensor * r =
        (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
    if (r == NULL) {
        printf("FG_APEX_GRAPH builder returned null\n");
        return 1;
    }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    struct ggml_cgraph * gf = ggml_new_graph(gc);
    ggml_build_forward_expand(gf, r);
    const enum ggml_status st = ggml_backend_graph_compute(backend, gf);

    // Two checks that the node is real: the result extent, and a value computed
    // independently on the host for the first output row.
    const int shape_ok = r->ne[0] == N && r->ne[1] == 1;
    double want0 = 0.0;
    for (int64_t i = 0; i < K; ++i) want0 += (double) wd[i] * (double) xd[i];
    float got0 = 0.0f;
    ggml_backend_tensor_get(r, &got0, 0, sizeof got0);
    const double rel = (want0 != 0.0) ? ((double) got0 - want0) / want0 : (double) got0;

    printf("FG_APEX_GRAPH K=%lld N=%lld declared=%lld declared_ok=%d shape_ok=%d "
           "status=%d first=%.6f want=%.6f rel=%.3g %s\n",
           (long long) K, (long long) N, (long long) declared, declared_ok, shape_ok,
           (int) st, got0, want0, rel,
           (declared_ok && shape_ok && st == GGML_STATUS_SUCCESS && rel < 1e-4) ? "PASS" : "FAIL");

    free(wd);
    free(xd);
    ggml_backend_free(backend);
    ggml_free(gc);
    gguf_free(gguf);
    if (meta) ggml_free(meta);
    return 0;
}
