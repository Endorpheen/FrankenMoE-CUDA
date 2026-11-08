/* worker-14 / APX025: typed imports from the ready Goose CUDA numerical
 * modules.  These replace the former "not wired" aborts.  The parent links the
 * real modules (cuda-elementwise-ready, cuda-movement-ready,
 * cuda-matrix-final-ready, cuda-fa-ready, cuda-moe-ready); this header is the
 * exact call contract required by the control layer.
 *
 * Each function performs exactly one fused launch and returns nodes-to-skip,
 * or 0 on refusal.  No original C++ numeric kernel is called as a completion.
 */
#ifndef FG14_CUDA_NUMERIC_READY_H
#define FG14_CUDA_NUMERIC_READY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t fgn_cuda_shared_expert(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                               int64_t routed_idx, int64_t shared_idx);
int64_t fgn_cuda_weighted_reduction(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                                    int64_t node_count, int64_t out_idx);
int64_t fgn_cuda_gdn_cache(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                           int64_t cpy_idx);
int64_t fgn_cuda_ssm_cache(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                           int64_t cpy_idx);
int64_t fgn_cuda_topk_moe(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                          int64_t route, int64_t n_ops, int64_t out0, int64_t out1);
int64_t fgn_cuda_mm_glu(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                        int64_t route, int64_t n_ops, int64_t vec_kind);
int64_t fgn_cuda_mm_scale_bias(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                               int64_t route, int64_t n_ops, int64_t vec_kind);
int64_t fgn_cuda_mm_bias(int64_t native_ctx, int64_t cgraph, int64_t node_idx,
                         int64_t route, int64_t vec_kind);

#ifdef __cplusplus
}
#endif
#endif
