/* worker-14 / APX025: strict primitive ABI for the CUDA control modules.
 * Only raw CUDA/cuBLAS/vendor calls and POD metadata projections live behind
 * these symbols.  Decisions, loops, ownership and lifetimes are Goose.
 */
#ifndef FG14_ABI_H
#define FG14_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Strong generated entries. */
int64_t fg14_goose_resource(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg14_goose_launch(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg14_goose_fusion(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

/* --- CUDA/vendor primitives ------------------------------------------------ */
int64_t fg14_s_device_set(int64_t dev);
int64_t fg14_s_device_get(void);
int64_t fg14_s_check(int64_t err, int64_t site);
int64_t fg14_s_last_error(void);
int64_t fg14_s_clear_error(void);
int64_t fg14_s_malloc(int64_t size);
int64_t fg14_s_free(int64_t ptr);
int64_t fg14_s_stream_create(void);
int64_t fg14_s_stream_destroy(int64_t stream);
int64_t fg14_s_stream_sync(int64_t stream);
int64_t fg14_s_cublas_create(int64_t ctx, int64_t dev, int64_t slot, int64_t stream);
int64_t fg14_s_cublas_destroy(int64_t handle);
int64_t fg14_s_cublas_set_math(int64_t handle, int64_t mode);
int64_t fg14_s_cublas_set_stream(int64_t handle, int64_t stream);
int64_t fg14_s_cublas_set_workspace(int64_t handle, int64_t ptr, int64_t size);
int64_t fg14_s_event_create(void);
int64_t fg14_s_event_destroy(int64_t ev);
int64_t fg14_s_event_record(int64_t ev, int64_t stream);
int64_t fg14_s_stream_wait_event(int64_t stream, int64_t ev);
int64_t fg14_s_stream_per_thread(void);
int64_t fg14_s_device_sync(void);
int64_t fg14_s_info_cc(int64_t dev);
int64_t fg14_s_info_warp(int64_t dev);
int64_t fg14_s_info_integrated(int64_t dev);
int64_t fg14_s_info_vmm(int64_t dev);
int64_t fg14_s_pool_create(int64_t native_ctx, int64_t dev, int64_t slot, int64_t vmm);
int64_t fg14_s_now_us(void);

/* native context bridge (control path only; numeric kernels keep their API) */
int64_t fg14_s_ctx_device(int64_t native_ctx);
int64_t fg14_s_ctx_curr_stream(int64_t native_ctx);
int64_t fg14_s_ctx_curr_stream_set(int64_t native_ctx, int64_t value);
int64_t fg14_s_ctx_stream(int64_t native_ctx, int64_t dev, int64_t slot);
int64_t fg14_s_ctx_stream_set(int64_t native_ctx, int64_t dev, int64_t slot, int64_t stream);
int64_t fg14_s_ctx_cublas(int64_t native_ctx, int64_t dev, int64_t slot);
int64_t fg14_s_ctx_cublas_set(int64_t native_ctx, int64_t dev, int64_t slot, int64_t handle);
int64_t fg14_s_ctx_ws_size(int64_t native_ctx, int64_t dev);
int64_t fg14_s_ctx_ws_size_set(int64_t native_ctx, int64_t dev, int64_t size);
int64_t fg14_s_ctx_copy_event(int64_t native_ctx);
int64_t fg14_s_ctx_copy_event_set(int64_t native_ctx, int64_t ev);
int64_t fg14_s_ctx_rec(int64_t native_ctx);
int64_t fg14_s_ctx_rec_set(int64_t native_ctx, int64_t rec);
int64_t fg14_s_ctx_pool(int64_t native_ctx, int64_t dev, int64_t slot);
int64_t fg14_s_ctx_pool_set(int64_t native_ctx, int64_t dev, int64_t slot, int64_t pool);
int64_t fg14_s_pool_alloc(int64_t pool, int64_t size, int64_t out_actual);
int64_t fg14_s_pool_free(int64_t pool, int64_t ptr, int64_t size);
int64_t fg14_s_pool_destroy(int64_t pool);

/* --- canonical graph / tensor metadata projections ------------------------ */
int64_t fg14_s_g_n(int64_t g);
int64_t fg14_s_g_node(int64_t g, int64_t i);
int64_t fg14_s_g_node_set(int64_t g, int64_t i, int64_t t);
int64_t fg14_s_g_use_count(int64_t g, int64_t i);
int64_t fg14_s_g_name_has(int64_t g, int64_t i, int64_t needle);
int64_t fg14_s_g_np_capture(int64_t np_vec, int64_t g, int64_t i);
int64_t fg14_s_g_np_same(int64_t np_vec, int64_t g, int64_t i);
int64_t fg14_s_g_np_len(int64_t np_vec);
int64_t fg14_s_np_get(int64_t np_vec, int64_t i, int64_t field);
int64_t fg14_s_g_set_node_checked(int64_t g, int64_t i, int64_t t);
int64_t fg14_s_g_np_same(int64_t np_vec, int64_t g, int64_t i);
int64_t fg14_s_g_find_node(int64_t g, int64_t t);
int64_t fg14_s_g_uid(int64_t g);
int64_t fg14_s_g_set_uid(int64_t g, int64_t uid);
int64_t fg14_s_mul_mat_id_needs_sync(int64_t t, int64_t cc);
int64_t fg14_s_graph_env_disabled(void);
int64_t fg14_s_have_cuda_graph(void);
int64_t fg14_s_graph_node_cc(int64_t dev);
int64_t fg14_s_g_can_fuse(int64_t g, int64_t i, int64_t ops, int64_t n_ops);
int64_t fg14_s_g_can_fuse_sub(int64_t g, int64_t i, int64_t n_ops, int64_t ops,
                              int64_t outs, int64_t n_outs);
int64_t fg14_s_g_can_fuse_sub1(int64_t g, int64_t i, int64_t ops, int64_t n_ops);
int64_t fg14_s_g_can_fuse_sub2(int64_t g, int64_t i, int64_t ops, int64_t n_ops,
                               int64_t out0, int64_t out1);
int64_t fg14_s_g_edge(int64_t g, int64_t i, int64_t dst, int64_t src_idx, int64_t src_node);
int64_t fg14_s_t_op_f32_bits(int64_t t, int64_t i);
int64_t fg14_s_t_nelements(int64_t t);
int64_t fg14_s_t_nrows(int64_t t);
int64_t fg14_s_t_buffer_usage(int64_t t);
int64_t fg14_s_t_alloc_size(int64_t t);
int64_t fg14_s_t_is_quant_type(int64_t ty);
int64_t fg14_s_t_has_buffer(int64_t t);
int64_t fg14_s_g_check_edges(int64_t g, int64_t i, int64_t edges, int64_t n_edges);
int64_t fg14_s_t_get(int64_t t, int64_t field, int64_t i);
int64_t fg14_s_t_nbytes(int64_t t);
int64_t fg14_s_t_view_src(int64_t t);
int64_t fg14_s_t_src(int64_t t, int64_t i);
int64_t fg14_s_t_data(int64_t t);
int64_t fg14_s_t_set_field(int64_t t, int64_t field, int64_t i, int64_t v);
int64_t fg14_s_t_same_shape(int64_t a, int64_t b);
int64_t fg14_s_t_same_stride(int64_t a, int64_t b);
int64_t fg14_s_t_same_layout(int64_t a, int64_t b);
int64_t fg14_s_t_is_matrix(int64_t t);
int64_t fg14_s_contig(int64_t t, int64_t kind);
int64_t fg14_s_t_use_field(int64_t t, int64_t field);
int64_t fg14_s_ggml_row_size(int64_t ty, int64_t n);
int64_t fg14_s_t_name_has(int64_t t, int64_t needle);
int64_t fg14_s_assert(int64_t cond, int64_t site);
int64_t fg14_s_g_can_fuse_shared(int64_t g, int64_t routed, int64_t shared);
int64_t fg14_s_should_use_mmvf_t(int64_t t, int64_t batch);
int64_t fg14_s_should_use_mmf_t(int64_t t, int64_t batch, int64_t mul_mat_id);
int64_t fg14_s_should_use_mmvq_t(int64_t t);
int64_t fg14_s_should_use_mmq_t(int64_t t, int64_t n_experts);
int64_t fg14_s_mmvq_max_batch(void);

/* --- numeric-kernel selection predicates (vendor layer) ------------------- */
int64_t fg14_s_should_use_mmvf(int64_t type, int64_t ne_ptr, int64_t nb_ptr, int64_t batch);
int64_t fg14_s_should_use_mmf(int64_t type, int64_t cc, int64_t warp, int64_t ne_ptr,
                              int64_t nb_ptr, int64_t batch, int64_t mul_mat_id);
int64_t fg14_s_should_use_mmvq(int64_t type, int64_t cc, int64_t batch);
int64_t fg14_s_should_use_mmq(int64_t type, int64_t cc, int64_t batch, int64_t n_experts);
int64_t fg14_s_should_use_topk_moe(int64_t node, int64_t logits, int64_t weights, int64_t ids);
int64_t fg14_s_mmvq_mmid_max_batch(int64_t type, int64_t cc);
int64_t fg14_s_fusion_disabled(void);
int64_t fg14_s_log(int64_t site, int64_t a, int64_t b);
int64_t fg14_s_map_get(int64_t map, int64_t key); /* -1 when absent */
int64_t fg14_s_record_new(int64_t kind);
int64_t fg14_s_record_get(int64_t h, int64_t key);
int64_t fg14_s_record_set(int64_t h, int64_t key, int64_t value);
int64_t fg14_s_record_free(int64_t h);
int64_t fg14_s_vec_new(int64_t item_bytes);
int64_t fg14_s_vec_len(int64_t h);
int64_t fg14_s_vec_data(int64_t h);
int64_t fg14_s_vec_resize(int64_t h, int64_t count);
int64_t fg14_s_vec_free(int64_t h);
int64_t fg14_s_map_new(void);
int64_t fg14_s_map_put(int64_t h, int64_t key, int64_t value);
int64_t fg14_s_map_erase(int64_t h, int64_t key);
int64_t fg14_s_map_free(int64_t h);
int64_t fg14_s_map_size(int64_t h);

/* --- vendor fused/numeric launches; return nodes-to-skip or 0 ------------- */
int64_t fg14_s_launch_shared_expert(int64_t ctx, int64_t g, int64_t i, int64_t routed,
                                    int64_t shared);
int64_t fg14_s_launch_weighted_reduction(int64_t ctx, int64_t g, int64_t i, int64_t n,
                                         int64_t out_idx);
int64_t fg14_s_launch_gdn_cache(int64_t ctx, int64_t g, int64_t i, int64_t cpy_idx);
int64_t fg14_s_launch_ssm_cache(int64_t ctx, int64_t g, int64_t i, int64_t cpy_idx);
int64_t fg14_s_launch_topk_moe(int64_t ctx, int64_t g, int64_t i, int64_t route,
                               int64_t n_ops, int64_t out0, int64_t out1);
int64_t fg14_s_launch_rope_set_rows(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_snake(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_multi(int64_t ctx, int64_t g, int64_t i, int64_t n, int64_t op);
int64_t fg14_s_launch_mm_glu(int64_t ctx, int64_t g, int64_t i, int64_t route,
                             int64_t n_ops, int64_t vec_kind);
int64_t fg14_s_launch_mm_scale_bias(int64_t ctx, int64_t g, int64_t i, int64_t route,
                                    int64_t n_ops, int64_t vec_kind);
int64_t fg14_s_launch_mm_bias(int64_t ctx, int64_t g, int64_t i, int64_t route,
                              int64_t vec_kind);
int64_t fg14_s_launch_rms_rope(int64_t ctx, int64_t g, int64_t i, int64_t with_set_rows);
int64_t fg14_s_launch_rms_add(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_rms_mul(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_rms_scale(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_ssm_conv(int64_t ctx, int64_t g, int64_t i, int64_t with_add);
int64_t fg14_s_launch_unary_mul(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_relu_sqr(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_dsv4_hc_post(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_launch_softcap(int64_t ctx, int64_t g, int64_t i);
int64_t fg14_s_dispatch_node(int64_t native_ctx, int64_t node);
int64_t fg14_s_node_is_view_or_noop(int64_t t);

/* --- CUDA graph vendor primitives ---------------------------------------- */
int64_t fg14_s_graph_key(int64_t g);
int64_t fg14_s_graph_enabled(int64_t native_ctx, int64_t key);
int64_t fg14_s_graph_compatible(int64_t g);
int64_t fg14_s_graph_update_required(int64_t native_ctx, int64_t g, int64_t graph_rec);
int64_t fg14_s_graph_capture_begin(int64_t native_ctx);
int64_t fg14_s_graph_capture_end(int64_t native_ctx, int64_t graph_rec);
int64_t fg14_s_graph_update_exec(int64_t native_ctx, int64_t graph_rec);
int64_t fg14_s_graph_instantiate(int64_t graph_rec);
int64_t fg14_s_graph_launch(int64_t native_ctx, int64_t graph_rec);
int64_t fg14_s_graph_destroy(int64_t graph_rec);
int64_t fg14_s_graph_sweep(int64_t native_ctx, int64_t now_us);
int64_t fg14_s_graph_opt_env(void);
int64_t fg14_s_add_dep(int64_t params, int64_t a, int64_t b);
int64_t fg14_s_t_depends(int64_t dst, int64_t src);
int64_t fg14_s_t_op(int64_t t);
int64_t fg14_s_t_src_at(int64_t t, int64_t i);
int64_t fg14_s_t_view_src_at(int64_t t);
int64_t fg14_s_t_nrows_at(int64_t t);
int64_t fg14_s_t_name_attn(int64_t t);
int64_t fg14_s_g_set_node_at(int64_t g, int64_t i, int64_t t);
int64_t fg14_s_g_rotate3(int64_t g, int64_t first, int64_t mid, int64_t last);
int64_t fg14_match_shared(int64_t g, int64_t r, int64_t s);

/* --- raw POD frames ------------------------------------------------------- */
int64_t fg14_s_frame_get(int64_t frame, int64_t i);
int64_t fg14_s_frame_set(int64_t frame, int64_t i, int64_t v);

#ifdef __cplusplus
}
#endif
#endif /* FG14_ABI_H */
