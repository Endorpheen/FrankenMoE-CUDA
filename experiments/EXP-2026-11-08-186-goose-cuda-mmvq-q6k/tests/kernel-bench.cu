// Kernel-time microbenchmark for the Goose mul_mat_vec_q Q6_K against the native one.
//
// The Apex pair showed decode -3.66% with prefill flat. Before attributing that to the
// kernel, measure the kernel itself on the real shapes: if the Goose body is several
// times slower per launch on the mmvq path, the decode cost follows directly.
//
// Both arms run in the same process through ggml_cuda_mul_mat_vec_q, so the DSO, the
// driver and the clocks are shared. GGML_GOOSE_CUDA_MMVQ_Q6K selects the arm.
#include "common.cuh"
#include "mmvq.cuh"
#include "ggml-backend.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>

static float median(std::vector<float> v) { std::sort(v.begin(), v.end()); return v[v.size()/2]; }

int main(int argc, char ** argv) {
    if (ggml_backend_cuda_get_device_count() < 1) return 77;
    cudaSetDevice(0);
    const int64_t ncols = argc > 1 ? atoll(argv[1]) : 4096;
    const int64_t nrows = argc > 2 ? atoll(argv[2]) : 5120;
    const int reps      = argc > 3 ? atoi(argv[3]) : 60;
    const int64_t nb = ncols/256;

    std::vector<uint8_t> x((size_t)(nb*nrows)*210);
    std::mt19937 rng(186002);
    for (auto & v : x) v = (uint8_t)rng();
    for (size_t b = 0; b < x.size()/210; ++b) {
        half d = __float2half(0.01f);
        std::memcpy(x.data()+b*210+208, &d, 2);
        for (int s = 0; s < 16; ++s) x[b*210+192+s] = (uint8_t)(int8_t)((s%5)-2);
    }
    std::vector<float> y(ncols);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    for (auto & v : y) v = dist(rng);
    std::vector<float> out(nrows, 0.f);

    uint8_t * dx=nullptr; float * dy=nullptr, * dd=nullptr;
    cudaMalloc(&dx, x.size()); cudaMalloc(&dy, y.size()*4); cudaMalloc(&dd, out.size()*4);
    cudaMemcpy(dx, x.data(), x.size(), cudaMemcpyHostToDevice);
    cudaMemcpy(dy, y.data(), y.size()*4, cudaMemcpyHostToDevice);

    ggml_tensor s0{}, s1{}, d{};
    s0.type=GGML_TYPE_Q6_K; s1.type=d.type=GGML_TYPE_F32;
    s0.ne[0]=ncols; s0.ne[1]=nrows; s0.ne[2]=1; s0.ne[3]=1;
    s0.nb[0]=210; s0.nb[1]=nb*210; s0.nb[2]=s0.nb[1]*nrows; s0.nb[3]=s0.nb[2];
    s1.ne[0]=ncols; s1.ne[1]=1; s1.ne[2]=1; s1.ne[3]=1;
    s1.nb[0]=4; s1.nb[1]=ncols*4; s1.nb[2]=s1.nb[1]; s1.nb[3]=s1.nb[2];
    d.ne[0]=nrows; d.ne[1]=1; d.ne[2]=1; d.ne[3]=1;
    d.nb[0]=4; d.nb[1]=nrows*4; d.nb[2]=d.nb[1]; d.nb[3]=d.nb[2];
    s0.data=dx; s1.data=dy; d.data=dd;
    ggml_backend_buffer_type_t buft = ggml_backend_cuda_buffer_type(0);
    ggml_backend_buffer_t bx=ggml_backend_buft_alloc_buffer(buft,x.size());
    ggml_backend_buffer_t by=ggml_backend_buft_alloc_buffer(buft,y.size()*4);
    ggml_backend_buffer_t bd=ggml_backend_buft_alloc_buffer(buft,out.size()*4);
    s0.buffer=bx; s1.buffer=by; d.buffer=bd;

    ggml_backend_cuda_context ctx(0);
    const bool is_native = getenv("GGML_GOOSE_CUDA_MMVQ_Q6K") &&
                           !strcmp(getenv("GGML_GOOSE_CUDA_MMVQ_Q6K"),"0");
    std::vector<float> ms;
    for (int i = 0; i < reps; ++i) {
        cudaEvent_t a, b; cudaEventCreate(&a); cudaEventCreate(&b);
        cudaEventRecord(a);
        ggml_cuda_mul_mat_vec_q(ctx, &s0, &s1, nullptr, &d, nullptr);
        cudaEventRecord(b);
        cudaEventSynchronize(b);
        float t = 0; cudaEventElapsedTime(&t, a, b);
        if (i >= reps/4) ms.push_back(t);
        cudaEventDestroy(a); cudaEventDestroy(b);
    }
    printf("FG186Q_MICROBENCH arm=%s ncols=%lld nrows=%lld launches=%zu median_ms=%.3f\n",
           is_native?"native":"goose",(long long)ncols,(long long)nrows,ms.size(),median(ms));
    return 0;
}
