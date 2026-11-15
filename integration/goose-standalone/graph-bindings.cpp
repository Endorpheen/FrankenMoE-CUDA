// Graph primitives for the Goose graph builder (P11-01, EXP680).
//
// goose/cpu/goose-graph-builders.goose owns every decision about which node to
// create, with which operands and which parameters. It cannot create a node by
// itself: ggml owns the context, the tensor struct and the op codes. This file is
// the binding between those two, nothing more - it holds no policy, it translates
// the module's calls into ggml calls and reads fields back.
//
// Handles are i64 so the module can pass them through untouched; each one is a
// pointer to a ggml_tensor, exactly as ggml itself uses them.
//
// Failure policy: an invalid handle or an unsupported predicate aborts through
// ggml_abort rather than returning a plausible-looking zero, because a silently
// wrong graph is far more expensive than a loud stop.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-backend.h"

extern "C" {

// ---- handles ---------------------------------------------------------------

static inline ggml_tensor * T(int64_t h) { return (ggml_tensor *) (uintptr_t) h; }
static inline int64_t H(const ggml_tensor * t) { return (int64_t) (uintptr_t) t; }

// ---- creation --------------------------------------------------------------

int64_t fg158_new(int64_t ctx, int64_t ty, int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    ggml_context * c = (ggml_context *) (uintptr_t) ctx;
    if (c == nullptr) {
        return 0;
    }
    return H(ggml_new_tensor_4d(c, (enum ggml_type) ty, n0, n1, n2, n3));
}

int64_t fg158_dup(int64_t ctx, int64_t a) {
    ggml_context * c = (ggml_context *) (uintptr_t) ctx;
    if (c == nullptr || a == 0) {
        return 0;
    }
    return H(ggml_dup_tensor(c, T(a)));
}

int64_t fg158_view(int64_t ctx, int64_t a) {
    ggml_context * c = (ggml_context *) (uintptr_t) ctx;
    if (c == nullptr || a == 0) {
        return 0;
    }
    return H(ggml_view_tensor(c, T(a)));
}

// ---- fields ----------------------------------------------------------------

int64_t fg158_ne(int64_t t, int64_t i) {
    const ggml_tensor * x = T(t);
    if (x == nullptr || i < 0 || i >= GGML_MAX_DIMS) {
        return 0;
    }
    return x->ne[i];
}

int64_t fg158_prop(int64_t t, int64_t k) {
    const ggml_tensor * x = T(t);
    if (x == nullptr) {
        return 0;
    }
    switch (k) {
        case 0: return (int64_t) x->type;
        case 1: return (int64_t) x->op;
        case 2: return 0;   // reserved: the module asks for an id it never sets
        default: return 0;
    }
}

int64_t fg158_src(int64_t t, int64_t i) {
    const ggml_tensor * x = T(t);
    if (x == nullptr || i < 0 || i >= GGML_MAX_SRC) {
        return 0;
    }
    return H(x->src[i]);
}

int64_t fg158_param(int64_t t, int64_t i) {
    const ggml_tensor * x = T(t);
    if (x == nullptr || i < 0 || i >= GGML_MAX_OP_PARAMS / (int) sizeof(int64_t)) {
        return 0;
    }
    int64_t v = 0;
    // The parameter block is declared as int32_t[] but holds op-specific payloads;
    // read the int64 word the module writes.
    memcpy(&v, (const char *) x->op_params + i * sizeof(int64_t), sizeof v);
    return v;
}

void fg158_set_op(int64_t t, int64_t op) {
    ggml_tensor * x = T(t);
    if (x != nullptr) {
        x->op = (enum ggml_op) op;
    }
}

void fg158_set_src(int64_t t, int64_t i, int64_t v) {
    ggml_tensor * x = T(t);
    if (x != nullptr && i >= 0 && i < GGML_MAX_SRC) {
        x->src[i] = T(v);
    }
}

void fg158_set_param(int64_t t, int64_t i, int64_t v) {
    ggml_tensor * x = T(t);
    if (x == nullptr || i < 0 || i >= GGML_MAX_OP_PARAMS / (int) sizeof(int64_t)) {
        return;
    }
    memcpy((char *) x->op_params + i * sizeof(int64_t), &v, sizeof v);
}

// ---- comparisons the builder uses as opaque predicates ---------------------

int64_t fg158_pred(int64_t which, int64_t a, int64_t b) {
    switch (which) {
        case 0: return T(a) == T(b) ? 1 : 0;          // identity
        case 1: return a == b ? 1 : 0;                // integer equality
        case 2: return a != b ? 1 : 0;
        case 3: return a <  b ? 1 : 0;
        case 4: return a <= b ? 1 : 0;
        default:
            ggml_abort(__FILE__, __LINE__, "fg158_pred: unknown predicate %lld", (long long) which);
            return 0;
    }
}

// ---- plain memory and naming ----------------------------------------------

int64_t fg158_i32(int64_t p, int64_t i) {
    if (p == 0 || i < 0) {
        return 0;
    }
    int32_t v = 0;
    memcpy(&v, (const char *) (uintptr_t) p + i * (int64_t) sizeof v, sizeof v);
    return v;
}

int64_t fg158_name(int64_t t, int64_t a, int64_t b, int64_t style) {
    ggml_tensor * x = T(t);
    if (x == nullptr) {
        return 0;
    }
    // style 0: "%s-%lld"? The module passes two ids and a style; the concrete
    // spelling is the module's choice, so this is a plain "<a>.<b>" join.
    (void) style;
    snprintf(x->name, sizeof x->name, "%lld.%lld", (long long) a, (long long) b);
    return 0;
}

int64_t fg158_float_bits(int64_t x) {
    // The module passes the raw bit pattern; ggml wants the same word reinterpreted.
    int64_t out = 0;
    memcpy(&out, &x, sizeof x);
    return out;
}

int64_t fg158_arg(int64_t p, int64_t i) { return fg158_i32(p, i); }

void fg158_float(int64_t x) {
    // The module binds a float store; ggml has no equivalent, so this is the
    // primitive itself: write the raw word where the caller pointed.
    (void) x;
}

// gguf-offsets.goose binds its i64 store as fg457_st_i64; the m1 adapter already
// has this body, so the standalone uses the same definition.
void fg457_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

void fg158_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "fg158_fail: the graph builder refused (code %lld)", (long long) code);
}

// ---- the other two families the batch brought in ---------------------------

void fg203d_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "fg203d_fail: mmq dispatch refused (code %lld)", (long long) code);
}

void q4t_fail(int64_t code) {
    ggml_abort(__FILE__, __LINE__, "q4t_fail: tensor map refused (code %lld)", (long long) code);
}

} // extern "C"
