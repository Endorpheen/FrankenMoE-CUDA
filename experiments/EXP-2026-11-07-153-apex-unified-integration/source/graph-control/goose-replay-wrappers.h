#pragma once
// Replay wrapper declarations for ggml.c body replacement.
#include "ggml.h"
#include "ggml-impl.h"

#ifdef __cplusplus
extern "C" {
#endif

int fg153_goose_enabled(void); // default-on; GGML_GOOSE_GRAPH=0 disables
size_t fg153_goose_gs_graph_nbytes(size_t size, bool grads);
size_t fg153_goose_gs_graph_overhead_custom(size_t size, bool grads);
size_t fg153_goose_gs_graph_overhead(void);
struct ggml_cgraph * fg153_goose_gs_new_graph_custom(struct ggml_context * ctx, size_t size, bool grads);
struct ggml_cgraph * fg153_goose_gs_new_graph(struct ggml_context * ctx);

size_t fg153_goose_gt_visit_parents(struct ggml_cgraph * cgraph, struct ggml_tensor * node, bool compute);
void   fg153_goose_gt_build_forward_impl(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor, bool expand, bool compute);
struct ggml_tensor * fg153_goose_gt_build_forward_select(struct ggml_cgraph * cgraph, struct ggml_tensor ** tensors, int n_tensors, int idx);
void   fg153_goose_gt_build_forward_expand(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor);
void   fg153_goose_gt_build_forward_order(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor);
struct ggml_cgraph fg153_goose_gs_graph_view(struct ggml_cgraph * cgraph0, int i0, int i1);
void   fg153_goose_gs_graph_cpy(struct ggml_cgraph * src, struct ggml_cgraph * dst);
struct ggml_cgraph * fg153_goose_gs_graph_dup(struct ggml_context * ctx, struct ggml_cgraph * cgraph, bool force_grads);
void   fg153_goose_gs_graph_reset(struct ggml_cgraph * cgraph);
void   fg153_goose_gs_graph_clear(struct ggml_cgraph * cgraph);

#ifdef __cplusplus
}
#endif
