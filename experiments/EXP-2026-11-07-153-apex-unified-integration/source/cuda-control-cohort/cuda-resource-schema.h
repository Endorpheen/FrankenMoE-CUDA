/* worker-14 / APX025: canonical CUDA private-resource schema v1.
 *
 * Schema owner: worker-14.  These are the only field keys other modules may
 * use for CUDA private resources.  All handles are canonical-storage int64_t
 * handles (worker-8 ABI), not C++ classes or private offsets.
 *
 * Records:
 *   FG14_KIND_CTX     one per ggml_backend_cuda_context
 *   FG14_KIND_GRAPH   one per CUDA-graph cache entry
 *   FG14_KIND_CONC    one per concurrent-stream region
 */
#ifndef FG14_CUDA_RESOURCE_SCHEMA_H
#define FG14_CUDA_RESOURCE_SCHEMA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FG14_KIND_CTX   = 1401,
    FG14_KIND_GRAPH = 1402,
    FG14_KIND_CONC  = 1403
};

/* Context keys.  Map/vector values are canonical handles. */
enum {
    FG14_CTX_DEVICE          = 1,
    FG14_CTX_CURR_STREAM     = 2,
    FG14_CTX_COPY_EVENT      = 3,
    FG14_CTX_STREAMS         = 4,  /* map: (device * 8 + slot) -> cudaStream_t */
    FG14_CTX_CUBLAS          = 5,  /* map: (device * 8 + slot) -> cublasHandle_t */
    FG14_CTX_WS_SIZE         = 6,  /* map: device -> workspace bytes */
    FG14_CTX_WS              = 7,  /* map: (device * 8 + slot) -> workspace ptr */
    FG14_CTX_POOLS           = 8,  /* map: (device * 8 + slot) -> pool resource */
    FG14_CTX_GRAPHS          = 9,  /* map: graph_key -> FG14_KIND_GRAPH */
    FG14_CTX_CONC_MAP        = 10, /* map: tensor ptr -> FG14_KIND_CONC */
    FG14_CTX_CONC_KEYS       = 11, /* vec<i64>: insertion order of conc keys */
    FG14_CTX_LAST_SWEEP      = 12,
    FG14_CTX_GRAPH_KEYS      = 13, /* vec<i64> */
    FG14_CTX_STREAM_KEYS     = 14, /* vec<i64> */
    FG14_CTX_CUBLAS_KEYS     = 15, /* vec<i64> */
    FG14_CTX_WS_KEYS         = 16, /* vec<i64> */
    FG14_CTX_POOL_KEYS       = 17, /* vec<i64> */
    FG14_CTX_NATIVE          = 18  /* native ggml_backend_cuda_context pointer */
};

/* Graph record keys. */
enum {
    FG14_GRAPH_PTR         = 1,
    FG14_GRAPH_EXEC        = 2,
    FG14_GRAPH_NUM_NODES   = 3,
    FG14_GRAPH_DISABLE_ARCH = 4,
    FG14_GRAPH_WARMUP      = 5,
    FG14_GRAPH_UID         = 6,
    FG14_GRAPH_LAST_USED   = 7,
    FG14_GRAPH_NODE_PROPS  = 8,  /* vec: packed fg14_node_props */
    FG14_GRAPH_KEY         = 9
};

/* Concurrent-region record keys. */
enum {
    FG14_CONC_FORK_EVENT   = 1,
    FG14_CONC_NSTREAMS     = 2,
    FG14_CONC_STREAM_MAP   = 3,  /* map: tensor ptr -> stream number (1..n) */
    FG14_CONC_ORDER        = 4,  /* vec<i64>: original node order */
    FG14_CONC_JOIN_NODE    = 5,
    FG14_CONC_JOIN_EVENTS  = 6,  /* vec<i64>: join cudaEvent_t per stream */
    FG14_CONC_STREAM_KEYS  = 7   /* vec<i64>: insertion order of stream-map keys */
};

/* Packed node-properties projection.  The layout is published here; services
 * fill it from a real ggml_tensor and Goose compares field-by-field. */
enum {
    FG14_NP_NODE_PTR      = 0,
    FG14_NP_NODE_OP       = 1,
    FG14_NP_NODE_TYPE     = 2,
    FG14_NP_NODE_FLAGS    = 3,
    FG14_NP_NODE_NE0      = 4,   /* + dim */
    FG14_NP_NODE_NB0      = 8,   /* + dim */
    FG14_NP_SRC_PTR0      = 12,  /* + src index */
    FG14_NP_SRC_DATA0     = 22,  /* + src index */
    FG14_NP_SRC_NE0       = 32,  /* + src index*4 + dim */
    FG14_NP_SRC_NB0       = 72,  /* + src index*4 + dim */
    FG14_NP_FIELD_COUNT   = 112
};

/* Strong exports implemented by cuda-resource.goose. */
int64_t fg14_res_ctx_new(int64_t device);
int64_t fg14_res_ctx_attach(int64_t native_ctx, int64_t device);
int64_t fg14_res_ctx_free(int64_t ctx);
int64_t fg14_res_ctx_get(int64_t ctx, int64_t key);
int64_t fg14_res_ctx_set(int64_t ctx, int64_t key, int64_t value);
int64_t fg14_res_stream(int64_t ctx, int64_t dev, int64_t slot);
int64_t fg14_res_cublas(int64_t ctx, int64_t dev, int64_t slot);
int64_t fg14_res_pool(int64_t ctx, int64_t dev, int64_t slot);
int64_t fg14_res_pool_alloc(int64_t ctx, int64_t dev, int64_t slot, int64_t size,
                            int64_t out_actual);
int64_t fg14_res_pool_free(int64_t ctx, int64_t dev, int64_t slot, int64_t ptr,
                           int64_t size);
int64_t fg14_res_graph(int64_t ctx, int64_t key);
int64_t fg14_res_graph_get(int64_t graph, int64_t key);
int64_t fg14_res_graph_set(int64_t graph, int64_t key, int64_t value);
int64_t fg14_res_graph_node_props(int64_t graph, int64_t cgraph);
int64_t fg14_res_graph_invalidate(int64_t ctx);
int64_t fg14_res_graph_any_enabled(int64_t ctx);
int64_t fg14_res_graph_any_instance(int64_t ctx);
int64_t fg14_res_conc_new(int64_t ctx, int64_t n_streams);
int64_t fg14_res_conc_free(int64_t conc);
int64_t fg14_res_conc_get(int64_t conc, int64_t key);
int64_t fg14_res_conc_set(int64_t conc, int64_t key, int64_t value);
int64_t fg14_res_conc_stream_get(int64_t conc, int64_t tensor);
int64_t fg14_res_conc_stream_set(int64_t conc, int64_t tensor, int64_t stream_no);
int64_t fg14_res_conc_order_len(int64_t conc);
int64_t fg14_res_conc_order_at(int64_t conc, int64_t index);
int64_t fg14_res_conc_order_push(int64_t conc, int64_t tensor);
int64_t fg14_res_conc_join_event(int64_t conc, int64_t index);
int64_t fg14_res_conc_valid(int64_t conc);
int64_t fg14_res_stream_ctx_reset(int64_t ctx);
int64_t fg14_res_stream_ctx_size(int64_t ctx);
int64_t fg14_res_stream_ctx_key_at(int64_t ctx, int64_t index);
int64_t fg14_res_stream_ctx_lookup(int64_t ctx, int64_t tensor);
int64_t fg14_res_stream_ctx_insert(int64_t ctx, int64_t tensor, int64_t conc);

/* Strong exports implemented by cuda-launch.goose and cuda-fusion.goose. */
int64_t fg14_launch_graph(int64_t ctx_rec, int64_t cgraph, int64_t use_graph,
                          int64_t update_required, int64_t graph_key);
int64_t fg14_can_fuse(int64_t cgraph, int64_t node_idx, int64_t ops_ptr,
                      int64_t n_ops, int64_t unary_ptr, int64_t n_unary);
int64_t fg14_try_fuse(int64_t ctx_rec, int64_t cgraph, int64_t node_idx);
int64_t fg14_graph_optimize(int64_t native_ctx, int64_t cgraph, int64_t params);

#ifdef __cplusplus
}
#endif
#endif /* FG14_CUDA_RESOURCE_SCHEMA_H */
