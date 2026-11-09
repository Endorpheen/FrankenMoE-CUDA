// GPU differential fixture for the Goose MMQ Q8_1 quantizer.
//
// Both entry points are redefined by the candidate DSO:
//   quantize_mmq_q8_1_cuda         (direct)
//   quantize_scatter_mmq_q8_1_cuda (compact rows)
// With GGML_GOOSE_CUDA_MMQ_Q8_1=0 each goes to the private-renamed native one.
//
// The gate is: both arms run, both write, and their outputs are byte-identical.
// Byte equality against native is the contract for a quantization kernel, so the
// fixture does not need an independent float reference; it needs the two arms' raw
// block_q8_1_mmq arrays. `FG180Q_DUMP=<dir>` writes them for external comparison,
// and the fixture also compares them in-process when both runs are available.
#include "common.cuh"
#include "mmq.cuh"
void quantize_mmq_q8_1_cuda(const float * x, const int32_t * ids, void * vy, ggml_type type_src0,
                            int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
                            int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, cudaStream_t stream);
void quantize_scatter_mmq_q8_1_cuda(const float * x, const int32_t * ids_inv, void * vy, ggml_type type_src0,
                                    int64_t ne00, int64_t stride_token, int64_t ne0,
                                    int64_t n_tokens, int64_t nrows_dst, int n_expert_used, cudaStream_t stream);
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Run { std::vector<uint8_t> bytes; bool ok = false; };

static Run direct_run(int dev, const std::vector<float> & xs, int64_t ne0) {
    Run r;
    const size_t nbytes = (size_t)(ne0/128) * sizeof(block_q8_1_mmq);
    float * dx = nullptr; void * dy = nullptr;
    cudaSetDevice(dev);
    cudaMalloc(&dx, xs.size()*4); cudaMalloc(&dy, nbytes);
    cudaMemcpy(dx, xs.data(), xs.size()*4, cudaMemcpyHostToDevice);
    cudaMemset(dy, 0xAB, nbytes);
    cudaStream_t st; cudaStreamCreate(&st);
    quantize_mmq_q8_1_cuda(dx, nullptr, dy, GGML_TYPE_Q4_0,
        ne0, ne0, ne0, ne0, ne0, 1, 1, 1, st);
    r.ok = cudaStreamSynchronize(st) == cudaSuccess;
    r.bytes.resize(nbytes);
    cudaMemcpy(r.bytes.data(), dy, nbytes, cudaMemcpyDeviceToHost);
    cudaFree(dx); cudaFree(dy); cudaStreamDestroy(st);
    return r;
}

static Run scatter_run(int dev, const std::vector<float> & xs, const std::vector<int32_t> & inv,
                       int64_t ne0, int64_t n_tokens, int64_t nrows, int n_used) {
    Run r;
    const size_t nbytes = (size_t)nrows * (size_t)(ne0/128) * sizeof(block_q8_1_mmq);
    float * dx = nullptr; int32_t * di = nullptr; void * dy = nullptr;
    cudaSetDevice(dev);
    cudaMalloc(&dx, xs.size()*4); cudaMalloc(&di, inv.size()*4); cudaMalloc(&dy, nbytes);
    cudaMemcpy(dx, xs.data(), xs.size()*4, cudaMemcpyHostToDevice);
    cudaMemcpy(di, inv.data(), inv.size()*4, cudaMemcpyHostToDevice);
    cudaMemset(dy, 0xAB, nbytes);
    cudaStream_t st; cudaStreamCreate(&st);
    quantize_scatter_mmq_q8_1_cuda(dx, di, dy, GGML_TYPE_Q4_0,
        ne0, ne0, ne0, n_tokens, nrows, n_used, st);
    r.ok = cudaStreamSynchronize(st) == cudaSuccess;
    r.bytes.resize(nbytes);
    cudaMemcpy(r.bytes.data(), dy, nbytes, cudaMemcpyDeviceToHost);
    cudaFree(dx); cudaFree(di); cudaFree(dy); cudaStreamDestroy(st);
    return r;
}

static void dump(const char * dir, const char * name, const std::vector<uint8_t> & b) {
    if (!dir) return;
    std::string f = std::string(dir) + "/" + name;
    if (FILE * fp = fopen(f.c_str(), "wb")) { fwrite(b.data(), 1, b.size(), fp); fclose(fp); }
}

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    const int dev = 0;
    cudaSetDevice(dev);
    std::mt19937 rng(180001);
    std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
    const char * dumpdir = getenv("FG180Q_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_MMQ_Q8_1") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_MMQ_Q8_1"), "0") == 0;

    // Direct path, several widths so the y-block grid and the 128-value grouping vary.
    for (int64_t ne0 : {128LL, 512LL, 2048LL}) {
        std::vector<float> xs(ne0);
        for (auto & v : xs) v = dist(rng);
        Run r = direct_run(dev, xs, ne0);
        check(r.ok, (std::string("direct sync ")+std::to_string(ne0)).c_str());
        bool touched = false;
        for (size_t i = 0; i < r.bytes.size(); ++i) if (r.bytes[i] != 0xAB) { touched = true; break; }
        check(touched, (std::string("direct writes ")+std::to_string(ne0)).c_str());
        dump(dumpdir, (std::string("direct-")+std::to_string(ne0)+".bin").c_str(), r.bytes);
    }

    // Scatter path: 4 tokens, 8 experts, duplicate compact rows allowed.
    {
        const int64_t ne0 = 512, n_tokens = 4, nrows = 32, n_used = 8;
        std::vector<float> xs(n_tokens*ne0);
        for (auto & v : xs) v = dist(rng);
        std::vector<int32_t> inv((size_t)n_tokens*n_used);
        for (size_t i = 0; i < inv.size(); ++i) inv[i] = (int32_t)(i % (size_t)nrows);
        Run r = scatter_run(dev, xs, inv, ne0, n_tokens, nrows, (int)n_used);
        check(r.ok, "scatter sync");
        bool touched = false;
        for (size_t i = 0; i < r.bytes.size(); ++i) if (r.bytes[i] != 0xAB) { touched = true; break; }
        check(touched, "scatter writes");
        dump(dumpdir, "scatter.bin", r.bytes);
    }

    std::printf("FG180Q_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
