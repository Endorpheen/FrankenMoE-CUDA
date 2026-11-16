// The value path: a REAL quantised tensor from the Apex GGUF, through a kernel.
//
// Every other harness in this directory runs at real dimensions with fabricated values (apex_graph says so
// in its own header). frankenmoe-v4 reads the real 21.7 GB payload but feeds no kernel. Nothing joined the
// two - see REAL-VALUES-GAP-20261117.
//
// This joins them for one tensor. It loads blk.3.attn_q.weight in its own quantised type, multiplies it by
// a deterministic vector through ggml's quantised mul_mat, and compares that against the same weights
// dequantised to f32 and multiplied with a plain f32 mul_mat. The two paths do not accumulate identically
// by design, so the check is relative agreement, not equality - but a broken dequantisation, a wrong block
// size or a misread offset moves the result far outside that tolerance.
//
// What it does NOT prove: that the model's answer is right. That needs the integrated runtime's logits for
// the same tensor, which is the next step, not this one.

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

// The Goose graph builder, called the way apex_layer calls it: a mode word, then a packed argument block.
// Adding this path is the point of the harness - apex_layer proves the builder on SYNTHETIC values, this
// proves it on the real quantised tensor, which nothing did before.
static struct ggml_tensor * gb(int64_t mode, struct ggml_context * gc,
                               struct ggml_tensor * a, struct ggml_tensor * b,
                               struct ggml_tensor * d, int64_t op) {
    uint64_t args[16] = { (uint64_t) mode, (uint64_t) (uintptr_t) gc,
                          (uint64_t) (uintptr_t) a, (uint64_t) (uintptr_t) b, (uint64_t) (uintptr_t) d,
                          (uint64_t) op, 0, 0,0,0,0,0,0,0,0,0 };
    return (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
}

// The two paths do not accumulate identically: the quantised mul_mat dequantises per block and folds the
// scales in, while the reference dequantises once then does f32. Measured on q4_K this is 2.8e-03 of the
// result's magnitude, so the gate allows 5e-03 - tight enough that a wrong block size, a misread offset or a
// broken to_float moves it far outside.
#define MAX_REL 5e-3

static void fill_vec(float * v, int64_t n, int seed) {
    for (int64_t i = 0; i < n; ++i) {
        v[i] = (float) ((i % seed) - seed / 2) / 64.0f;
    }
}

// One tensor of every quantised type the model actually contains, so the value path is covered for the
// whole model and not for q4_K alone. Apex holds Q4_K (379 tensors), F32 (310) and Q6_K (64).
static const char * DEFAULT_TENSORS[] = {
    "blk.3.attn_q.weight",      // q4_K - the widest tensor in an attention block
    "output.weight",            // q6_K
    "output_norm.weight",       // f32
};

static int check_one(struct gguf_context * gguf, const char * model_path, const char * name);

int main(int argc, char ** argv) {
    goose_init(0, (char **) 0);
    if (argc < 2) { fprintf(stderr, "usage: %s <apex.gguf> [tensor...]\n", argv[0]); return 2; }

    if (argc > 2) {
        int bad = 0;
        for (int i = 2; i < argc; ++i) bad |= check_one(NULL, argv[1], argv[i]);
        return bad ? 1 : 0;
    }
    int bad = 0;
    for (size_t i = 0; i < sizeof DEFAULT_TENSORS / sizeof *DEFAULT_TENSORS; ++i) {
        bad |= check_one(NULL, argv[1], DEFAULT_TENSORS[i]);
    }
    printf("FG_VALUES_ALL %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}

static int check_one(struct gguf_context * gguf_unused, const char * model_path, const char * tname) {
    (void) gguf_unused;

    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ NULL };
    struct ggml_context * meta = NULL;
    mp.ctx = &meta;
    struct gguf_context * gguf = gguf_init_from_file(model_path, mp);
    if (gguf == NULL) { printf("FG_VALUES cannot read %s\n", model_path); return 2; }

    // blk.3 is an attention layer (stack_dispatch: first_attn=3) and its q projection is the widest tensor
    // in the block, so a misread offset shows up immediately.
    const int ti = gguf_find_tensor(gguf, tname);
    if (ti < 0) { printf("FG_VALUES no %s\n", tname); gguf_free(gguf); return 2; }

    const enum ggml_type type = gguf_get_tensor_type(gguf, ti);
    const int64_t ne0_full = gguf_get_tensor_ne(gguf, ti)[0];
    const int64_t ne1_full = gguf_get_tensor_ne(gguf, ti)[1];
    const int64_t ne0 = ne0_full;
    // Row stride and block layout are per row, so testing the first R rows exercises the same
    // dequantisation as all of them. output.weight is 151936 rows (1.2 GB dequantised), which is why the
    // comparison is bounded rather than laid out in full.
    const int64_t ne1 = ne1_full < 256 ? ne1_full : 256;
    const size_t  off = gguf_get_tensor_offset(gguf, ti);
    const size_t  nb  = ggml_row_size(type, ne0);

    // read the tensor's bytes straight out of the file at its declared offset
    FILE * fp = fopen(model_path, "rb");
    if (fp == NULL) { printf("FG_VALUES cannot reopen the file\n"); gguf_free(gguf); return 2; }
    // the data section starts where the tensor directory ends; gguf gives the offset relative to it
    const size_t data_start = gguf_get_data_offset(gguf);
    uint8_t * raw = (uint8_t *) malloc((size_t) nb * (size_t) ne1);
    if (raw == NULL || nb * (size_t) ne1 == 0) { printf("FG_VALUES alloc failed\n"); fclose(fp); gguf_free(gguf); return 2; }
    if (fseek(fp, (long) (data_start + off), SEEK_SET) != 0 ||
        fread(raw, 1, nb * (size_t) ne1, fp) != nb * (size_t) ne1) {
        printf("FG_VALUES short read\n"); free(raw); fclose(fp); gguf_free(gguf); return 2;
    }
    fclose(fp);

    // one tensor as the model declares it, one dequantised copy, one vector and two results
    struct ggml_init_params gp = { 256u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_VALUES ggml_init failed\n"); free(raw); gguf_free(gguf); return 2; }

    struct ggml_tensor * wq = ggml_new_tensor_2d(gc, type, ne0, ne1);
    struct ggml_tensor * wf = ggml_new_tensor_2d(gc, GGML_TYPE_F32, ne0, ne1);
    struct ggml_tensor * x  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, ne0, 1);
    memcpy(wq->data, raw, (size_t) nb * (size_t) ne1);
    free(raw);

    // dequantise through ggml's own table: the independent path the quantised mul_mat must agree with
    // F32 has no to_float - it is already the reference - so that case is a copy. Every other type in this
    // model (q4_K, q6_K) goes through the table.
    const size_t row_size_q = ggml_row_size(type, ne0);
    if (type == GGML_TYPE_F32) {
        memcpy(wf->data, wq->data, row_size_q * (size_t) ne1);
    } else {
        const struct ggml_type_traits * tr = ggml_get_type_traits(type);
        if (tr == NULL || tr->to_float == NULL) { printf("FG_VALUES no to_float for type %d\n", (int) type); ggml_free(gc); gguf_free(gguf); return 2; }
        for (int64_t r = 0; r < ne1; ++r) {
            tr->to_float((const uint8_t *) wq->data + (size_t) r * row_size_q,
                         (float *) ((uint8_t *) wf->data + (size_t) r * wf->nb[1]), ne0);
        }
    }

    float * xd = (float *) malloc((size_t) ne0 * sizeof(float));
    fill_vec(xd, ne0, 17);
    memcpy(x->data, xd, (size_t) ne0 * sizeof(float));
    free(xd);

    struct ggml_tensor * a = ggml_mul_mat(gc, wq, x);   // quantised path, raw ggml
    struct ggml_tensor * b = ggml_mul_mat(gc, wf, x);   // dequantised path, raw ggml
    struct ggml_tensor * c = gb(11, gc, wq, x, NULL, GGML_OP_MUL_MAT);  // same op via the Goose builder

    struct ggml_cgraph * cg = ggml_new_graph(gc);
    ggml_build_forward_expand(cg, a);
    ggml_build_forward_expand(cg, b);
    if (c != NULL) ggml_build_forward_expand(cg, c);

    // plain ggml_graph_compute, not the backend API: this context owns its tensor memory already
    // (no_alloc = false), which the backend allocator rejects.
    struct ggml_cplan plan = ggml_graph_plan(cg, 4, NULL);
    if (plan.work_size > 0) {
        plan.work_data = (uint8_t *) malloc(plan.work_size);
        if (plan.work_data == NULL) { printf("FG_VALUES work alloc failed\n"); ggml_free(gc); gguf_free(gguf); return 2; }
    }
    if (ggml_graph_compute(cg, &plan) != GGML_STATUS_SUCCESS) { printf("FG_VALUES compute failed\n"); ggml_free(gc); gguf_free(gguf); return 2; }

    const float * ra = (const float *) a->data;
    const float * rb = (const float *) b->data;
    const float * rc = c != NULL ? (const float *) c->data : NULL;
    // Relative error is measured against the magnitude of the whole result vector, not per element: a dot
    // product that lands near zero makes per-element relative error meaningless (the first version of this
    // gate reported max_rel=26 for agreement of 1e-03 absolute, which is a metric bug, not a value bug).
    double max_abs = 0.0, scale = 0.0, sum_sq = 0.0;
    int nonfinite = 0, nonzero = 0;
    for (int64_t i = 0; i < ne1; ++i) {
        if (!isfinite(ra[i])) ++nonfinite;
        if (ra[i] != 0.0f) ++nonzero;
        const double d = fabs((double) ra[i] - (double) rb[i]);
        if (d > max_abs) max_abs = d;
        if (fabs((double) rb[i]) > scale) scale = fabs((double) rb[i]);
        sum_sq += (double) rb[i] * (double) rb[i];
    }
    const double rms = sqrt(sum_sq / (double) ne1);
    const double max_rel = scale > 0.0 ? max_abs / scale : 0.0;
    // third path: the Goose builder must agree with raw ggml on the same tensor
    double gb_abs = 0.0;
    int gb_missing = c == NULL ? 1 : 0;
    if (rc != NULL) {
        for (int64_t i = 0; i < ne1; ++i) {
            const double d = fabs((double) ra[i] - (double) rc[i]);
            if (d > gb_abs) gb_abs = d;
        }
    }

    const int ok = (nonfinite == 0) && (nonzero > 0) && (max_rel <= MAX_REL) && (gb_abs == 0.0);
    printf("FG_VALUES tensor=%s type=%s ne=[%lld %lld] bytes=%zu max_abs=%.6e max_rel=%.3e "
           "rms=%.6e gb_vs_ggml=%.3e%s nonfinite=%d nonzero=%d %s\n",
           tname, ggml_type_name(type), (long long) ne0_full, (long long) ne1_full, nb * (size_t) ne1,
           max_abs, max_rel, rms, gb_abs, gb_missing ? " GB_MISSING" : "",
           nonfinite, nonzero, (ok && gb_missing == 0) ? "PASS" : "FAIL");

    ggml_free(gc);
    gguf_free(gguf);
    return ok ? 0 : 1;
}
