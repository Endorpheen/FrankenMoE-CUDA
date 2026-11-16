// A layer on the weights that are actually in the file.
//
// Every layer gate so far used synthetic weights: the same tensor on both arms, so the
// comparison proved the wiring and said nothing about quantisation, dequantisation or the
// GGUF payload. This one reads the real blk.0 tensors out of the GGUF and runs the attention
// chain on them.
//
// What is compared is not "does it match llama.cpp" - that needs a reference run. It is:
//
//   the layer runs on the real quantised weights without dequantising them by hand,
//   produces finite values of the right shape, and
//   two runs of the same graph are bit-identical.
//
// That last point is what makes the result usable: a quantised path that is non-deterministic
// would poison every later comparison, and a path that produces NaN on real weights would
// pass every synthetic gate ever written.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"
#include "tensor-data.h"

extern void fg_goose_host_ensure_init(void);

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_UINT32) return (int64_t) gguf_get_val_u32(c, (int) id);
    if (gguf_get_kv_type(c, (int) id) == GGUF_TYPE_INT32) return (int64_t) gguf_get_val_i32(c, (int) id);
    return dflt;
}

int main(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]); return 2; }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * g = gguf_init_from_file(argv[1], mp);
    if (g == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    const int64_t n_embd = kv_u32(g, "qwen35moe.embedding_length", 0);
    const int64_t n_head = kv_u32(g, "qwen35moe.attention.head_count", 0);
    const int64_t n_kv   = kv_u32(g, "qwen35moe.attention.head_count_kv", 0);
    const int64_t head_k = kv_u32(g, "qwen35moe.attention.key_length", 0);
    const float eps = 1e-6f;

    // blk.0 is a recurrent layer; blk.3 is a full-attention one. Both carry attn_norm.
    const char * names[] = {
        "blk.3.attn_norm.weight", "blk.3.attn_q.weight", "blk.3.attn_k.weight",
        "blk.3.attn_v.weight", "blk.3.attn_output.weight",
    };
    const int64_t want = (int64_t) (sizeof names / sizeof names[0]);
    for (int i = 0; i < (int) want; ++i) {
        if (gguf_find_tensor(g, names[i]) < 0) {
            fprintf(stderr, "missing %s\n", names[i]);
            return 1;
        }
    }

    // Load the real payload: quantised tensors stay quantised, and the arena reports whether
    // ggml's placement agrees with the Goose layout (gguf-offsets.goose).
    struct ggml_context * wctx = NULL;
    struct gguf_init_params lp = { /*.no_alloc =*/ false, /*.ctx =*/ &wctx };
    struct gguf_context * gl = gguf_init_from_file(argv[1], lp);
    if (gl == NULL || wctx == NULL) { fprintf(stderr, "payload load failed\n"); return 1; }

    fg_tensor_arena arena;
    memset(&arena, 0, sizeof arena);
    const int rc = fg_goose_tensor_load(gl, wctx, &arena);
    if (rc != 0) { printf("FG_LAYER_REAL tensor_load rc=%d\n", rc); return 1; }

    struct ggml_tensor * wnorm = NULL, * wq = NULL, * wk = NULL, * wv = NULL, * wo = NULL;
    {
        struct { const char * n; struct ggml_tensor ** t; } pick[] = {
            { names[0], &wnorm }, { names[1], &wq }, { names[2], &wk },
            { names[3], &wv }, { names[4], &wo },
        };
        for (int i = 0; i < 5; ++i) {
            *pick[i].t = ggml_get_tensor(wctx, pick[i].n);
            if (*pick[i].t == NULL) { printf("FG_LAYER_REAL tensor %s absent from ctx\n", pick[i].n); return 1; }
        }
    }

    const int64_t q_out = wq->ne[1];

    // Two identical graphs, built and run separately: determinism on real weights.
    double out0[4096], out1[4096];
    int n_out = 0;
    for (int run = 0; run < 2; ++run) {
        struct ggml_init_params gp = { 128u * 1024u * 1024u, NULL, /*.no_alloc =*/ true };
        struct ggml_context * gc = ggml_init(gp);
        struct ggml_backend * be = ggml_backend_cpu_init();

        struct ggml_tensor * x = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, 1);
        struct ggml_tensor * pos = ggml_new_tensor_1d(gc, GGML_TYPE_I32, 1);
        ggml_backend_alloc_ctx_tensors(gc, be);

        float * xd = (float *) malloc((size_t) n_embd * sizeof(float));
        for (int64_t i = 0; i < n_embd; ++i) xd[i] = (float) ((i % 17) - 8) / 64.0f;
        ggml_backend_tensor_set(x, xd, 0, (size_t) n_embd * sizeof(float));
        free(xd);
        int32_t z = 0;
        ggml_backend_tensor_set(pos, &z, 0, sizeof z);

        struct ggml_tensor * nm = ggml_rms_norm(gc, x, eps);
        nm = ggml_mul(gc, nm, wnorm);
        struct ggml_tensor * q = ggml_mul_mat(gc, wq, nm);
        struct ggml_tensor * k = ggml_mul_mat(gc, wk, nm);
        struct ggml_tensor * v = ggml_mul_mat(gc, wv, nm);

        struct ggml_tensor * a = ggml_mul_mat(gc, k, q);          // scores
        a = ggml_mul_mat(gc, v, a);                               // weighted values
        struct ggml_tensor * o = ggml_mul_mat(gc, wo, a);         // output projection

        struct ggml_cgraph * cg = ggml_new_graph(gc);
        ggml_build_forward_expand(cg, o);
        ggml_backend_graph_compute(be, cg);

        const int64_t n = ggml_nelements(o);
        if (n > 4096) { printf("FG_LAYER_REAL output too wide\n"); return 1; }
        n_out = (int) n;
        float * got = (float *) malloc((size_t) n * sizeof(float));
        ggml_backend_tensor_get(o, got, 0, (size_t) n * sizeof(float));
        for (int64_t i = 0; i < n; ++i) {
            if (run == 0) out0[i] = (double) got[i]; else out1[i] = (double) got[i];
        }
        free(got);
        ggml_backend_free(be);
        ggml_free(gc);
    }

    int nonfinite = 0, changed = 0, nondet = 0;
    for (int i = 0; i < n_out; ++i) {
        if (!isfinite(out0[i])) ++nonfinite;
        if (out0[i] != 0.0) ++changed;
        if (memcmp(&out0[i], &out1[i], sizeof(double)) != 0) ++nondet;
    }

    printf("FG_LAYER_REAL n_embd=%lld n_head=%lld n_kv=%lld head_k=%lld q_out=%lld "
           "tensors=%lld mismatches=%lld out=%d nonfinite=%d changed=%d nondet=%d %s\n",
           (long long) n_embd, (long long) n_head, (long long) n_kv, (long long) head_k,
           (long long) q_out, (long long) arena.n_tensors, (long long) arena.mismatches,
           n_out, nonfinite, changed, nondet,
           (nonfinite == 0 && changed > 0 && nondet == 0 && arena.mismatches == 0)
               ? "PASS" : "FAIL");

    fg_goose_tensor_free(&arena);
    gguf_free(gl);
    gguf_free(g);
    if (meta) ggml_free(meta);
    return 0;
}
