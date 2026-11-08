// Replay wrappers: ggml.c function bodies -> exact Goose entry ABI.
// Each function maps one ggml graph function to gt_entry/gs_entry modes.
// No numeric emitter ID is used here; generated C is bound by wrappers.
#include "goose-replay-wrappers.h"
#include "graph-bridge.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int fg153_goose_enabled(void) {
    const char * v = getenv("GGML_GOOSE_GRAPH");
    return !v || strcmp(v, "0") != 0;
}

extern int64_t gt_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern int64_t gs_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

size_t fg153_goose_gs_graph_nbytes(size_t size, bool grads) {
    if ((uint64_t) size > (uint64_t) INT64_MAX) { return 0; }
    return (size_t) gs_entry(20, (int64_t) size, grads ? 1 : 0, 0, 0, 0, 0);
}

size_t fg153_goose_gs_graph_overhead_custom(size_t size, bool grads) {
    if ((uint64_t) size > (uint64_t) INT64_MAX) { return 0; }
    return (size_t) gs_entry(21, (int64_t) size, grads ? 1 : 0, 0, 0, 0, 0);
}

size_t fg153_goose_gs_graph_overhead(void) {
    return (size_t) gs_entry(22, 0, 0, 0, 0, 0, 0);
}

struct ggml_cgraph * fg153_goose_gs_new_graph_custom(struct ggml_context * ctx, size_t size, bool grads) {
    return (struct ggml_cgraph *) (intptr_t) gs_entry(0, (int64_t) (intptr_t) ctx,
                                                      (int64_t) size, grads ? 1 : 0, 0, 0, 0);
}

struct ggml_cgraph * fg153_goose_gs_new_graph(struct ggml_context * ctx) {
    return (struct ggml_cgraph *) (intptr_t) gs_entry(23, (int64_t) (intptr_t) ctx, 0, 0, 0, 0, 0);
}

size_t fg153_goose_gt_visit_parents(struct ggml_cgraph * cgraph, struct ggml_tensor * node, bool compute) {
    return (size_t) gt_entry(3, (int64_t) (intptr_t) cgraph, (int64_t) (intptr_t) node,
                             compute ? 1 : 0, 0, 0, 0);
}

void fg153_goose_gt_build_forward_impl(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor, bool expand, bool compute) {
    // expand=false is unreachable in the native API (see native source note).
    assert(expand && "expand=false is unreachable");
    (void) gt_entry(compute ? 0 : 1, (int64_t) (intptr_t) cgraph,
                    (int64_t) (intptr_t) tensor, 0, 0, 0, 0);
}

struct ggml_tensor * fg153_goose_gt_build_forward_select(struct ggml_cgraph * cgraph,
                                                          struct ggml_tensor ** tensors,
                                                          int n_tensors, int idx) {
    int64_t r = gt_entry(2, (int64_t) (intptr_t) cgraph, (int64_t) (intptr_t) tensors, idx, n_tensors, 0, 0);
    (void) r;
    return tensors[idx];
}

void fg153_goose_gt_build_forward_expand(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor) {
    (void) gt_entry(0, (int64_t) (intptr_t) cgraph, (int64_t) (intptr_t) tensor, 0, 0, 0, 0);
}

void fg153_goose_gt_build_forward_order(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor) {
    (void) gt_entry(1, (int64_t) (intptr_t) cgraph, (int64_t) (intptr_t) tensor, 0, 0, 0, 0);
}

struct ggml_cgraph fg153_goose_gs_graph_view(struct ggml_cgraph * cgraph0, int i0, int i1) {
    struct ggml_cgraph out;
    memset(&out, 0, sizeof(out));
    (void) gs_entry(4, (int64_t) (intptr_t) &out, (int64_t) (intptr_t) cgraph0, i0, i1, 0, 0);
    return out;
}

void fg153_goose_gs_graph_cpy(struct ggml_cgraph * src, struct ggml_cgraph * dst) {
    (void) gs_entry(5, (int64_t) (intptr_t) src, (int64_t) (intptr_t) dst, 0, 0, 0, 0);
}

struct ggml_cgraph * fg153_goose_gs_graph_dup(struct ggml_context * ctx, struct ggml_cgraph * cgraph, bool force_grads) {
    return (struct ggml_cgraph *) (intptr_t) gs_entry(6, (int64_t) (intptr_t) ctx,
                                                      (int64_t) (intptr_t) cgraph,
                                                      force_grads ? 1 : 0, 0, 0, 0);
}

void fg153_goose_gs_graph_reset(struct ggml_cgraph * cgraph) {
    (void) gs_entry(7, (int64_t) (intptr_t) cgraph, 0, 0, 0, 0, 0);
}

void fg153_goose_gs_graph_clear(struct ggml_cgraph * cgraph) {
    (void) gs_entry(15, (int64_t) (intptr_t) cgraph, 0, 0, 0, 0, 0);
}
