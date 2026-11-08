/* worker-14 / APX025: primitive layer for the canonical CUDA control path.
 * Included at the end of the patched real CUDA TU.  Raw CUDA/cuBLAS effects
 * and POD metadata projections only; routing/lifetime is Goose.
 */
#ifndef FG14_SERVICES_REAL_CPP
#define FG14_SERVICES_REAL_CPP 1
#include "fg14_abi.h"
#include "cuda-resource-schema.h"
#include "canonical-storage.h"
#include "cuda-numeric-ready.h"
#include "fg153-goose-cuda.h"
#include "moe-routing/fg153-moe-cuda.h"
#include "apex-cuda-goose-ops.h"

namespace fg14_real {
static int64_t * i64p(int64_t h) { return (int64_t *) (intptr_t) h; }
static ggml_tensor * tp(int64_t h) { return (ggml_tensor *) (intptr_t) h; }
static ggml_cgraph * gp(int64_t h) { return (ggml_cgraph *) (intptr_t) h; }
static ggml_backend_cuda_context * cp(int64_t h) { return (ggml_backend_cuda_context *) (intptr_t) h; }
static int cc_of(int64_t dev) { return ggml_cuda_info().devices[dev].cc; }
static int warp_of(int64_t dev) { return ggml_cuda_info().devices[dev].warp_size; }
static void np_fill(const ggml_tensor * t, int64_t * o) {
    for (int i = 0; i < FG14_NP_FIELD_COUNT; ++i) { o[i] = 0; }
    o[FG14_NP_NODE_PTR] = (int64_t) (intptr_t) t;
    o[FG14_NP_NODE_OP] = t->op;
    o[FG14_NP_NODE_TYPE] = t->type;
    o[FG14_NP_NODE_FLAGS] = t->flags;
    for (int d = 0; d < 4; ++d) { o[FG14_NP_NODE_NE0+d] = t->ne[d]; o[FG14_NP_NODE_NB0+d] = (int64_t) t->nb[d]; }
    for (int s = 0; s < GGML_MAX_SRC; ++s) {
        const ggml_tensor * q = t->src[s];
        if (!q) { continue; }
        o[FG14_NP_SRC_PTR0+s] = (int64_t) (intptr_t) q;
        o[FG14_NP_SRC_DATA0+s] = (int64_t) (intptr_t) q->data;
        for (int d = 0; d < 4; ++d) {
            o[FG14_NP_SRC_NE0 + s*4 + d] = q->ne[d];
            o[FG14_NP_SRC_NB0 + s*4 + d] = (int64_t) q->nb[d];
        }
    }
}
}
using namespace fg14_real;

extern "C" {

int64_t fg14_s_device_set(int64_t d) { ggml_cuda_set_device((int) d); return 0; }
int64_t fg14_s_device_get(void) { return ggml_cuda_get_device(); }
int64_t fg14_s_check(int64_t err, int64_t site) {
    if ((cudaError_t) err != cudaSuccess) { ggml_cuda_error("fg14", __func__, __FILE__, (int) site, cudaGetErrorString((cudaError_t) err)); }
    return 0;
}
int64_t fg14_s_last_error(void) { return (int64_t) cudaGetLastError(); }
int64_t fg14_s_clear_error(void) { (void) cudaGetLastError(); return 0; }
int64_t fg14_s_malloc(int64_t size) { void * p = nullptr; cudaError_t e = cudaMalloc(&p, (size_t) size); if (e != cudaSuccess) { return (int64_t) e; } return (int64_t) (intptr_t) p; }
int64_t fg14_s_free(int64_t p) { return (int64_t) cudaFree((void *) (intptr_t) p); }
int64_t fg14_s_stream_create(void) { cudaStream_t s = nullptr; cudaError_t e = cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking); if (e != cudaSuccess) { return 0; } return (int64_t) (intptr_t) s; }
int64_t fg14_s_stream_destroy(int64_t s) { return (int64_t) cudaStreamDestroy((cudaStream_t) (intptr_t) s); }
int64_t fg14_s_stream_sync(int64_t s) { return (int64_t) cudaStreamSynchronize((cudaStream_t) (intptr_t) s); }
int64_t fg14_s_cublas_create(int64_t ctx, int64_t dev, int64_t slot, int64_t stream) {
    (void) ctx; (void) dev; (void) slot;
    cublasHandle_t h = nullptr; cublasStatus_t st = cublasCreate(&h);
    if (st != CUBLAS_STATUS_SUCCESS) { return 0; }
    st = cublasSetStream(h, (cudaStream_t) (intptr_t) stream);
    if (st != CUBLAS_STATUS_SUCCESS) { (void) cublasDestroy(h); return 0; }
    return (int64_t) (intptr_t) h;
}
int64_t fg14_s_cublas_destroy(int64_t h) { return (int64_t) cublasDestroy((cublasHandle_t) (intptr_t) h); }
int64_t fg14_s_cublas_set_math(int64_t h, int64_t mode) { return (int64_t) cublasSetMathMode((cublasHandle_t) (intptr_t) h, (cublasMath_t) mode); }
int64_t fg14_s_cublas_set_stream(int64_t h, int64_t s) { return (int64_t) cublasSetStream((cublasHandle_t) (intptr_t) h, (cudaStream_t) (intptr_t) s); }
int64_t fg14_s_cublas_set_workspace(int64_t h, int64_t p, int64_t n) { return (int64_t) cublasSetWorkspace((cublasHandle_t) (intptr_t) h, (void *) (intptr_t) p, (size_t) n); }
int64_t fg14_s_event_create(void) { cudaEvent_t e = nullptr; cudaError_t r = cudaEventCreateWithFlags(&e, cudaEventDisableTiming); if (r != cudaSuccess) { return 0; } return (int64_t) (intptr_t) e; }
int64_t fg14_s_event_destroy(int64_t e) { return (int64_t) cudaEventDestroy((cudaEvent_t) (intptr_t) e); }
int64_t fg14_s_event_record(int64_t e, int64_t s) { return (int64_t) cudaEventRecord((cudaEvent_t) (intptr_t) e, (cudaStream_t) (intptr_t) s); }
int64_t fg14_s_stream_wait_event(int64_t s, int64_t e) { return (int64_t) cudaStreamWaitEvent((cudaStream_t) (intptr_t) s, (cudaEvent_t) (intptr_t) e, 0); }
int64_t fg14_s_stream_per_thread(void) { return (int64_t) (intptr_t) cudaStreamPerThread; }
int64_t fg14_s_device_sync(void) { return (int64_t) cudaDeviceSynchronize(); }
int64_t fg14_s_info_cc(int64_t d) { return cc_of(d); }
int64_t fg14_s_info_warp(int64_t d) { return warp_of(d); }
int64_t fg14_s_info_integrated(int64_t d) { return ggml_cuda_info().devices[d].integrated ? 1 : 0; }
int64_t fg14_s_info_vmm(int64_t d) { return ggml_cuda_info().devices[d].vmm ? 1 : 0; }
int64_t fg14_s_pool_create(int64_t ctx, int64_t dev, int64_t slot, int64_t vmm) {
    (void) slot;
#if defined(GGML_USE_VMM)
    if (vmm != 0) { return (int64_t) (intptr_t) new ggml_cuda_pool_vmm((int) dev); }
#endif
    (void) vmm;
    return (int64_t) (intptr_t) new ggml_cuda_pool_leg(*cp(ctx), (int) dev);
}
int64_t fg14_s_now_us(void) { return (int64_t) ggml_time_us(); }

int64_t fg14_s_ctx_device(int64_t n) { return cp(n)->device; }
int64_t fg14_s_ctx_curr_stream(int64_t n) { return cp(n)->curr_stream_no; }
int64_t fg14_s_ctx_curr_stream_set(int64_t n, int64_t v) { cp(n)->curr_stream_no = (int) v; return 0; }
int64_t fg14_s_ctx_stream(int64_t n, int64_t d, int64_t s) { return (int64_t) (intptr_t) cp(n)->streams[d][s]; }
int64_t fg14_s_ctx_stream_set(int64_t n, int64_t d, int64_t s, int64_t v) { cp(n)->streams[d][s] = (cudaStream_t) (intptr_t) v; return 0; }
int64_t fg14_s_ctx_cublas(int64_t n, int64_t d, int64_t s) { return (int64_t) (intptr_t) cp(n)->cublas_handles[d][s]; }
int64_t fg14_s_ctx_cublas_set(int64_t n, int64_t d, int64_t s, int64_t h) { cp(n)->cublas_handles[d][s] = (cublasHandle_t) (intptr_t) h; return 0; }
int64_t fg14_s_ctx_ws_size(int64_t n, int64_t d) { return (int64_t) cp(n)->cublas_workspace_sizes[d]; }
int64_t fg14_s_ctx_ws_size_set(int64_t n, int64_t d, int64_t v) { cp(n)->cublas_workspace_sizes[d] = (size_t) v; return 0; }
int64_t fg14_s_ctx_copy_event(int64_t n) { return (int64_t) (intptr_t) cp(n)->copy_event; }
int64_t fg14_s_ctx_copy_event_set(int64_t n, int64_t e) { cp(n)->copy_event = (cudaEvent_t) (intptr_t) e; return 0; }
int64_t fg14_s_ctx_rec(int64_t n) { return cp(n)->fg14_rec; }
int64_t fg14_s_ctx_rec_set(int64_t n, int64_t r) { cp(n)->fg14_rec = r; return 0; }
int64_t fg14_s_ctx_pool(int64_t n, int64_t d, int64_t s) { return (int64_t) (intptr_t) cp(n)->pools[d][s].get(); }
int64_t fg14_s_ctx_pool_set(int64_t n, int64_t d, int64_t s, int64_t p) { (void) n; (void) d; (void) s; (void) p; return 0; }
int64_t fg14_s_pool_alloc(int64_t p, int64_t size, int64_t out) {
    size_t actual = 0;
    void * ptr = ((ggml_cuda_pool *) (intptr_t) p)->alloc((size_t) size, &actual);
    *i64p(out) = (int64_t) actual;
    return (int64_t) (intptr_t) ptr;
}
int64_t fg14_s_pool_free(int64_t p, int64_t ptr, int64_t size) { ((ggml_cuda_pool *) (intptr_t) p)->free((void *) (intptr_t) ptr, (size_t) size); return 0; }
int64_t fg14_s_pool_destroy(int64_t p) { delete (ggml_cuda_pool *) (intptr_t) p; return 0; }

int64_t fg14_s_g_n(int64_t g) { return gp(g)->n_nodes; }
int64_t fg14_s_g_node(int64_t g, int64_t i) { return (int64_t) (intptr_t) gp(g)->nodes[i]; }
int64_t fg14_s_g_node_set(int64_t g, int64_t i, int64_t t) { gp(g)->nodes[i] = tp(t); return 0; }
int64_t fg14_s_g_rotate3(int64_t g, int64_t first, int64_t mid, int64_t last) {
    std::rotate(gp(g)->nodes + first, gp(g)->nodes + mid, gp(g)->nodes + last);
    return 0;
}
int64_t fg14_s_graph_opt_env(void) { const char * e = getenv("GGML_CUDA_GRAPH_OPT"); return e && atoi(e) == 1 ? 1 : 0; }
int64_t fg14_s_add_dep(int64_t params, int64_t a, int64_t b) {
    auto * p = (ggml_backend_graph_optimize_params *) (intptr_t) params;
    p->add_alloc_dep(p->user_data, tp(a), tp(b));
    return 0;
}
int64_t fg14_s_t_depends(int64_t dst, int64_t src) {
    auto * d = tp(dst); auto * s = tp(src);
    for (int i = 0; i < GGML_MAX_SRC; ++i) { if (d->src[i] == s) { return 1; } }
    const ggml_tensor * d2 = d->view_src ? d->view_src : d;
    const ggml_tensor * s2 = s->view_src ? s->view_src : s;
    return d2 == s2 ? 1 : 0;
}
int64_t fg14_s_t_op(int64_t t) { return tp(t)->op; }
int64_t fg14_s_t_src_at(int64_t t, int64_t i) { return (i >= 0 && i < GGML_MAX_SRC && tp(t)->src[i]) ? (int64_t) (intptr_t) tp(t)->src[i] : 0; }
int64_t fg14_s_t_view_src_at(int64_t t) { return (int64_t) (intptr_t) tp(t)->view_src; }
int64_t fg14_s_t_nrows_at(int64_t t) { return (int64_t) ggml_nrows(tp(t)); }
int64_t fg14_s_t_name_attn(int64_t t) { return strstr(tp(t)->name, "attn_norm") ? 1 : 0; }
int64_t fg14_s_g_use_count(int64_t g, int64_t i) { return ggml_node_get_use_count(gp(g), (int) i); }
int64_t fg14_s_g_name_has(int64_t g, int64_t i, int64_t n) { (void) n; return strstr(gp(g)->nodes[i]->name, "attn_norm") ? 1 : 0; }
int64_t fg14_s_g_np_len(int64_t v) { return v ? fgc_vec_len(v) : 0; }
int64_t fg14_s_t_name_has(int64_t t, int64_t n) { (void) n; return strstr(tp(t)->name, "attn_norm") ? 1 : 0; }
int64_t fg14_s_g_np_capture(int64_t v, int64_t g, int64_t i) {
    int64_t * d = i64p(v);
    int64_t * o = d + (size_t) i * FG14_NP_FIELD_COUNT;
    np_fill(gp(g)->nodes[i], o);
    return 0;
}
int64_t fg14_s_g_np_same(int64_t v, int64_t g, int64_t i) {
    int64_t cur[FG14_NP_FIELD_COUNT];
    np_fill(gp(g)->nodes[i], cur);
    const int64_t * old = i64p(v) + (size_t) i * FG14_NP_FIELD_COUNT;
    return memcmp(cur, old, sizeof(cur)) == 0 ? 1 : 0;
}
int64_t fg14_s_g_find_node(int64_t g, int64_t t) {
    auto * cg = gp(g);
    for (int i = 0; i < cg->n_nodes; ++i) { if (cg->nodes[i] == tp(t)) { return i; } }
    return -1;
}
int64_t fg14_s_g_uid(int64_t g) { return (int64_t) gp(g)->uid; }
int64_t fg14_s_g_set_uid(int64_t g, int64_t u) { gp(g)->uid = (size_t) u; return 0; }
int64_t fg14_s_mul_mat_id_needs_sync(int64_t t, int64_t cc) { return ggml_cuda_mul_mat_id_needs_sync(tp(t), (int) cc) ? 1 : 0; }
int64_t fg14_s_graph_env_disabled(void) { return getenv("GGML_CUDA_DISABLE_GRAPHS") ? 1 : 0; }
int64_t fg14_s_graph_node_cc(int64_t d) { return cc_of(d); }

int64_t fg14_s_g_can_fuse(int64_t g, int64_t i, int64_t ops, int64_t n) {
    return ggml_can_fuse(gp(g), (int) i, (const ggml_op *) (intptr_t) ops, (int) n) ? 1 : 0;
}
int64_t fg14_s_g_can_fuse_sub(int64_t g, int64_t i, int64_t n, int64_t ops, int64_t outs, int64_t no) {
    return ggml_can_fuse_subgraph(gp(g), (int) i, (int) n, (const ggml_op *) (intptr_t) ops,
                                  (const int *) (intptr_t) outs, (int) no) ? 1 : 0;
}
int64_t fg14_s_g_can_fuse_sub1(int64_t g, int64_t i, int64_t ops, int64_t n) {
    int out = (int) (i + n - 1);
    return ggml_can_fuse_subgraph(gp(g), (int) i, (int) n, (const ggml_op *) (intptr_t) ops, &out, 1) ? 1 : 0;
}
int64_t fg14_s_g_can_fuse_sub2(int64_t g, int64_t i, int64_t ops, int64_t n, int64_t o0, int64_t o1) {
    int outs[2] = { (int) o0, (int) o1 };
    return ggml_can_fuse_subgraph(gp(g), (int) i, (int) n, (const ggml_op *) (intptr_t) ops, outs, 2) ? 1 : 0;
}
int64_t fg14_s_g_can_fuse_shared(int64_t g, int64_t r, int64_t s) {
    int idxs[6] = { (int) r, (int) r+1, (int) r+2, (int) s, (int) s+1, (int) s+2 };
    ggml_op ops[6] = { GGML_OP_MUL_MAT_ID, GGML_OP_MUL_MAT_ID, GGML_OP_GLU,
                       GGML_OP_MUL_MAT, GGML_OP_MUL_MAT, GGML_OP_GLU };
    int outs[2] = { (int) r+2, (int) s+2 };
    return ggml_can_fuse_subgraph_ext(gp(g), idxs, 6, ops, outs, 2) ? 1 : 0;
}
int64_t fg14_s_g_edge(int64_t g, int64_t i, int64_t d, int64_t si, int64_t sn) {
    return gp(g)->nodes[i+d]->src[si] == gp(g)->nodes[i+sn] ? 1 : 0;
}
int64_t fg14_s_t_get(int64_t t, int64_t f, int64_t i) {
    auto * x = tp(t);
    switch (f) {
        case 0: return x->op; case 1: return x->type; case 2: return x->flags;
        case 3: return (i>=0&&i<4)?x->ne[i]:0; case 7: return (i>=0&&i<4)?(int64_t)x->nb[i]:0;
        case 11: return (i>=0&&i<GGML_MAX_SRC&&x->src[i])?(int64_t)(intptr_t)x->src[i]:0;
        case 21: return (int64_t)(intptr_t)x->view_src;
        case 22: return (int64_t)x->view_offs;
        case 23: return (int64_t)(intptr_t)x->data;
        case 24: return (int64_t) ggml_nbytes(x);
        case 25: return (int64_t) ggml_nelements(x);
        case 26: return (int64_t) ggml_nrows(x);
        case 27: return ggml_get_op_params_i32(x, (int) i);
        default: return 0;
    }
}
int64_t fg14_s_t_set_field(int64_t t, int64_t f, int64_t i, int64_t v) { (void) t; (void) f; (void) i; (void) v; return 0; }
int64_t fg14_s_t_nbytes(int64_t t) { return (int64_t) ggml_nbytes(tp(t)); }
int64_t fg14_s_t_view_src(int64_t t) { return (int64_t) (intptr_t) tp(t)->view_src; }
int64_t fg14_s_t_src(int64_t t, int64_t i) { return (i>=0&&i<GGML_MAX_SRC&&tp(t)->src[i])?(int64_t)(intptr_t)tp(t)->src[i]:0; }
int64_t fg14_s_t_data(int64_t t) { return (int64_t) (intptr_t) tp(t)->data; }
int64_t fg14_s_t_same_shape(int64_t a, int64_t b) { return ggml_are_same_shape(tp(a), tp(b)) ? 1 : 0; }
int64_t fg14_s_t_same_stride(int64_t a, int64_t b) { return ggml_are_same_stride(tp(a), tp(b)) ? 1 : 0; }
int64_t fg14_s_t_same_layout(int64_t a, int64_t b) { return ggml_are_same_layout(tp(a), tp(b)) ? 1 : 0; }
int64_t fg14_s_t_is_matrix(int64_t t) { return ggml_is_matrix(tp(t)) ? 1 : 0; }
int64_t fg14_s_contig(int64_t t, int64_t k) {
    auto * x = tp(t);
    switch (k) { case 12: return ggml_is_contiguous(x); case 13: return ggml_is_contiguous_1(x);
                 case 14: return ggml_is_contiguous_2(x); case 15: return ggml_is_contiguous_rows(x);
                 case 16: return ggml_is_contiguously_allocated(x); case 17: return ggml_is_transposed(x);
                 case 18: return ggml_is_contiguous_to_3(x); default: return 0; }
}
int64_t fg14_s_t_nelements(int64_t t) { return (int64_t) ggml_nelements(tp(t)); }
int64_t fg14_s_t_nrows(int64_t t) { return (int64_t) ggml_nrows(tp(t)); }
int64_t fg14_s_t_buffer_usage(int64_t t) { return tp(t)->buffer ? (int64_t) ggml_backend_buffer_get_usage(tp(t)->buffer) : -1; }
int64_t fg14_s_t_alloc_size(int64_t t) { return (int64_t) ggml_backend_buft_get_alloc_size(tp(t)->buffer->buft, tp(t)); }
int64_t fg14_s_t_is_quant_type(int64_t ty) { return ggml_is_quantized((ggml_type) ty) ? 1 : 0; }
int64_t fg14_s_t_has_buffer(int64_t t) { return tp(t)->buffer ? 1 : 0; }
int64_t fg14_s_t_op_f32_bits(int64_t t, int64_t i) { float f = 0; memcpy(&f, (const char *) tp(t)->op_params + i*sizeof(float), sizeof(f)); uint32_t u; memcpy(&u,&f,sizeof(u)); return u; }
int64_t fg14_s_t_use_field(int64_t t, int64_t f) { return fg14_s_t_get(t, f, 0); }
int64_t fg14_s_ggml_row_size(int64_t ty, int64_t n) { return (int64_t) ggml_row_size((ggml_type) ty, n); }
int64_t fg14_s_assert(int64_t c, int64_t s) { if (!c) { GGML_ABORT("fg14 assertion %lld", (long long) s); } return 0; }

int64_t fg14_s_should_use_mmvf_t(int64_t t, int64_t batch) {
    auto * x = tp(t); int dev = ggml_cuda_get_device();
    return ggml_cuda_should_use_mmvf(x->src[0]->type, cc_of(dev), warp_of(dev), x->src[0]->ne, x->src[0]->nb, batch) ? 1 : 0;
}
int64_t fg14_s_should_use_mmf_t(int64_t t, int64_t batch, int64_t mulid) {
    auto * x = tp(t); int dev = ggml_cuda_get_device();
    return ggml_cuda_should_use_mmf(x->src[0]->type, cc_of(dev), warp_of(dev), x->src[0]->ne, x->src[0]->nb, (int) batch, mulid != 0) ? 1 : 0;
}
int64_t fg14_s_should_use_mmvq_t(int64_t t) { auto * x = tp(t); int dev = ggml_cuda_get_device(); return ggml_cuda_should_use_mmvq(x->src[0]->type, cc_of(dev), x->src[1]->ne[1]) ? 1 : 0; }
int64_t fg14_s_should_use_mmq_t(int64_t t, int64_t ne) { auto * x = tp(t); int dev = ggml_cuda_get_device(); return ggml_cuda_should_use_mmq(x->src[0]->type, cc_of(dev), x->src[1]->ne[1], ne) ? 1 : 0; }
int64_t fg14_s_should_use_topk_moe(int64_t node, int64_t logits, int64_t weights, int64_t ids) { return ggml_cuda_should_use_topk_moe(tp(node), tp(logits), tp(weights), tp(ids)) ? 1 : 0; }
int64_t fg14_s_mmvq_mmid_max_batch(int64_t ty, int64_t cc) { return get_mmvq_mmid_max_batch((ggml_type) ty, (int) cc); }
int64_t fg14_s_mmvq_max_batch(void) { return MMVQ_MAX_BATCH_SIZE; }
int64_t fg14_s_fusion_disabled(void) { return ggml_cuda_fusion_disabled() ? 1 : 0; }
int64_t fg14_s_log(int64_t site, int64_t a, int64_t b) {
    if (site == 99) { GGML_LOG_ERROR("%s: op not supported %s\n", __func__, tp(a)->name); }
    else { GGML_LOG_DEBUG("fg14 site %lld: %lld %lld\n", (long long) site, (long long) a, (long long) b); }
    return 0;
}
int64_t fg14_s_map_get(int64_t m, int64_t k) {
    int64_t out = -1;
    if (!fgc_map_get(m, k, &out)) { return -1; }
    return out;
}
int64_t fg14_s_record_new(int64_t kind) { return fgc_record_new(kind); }
int64_t fg14_s_record_get(int64_t h, int64_t k) { return fgc_record_get(h, k); }
int64_t fg14_s_record_set(int64_t h, int64_t k, int64_t v) { return fgc_record_set(h, k, v); }
int64_t fg14_s_record_free(int64_t h) { fgc_record_free(h); return 0; }
int64_t fg14_s_vec_new(int64_t b) { return fgc_vec_new(b); }
int64_t fg14_s_vec_len(int64_t h) { return fgc_vec_len(h); }
int64_t fg14_s_vec_data(int64_t h) { return (int64_t) (intptr_t) fgc_vec_data(h); }
int64_t fg14_s_vec_resize(int64_t h, int64_t n) { return fgc_vec_resize(h, n); }
int64_t fg14_s_vec_free(int64_t h) { fgc_vec_free(h); return 0; }
int64_t fg14_s_map_new(void) { return fgc_map_new(); }
int64_t fg14_s_map_put(int64_t h, int64_t k, int64_t v) { return fgc_map_put(h, k, v); }
int64_t fg14_s_map_erase(int64_t h, int64_t k) { return fgc_map_erase(h, k); }
int64_t fg14_s_map_free(int64_t h) { fgc_map_free(h); return 0; }
int64_t fg14_s_map_size(int64_t h) { return fgc_map_size(h); }

int64_t fg14_s_dispatch_node(int64_t n, int64_t t) { return ggml_cuda_compute_forward(*cp(n), tp(t)) ? 1 : 0; }
int64_t fg14_s_node_is_view_or_noop(int64_t t) { return ggml_cuda_is_view_or_noop(tp(t)) ? 1 : 0; }

int64_t fg14_s_graph_key(int64_t g) { return (int64_t) (intptr_t) gp(g)->nodes[0]; }
int64_t fg14_s_graph_enabled(int64_t n, int64_t key) { (void) n; (void) key; return 1; }
int64_t fg14_s_graph_compatible(int64_t g) {
#ifdef USE_CUDA_GRAPH
    return ggml_cuda_graph_check_compability(gp(g)) ? 1 : 0;
#else
    (void) g; return 1;
#endif
}
int64_t fg14_s_have_cuda_graph(void) {
#ifdef USE_CUDA_GRAPH
    return 1;
#else
    return 0;
#endif
}
int64_t fg14_s_graph_update_required(int64_t n, int64_t g, int64_t r) { (void) n; (void) g; (void) r; return 0; }
int64_t fg14_s_graph_capture_begin(int64_t n) {
    { std::lock_guard<std::mutex> lock(ggml_cuda_lock); ggml_cuda_lock_counter.fetch_add(1); }
    return (int64_t) cudaStreamBeginCapture(cp(n)->stream(), cudaStreamCaptureModeRelaxed);
}
int64_t fg14_s_graph_capture_end(int64_t n, int64_t r) {
    cudaGraph_t graph = nullptr;
    cudaError_t err = cudaStreamEndCapture(cp(n)->stream(), &graph);
    fgc_record_set(r, FG14_GRAPH_PTR, (int64_t) (intptr_t) graph);
    { std::lock_guard<std::mutex> lock(ggml_cuda_lock); if (ggml_cuda_lock_counter.fetch_sub(1) == 1) { ggml_cuda_lock_cv.notify_all(); } }
    return (int64_t) err;
}
int64_t fg14_s_graph_update_exec(int64_t n, int64_t r) {
    (void) n;
    cudaGraph_t graph = (cudaGraph_t) (intptr_t) fgc_record_get(r, FG14_GRAPH_PTR);
    cudaGraphExec_t exec = (cudaGraphExec_t) (intptr_t) fgc_record_get(r, FG14_GRAPH_EXEC);
    cudaGraphExecUpdateResultInfo info;
    cudaError_t st = cudaGraphExecUpdate(exec, graph, &info);
    if (st == cudaErrorGraphExecUpdateFailure) { (void) cudaGetLastError(); cudaGraphExecDestroy(exec); exec = nullptr; st = cudaGraphInstantiate(&exec, graph, NULL, NULL, 0); }
    fgc_record_set(r, FG14_GRAPH_EXEC, (int64_t) (intptr_t) exec);
    return (int64_t) st;
}
int64_t fg14_s_graph_instantiate(int64_t r) {
    cudaGraph_t graph = (cudaGraph_t) (intptr_t) fgc_record_get(r, FG14_GRAPH_PTR);
    cudaGraphExec_t exec = nullptr;
    cudaError_t st = cudaGraphInstantiate(&exec, graph, NULL, NULL, 0);
    fgc_record_set(r, FG14_GRAPH_EXEC, (int64_t) (intptr_t) exec);
    return (int64_t) st;
}
int64_t fg14_s_graph_launch(int64_t n, int64_t r) { return (int64_t) cudaGraphLaunch((cudaGraphExec_t) (intptr_t) fgc_record_get(r, FG14_GRAPH_EXEC), cp(n)->stream()); }
int64_t fg14_s_graph_destroy(int64_t r) { (void) r; return 0; }
int64_t fg14_s_graph_sweep(int64_t n, int64_t now) { (void) n; (void) now; return 0; }

/* Real ready-module bindings for the routes that map directly to a qualified
 * Goose numerical module.  Composite routes that need multiple module calls
 * are declined explicitly (return 0) so the normal per-op Goose dispatch runs;
 * they are never aborted and never fake-success. */
int64_t fgn_cuda_weighted_reduction(int64_t c, int64_t g, int64_t i, int64_t n, int64_t o) {
    (void) n; (void) o;
    ggml_cuda_moe_weighted_reduction_match m;
    if (!ggml_cuda_match_moe_weighted_reduction(gp(g), (int) i, m)) { return 0; }
    const int64_t n_embd = m.experts->ne[0];
    const int64_t n_tokens = m.experts->ne[2] * m.experts->ne[3];
    const int n_used = (int) m.experts->ne[1];
    const float * scale = m.expert_scale ? (const float *) m.expert_scale->data : nullptr;
    if (!fg153_goose_moe_reduction_launch((const float *) m.experts->data, scale,
            (const float *) m.weights->data, (float *) m.dst->data,
            n_embd, n_tokens, n_used, cp(c)->stream())) { return 0; }
    return m.node_count - 1;
}
int64_t fgn_cuda_gdn_cache(int64_t c,int64_t g,int64_t i,int64_t cpy){
    (void)c;(void)g;(void)i;(void)cpy;
    GGML_LOG_DEBUG("%s: fused gdn cache declined (ready-module composition unavailable)\n", __func__);
    return 0;
}
int64_t fgn_cuda_ssm_cache(int64_t c,int64_t g,int64_t i,int64_t cpy){
    (void)c;(void)g;(void)i;(void)cpy;
    GGML_LOG_DEBUG("%s: fused ssm cache declined (ready-module composition unavailable)\n", __func__);
    return 0;
}
int64_t fgn_cuda_shared_expert(int64_t c,int64_t g,int64_t i,int64_t r,int64_t s){(void)c;(void)g;(void)i;(void)r;(void)s;GGML_LOG_DEBUG("%s: shared-expert fusion declined\n",__func__);return 0;}
int64_t fgn_cuda_topk_moe(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t a,int64_t b){(void)c;(void)g;(void)i;(void)r;(void)n;(void)a;(void)b;GGML_LOG_DEBUG("%s: topk-moe fusion declined\n",__func__);return 0;}
int64_t fgn_cuda_mm_glu(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t v){(void)c;(void)g;(void)i;(void)r;(void)n;(void)v;GGML_LOG_DEBUG("%s: mm-glu fusion declined\n",__func__);return 0;}
int64_t fgn_cuda_mm_scale_bias(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t v){(void)c;(void)g;(void)i;(void)r;(void)n;(void)v;GGML_LOG_DEBUG("%s: mm-scale-bias fusion declined\n",__func__);return 0;}
int64_t fgn_cuda_mm_bias(int64_t c,int64_t g,int64_t i,int64_t r,int64_t v){(void)c;(void)g;(void)i;(void)r;(void)v;GGML_LOG_DEBUG("%s: mm-bias fusion declined\n",__func__);return 0;}

/* Fused launches are typed imports from the ready Goose numerical modules.
 * They are live calls; this control layer never treats a kernel as complete. */
int64_t fg14_s_launch_shared_expert(int64_t c,int64_t g,int64_t i,int64_t r,int64_t s){return fgn_cuda_shared_expert(c,g,i,r,s);}
int64_t fg14_s_launch_weighted_reduction(int64_t c,int64_t g,int64_t i,int64_t n,int64_t o){return fgn_cuda_weighted_reduction(c,g,i,n,o);}
int64_t fg14_s_launch_gdn_cache(int64_t c,int64_t g,int64_t i,int64_t x){return fgn_cuda_gdn_cache(c,g,i,x);}
int64_t fg14_s_launch_ssm_cache(int64_t c,int64_t g,int64_t i,int64_t x){return fgn_cuda_ssm_cache(c,g,i,x);}
int64_t fg14_s_launch_topk_moe(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t o0,int64_t o1){return fgn_cuda_topk_moe(c,g,i,r,n,o0,o1);}
int64_t fg14_s_launch_mm_glu(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t v){return fgn_cuda_mm_glu(c,g,i,r,n,v);}
int64_t fg14_s_launch_mm_scale_bias(int64_t c,int64_t g,int64_t i,int64_t r,int64_t n,int64_t v){return fgn_cuda_mm_scale_bias(c,g,i,r,n,v);}
int64_t fg14_s_launch_mm_bias(int64_t c,int64_t g,int64_t i,int64_t r,int64_t v){return fgn_cuda_mm_bias(c,g,i,r,v);}
int64_t fg14_s_launch_rope_set_rows(int64_t c,int64_t g,int64_t i){ggml_cuda_op_rope_fused(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+2]);return 2;}
int64_t fg14_s_launch_snake(int64_t c,int64_t g,int64_t i){
    auto * m0=gp(g)->nodes[i]; auto * sqr=gp(g)->nodes[i+2]; auto * m1=gp(g)->nodes[i+3]; auto * add=gp(g)->nodes[i+4];
    const ggml_tensor * x = ggml_are_same_shape(m0, m0->src[0]) ? m0->src[0] : m0->src[1];
    const ggml_tensor * a = (x == m0->src[0]) ? m0->src[1] : m0->src[0];
    const ggml_tensor * invb = (m1->src[0] == sqr) ? m1->src[1] : m1->src[0];
    ggml_cuda_op_snake_fused(*cp(c), x, a, invb, add); return 4;
}
int64_t fg14_s_launch_multi(int64_t c,int64_t g,int64_t i,int64_t n,int64_t op){
    ggml_tensor fused; memcpy(&fused,gp(g)->nodes[i],sizeof(fused));
    for(int j=0;j<n-1;++j){fused.src[j+2]=gp(g)->nodes[i+j+1]->src[1];}
    fused.data=gp(g)->nodes[i+n-1]->data;
    if(op==2){ggml_cuda_op_fused_add(*cp(c),&fused,(int)n);}else{ggml_cuda_op_fused_mul(*cp(c),&fused,(int)n);}
    return n-1;
}
int64_t fg14_s_launch_rms_rope(int64_t c,int64_t g,int64_t i,int64_t sr){ggml_cuda_op_rms_norm_mul_rope_fused(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1],gp(g)->nodes[i+2],sr?gp(g)->nodes[i+4]:nullptr);return sr?4:2;}
int64_t fg14_s_launch_rms_add(int64_t c,int64_t g,int64_t i){ggml_cuda_op_rms_norm_fused_add(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1],gp(g)->nodes[i+2]);return 2;}
int64_t fg14_s_launch_rms_mul(int64_t c,int64_t g,int64_t i){ggml_cuda_op_rms_norm_fused(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1]);return 1;}
int64_t fg14_s_launch_rms_scale(int64_t c,int64_t g,int64_t i){ggml_cuda_op_rms_norm_scale_fused(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1]);return 1;}
int64_t fg14_s_launch_ssm_conv(int64_t c,int64_t g,int64_t i,int64_t add){ggml_cuda_op_ssm_conv(*cp(c),gp(g)->nodes[i],add?gp(g)->nodes[i+1]:nullptr,gp(g)->nodes[i+(add?2:1)]);return add?2:1;}
int64_t fg14_s_launch_unary_mul(int64_t c,int64_t g,int64_t i){ggml_cuda_op_unary_mul(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1]);return 1;}
int64_t fg14_s_launch_relu_sqr(int64_t c,int64_t g,int64_t i){ggml_cuda_op_relu_sqr(*cp(c),gp(g)->nodes[i],gp(g)->nodes[i+1]);return 1;}
int64_t fg14_s_launch_dsv4_hc_post(int64_t c,int64_t g,int64_t i){ggml_cuda_op_dsv4_hc_post(*cp(c),gp(g)->nodes[i+3],gp(g)->nodes[i],gp(g)->nodes[i+2]);return 3;}
int64_t fg14_s_launch_softcap(int64_t c,int64_t g,int64_t i){ggml_cuda_op_softcap(*cp(c),gp(g)->nodes[i+2],gp(g)->nodes[i]);return 2;}

int64_t fg14_s_frame_get(int64_t p, int64_t i) { return i64p(p)[i]; }
int64_t fg14_s_frame_set(int64_t p, int64_t i, int64_t v) { i64p(p)[i] = v; return 0; }

} // extern "C"
#endif
