// Graph primitives for the Goose graph builder (P11-01, EXP681).
//
// The implementations are taken from the authoritative fixture of the experiment
// that first ported this module - experiments/EXP-2026-11-08-158-goose-graph-
// builders/source/builders.h - rather than written fresh. That fixture is the
// code the differential gates accepted, so its semantics are the ones the Goose
// module was validated against. Two hand-written versions of these had already
// drifted (`prop` returned a different field, `arg` read the wrong width), which
// is exactly the failure a rewrite invites.
//
// What was removed from the fixture: its own generated entry point, the atomic
// "seen modes" counters and the constructor that read GGML_GOOSE_GRAPH_BUILDERS.
// Those belong to the fixture's reporting, not to a runtime binding.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "ggml.h"

// ---- the fixture's helpers, verbatim in behaviour ---------------------------

static int64_t fg_arg(int64_t p, int64_t i) {
    return (int64_t) ((const uint64_t *) (intptr_t) p)[i];
}

static int64_t fg_ne(int64_t t, int64_t i) {
    return ((const struct ggml_tensor *) (intptr_t) t)->ne[i];
}

static int64_t fg_prop(int64_t t, int64_t k) {
    const struct ggml_tensor * a = (const struct ggml_tensor *) (intptr_t) t;
    switch (k) {
        case 0: return a->type;
        case 1: return ggml_is_quantized(a->type);
        case 2: return a->op;
        case 3: return a->name[0] != 0;
        default: ggml_abort(__FILE__, __LINE__, "Invalid builder field"); return 0;
    }
}

static int32_t fg_get_op_i32(const struct ggml_tensor * t, uint32_t i) {
    int32_t v = 0;
    memcpy(&v, (const char *) t->op_params + i * sizeof v, sizeof v);
    return v;
}

static void fg_set_op_i32(struct ggml_tensor * t, uint32_t i, int32_t v) {
    memcpy((char *) t->op_params + i * sizeof v, &v, sizeof v);
}

int64_t fg158_arg(int64_t p, int64_t i) { return fg_arg(p, i); }
int64_t fg158_ne(int64_t t, int64_t i) { return fg_ne(t, i); }
int64_t fg158_prop(int64_t t, int64_t k) { return fg_prop(t, k); }

int64_t fg158_src(int64_t t, int64_t i) {
    return (int64_t) (intptr_t) ((const struct ggml_tensor *) (intptr_t) t)->src[i];
}

int64_t fg158_param(int64_t t, int64_t i) {
    return fg_get_op_i32((const struct ggml_tensor *) (intptr_t) t, (uint32_t) i);
}

void fg158_set_op(int64_t t, int64_t op) {
    ((struct ggml_tensor *) (intptr_t) t)->op = (enum ggml_op) op;
}

void fg158_set_src(int64_t t, int64_t i, int64_t v) {
    ((struct ggml_tensor *) (intptr_t) t)->src[i] = (struct ggml_tensor *) (intptr_t) v;
}

void fg158_set_param(int64_t t, int64_t i, int64_t v) {
    fg_set_op_i32((struct ggml_tensor *) (intptr_t) t, (uint32_t) i, (int32_t) v);
}

int64_t fg158_new(int64_t c, int64_t ty, int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    int64_t ne[] = { n0, n1, n2, n3 };
    return (int64_t) (intptr_t) ggml_new_tensor((struct ggml_context *) (intptr_t) c, (enum ggml_type) ty, 4, ne);
}

int64_t fg158_dup(int64_t c, int64_t a) {
    return (int64_t) (intptr_t) ggml_dup_tensor((struct ggml_context *) (intptr_t) c, (const struct ggml_tensor *) (intptr_t) a);
}

// View with explicit strides and offset. fg158_view maps to ggml_view_tensor, which
// has neither, and the q/gate split needs both: q and gate are interleaved per head,
// so the head stride is 2*n_embd_head and the gate offset is n_embd_head inside it.
// Translation only - the extents, strides and offsets are decided in Goose.
// L2 normalisation and repeat-to-extents: the two ops the recurrent path needs and
// the graph builder has no mode for. Translation only; the epsilon and the target
// extents are decided in Goose.
int64_t fg158_l2norm(int64_t c, int64_t a, int64_t eps_bits) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    float eps = 0.0f;
    int32_t bits = (int32_t) eps_bits;
    memcpy(&eps, &bits, sizeof eps);
    return (int64_t) (intptr_t) ggml_l2_norm(ctx, src, eps);
}

int64_t fg158_repeat4(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_repeat_4d(ctx, src, n0, n1, n2, n3);
}

// The three nodes the recurrent state update needs. Translation only: when to
// transpose, what to concatenate and where to write are decided in Goose.
// IMRoPE. The Apex layers use ggml_rope_multi with four dimension sections, not the
// plain ggml_rope_ext the simpler rope gate exercises; the sections are a family
// decision and arrive from Goose.
int64_t fg158_rope_multi(int64_t c, int64_t a, int64_t pos, int64_t n_rot,
                         int64_t s0, int64_t s1, int64_t s2, int64_t s3,
                         int64_t mode, int64_t n_ctx_orig, int64_t fb_bits,
                         int64_t fs_bits, int64_t ef_bits, int64_t af_bits,
                         int64_t bf_bits, int64_t bs_bits) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    struct ggml_tensor  * p   = (struct ggml_tensor *) (intptr_t) pos;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    int sections[4] = { (int) s0, (int) s1, (int) s2, (int) s3 };
    float fb, fs, ef, af, bf, bs;
    int32_t b;
    b = (int32_t) fb_bits; memcpy(&fb, &b, sizeof fb);
    b = (int32_t) fs_bits; memcpy(&fs, &b, sizeof fs);
    b = (int32_t) ef_bits; memcpy(&ef, &b, sizeof ef);
    b = (int32_t) af_bits; memcpy(&af, &b, sizeof af);
    b = (int32_t) bf_bits; memcpy(&bf, &b, sizeof bf);
    b = (int32_t) bs_bits; memcpy(&bs, &b, sizeof bs);
    return (int64_t) (intptr_t) ggml_rope_multi(ctx, src, p, NULL, (int) n_rot,
        sections, (int) mode, (int) n_ctx_orig, fb, fs, ef, af, bf, bs);
}

int64_t fg158_transpose(int64_t c, int64_t a) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_transpose(ctx, src);
}

int64_t fg158_concat(int64_t c, int64_t a, int64_t b, int64_t dim) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * x = (struct ggml_tensor *) (intptr_t) a;
    struct ggml_tensor  * y = (struct ggml_tensor *) (intptr_t) b;
    if (ctx == NULL || x == NULL || y == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_concat(ctx, x, y, (int) dim);
}

int64_t fg158_cpy(int64_t c, int64_t a, int64_t b) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * x = (struct ggml_tensor *) (intptr_t) a;
    struct ggml_tensor  * y = (struct ggml_tensor *) (intptr_t) b;
    if (ctx == NULL || x == NULL || y == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_cpy(ctx, x, y);
}

int64_t fg158_view2(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t nb1, int64_t offset) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_view_2d(ctx, src, n0, n1, (size_t) nb1, (size_t) offset);
}

int64_t fg158_reshape3(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_reshape_3d(ctx, src, n0, n1, n2);
}

int64_t fg158_view4(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2, int64_t n3,
                    int64_t nb1, int64_t nb2, int64_t nb3, int64_t offset) {
    struct ggml_context * ctx = (struct ggml_context *) (intptr_t) c;
    struct ggml_tensor  * src = (struct ggml_tensor *) (intptr_t) a;
    if (ctx == NULL || src == NULL) {
        return 0;
    }
    return (int64_t) (intptr_t) ggml_view_4d(ctx, src, n0, n1, n2, n3,
                                             (size_t) nb1, (size_t) nb2, (size_t) nb3,
                                             (size_t) offset);
}

int64_t fg158_view(int64_t c, int64_t a) {
    return (int64_t) (intptr_t) ggml_view_tensor((struct ggml_context *) (intptr_t) c, (struct ggml_tensor *) (intptr_t) a);
}

// Three predicates the fixture used are static helpers inside ggml.c, so they are
// spelled out here from their definitions there (ggml.c:1544, 3270, 3357). The
// semantics are ggml's, not this file's choice.
static bool fg_is_padded_1d(const struct ggml_tensor * t) {
    return t->nb[0] == ggml_type_size(t->type) &&
           t->nb[2] == t->nb[1] * t->ne[1] &&
           t->nb[3] == t->nb[2] * t->ne[2];
}

static bool fg_can_mul_mat(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {
    return t0->ne[0] == t1->ne[0] &&
           t1->ne[2] % t0->ne[2] == 0 &&
           t1->ne[3] % t0->ne[3] == 0;
}

static bool fg_can_out_prod(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {
    return t0->ne[1] == t1->ne[1] &&
           t1->ne[2] % t0->ne[2] == 0 &&
           t1->ne[3] % t0->ne[3] == 0;
}

static bool fg_can_repeat_rows(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {
    return t0->ne[0] == t1->ne[0] && ggml_can_repeat(t0, t1);
}

// The full predicate table, taken from the accepted fixture. Fifteen kinds, and
// every one of them is a ggml query - the module supplies only the choice.
int64_t fg158_pred(int64_t which, int64_t a, int64_t b) {
    const struct ggml_tensor * x = (const struct ggml_tensor *) (intptr_t) a;
    const struct ggml_tensor * y = (const struct ggml_tensor *) (intptr_t) b;
    switch (which) {
        case 0:  return ggml_is_contiguous(x);
        case 1:  return ggml_is_contiguous_rows(x);
        case 2:  return fg_is_padded_1d(x);
        case 3:  return ggml_are_same_shape(x, y);
        case 4:  return ggml_can_repeat(x, y);
        case 5:  return ggml_is_scalar(x);
        case 6:  return fg_can_repeat_rows(x, y);
        case 7:  return ggml_is_contiguous_1(x);
        case 8:  return fg_can_mul_mat(x, y);
        case 9:  return fg_can_out_prod(x, y);
        case 10: return ggml_is_transposed(x);
        case 11: return ggml_is_matrix(x);
        case 12: return ggml_is_empty(x);
        case 13: return ggml_nelements(x);
        case 14: return ggml_is_vector(x);
        default: ggml_abort(__FILE__, __LINE__, "Invalid builder predicate"); return 0;
    }
}

int64_t fg158_i32(int64_t p, int64_t i) {
    return ((const int32_t *) (intptr_t) p)[i];
}

void fg158_name(int64_t t, int64_t a, int64_t b, int64_t style) {
    struct ggml_tensor * r = (struct ggml_tensor *) (intptr_t) t;
    const struct ggml_tensor * x = (const struct ggml_tensor *) (intptr_t) a;
    const struct ggml_tensor * y = (const struct ggml_tensor *) (intptr_t) b;
    if (style == 0) {
        ggml_format_name(r, "%s (copy of %s)", y->name, x->name);
    } else {
        ggml_format_name(r, style == 1 ? "%s (copy)" : "%s (cont)", x->name);
    }
}

float fg158_float(int64_t x) {
    uint32_t bits = (uint32_t) x;
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

void fg158_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "Goose graph builder validation failed:code=%" PRId64, code);
}

void fg200g_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "model graph refused:code=%" PRId64, code);
}

void fg202t_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "tokenizer decision refused:code=%" PRId64, code);
}

void fg203d_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "mmq dispatch refused:code=%" PRId64, code);
}

void q4t_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "tensor map refused:code=%" PRId64, code);
}

// fg457_st_i64 and the libc-level primitives live in integration/m1/host/
// goose_host.c, which is the shared host adapter. They are not repeated here: a
// second definition of the same binding is pure debt, and the linker would reject
// a runtime that linked both.

// EXP913: reuse the accepted EXP156 POD tensor projections; no shape policy.
uint64_t fg156_tget(int64_t p, int64_t k, int64_t i) {
    const struct ggml_tensor * t = (const void *) (intptr_t) p;
    switch (k) {
        case 0: return (uint64_t) t->ne[i];
        case 1: return t->nb[i];
        case 2: return (uintptr_t) t->view_src;
        case 3: return t->view_offs;
        case 4: return (uintptr_t) t->data;
        case 5: return t->type;
        case 6: return (uintptr_t) t->ne;
        default: ggml_abort(__FILE__, __LINE__, "Invalid tensor projection");
    }
}

void fg156_tset(int64_t p, int64_t k, int64_t i, uint64_t v) {
    struct ggml_tensor * t = (void *) (intptr_t) p;
    switch (k) {
        case 0: t->ne[i] = (int64_t) v; return;
        case 1: t->nb[i] = v; return;
        case 2: t->view_src = (void *) (uintptr_t) v; return;
        case 3: t->view_offs = v; return;
        case 4: t->data = (void *) (uintptr_t) v; return;
        case 5: t->type = (enum ggml_type) v; return;
        case 7: t->op = (enum ggml_op) v; return;
        case 8: t->src[i] = (void *) (uintptr_t) v; return;
        case 9: memcpy(t->op_params, &v, sizeof v); return;
        case 10: { int32_t q = (int32_t) v; memcpy((char *) t->op_params + i * sizeof q, &q, sizeof q); return; }
        default: ggml_abort(__FILE__, __LINE__, "Invalid tensor projection write");
    }
}

void fg156_name(int64_t p, int64_t source, int64_t style) {
    const struct ggml_tensor * s = (const void *) (intptr_t) source;
    const char * format;
    switch (style) {
        case 0: format = "%s (view)"; break;
        case 1: format = "%s (reshaped)"; break;
        case 2: format = "%s (permuted)"; break;
        case 3: format = "%s (transposed)"; break;
        default: ggml_abort(__FILE__, __LINE__, "Invalid constructor name style");
    }
    ggml_format_name((void *) (intptr_t) p, format, s->name);
}
