// Real tensor/workspace effects consumed by repack-control.goose.
// The Goose module owns selection, work-size, chunking, row/ID mapping and
// loop control; this TU only reads tensor fields, computes byte addresses,
// calls the ready numerical module, and performs threadpool/barrier effects.
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-cpu/repack.h"
#include "ggml-cpu/quants.h"
#include "ggml-cpu/ggml-cpu-impl.h"
#include <cstdint>
#include <cstring>

extern "C" {
int64_t fg006_rc_entry(int64_t, int64_t, int64_t, int64_t, int64_t,
                       int64_t, int64_t, int64_t, int64_t);
int fg006_rc_enabled(void);
int fg006_quantize_mat_q8_K_4x8(const float *, void *, int64_t);
void fg148_row_q8_K(const float *,void *,int64_t);
}

struct fg006_rc_ctx {
    ggml_compute_params * params;
    ggml_tensor * op;
    char * wdata;
    size_t nbw1, nbw2;
    size_t src1_col_stride;
    int64_t ne00, ne01, ne02, ne10, ne11, ne12;
    size_t nb0, nb01, nb02, nb11, nb12, nb1, nb2;
    int64_t * row_counts;
    struct row_map { int32_t i1, i2; } * rows;
};
static thread_local fg006_rc_ctx * g_rc;

extern "C" int64_t fg006_rc_work_size_op(const ggml_tensor * op) {
    if (!op || (op->op != GGML_OP_MUL_MAT && op->op != GGML_OP_MUL_MAT_ID)) return 0;
    const int is_id = op->op == GGML_OP_MUL_MAT_ID;
    const ggml_tensor * src1 = op->src[1];
    const int64_t ne02 = is_id ? op->src[0]->ne[2] : 1;
    return fg006_rc_entry(1, is_id, src1->ne[0], src1->ne[1], src1->ne[2], ne02, src1->ne[3], 0, 0);
}
extern "C" int fg006_rc_supports_op(const ggml_tensor * op) {
    if (!op) return 0;
    const int is_mm = op->op == GGML_OP_MUL_MAT;
    const int is_id = op->op == GGML_OP_MUL_MAT_ID;
    const int dims = op->src[0] ? ggml_n_dims(op->src[0]) : 0;
    const int repack = op->src[0] && op->src[0]->buffer &&
                       op->src[0]->buffer->buft == ggml_backend_cpu_repack_buffer_type();
    const int traits = op->src[0] && op->src[0]->extra != nullptr;
    const int nonhost = op->src[1] && op->src[1]->buffer &&
                        !ggml_backend_buft_is_host(op->src[1]->buffer->buft);
    const int f32 = op->src[1] && op->src[1]->type == GGML_TYPE_F32;
    return (int) fg006_rc_entry(3, is_mm, is_id, dims, repack, traits, nonhost, f32, 0);
}
extern "C" int fg006_rc_get_traits_op(const ggml_tensor * op) {
    if (!op || !op->src[0] || !op->src[0]->buffer) return 0;
    const int is_mm = op->op == GGML_OP_MUL_MAT;
    const int is_id = op->op == GGML_OP_MUL_MAT_ID;
    const int repack = op->src[0]->buffer->buft == ggml_backend_cpu_repack_buffer_type();
    return (int) fg006_rc_entry(4, is_mm, is_id, repack, op->src[0]->extra != nullptr, 0, 0, 0, 0);
}
extern "C" int fg006_rc_select_q4_K(const ggml_tensor * cur) {
    return (int) fg006_rc_entry(0, cur->ne[1], ggml_cpu_has_avx2(), ggml_cpu_has_neon(),
                                ggml_cpu_has_matmul_int8(), ggml_cpu_has_dotprod(), 0, 0, 0);
}

static void rc_quant4(int64_t i12, int64_t i11, int64_t ne10) {
    fg006_rc_ctx * c = g_rc;
    const float * src = (const float *) ((const char *) c->op->src[1]->data + i12 * c->nb12 + i11 * c->nb11);
    void * dst = c->wdata + i12 * c->nbw2 + i11 * c->nbw1;
    fg006_quantize_mat_q8_K_4x8(src, dst, ne10);
    (void) c;
}
static void rc_quant1(int64_t i12, int64_t i11, int64_t ne10) {
    fg006_rc_ctx * c = g_rc;
    const float * src = (const float *) ((const char *) c->op->src[1]->data + i12 * c->nb12 + i11 * c->nb11);
    void * dst = c->wdata + i12 * c->nbw2 + i11 * c->nbw1;
    fg148_row_q8_K(src, dst, ne10);
}

extern "C" int64_t fg006_rc_k(void) {return g_rc->ne10;}
extern "C" void fg006_rc_quant4(int64_t i12, int64_t i11, int64_t ne10) { rc_quant4(i12, i11, ne10); }
extern "C" void fg006_rc_quant1(int64_t i12, int64_t i11, int64_t ne10) { rc_quant1(i12, i11, ne10); }
extern "C" void fg006_rc_chunk_set(int64_t v) { ggml_threadpool_chunk_set(g_rc->params->threadpool, (int) v); }
extern "C" int64_t fg006_rc_chunk_add(int64_t d) {
    return ggml_threadpool_chunk_add(g_rc->params->threadpool, (int) d);
}
extern "C" void fg006_rc_barrier(void) { ggml_barrier(g_rc->params->threadpool); }

extern "C" void fg006_rc_chunk_gemm(int64_t s0s,int64_t s0e,int64_t s1s,int64_t rows) {
    fg006_rc_ctx *c=g_rc;
    const int64_t i12=s1s/c->ne11,i11=s1s-i12*c->ne11,i02=i12/(c->ne12/c->ne02);
    const char *x=(const char*)c->op->src[0]->data+i02*c->nb02+s0s*c->nb01;
    const char *y=c->wdata+(i11+i12*c->ne11)*c->src1_col_stride;
    char *d=(char*)c->op->data+i11*c->nb1+i12*c->nb2;
    ggml_gemm_q4_K_8x8_q8_K((int)c->ne00,(float*)d+s0s,c->nb1/c->nb0,x,y,(int)rows,(int)(s0e-s0s));
}
extern "C" void fg006_rc_chunk_gemv(int64_t s0s,int64_t s0e,int64_t s1s,int64_t row) {
    fg006_rc_ctx *c=g_rc;
    const int64_t i12=s1s/c->ne11,i11=s1s-i12*c->ne11,i02=i12/(c->ne12/c->ne02);
    const char *x=(const char*)c->op->src[0]->data+i02*c->nb02+s0s*c->nb01;
    const char *y=c->wdata+(i11+i12*c->ne11)*c->src1_col_stride;
    char *d=(char*)c->op->data+i11*c->nb1+i12*c->nb2;
    ggml_gemv_q4_K_8x8_q8_K((int)c->ne00,(float*)(d+row*c->nb1)+s0s,c->ne01,x,y+row*c->src1_col_stride,1,(int)(s0e-s0s));
}

extern "C" void fg006_rc_rows_clear(int64_t n_as) { std::memset(g_rc->row_counts, 0, (size_t) n_as * sizeof(int64_t)); }
extern "C" int32_t fg006_rc_ids_get(int64_t iid1, int64_t id) {
    const ggml_tensor * ids = g_rc->op->src[2];
    return *(const int32_t *) ((const char *) ids->data + iid1 * ids->nb[1] + id * ids->nb[0]);
}
extern "C" int64_t fg006_rc_row_count(int64_t expert) { return g_rc->row_counts[expert]; }
extern "C" void fg006_rc_row_set(int64_t expert, int64_t index, int64_t i1, int64_t i2) {
    GGML_ASSERT(expert>=0 && expert<g_rc->ne02 && index>=0 && index<g_rc->ne12);
    g_rc->rows[expert * g_rc->ne12 + index] = {(int32_t) i1, (int32_t) i2};
}
extern "C" void fg006_rc_row_inc(int64_t expert) { ++g_rc->row_counts[expert]; }
extern "C" int64_t fg006_rc_row_i1(int64_t expert, int64_t index) {
    return g_rc->rows[expert * g_rc->ne12 + index].i1;
}
extern "C" int64_t fg006_rc_row_i2(int64_t expert, int64_t index) {
    return g_rc->rows[expert * g_rc->ne12 + index].i2;
}
extern "C" void fg006_rc_mmid_gemv(int64_t expert, int64_t i1, int64_t i2, int64_t i11,
                                   int64_t s0s, int64_t s0e) {
    fg006_rc_ctx * c = g_rc;
    const char * src0 = (const char *) c->op->src[0]->data + expert * c->nb02;
    const char * src1 = c->wdata + (i11 * c->nbw1 + i2 * c->nbw2);
    char * dst = (char *) c->op->data + (i1 * c->nb1 + i2 * c->nb2);
    ggml_gemv_q4_K_8x8_q8_K((int) c->ne00, (float *) (dst) + s0s, c->ne01,
                            src0 + s0s * c->nb01, src1, 1, (int) (s0e - s0s));
}
extern "C" void fg006_rc_witness(int64_t mode, int64_t value) { (void) mode; (void) value; }

static void rc_init(ggml_compute_params * p, ggml_tensor * op) {
    fg006_rc_ctx * c = new fg006_rc_ctx();
    c->params = p; c->op = op;
    c->ne00 = op->src[0]->ne[0]; c->ne01 = op->src[0]->ne[1];
    c->ne02 = op->src[0]->ne[2]; c->ne10 = op->src[1]->ne[0];
    c->ne11 = op->src[1]->ne[1]; c->ne12 = op->src[1]->ne[2];
    c->nb0 = op->nb[0];
    c->nb01 = op->src[0]->nb[1]; c->nb02 = op->src[0]->nb[2];
    c->nb11 = op->src[1]->nb[1]; c->nb12 = op->src[1]->nb[2];
    c->nb1 = op->nb[1]; c->nb2 = op->nb[2];
    c->wdata = (char *) p->wdata;
    c->nbw1 = ggml_row_size(GGML_TYPE_Q8_K, c->ne10);
    c->nbw2 = c->nbw1 * c->ne11;
    c->src1_col_stride = c->nbw1;
    c->row_counts = nullptr; c->rows = nullptr;
    if (op->op == GGML_OP_MUL_MAT_ID) {
        const size_t nbw3 = c->nbw2 * c->ne12;
        char * end = c->wdata + GGML_PAD(nbw3, sizeof(int64_t));
        c->row_counts = (int64_t *) end;
        c->rows = (fg006_rc_ctx::row_map *) (c->row_counts + c->ne02);
    }
    g_rc = c;
}
static void rc_done(void) { delete g_rc; g_rc = nullptr; }

extern "C" int fg006_rc_forward_mul_mat(ggml_compute_params * p, ggml_tensor * op) {
    if (!fg006_rc_enabled()) return 0;
    rc_init(p, op);
    fg006_rc_entry(9, p->ith, p->nth, g_rc->ne11, g_rc->ne12, g_rc->ne10, 0, 0, 0);
    fg006_rc_entry(5, p->ith, p->nth, g_rc->ne01, g_rc->ne01, g_rc->ne11, g_rc->ne12, ggml_is_numa(), 0);
    rc_done(); return 1;
}
extern "C" int fg006_rc_forward_mul_mat_id(ggml_compute_params * p, ggml_tensor * op) {
    if (!fg006_rc_enabled()) return 0;
    rc_init(p, op);
    fg006_rc_entry(6, p->ith, p->nth, g_rc->ne01, g_rc->ne11, g_rc->ne12, g_rc->ne02,
                   op->src[2]->ne[0], op->src[2]->ne[1]);
    rc_done(); return 1;
}
