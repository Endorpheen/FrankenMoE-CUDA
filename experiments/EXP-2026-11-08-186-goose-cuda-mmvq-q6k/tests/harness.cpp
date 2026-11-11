// Single-call harness: initialize the CUDA backend, build Q6_K x F32 tensors, call the dispatched
// ggml_cuda_mul_mat_vec_q once. Built with ASan on the host part so the first bad write is reported.
#include "common.cuh"
#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "mmvq.cuh"
#include "ggml-backend-impl.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>

int main() {
    ggml_backend_t be = ggml_backend_cuda_init(0);
    if (!be) { printf("no backend\n"); return 1; }
    ggml_init_params ip{};
    ip.mem_size = 64u * 1024 * 1024;
    ip.mem_buffer = nullptr;
    ip.no_alloc = true;
    ggml_context * ctx = ggml_init(ip);
    ggml_tensor * src0 = ggml_new_tensor_2d(ctx, GGML_TYPE_Q6_K, 2048, 4);
    ggml_tensor * src1 = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 2048, 1);
    ggml_tensor * dst  = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 4, 1);
    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors(ctx, be);
    if (!buf) { printf("no buffer\n"); return 1; }
    std::mt19937 rng(186);
    std::vector<unsigned char> q6(ggml_nbytes(src0)), f32(ggml_nbytes(src1));
    for (auto & b : q6) b = (unsigned char) rng();
    float * f = (float *) f32.data();
    for (size_t i = 0; i < f32.size() / 4; ++i) f[i] = 0.001f * (float)((int)(rng() % 1000) - 500);
    ggml_backend_tensor_set(src0, q6.data(), 0, q6.size());
    ggml_backend_tensor_set(src1, f32.data(), 0, f32.size());
    auto * c = static_cast<ggml_backend_cuda_context *>(be->context);
    printf("harness: calling ggml_cuda_mul_mat_vec_q\n"); fflush(stdout);
    ggml_cuda_mul_mat_vec_q(*c, src0, src1, nullptr, dst, nullptr);
    cudaDeviceSynchronize();
    printf("harness: returned\n");
    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    ggml_backend_free(be);
    return 0;
}
