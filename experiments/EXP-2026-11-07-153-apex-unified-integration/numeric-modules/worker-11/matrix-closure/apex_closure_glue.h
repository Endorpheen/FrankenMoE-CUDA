#pragma once
// Strong C symbols exported by matrix-closure/closure_device.cu.  The two
// patched translation units (mmq.cu, quantize.cu) only call these.
#include <cuda_runtime.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int fgc_cuda_enabled(void);

int fgc_cuda_quant_row_q8_1(const float * x, void * vy,
                            int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
                            int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
                            cudaStream_t stream);

int fgc_cuda_quant_mmq_q8_1(const float * x, void * vy,
                            int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
                            int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
                            int64_t layout, cudaStream_t stream);

int fgc_cuda_mmq_dense(int64_t kind, const void * w, const float * x, float * dst,
                       int64_t ne00, int64_t ne01, int64_t ne11,
                       int64_t s01, int64_t dst_stride,
                       int64_t s11, int64_t s12, int64_t s13,
                       int64_t ne12, int64_t ne13,
                       int cc, int64_t smpbo, int64_t nsm, cudaStream_t stream);

int fgc_cuda_gemm_dense(int64_t kind, const void * a, const float * b, float * c,
                        int64_t m, int64_t n, int64_t k,
                        int64_t b_stride, cudaStream_t stream);

int64_t fgc_cuda_route(int64_t kind, int64_t batch, int64_t dense, int64_t cc,
                       int64_t smpbo, int64_t ne00, int64_t ne11, int64_t nsm,
                       int64_t * reason);

int64_t fgc_cuda_plan(int64_t kind, int64_t ne00, int64_t ne01, int64_t ne11,
                      int64_t smpbo, int64_t cc, int64_t nsm, int64_t * out, int64_t nout);

#ifdef __cplusplus
}
#endif
