// Kernel-level timing for the MMQ Q8_1 quantizer, both arms.
//
// The end-to-end Apex delta is noisy, so this isolates the operator: it launches
// the real entry point many times on a fixed input and reports the elapsed time
// per launch. Alternating arms on the same process would be wrong here because the
// switch is read per call from the environment, so each arm is a separate process
// and the runs are interleaved by the caller.
#include "common.cuh"
#include "mmq.cuh"
void quantize_mmq_q8_1_cuda(const float * x, const int32_t * ids, void * vy, ggml_type type_src0,
                            int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
                            int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, cudaStream_t stream);
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>

int main() {
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) return 77;
    cudaSetDevice(dev);
    // A shape close to the Apex prefill inner size: 4096 columns, 8 rows per launch.
    const int64_t ne0 = 4096, ne1 = 8;
    const size_t nbytes = (size_t)(ne0/128) * (size_t)ne1 * sizeof(block_q8_1_mmq);
    std::vector<float> xs((size_t)ne0*ne1);
    std::mt19937 rng(180777);
    std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
    for (auto & v : xs) v = dist(rng);
    float * dx = nullptr; void * dy = nullptr;
    cudaMalloc(&dx, xs.size()*4); cudaMalloc(&dy, nbytes);
    cudaMemcpy(dx, xs.data(), xs.size()*4, cudaMemcpyHostToDevice);
    cudaStream_t st; cudaStreamCreate(&st);

    const int iters = 2000;
    for (int i = 0; i < 100; ++i)  // warmup
        quantize_mmq_q8_1_cuda(dx, nullptr, dy, GGML_TYPE_Q4_0,
            ne0, ne0, ne0, ne0, ne0, ne1, 1, 1, st);
    cudaStreamSynchronize(st);

    std::vector<double> samples;
    for (int r = 0; r < 5; ++r) {
        cudaEvent_t a, b; cudaEventCreate(&a); cudaEventCreate(&b);
        cudaEventRecord(a, st);
        for (int i = 0; i < iters; ++i)
            quantize_mmq_q8_1_cuda(dx, nullptr, dy, GGML_TYPE_Q4_0,
                ne0, ne0, ne0, ne0, ne0, ne1, 1, 1, st);
        cudaEventRecord(b, st);
        cudaEventSynchronize(b);
        float ms = 0.0f; cudaEventElapsedTime(&ms, a, b);
        samples.push_back((double)ms * 1e6 / iters);   // ns per launch
        cudaEventDestroy(a); cudaEventDestroy(b);
    }
    std::sort(samples.begin(), samples.end());
    const char * arm = (getenv("GGML_GOOSE_CUDA_MMQ_Q8_1") &&
                        std::strcmp(getenv("GGML_GOOSE_CUDA_MMQ_Q8_1"), "0") == 0) ? "native" : "goose";
    std::printf("FG180Q_TIMING arm=%s median_ns_per_launch=%.1f min=%.1f max=%.1f iters=%d\n",
        arm, samples[samples.size()/2], samples.front(), samples.back(), iters);
    cudaFree(dx); cudaFree(dy); cudaStreamDestroy(st);
    return 0;
}
