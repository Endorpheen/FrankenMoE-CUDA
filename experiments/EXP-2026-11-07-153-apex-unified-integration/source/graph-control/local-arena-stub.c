// LOCAL CHECK ONLY arena adapter stub.
// The real adapter must be included inside ggml.c because ggml_new_object is
// private. This snapshot has no ggml.c core source, so local checks link this
// stub: creation returns 0, while raw field/address operations are real and
// sufficient for traversal, view, copy and clear fixtures.
#include "graph-bridge.h"
#include "ggml.h"
#include "ggml-impl.h"

#include <string.h>

int64_t fg153_gs_arena_new_object(int64_t ctx, int64_t bytes) {
    (void) ctx; (void) bytes;
    return 0;
}

int64_t fg153_gs_arena_graph_ptr(int64_t ctx, int64_t obj) {
    (void) ctx; (void) obj;
    return 0;
}

int64_t fg153_gs_addr_add(int64_t base, int64_t off) {
    if (!base) return 0;
    return (int64_t) (intptr_t) ((char *) (intptr_t) base + off);
}

void fg153_gs_raw_graph_set(int64_t base, int64_t code, int64_t i, int64_t v) {
    (void) i;
    struct ggml_cgraph * g = (struct ggml_cgraph *) (intptr_t) base;
    if (!g) return;
    switch (code) {
        case 0: g->size = (int) v; return;
        case 1: g->n_nodes = (int) v; return;
        case 2: g->n_leafs = (int) v; return;
        case 3: g->nodes = (struct ggml_tensor **) (intptr_t) v; return;
        case 4: g->leafs = (struct ggml_tensor **) (intptr_t) v; return;
        case 5: g->use_counts = (int32_t *) (intptr_t) v; return;
        case 6: g->order = (enum ggml_cgraph_eval_order) v; return;
        case 9: g->visited_hash_set.size = (size_t) v; return;
        case 10: g->grads = (struct ggml_tensor **) (intptr_t) v; return;
        case 11: g->grad_accs = (struct ggml_tensor **) (intptr_t) v; return;
        case 12: g->uid = (uint64_t) v; return;
        case 19: g->visited_hash_set.used = (ggml_bitset_t *) (intptr_t) v; return;
        case 20: g->visited_hash_set.keys = (struct ggml_tensor **) (intptr_t) v; return;
        default: return;
    }
}

void fg153_gs_raw_zero(int64_t addr, int64_t bytes) {
    if (addr && bytes > 0) {
        memset((void *) (intptr_t) addr, 0, (size_t) bytes);
    }
}

int64_t fg153_gs_sizeof_cgraph(void)  { return (int64_t) sizeof(struct ggml_cgraph); }
int64_t fg153_gs_sizeof_ptr(void)     { return (int64_t) sizeof(void *); }
int64_t fg153_gs_alignof_ptr(void)    { return (int64_t) _Alignof(void *); }
int64_t fg153_gs_sizeof_i32(void)     { return (int64_t) sizeof(int32_t); }
int64_t fg153_gs_alignof_i32(void)    { return (int64_t) _Alignof(int32_t); }
int64_t fg153_gs_alignof_bitset(void) { return (int64_t) _Alignof(ggml_bitset_t); }
int64_t fg153_gs_sizeof_object(void)  { return 0; } // private locally
int64_t fg153_gs_mem_align(void)      { return GGML_MEM_ALIGN; }
