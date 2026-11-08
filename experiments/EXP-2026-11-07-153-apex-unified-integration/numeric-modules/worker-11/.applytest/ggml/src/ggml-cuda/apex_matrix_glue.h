#pragma once
// Integration seam for the default-on Goose CUDA matrix path.
// The parent build compiles apex_matrix_device.cu and links this object; the
// patch in ggml/src/ggml-cuda/mmvq.cu only calls these entry points.
#include <cuda_runtime.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1 when GGML_GOOSE_CUDA_MATRIX is unset or not "0".
int fgm_cuda_enabled(void);
int64_t fgm_cuda_matrix_entry(int64_t mode, const void * w, int64_t wbytes,
                              const void * x, int64_t xbytes, float * y, int64_t ylen,
                              int64_t p0, int64_t p1, int64_t p2, int64_t p3);
int fgm_cuda_dequant(int64_t kind, const void * w, float * y, int64_t lanes, cudaStream_t stream);
int fgm_cuda_gemv(const void * w, int is_f16, int64_t nrows, int64_t k,
                  const float * x, float * y, cudaStream_t stream);
int fgm_cuda_gemm(const void * w, int is_f16, int64_t m, int64_t n, int64_t k,
                  const float * b, float * c, cudaStream_t stream);
int fgm_cuda_mul_mat_vec_kq(int64_t kind, const void * w, const void * q8, float * dst,
                            int64_t k, int64_t row_low, int64_t row_high,
                            int64_t stride_row_x, int64_t stride_col_y, cudaStream_t stream);
int fgm_cuda_mmq_kq(int64_t kind, const void * w, const void * q8, float * dst,
                    int64_t nrows, int64_t k, int64_t ncols,
                    int64_t stride_row_x, int64_t q8_col_blocks, cudaStream_t stream);
int fgm_cuda_mmq_sram_probe(int64_t kind, int64_t I, int64_t stride,
                            int64_t * qs, int64_t * need, int64_t * cap, cudaStream_t stream);

#ifdef __cplusplus
}
#endif
