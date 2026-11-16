// Two real Apex tensors chained through the Goose builder.
//
// real_values proved one real tensor through one node. Composition needs nodes in sequence, carrying the
// intermediate result — that is the part PASS-COMPOSITION names as missing ("nothing holds x across
// iterations"). This does the smallest honest version of it: blk.3.attn_q.weight then
// blk.3.attn_output.weight, both real q4_K rows, both nodes emitted by gb_common, with the first node's
// output feeding the second.
//
// The reference is the same two steps with raw ggml calls on the same bytes. If the builder emits equivalent
// nodes, the two chains agree exactly; if it mis-orders, mis-shapes or drops the intermediate, they do not.
//
// What it does not prove: that the chain is a layer. The RMS norm, the QG split, rope and the attention over
// k/v are all absent, so this is a chaining test, not an attention test.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

static struct ggml_tensor * chain_gb(int64_t mode, struct ggml_context * gc,
                                     struct ggml_tensor * a, struct ggml_tensor * b, int64_t op) {
    uint64_t args[16] = { (uint64_t) mode, (uint64_t) (uintptr_t) gc,
                          (uint64_t) (uintptr_t) a, (uint64_t) (uintptr_t) b, 0,
                          (uint64_t) op, 0, 0,0,0,0,0,0,0,0,0 };
    return (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
}

static struct ggml_tensor * gb(int64_t mode, struct ggml_context * gc,
                               struct ggml_tensor * a, struct ggml_tensor * b) {
    return chain_gb(mode, gc, a, b, GGML_OP_MUL_MAT);
}

int main(int argc, char ** argv) {
    goose_init(0, (char **) 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]); return 2; }

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * g = gguf_init_from_file(argv[1], mp);
    if (g == NULL) { printf("FG_REAL_CHAIN cannot read %s\n", argv[1]); return 2; }

    const int tn = gguf_find_tensor(g, "blk.3.attn_norm.weight");
    const int tq = gguf_find_tensor(g, "blk.3.attn_q.weight");
    const int to = gguf_find_tensor(g, "blk.3.attn_output.weight");
    if (tn < 0 || tq < 0 || to < 0) { printf("FG_REAL_CHAIN missing tensors\n"); gguf_free(g); return 2; }

    const enum ggml_type tq_type = gguf_get_tensor_type(g, tq);
    const enum ggml_type to_type = gguf_get_tensor_type(g, to);
    const int64_t q_ne0 = gguf_get_tensor_ne(g, tq)[0];   // 2048 = n_embd
    const int64_t o_ne0 = gguf_get_tensor_ne(g, to)[0];   // 4096 = HD*H
    const int64_t o_ne1 = gguf_get_tensor_ne(g, to)[1];   // 2048
    // the q projection emits 8192 columns; the o projection consumes 4096 of them (HD*H), so the middle of
    // the chain is 4096 wide. Take that many rows of wq: rows are columns of the projection.
    const int64_t ROWS = 4096;

    const size_t q_row = ggml_row_size(tq_type, q_ne0);
    const size_t o_row = ggml_row_size(to_type, o_ne0);
    const int64_t o_rows = o_ne1 < 256 ? o_ne1 : 256;

    FILE * fp = fopen(argv[1], "rb");
    if (fp == NULL) { printf("FG_REAL_CHAIN cannot reopen\n"); gguf_free(g); return 2; }
    const size_t data = gguf_get_data_offset(g);

    const int64_t n_norm = gguf_get_tensor_ne(g, tn)[0];
    const size_t n_norm_bytes = ggml_row_size(gguf_get_tensor_type(g, tn), n_norm);
    uint8_t * n_raw = malloc(n_norm_bytes);
    if (n_raw == NULL) { printf("FG_REAL_CHAIN alloc norm\n"); return 2; }
    if (fseek(fp, (long) (data + gguf_get_tensor_offset(g, tn)), SEEK_SET) != 0 ||
        fread(n_raw, 1, n_norm_bytes, fp) != n_norm_bytes) {
        printf("FG_REAL_CHAIN short read norm\n"); return 2;
    }
    uint8_t * q_raw = malloc(q_row * (size_t) ROWS);
    uint8_t * o_raw = malloc(o_row * (size_t) o_rows);
    if (q_raw == NULL || o_raw == NULL) { printf("FG_REAL_CHAIN alloc\n"); fclose(fp); gguf_free(g); return 2; }
    if (fseek(fp, (long) (data + gguf_get_tensor_offset(g, tq)), SEEK_SET) != 0 ||
        fread(q_raw, 1, q_row * (size_t) ROWS, fp) != q_row * (size_t) ROWS) {
        printf("FG_REAL_CHAIN short read q\n"); return 2;
    }
    if (fseek(fp, (long) (data + gguf_get_tensor_offset(g, to)), SEEK_SET) != 0 ||
        fread(o_raw, 1, o_row * (size_t) o_rows, fp) != o_row * (size_t) o_rows) {
        printf("FG_REAL_CHAIN short read o\n"); return 2;
    }
    fclose(fp);

    struct ggml_init_params gp = { 512u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_REAL_CHAIN ggml_init\n"); return 2; }

    // wq as [q_ne0 x ROWS]: ROWS columns of the projection, so wq @ x is ROWS wide
    struct ggml_tensor * wq = ggml_new_tensor_2d(gc, tq_type, q_ne0, ROWS);
    struct ggml_tensor * wo = ggml_new_tensor_2d(gc, to_type, o_ne0, o_rows);
    struct ggml_tensor * x  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, q_ne0, 1);
    struct ggml_tensor * nw = ggml_new_tensor_1d(gc, GGML_TYPE_F32, n_norm);
    memcpy(nw->data, n_raw, n_norm_bytes);
    free(n_raw);
    memcpy(wq->data, q_raw, q_row * (size_t) ROWS);
    memcpy(wo->data, o_raw, o_row * (size_t) o_rows);
    free(q_raw); free(o_raw);

    float * xd = malloc((size_t) q_ne0 * sizeof(float));
    for (int64_t i = 0; i < q_ne0; ++i) xd[i] = (float) ((i % 17) - 8) / 64.0f;
    memcpy(x->data, xd, (size_t) q_ne0 * sizeof(float));
    free(xd);

    // chain 1: the builder emits rms_norm, the norm scale, then both matmuls.
    // Mode 0 takes its op from the argument, so MUL is reachable there - mode 11 asserts can_mul_mat and
    // an elementwise multiply can never satisfy it.
    uint64_t nargs[16] = { 4, (uint64_t) (uintptr_t) gc, (uint64_t) (uintptr_t) x, 0, 0,
                           (uint64_t) GGML_OP_RMS_NORM, 0, 0,0,0,0,0,0,0,0,0 };
    {
        int32_t eps_bits; const float epsv = 1e-6f; memcpy(&eps_bits, &epsv, sizeof eps_bits);
        nargs[7] = (uint64_t) (int64_t) eps_bits;
    }
    struct ggml_tensor * hn = (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) nargs);
    struct ggml_tensor * hs = hn != NULL ? chain_gb(0, gc, hn, nw, GGML_OP_MUL) : NULL;
    struct ggml_tensor * h1 = hs != NULL ? gb(11, gc, wq, hs) : NULL;
    struct ggml_tensor * h2 = h1 != NULL ? gb(11, gc, wo, h1) : NULL;
    // chain 2: the same three steps with raw ggml
    struct ggml_tensor * rn = ggml_rms_norm(gc, x, 1e-6f);
    rn = ggml_mul(gc, rn, nw);
    struct ggml_tensor * r1 = ggml_mul_mat(gc, wq, rn);
    struct ggml_tensor * r2 = ggml_mul_mat(gc, wo, r1);

    struct ggml_cgraph * cg = ggml_new_graph(gc);
    if (h2 != NULL) ggml_build_forward_expand(cg, h2);
    ggml_build_forward_expand(cg, r2);

    struct ggml_cplan plan = ggml_graph_plan(cg, 4, NULL);
    if (plan.work_size > 0) plan.work_data = malloc(plan.work_size);
    if (ggml_graph_compute(cg, &plan) != GGML_STATUS_SUCCESS) { printf("FG_REAL_CHAIN compute failed\n"); return 2; }

    const float * ph = h2 != NULL ? (const float *) h2->data : NULL;
    const float * pr = (const float *) r2->data;
    double max_abs = 0.0, scale = 0.0;
    int nonfinite = 0, nonzero = 0;
    for (int64_t i = 0; i < o_rows; ++i) {
        if (!isfinite(pr[i])) ++nonfinite;
        if (pr[i] != 0.0f) ++nonzero;
        if (fabs((double) pr[i]) > scale) scale = fabs((double) pr[i]);
        if (ph != NULL) {
            const double d = fabs((double) ph[i] - (double) pr[i]);
            if (d > max_abs) max_abs = d;
        }
    }
    const int ok = h2 != NULL && nonfinite == 0 && nonzero > 0 && max_abs == 0.0;
    printf("FG_REAL_CHAIN norm -> wq[%lldx%lld %s] -> wo[%lldx%lld %s] nodes=4 out=%lld "
           "builder_vs_ggml=%.3e nonfinite=%d nonzero=%d %s\n",
           (long long) q_ne0, (long long) ROWS, ggml_type_name(tq_type),
           (long long) o_ne0, (long long) o_rows, ggml_type_name(to_type),
           (long long) o_rows, max_abs, nonfinite, nonzero, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
