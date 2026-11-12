// Differential fixture for the MXFP4 MMQ activation quantizer.
#include "common.cuh"
#include "mmq.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

void quantize_mmq_fp4_cuda(const float * x, const int32_t * ids, void * vy, float * scale,
    ggml_type type_src0, bool use_aligned_float8, int64_t ne00, int64_t s01, int64_t s02,
    int64_t s03, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, cudaStream_t stream);
void quantize_scatter_mmq_fp4_cuda(const float * x, const int32_t * ids_inv, void * vy, float * scale,
    ggml_type type_src0, bool use_aligned_float8, int64_t ne00, int64_t stride_token,
    int64_t ne0, int64_t n_tokens, int64_t nrows_dst, int n_expert_used, cudaStream_t stream);

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}
static void arm(bool goose) { setenv("GGML_GOOSE_CUDA_MXFP4", goose ? "1" : "0", 1); }

static std::vector<uint8_t> direct_run(const std::vector<float> & xs, int64_t ne0, bool goose) {
    const size_t nbytes = (size_t)(ne0/256) * sizeof(block_fp4_mmq);
    float * dx=nullptr; void * dy=nullptr;
    cudaMalloc(&dx, xs.size()*4); cudaMalloc(&dy, nbytes);
    cudaMemcpy(dx, xs.data(), xs.size()*4, cudaMemcpyHostToDevice);
    cudaMemset(dy, 0xAB, nbytes);
    cudaStream_t st; cudaStreamCreate(&st);
    arm(goose);
    quantize_mmq_fp4_cuda(dx, nullptr, dy, nullptr, GGML_TYPE_MXFP4, false,
        ne0, ne0, ne0, ne0, ne0, 1, 1, 1, st);
    cudaStreamSynchronize(st);
    std::vector<uint8_t> out(nbytes);
    cudaMemcpy(out.data(), dy, nbytes, cudaMemcpyDeviceToHost);
    cudaFree(dx); cudaFree(dy); cudaStreamDestroy(st);
    return out;
}

static std::vector<uint8_t> scatter_run(const std::vector<float> & xs, const std::vector<int32_t> & inv,
        int64_t ne0, int64_t n_tokens, int64_t nrows, int n_used, bool goose) {
    const size_t nbytes = (size_t)nrows * (size_t)(ne0/256) * sizeof(block_fp4_mmq);
    float * dx=nullptr; int32_t * di=nullptr; void * dy=nullptr;
    cudaMalloc(&dx, xs.size()*4); cudaMalloc(&di, inv.size()*4); cudaMalloc(&dy, nbytes);
    cudaMemcpy(dx, xs.data(), xs.size()*4, cudaMemcpyHostToDevice);
    cudaMemcpy(di, inv.data(), inv.size()*4, cudaMemcpyHostToDevice);
    cudaMemset(dy, 0xAB, nbytes);
    cudaStream_t st; cudaStreamCreate(&st);
    arm(goose);
    quantize_scatter_mmq_fp4_cuda(dx, di, dy, nullptr, GGML_TYPE_MXFP4, false,
        ne0, ne0, ne0, n_tokens, nrows, n_used, st);
    cudaStreamSynchronize(st);
    std::vector<uint8_t> out(nbytes);
    cudaMemcpy(out.data(), dy, nbytes, cudaMemcpyDeviceToHost);
    cudaFree(dx); cudaFree(di); cudaFree(dy); cudaStreamDestroy(st);
    return out;
}

int main(void) {
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(431001);
    std::uniform_real_distribution<float> dist(-4.0f, 4.0f);

    for (int64_t ne0 : {512LL, 1024LL, 2048LL}) {
        std::vector<float> xs(ne0);
        for (auto & v : xs) v = dist(rng);
        auto nat = direct_run(xs, ne0, false);
        auto goo = direct_run(xs, ne0, true);
        check(nat.size() == goo.size(), "direct size");
        check(memcmp(nat.data(), goo.data(), nat.size()) == 0,
              (std::string("direct bytes ")+std::to_string(ne0)).c_str());
        bool touched = false;
        for (auto b : goo) if (b != 0xAB) { touched = true; break; }
        check(touched, "direct writes");
    }
    {
        const int64_t ne0 = 512, n_tokens = 4, nrows = 32; const int n_used = 8;
        std::vector<float> xs(n_tokens*ne0);
        for (auto & v : xs) v = dist(rng);
        std::vector<int32_t> inv((size_t)n_tokens*n_used);
        for (size_t i = 0; i < inv.size(); ++i) inv[i] = (int32_t)(i % (size_t)nrows);
        auto nat = scatter_run(xs, inv, ne0, n_tokens, nrows, n_used, false);
        auto goo = scatter_run(xs, inv, ne0, n_tokens, nrows, n_used, true);
        check(nat.size() == goo.size(), "scatter size");
        check(memcmp(nat.data(), goo.data(), nat.size()) == 0, "scatter bytes");
    }
    printf("FG431_MXFP4 checks=%d failures=%d\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
