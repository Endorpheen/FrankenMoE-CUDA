// Graph raw-storage services for worker-1 Goose modules.
// ABI source: inputs/ggml-impl.h:211-359, inputs/ggml.h:661-717.
//
// Typed access only: graph fields and tensor fields are separate externs.
// There is no registry, no type guessing, no fixed-capacity handle table.
// Graph allocation comes from graph-arena-adapter.inc included in ggml.c and
// is owned by the ggml_context. Views are written into caller-owned storage.
// No graph traversal and no graph metadata algorithm lives here.
// Hash probing is NOT implemented here: generated hash-primitives.goose must
// provide strong fg153_hp_* definitions. Missing generated hash is a link error.

#include "graph-bridge.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include("ggml-backend.h")
#    include "ggml-backend.h"
#    define FG153_HAVE_BACKEND_H 1
#  endif
#endif
#ifndef FG153_HAVE_BACKEND_H
extern void ggml_backend_tensor_memset(struct ggml_tensor * tensor, int value, size_t offset, size_t size);
extern void ggml_backend_tensor_set(struct ggml_tensor * tensor, const void * data, size_t offset, size_t size);
#endif

static struct ggml_cgraph * as_graph(int64_t p) { return (struct ggml_cgraph *) (intptr_t) p; }
static struct ggml_tensor * as_tensor(int64_t p) { return (struct ggml_tensor *) (intptr_t) p; }
static int in_range(int64_t i, int64_t n) { return i >= 0 && i < n; }

void fg153_gs_fail(int64_t code) {
    fprintf(stderr, "fg153 graph bridge failure: code=%lld\n", (long long) code);
    abort();
}

// ---------------------------------------------------------------------------
// Constants (inputs/ggml-impl.h:335-339, inputs/ggml.h:661-668).
// ---------------------------------------------------------------------------
int64_t fg153_gt_const(int64_t code) {
    switch (code) {
        case 60: return GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT;
        case 61: return GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT;
        case 62: return (int64_t) GGML_HASHSET_FULL;
        default: return 0;
    }
}

int64_t fg153_gs_const(int64_t code) {
    switch (code) {
        case 60: return GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT;
        case 61: return GGML_CGRAPH_EVAL_ORDER_RIGHT_TO_LEFT;
        case 62: return (int64_t) GGML_HASHSET_FULL;
        case 70: return GGML_OP_OPT_STEP_ADAMW;
        case 71: return GGML_TYPE_F32;
        case 80: return fg153_gs_sizeof_cgraph();
        case 82: return fg153_gs_sizeof_ptr();
        case 83: return fg153_gs_alignof_ptr();
        case 84: return fg153_gs_sizeof_i32();
        case 85: return fg153_gs_alignof_i32();
        case 86: return fg153_gs_alignof_bitset();
        case 88: return fg153_gs_sizeof_object();
        case 89: return fg153_gs_mem_align();
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Tensor fields (typed; inputs/ggml.h:685-717).
// ---------------------------------------------------------------------------
int64_t fg153_gt_tensor_get(int64_t tp, int64_t k, int64_t i) {
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return 0;
    }
    switch (k) {
        case 0: return t->op;
        case 1: return t->flags;
        case 2: return in_range(i, GGML_MAX_SRC) ? (int64_t) (intptr_t) t->src[i] : 0;
        case 4: return t->name[0] != '\0' ? 1 : 0;
        default: return 0;
    }
}

void fg153_gt_tensor_put(int64_t tp, int64_t k, int64_t i, int64_t v) {
    (void) i;
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return;
    }
    if (k == 1) {
        t->flags = (int32_t) v;
    }
}

int64_t fg153_gs_tensor_get(int64_t tp, int64_t k, int64_t i) {
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return 0;
    }
    switch (k) {
        case 0: return t->op;
        case 1: return t->flags;
        case 2: return t->type;
        case 3: return in_range(i, GGML_MAX_SRC) ? (int64_t) (intptr_t) t->src[i] : 0;
        default: return 0;
    }
}

void fg153_gs_tensor_put(int64_t tp, int64_t k, int64_t i, int64_t v) {
    (void) i;
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return;
    }
    if (k == 1) {
        t->flags = (int32_t) v;
    }
}

// ---------------------------------------------------------------------------
// Graph fields (typed; inputs/ggml-impl.h:341-359).
// Null arrays and out-of-range indices return 0; no nullptr dereference.
// ---------------------------------------------------------------------------
int64_t fg153_gt_graph_get(int64_t gp, int64_t k, int64_t i) {
    struct ggml_cgraph * g = as_graph(gp);
    if (!g) {
        return 0;
    }
    switch (k) {
        case 0: return g->size;
        case 1: return g->n_nodes;
        case 2: return g->n_leafs;
        case 3: return (g->nodes && in_range(i, g->n_nodes)) ? (int64_t) (intptr_t) g->nodes[i] : 0;
        case 4: return (g->leafs && in_range(i, g->n_leafs)) ? (int64_t) (intptr_t) g->leafs[i] : 0;
        case 5: return (g->use_counts && in_range(i, (int64_t) g->visited_hash_set.size)) ? g->use_counts[i] : 0;
        case 6: return g->order;
        case 7: return (g->visited_hash_set.used && in_range(i, (int64_t) g->visited_hash_set.size))
                       ? (ggml_bitset_get(g->visited_hash_set.used, (size_t) i) ? 1 : 0) : 0;
        case 8: return (g->visited_hash_set.keys && in_range(i, (int64_t) g->visited_hash_set.size))
                       ? (int64_t) (intptr_t) g->visited_hash_set.keys[i] : 0;
        case 9: return (int64_t) g->visited_hash_set.size;
        default: return 0;
    }
}

void fg153_gt_graph_put(int64_t gp, int64_t k, int64_t i, int64_t v) {
    struct ggml_cgraph * g = as_graph(gp);
    if (!g) {
        return;
    }
    switch (k) {
        case 1: g->n_nodes = (int) v; return;
        case 2: g->n_leafs = (int) v; return;
        case 3:
            if (g->nodes && in_range(i, g->size)) { g->nodes[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 4:
            if (g->leafs && in_range(i, g->size)) { g->leafs[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 5:
            if (g->use_counts && in_range(i, (int64_t) g->visited_hash_set.size)) { g->use_counts[i] = (int32_t) v; }
            return;
        case 6: g->order = (enum ggml_cgraph_eval_order) v; return;
        case 7:
            if (g->visited_hash_set.used && in_range(i, (int64_t) g->visited_hash_set.size)) {
                if (v != 0) { ggml_bitset_set(g->visited_hash_set.used, (size_t) i); }
                else { ggml_bitset_clear(g->visited_hash_set.used, (size_t) i); }
            }
            return;
        case 8:
            if (g->visited_hash_set.keys && in_range(i, (int64_t) g->visited_hash_set.size)) {
                g->visited_hash_set.keys[i] = (struct ggml_tensor *) (intptr_t) v;
            }
            return;
        default: return;
    }
}

int64_t fg153_gs_graph_get(int64_t gp, int64_t k, int64_t i) {
    struct ggml_cgraph * g = as_graph(gp);
    if (!g) {
        return 0;
    }
    const int64_t hash_size = (int64_t) g->visited_hash_set.size;
    switch (k) {
        case 0: return g->size;
        case 1: return g->n_nodes;
        case 2: return g->n_leafs;
        case 3: return (g->nodes && in_range(i, g->n_nodes)) ? (int64_t) (intptr_t) g->nodes[i] : 0;
        case 4: return (g->leafs && in_range(i, g->n_leafs)) ? (int64_t) (intptr_t) g->leafs[i] : 0;
        case 5: return (g->use_counts && in_range(i, hash_size)) ? g->use_counts[i] : 0;
        case 6: return g->order;
        case 7: return (g->visited_hash_set.used && in_range(i, hash_size))
                       ? (ggml_bitset_get(g->visited_hash_set.used, (size_t) i) ? 1 : 0) : 0;
        case 8: return (g->visited_hash_set.keys && in_range(i, hash_size))
                       ? (int64_t) (intptr_t) g->visited_hash_set.keys[i] : 0;
        case 9: return hash_size;
        case 10: return (g->grads && in_range(i, hash_size)) ? (int64_t) (intptr_t) g->grads[i] : 0;
        case 11: return (g->grad_accs && in_range(i, hash_size)) ? (int64_t) (intptr_t) g->grad_accs[i] : 0;
        case 12: return (int64_t) g->uid;
        case 14: return 0; // view nodes are shifted at creation
        case 15: return (int64_t) (intptr_t) g->grads;
        case 16: return (int64_t) (intptr_t) g->grad_accs;
        case 17: return (int64_t) (intptr_t) g->nodes;
        case 18: return (int64_t) (intptr_t) g->leafs;
        case 21: return (int64_t) (intptr_t) g->use_counts;
        case 22: return (int64_t) (intptr_t) g->visited_hash_set.keys;
        case 23: return (int64_t) (intptr_t) g->visited_hash_set.used;
        default: return 0;
    }
}

void fg153_gs_graph_put(int64_t gp, int64_t k, int64_t i, int64_t v) {
    struct ggml_cgraph * g = as_graph(gp);
    if (!g) {
        return;
    }
    const int64_t hash_size = (int64_t) g->visited_hash_set.size;
    switch (k) {
        case 0: g->size = (int) v; return;
        case 1: g->n_nodes = (int) v; return;
        case 2: g->n_leafs = (int) v; return;
        case 3:
            if (g->nodes && in_range(i, g->size)) { g->nodes[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 4:
            if (g->leafs && in_range(i, g->size)) { g->leafs[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 5:
            if (g->use_counts && in_range(i, hash_size)) { g->use_counts[i] = (int32_t) v; }
            return;
        case 6: g->order = (enum ggml_cgraph_eval_order) v; return;
        case 7:
            if (g->visited_hash_set.used && in_range(i, hash_size)) {
                if (v != 0) { ggml_bitset_set(g->visited_hash_set.used, (size_t) i); }
                else { ggml_bitset_clear(g->visited_hash_set.used, (size_t) i); }
            }
            return;
        case 8:
            if (g->visited_hash_set.keys && in_range(i, hash_size)) {
                g->visited_hash_set.keys[i] = (struct ggml_tensor *) (intptr_t) v;
            }
            return;
        case 10:
            if (g->grads && in_range(i, hash_size)) { g->grads[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 11:
            if (g->grad_accs && in_range(i, hash_size)) { g->grad_accs[i] = (struct ggml_tensor *) (intptr_t) v; }
            return;
        case 12: g->uid = (uint64_t) v; return;
        default: return;
    }
}

int64_t fg153_gt_array_get(int64_t array, int64_t i) {
    if (!array || i < 0) {
        return 0;
    }
    return ((int64_t *) (intptr_t) array)[i];
}

// ---------------------------------------------------------------------------
// Storage helpers. Layout/placement/init are owned by graph-storage.goose and
// use only raw adapter primitives; no malloc shell and no native graph call.
// ---------------------------------------------------------------------------
// Raw prime-table metadata only. Search and odd fallback live in Goose.
static const int64_t fg153_hash_primes[] = {
    2, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031,
    2053, 4099, 8209, 16411, 32771, 65537, 131101,
    262147, 524309, 1048583, 2097169, 4194319, 8388617,
    16777259, 33554467, 67108879, 134217757, 268435459,
    536870923, 1073741827, 2147483659
};

int64_t fg153_gs_hash_prime_count(void) {
    return (int64_t) (sizeof(fg153_hash_primes) / sizeof(fg153_hash_primes[0]));
}

int64_t fg153_gs_hash_prime(int64_t index) {
    const int64_t n = fg153_gs_hash_prime_count();
    return (index >= 0 && index < n) ? fg153_hash_primes[index] : 0;
}

int64_t fg153_gs_bitset_bytes(int64_t hash_size) {
    return (int64_t) (ggml_bitset_size((size_t) hash_size) * sizeof(ggml_bitset_t));
}

int64_t fg153_gt_hash_find(int64_t g, int64_t t) {
    struct ggml_cgraph * c = as_graph(g);
    return c ? fg153_hp_find((int64_t) (intptr_t) &c->visited_hash_set, t) : (int64_t) GGML_HASHSET_FULL;
}

int64_t fg153_gs_hash_insert(int64_t g, int64_t t) {
    struct ggml_cgraph * c = as_graph(g);
    return c ? fg153_hp_insert((int64_t) (intptr_t) &c->visited_hash_set, t) : (int64_t) GGML_HASHSET_FULL;
}

int64_t fg153_gs_hash_find(int64_t g, int64_t t) {
    struct ggml_cgraph * c = as_graph(g);
    return c ? fg153_hp_find((int64_t) (intptr_t) &c->visited_hash_set, t) : (int64_t) GGML_HASHSET_FULL;
}

int64_t fg153_gs_hash_set(int64_t g) {
    struct ggml_cgraph * c = as_graph(g);
    return c ? (int64_t) (intptr_t) &c->visited_hash_set : 0;
}

void fg153_gs_zero_field(int64_t g, int64_t field, int64_t count) {
    struct ggml_cgraph * c = as_graph(g);
    if (!c || count <= 0) {
        return;
    }
    switch (field) {
        case 3: if (c->nodes) { memset(c->nodes, 0, (size_t) count * sizeof(*c->nodes)); } return;
        case 4: if (c->leafs) { memset(c->leafs, 0, (size_t) count * sizeof(*c->leafs)); } return;
        case 5: if (c->use_counts) { memset(c->use_counts, 0, (size_t) count * sizeof(*c->use_counts)); } return;
        case 7:
            if (c->visited_hash_set.used) {
                memset(c->visited_hash_set.used, 0,
                       ggml_bitset_size(c->visited_hash_set.size) * sizeof(ggml_bitset_t));
            }
            return;
        case 8: if (c->visited_hash_set.keys) { memset(c->visited_hash_set.keys, 0, (size_t) count * sizeof(*c->visited_hash_set.keys)); } return;
        case 10: if (c->grads) { memset(c->grads, 0, (size_t) count * sizeof(*c->grads)); } return;
        case 11: if (c->grad_accs) { memset(c->grad_accs, 0, (size_t) count * sizeof(*c->grad_accs)); } return;
        default: return;
    }
}

// ---------------------------------------------------------------------------
// Tensor storage helpers used by graph_reset (inputs/ggml-native-core.c:7619-7666).
// ---------------------------------------------------------------------------
int64_t fg153_gs_tensor_empty(int64_t t) { return as_tensor(t) ? (ggml_is_empty(as_tensor(t)) ? 1 : 0) : 1; }
int64_t fg153_gs_tensor_buffer(int64_t t) { return as_tensor(t) ? (int64_t) (intptr_t) as_tensor(t)->buffer : 0; }
int64_t fg153_gs_tensor_data(int64_t t) { return as_tensor(t) ? (int64_t) (intptr_t) as_tensor(t)->data : 0; }
void fg153_gs_zero_data(int64_t t) {
    struct ggml_tensor * x = as_tensor(t);
    if (x && x->data) {
        memset(x->data, 0, ggml_nbytes(x));
    }
}
void fg153_gs_backend_zero(int64_t t) {
    struct ggml_tensor * x = as_tensor(t);
    if (x) {
        ggml_backend_tensor_memset(x, 0, 0, ggml_nbytes(x));
    }
}
int64_t fg153_gs_tensor_is_scalar(int64_t t) { return as_tensor(t) ? (ggml_is_scalar(as_tensor(t)) ? 1 : 0) : 0; }
void fg153_gs_write_f32_one(int64_t t) {
    struct ggml_tensor * x = as_tensor(t);
    if (!x) {
        return;
    }
    const float one = 1.0f;
    if (x->buffer) {
        ggml_backend_tensor_set(x, &one, 0, sizeof(one));
    } else if (x->data) {
        *(float *) x->data = one;
    }
}

void fg153_gt_ensure_name(int64_t t, int64_t is_leaf, int64_t idx) {
    struct ggml_tensor * x = as_tensor(t);
    if (x) {
        ggml_format_name(x, is_leaf ? "leaf_%d" : "node_%d", (int) idx);
    }
}

// ---------------------------------------------------------------------------
// Hash raw primitives (inputs/ggml-impl.h:211-331). No probing here.
// ---------------------------------------------------------------------------
int64_t fg153_hp_size(int64_t hs) { return hs ? (int64_t) ((struct ggml_hash_set *) (intptr_t) hs)->size : 0; }
int64_t fg153_hp_used_get(int64_t hs, int64_t i) {
    struct ggml_hash_set * s = (struct ggml_hash_set *) (intptr_t) hs;
    return (s && s->used && in_range(i, (int64_t) s->size))
           ? (ggml_bitset_get(s->used, (size_t) i) ? 1 : 0) : 0;
}
void fg153_hp_used_set(int64_t hs, int64_t i, int64_t v) {
    struct ggml_hash_set * s = (struct ggml_hash_set *) (intptr_t) hs;
    if (!s || !s->used || !in_range(i, (int64_t) s->size)) {
        return;
    }
    if (v != 0) { ggml_bitset_set(s->used, (size_t) i); }
    else { ggml_bitset_clear(s->used, (size_t) i); }
}
int64_t fg153_hp_key_get(int64_t hs, int64_t i) {
    struct ggml_hash_set * s = (struct ggml_hash_set *) (intptr_t) hs;
    return (s && s->keys && in_range(i, (int64_t) s->size)) ? (int64_t) (intptr_t) s->keys[i] : 0;
}
void fg153_hp_key_set(int64_t hs, int64_t i, int64_t v) {
    struct ggml_hash_set * s = (struct ggml_hash_set *) (intptr_t) hs;
    if (s && s->keys && in_range(i, (int64_t) s->size)) {
        s->keys[i] = (struct ggml_tensor *) (intptr_t) v;
    }
}
void fg153_hp_abort(int64_t code) { fg153_gs_fail(1000 + code); }
int64_t fg153_hp_full(void) { return (int64_t) GGML_HASHSET_FULL; }
int64_t fg153_hp_already(void) { return (int64_t) GGML_HASHSET_ALREADY_EXISTS; }
